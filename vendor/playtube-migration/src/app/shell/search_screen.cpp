#include <memory>
#include <string>
#include <vector>

#include "app/settings.hpp"
#include "app/shell/cards.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/nav_bar.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/states.hpp"
#include "app/shell/video_pages.hpp"
#include "core/strings.hpp"
#include "core/utf.hpp"
#include "ui/i18n.hpp"
#include "ui/reveal.hpp"
#include "ui/widgets.hpp"
#include "youtube/links.hpp"
#include "youtube/search_filters.hpp"

namespace playtube::app {

namespace {

constexpr std::size_t kSuggestions = 4;      // in one row: four short ones fit the width
constexpr std::size_t kButtonTextBytes = 28;

// a term short enough for its button
std::string buttonText(const std::string& term) {
    const std::string_view shown = core::utf8Prefix(term, kButtonTextBytes);
    return shown.size() < term.size() ? std::string(shown) + "\xE2\x80\xA6" : term;
}

class SearchScreen : public ShellScreen {
public:
    explicit SearchScreen(AppSession& session) : ShellScreen(session) {
        column_ = add<ui::Box>(ui::Axis::Column);
        column_->width = ui::SizeSpec::fill();
        column_->height = ui::SizeSpec::fill();
        column_->padding = pagePadding();
        column_->padding.bottom = 0;
        column_->gap = ui::theme().spacingL;
        column_->add<NavBar>(session_, Tab::Search);
        platform::TextInputRequest request;
        request.title = ui::tr("shell.search.title");
        request.maxLength = 200;
        request.autoCapitalize = false;
        auto* line = column_->add<ui::Box>(ui::Axis::Row);
        line->gap = ui::theme().spacingM;
        line->crossAlign = ui::CrossAlign::Center;
        field_ = line->add<ui::TextField>(ui::tr("shell.search.field"), request,
                                          [this](const std::string& term) { submit(term); });
        field_->width = ui::SizeSpec::fixed(900);
        filtersButton_ = line->add<ui::Button>(ui::tr("search.filters"), [this] { later([this] { openFilters(); }); });
        setStart(field_);
        recent_ = column_->add<ui::Box>(ui::Axis::Column);
        recent_->gap = ui::theme().spacingM;
        suggestions_ = column_->add<ui::Box>(ui::Axis::Row);
        suggestions_->gap = ui::theme().spacingM;
        suggestions_->visible = false;
        messages_ = column_->add<ui::Box>(ui::Axis::Column);
        grid_ = addVideoGrid(*column_);
        grid_->visible = false;
        AppSession* shared = &session_;
        pages_ = std::make_unique<VideoPages>(
            *this, *grid_,
            [shared](const std::string& token, const core::CancelToken& cancel) {
                return shared->youtube().searchMore(token, cancel);
            },
            playAction(session_), channelAction(*this));
        showRecent();
    }

private:
    // the recent searches under the field while nothing is searched
    void showRecent() {
        moveFocusOff(recent_);
        recent_->clearChildren();
        const auto& terms = session_.settings().recentSearches;
        recent_->visible = !terms.empty();
        if (terms.empty()) {
            return;
        }
        auto* title = recent_->add<ui::Label>(ui::tr("shell.search.recent"), headingText());
        title->color = ui::theme().textSecondary;
        for (const std::string& term : terms) {
            recent_->add<ui::Button>(buttonText(term), [this, term] { later([this, term] { searchFor(term); }); });
        }
    }

    void submit(const std::string& typed) {
        if (const auto id = youtube::videoIdFrom(typed)) {
            openVideo(session_, *id);  // a pasted link plays
            return;
        }
        searchFor(typed);
    }

    void searchFor(const std::string& typed) {
        Settings probe;
        addRecentSearch(probe, typed);
        ++generation_;
        clearMessages(*this, *messages_);
        moveFocusOff(suggestions_);
        moveFocusOff(grid_);
        suggestions_->clearChildren();
        suggestions_->visible = false;
        grid_->visible = false;
        pages_->show({});
        if (probe.recentSearches.empty()) {  // nothing but spaces
            field_->setValue({});
            showRecent();
            return;
        }
        const std::string term = probe.recentSearches.front();
        field_->setValue(term);
        addRecentSearch(session_.settings(), term);
        session_.saveSettings();
        moveFocusOff(recent_);
        recent_->clearChildren();
        recent_->visible = false;
        showLoading(*this, *messages_);
        AppSession* shared = &session_;
        const core::CancelToken cancel = token_;
        const unsigned generation = generation_;
        const std::string params = youtube::searchParams(filters_);
        run<core::Result<youtube::VideoPage>>(
            [shared, term, cancel, params] { return shared->youtube().search(term, cancel, params); },
            [this, term, generation](core::Result<youtube::VideoPage> page) {
                if (generation == generation_) {
                    showResults(term, std::move(page));
                }
            });
        run<core::Result<std::vector<std::string>>>(
            [shared, term, cancel] { return shared->youtube().suggestions(term, cancel); },
            [this, term, generation](core::Result<std::vector<std::string>> words) {
                if (generation == generation_ && words.ok()) {
                    showSuggestions(term, words.value());
                }
            });
    }

    void showResults(const std::string& term, core::Result<youtube::VideoPage> page) {
        if (!page.ok()) {
            showFailure(*this, *messages_, page.error(), [this, term] { searchFor(term); });
            return;
        }
        if (page.value().items.empty()) {
            showText(*this, *messages_, ui::tr("shell.search.none", {term}));
            return;
        }
        clearMessages(*this, *messages_);
        pages_->show(std::move(page.value()));
        grid_->visible = true;
        ui::revealChildren(*column_, 3);
        focusContent(grid_);
    }

