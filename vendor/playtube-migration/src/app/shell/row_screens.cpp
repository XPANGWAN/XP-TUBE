#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "app/shell/cards.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/nav_bar.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/states.hpp"
#include "youtube/search_filters.hpp"
#include "app/shell/video_pages.hpp"
#include "core/log.hpp"
#include "ui/i18n.hpp"
#include "ui/reveal.hpp"
#include "ui/scroll.hpp"

namespace playtube::app {

namespace {

using Fetch = VideoPages::Fetch;

// A tab of titled rows of videos, each fetched on its own and shown when it arrives; the reason and "Réessayer"
// when none came.
class RowsScreen : public ShellScreen {
public:
    RowsScreen(AppSession& session, Tab tab) : ShellScreen(session), tab_(tab) {
        scroll_ = add<ui::VScroll>();
        scroll_->leadingInset = ui::theme().spacingXxl;
        auto content = std::make_unique<ui::Box>(ui::Axis::Column);
        content->padding = pagePadding();
        content->gap = ui::theme().rowGap;
        column_ = content.get();
        auto* bar = column_->add<NavBar>(session_, tab);
        scroll_->setHeader(bar);
        setStart(bar->currentTab());
        rows_ = column_->add<ui::Box>(ui::Axis::Column);
        rows_->width = ui::SizeSpec::fill();
        rows_->gap = ui::theme().rowGap;
        messages_ = column_->add<ui::Box>(ui::Axis::Column);
        scroll_->setContent(std::move(content));
    }

protected:
    virtual void addRows() = 0;
    void addRow(const std::string& title, First first, Fetch more) {
        auto* list = addVideoRow(*rows_, title);
        Row row;
        row.section = list->parent();
        row.section->visible = false;  // shown when its videos arrive
        row.pages = std::make_unique<VideoPages>(*this, *list, std::move(more), playAction(session_),
                                                 channelAction(*this));
        row.pages->hideShorts = !session_.settings().shorts;
        rowsData_.push_back(std::move(row));
        firsts_.push_back(std::move(first));
    }

    void load() {
        ++generation_;
        moveFocusOff(rows_);
        clearMessages(*this, *messages_);
        rowsData_.clear();
        firsts_.clear();
        rows_->clearChildren();
        addRows();
        for (std::size_t i = 0; i < rowsData_.size(); ++i) {
            fetchRow(i);
        }
    }

    ui::Box* messages_;
    Tab tab_;

private:
    struct Row {
        ui::View* section = nullptr;
        std::unique_ptr<VideoPages> pages;
        bool done = false;
        bool failed = false;
        core::Error error;
    };

    void fetchRow(std::size_t index) {
        auto first = firsts_[index];
        const core::CancelToken cancel = token_;
        const unsigned generation = generation_;
        run<core::Result<youtube::VideoPage>>(
            [first, cancel] { return first(cancel); },
            [this, index, generation](core::Result<youtube::VideoPage> page) {
                if (generation == generation_) {
                    showRow(index, std::move(page));
                }
            });
    }

    void showRow(std::size_t index, core::Result<youtube::VideoPage> page) {
        Row& row = rowsData_[index];
        row.done = true;
        if (!page.ok()) {
            PF_LOG_WARN("row: %s", page.error().message.c_str());
            if (page.error().kind == core::ErrorKind::Unauthorized && !session_.account().signedIn()) {
                // the sign-in was revoked: the tab again, signed out
                AppSession* session = &session_;
                const Tab tab = tab_;
                later([session, tab] { openTab(*session, tab); });
                return;
            }
            row.failed = true;
            row.error = page.error();
        } else if (row.pages->hideShorts && (dropShorts(page.value()), page.value().items.empty())) {
            // only Shorts: the row stays hidden
        } else if (!page.value().items.empty()) {
            row.pages->show(std::move(page.value()));
            row.section->visible = true;
            ui::revealChildren(*row.section, 0);
        }
        focusTopRow();
        checkAllFailed();
    }

    // rows answer in any order: the focus goes to the top row shown, once every row above it answered
    void focusTopRow() {
        for (const Row& row : rowsData_) {
            if (!row.done) {
                return;
            }
            if (row.section->visible) {
                focusContent(row.section);
                return;
            }
        }
    }

    // nothing to show once every row answered: the reason and "Réessayer"
    void checkAllFailed() {
        const core::Error* error = nullptr;
        for (const Row& row : rowsData_) {
            if (!row.done || row.section->visible) {
                return;
            }
            if (row.failed && error == nullptr) {
                error = &row.error;
            }
        }
        if (error != nullptr) {
            showFailure(*this, *messages_, *error, [this] { load(); });
        } else {
            showText(*this, *messages_, ui::tr("home.empty"));
        }
    }

    ui::VScroll* scroll_;
    ui::Box* column_;
    ui::Box* rows_;
    std::vector<Row> rowsData_;
    std::vector<First> firsts_;
    unsigned generation_ = 0;
};

class HomeScreen : public RowsScreen {
public:
    explicit HomeScreen(AppSession& session) : RowsScreen(session, Tab::Home) { load(); }

private:
    void addRows() override {
        AppSession* session = &session_;
        if (session_.account().signedIn()) {
            addRow(ui::tr("home.for_you"), signedInFeed(session_, "FEwhat_to_watch"), signedInFeedMore(session_));
            addRow(ui::tr("shell.tab.subscriptions"), signedInFeed(session_, "FEsubscriptions"),
                   signedInFeedMore(session_));
        }
        const std::vector<std::string> topics = homeTopics(session_);
        for (std::size_t i = 0; i < topics.size(); ++i) {
            const std::string topic = topics[i];
            First first = [session, topic](const core::CancelToken& cancel) {
                return session->youtube().search(topic, cancel);
            };
            addRow(topic, std::move(first), [session](const std::string& token, const core::CancelToken& cancel) {
                return session->youtube().searchMore(token, cancel);
            });
            if (i == 0 && session_.settings().shorts) {  // Shorts of the first topic, after its row
                youtube::SearchFilters shorts;
                shorts.type = youtube::SearchFilters::Type::Short;
                const std::string params = youtube::searchParams(shorts);
                // only the Shorts: a search for a creator's name also answers their channel
                const auto onlyShorts = [](core::Result<youtube::VideoPage> page) {
                    if (page.ok()) {
                        auto& items = page.value().items;
                        items.erase(std::remove_if(items.begin(), items.end(),
                                                   [](const youtube::Video& video) { return !video.isShort; }),
                                    items.end());
                    }
                    return page;
                };
                First firstShorts = [session, topic, params, onlyShorts](const core::CancelToken& cancel) {
                    return onlyShorts(session->youtube().search(topic, cancel, params));
                };
                addRow(ui::tr("video.short"), std::move(firstShorts),
                       [session, onlyShorts](const std::string& token, const core::CancelToken& cancel) {
                           return onlyShorts(session->youtube().searchMore(token, cancel));
                       });
            }
        }
    }
};

// Bibliothèque: the history, "watch later" and the playlists; an invitation to sign in when signed out.
class LibraryScreen : public RowsScreen {
public:
    explicit LibraryScreen(AppSession& session) : RowsScreen(session, Tab::Library) {
        if (!session_.account().signedIn()) {
            addSignInInvitation(*this, *messages_, ui::tr("library.signed_out"), Tab::Library);
            return;
        }
        load();
    }

private:
    void addRows() override {
        addRow(ui::tr("library.history"), signedInFeed(session_, "FEhistory"), signedInFeedMore(session_));
        addRow(ui::tr("library.watch_later"), signedInFeed(session_, "VLWL"), signedInFeedMore(session_));
        addRow(ui::tr("library.playlists"), signedInFeed(session_, "FEplaylist_aggregation"),
               signedInFeedMore(session_));
    }
};

}  // namespace

void addSignInInvitation(ShellScreen& screen, ui::Box& column, const std::string& text, Tab tab) {
    AppSession* session = &screen.session();
    addMessage(column, text, ui::tr("shell.signin"), [session, tab] {
        openSignIn(*session, [session, tab] { openTab(*session, tab); });
    });
}

std::unique_ptr<ui::Screen> makeLibrary(AppSession& session) { return std::make_unique<LibraryScreen>(session); }

std::vector<std::string> defaultTopics() {
    return {ui::tr("home.topic.music"), ui::tr("home.topic.gaming"), ui::tr("home.topic.news"),
            ui::tr("home.topic.sport"), ui::tr("home.topic.science")};
}

std::vector<std::string> homeTopics(const AppSession& session) {
    return session.settings().customTopics ? session.settings().homeTopics : defaultTopics();
}

std::unique_ptr<ui::Screen> makeHome(AppSession& session) { return std::make_unique<HomeScreen>(session); }

}  // namespace playtube::app