    // other searches YouTube suggests for the term, as buttons above the results
    void showSuggestions(const std::string& term, const std::vector<std::string>& words) {
        suggestions_->clearChildren();
        for (const std::string& word : words) {
            if (core::asciiLower(word) == core::asciiLower(term)) {
                continue;  // the term itself (the console keyboard capitalises the first letter)
            }
            suggestions_->add<ui::Button>(buttonText(word), [this, word] { later([this, word] { searchFor(word); }); });
            if (suggestions_->children().size() == kSuggestions) {
                break;
            }
        }
        suggestions_->visible = !suggestions_->children().empty();
    }

    // ---- filters: a list of the five, each opening its choices; a change searches again

    void openFilters() {
        using F = youtube::SearchFilters;
        const auto name = [](const char* key) { return ui::tr(key); };
        std::vector<std::pair<std::string, std::function<void()>>> buttons;
        buttons.emplace_back(ui::tr("search.filter.value", {name("search.filter.type"), typeName(filters_.type)}),
                             [this] { later([this] { chooseType(); }); });
        buttons.emplace_back(ui::tr("search.filter.value", {name("search.filter.date"), dateName(filters_.upload)}),
                             [this] { later([this] { chooseDate(); }); });
        buttons.emplace_back(ui::tr("search.filter.value", {name("search.filter.length"), lengthName(filters_.length)}),
                             [this] { later([this] { chooseLength(); }); });
        buttons.emplace_back(ui::tr("search.filter.value", {name("search.filter.sort"), sortName(filters_.sort)}),
                             [this] { later([this] { chooseSort(); }); });
        buttons.emplace_back(ui::tr("search.filter.value", {name("search.filter.live"), ui::tr(filters_.live ? "settings.on" : "settings.off")}),
                             [this] {
                                 filters_.live = !filters_.live;
                                 later([this] { applyFilters(); openFilters(); });
                             });
        if (filters_.any()) {
            buttons.emplace_back(ui::tr("search.filter.reset"), [this] {
                filters_ = F{};
                later([this] { applyFilters(); });
            });
        }
        ui::Application::current()->showModal(ui::makeDialog(ui::tr("search.filters"), "", std::move(buttons)));
    }

    static std::string typeName(youtube::SearchFilters::Type type) {
        static const char* const kKeys[] = {"any", "video", "channel", "playlist", "short"};
        return ui::tr(std::string("search.filter.type.") + kKeys[static_cast<int>(type)]);
    }
    static std::string dateName(youtube::SearchFilters::Upload upload) {
        static const char* const kKeys[] = {"any", "hour", "today", "week", "month", "year"};
        return ui::tr(std::string("search.filter.date.") + kKeys[static_cast<int>(upload)]);
    }
    static std::string lengthName(youtube::SearchFilters::Length length) {
        static const char* const kKeys[] = {"any", "under4", "medium", "over20"};
        return ui::tr(std::string("search.filter.length.") + kKeys[static_cast<int>(length)]);
    }
    static std::string sortName(youtube::SearchFilters::Sort sort) {
        static const char* const kKeys[] = {"relevance", "date", "views", "rating"};
        return ui::tr(std::string("search.filter.sort.") + kKeys[static_cast<int>(sort)]);
    }

    // a choice among `count` values named by `nameOf`; the chosen one applied by `apply`
    template <typename E, typename Name, typename Apply>
    void choose(const char* title, int count, E current, Name nameOf, Apply apply) {
        std::vector<std::string> names;
        for (int i = 0; i < count; ++i) {
            names.push_back(nameOf(static_cast<E>(i)));
        }
        ui::Application::current()->showModal(
            ui::makeChoiceDialog(ui::tr(title), names, static_cast<int>(current), [this, apply](int index) {
                apply(static_cast<E>(index));
                later([this] { applyFilters(); });
            }));
    }
    void chooseType() {
        choose("search.filter.type", 5, filters_.type, typeName, [this](auto v) { filters_.type = v; });
    }
    void chooseDate() {
        choose("search.filter.date", 6, filters_.upload, dateName, [this](auto v) { filters_.upload = v; });
    }
    void chooseLength() {
        choose("search.filter.length", 4, filters_.length, lengthName, [this](auto v) { filters_.length = v; });
    }
    void chooseSort() {
        choose("search.filter.sort", 4, filters_.sort, sortName, [this](auto v) { filters_.sort = v; });
    }

    // the button says how many filters are on; the current term is searched again with them
    void applyFilters() {
        int on = 0;
        on += filters_.type != youtube::SearchFilters::Type::Any;
        on += filters_.upload != youtube::SearchFilters::Upload::Any;
        on += filters_.length != youtube::SearchFilters::Length::Any;
        on += filters_.sort != youtube::SearchFilters::Sort::Relevance;
        on += filters_.live;
        filtersButton_->setText(on > 0 ? ui::tr("search.filters_count", {std::to_string(on)}) : ui::tr("search.filters"));
        const std::string term = field_->value();
        if (!core::trimmed(term).empty()) {
            searchFor(term);
        }
    }

    ui::Box* column_;
    ui::Button* filtersButton_ = nullptr;
    youtube::SearchFilters filters_;
    ui::TextField* field_;
    ui::Box* recent_;
    ui::Box* suggestions_;
    ui::Box* messages_;
    ui::Grid* grid_;
    std::unique_ptr<VideoPages> pages_;
    unsigned generation_ = 0;
};

}  // namespace

std::unique_ptr<ui::Screen> makeSearch(AppSession& session) { return std::make_unique<SearchScreen>(session); }

}  // namespace playtube::app
