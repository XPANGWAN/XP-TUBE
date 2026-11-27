#include <nanovg.h>

#include <algorithm>
#include <memory>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/settings.hpp"
#include "app/update.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/licenses.hpp"
#include "app/shell/nav_bar.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "core/data_dir.hpp"
#include "core/file_io.hpp"
#include "core/log.hpp"
#include "core/strings.hpp"
#include "core/utf.hpp"
#include "core/version.hpp"
#include "ui/draw.hpp"
#include "ui/i18n.hpp"
#include "ui/scroll.hpp"
#include "ui/widgets.hpp"

namespace playtube::app {

namespace {

constexpr std::size_t kMaxLicensesBytes = 256 * 1024;

std::string accentName(const std::string& id) { return ui::tr("settings.accent." + id); }

std::string languageName(const std::string& tag) {
    for (const auto& language : uiLanguages()) {
        if (language.first == tag) {
            return language.second;
        }
    }
    return ui::tr("settings.language.system");
}

std::string onOff(bool on) { return ui::tr(on ? "settings.on" : "settings.off"); }

std::string categoryName(const std::string& id) { return ui::tr("settings.sponsor." + id); }

const int kHeights[] = {360, 480, 720, 1080};

// The banner at the top of the settings while a newer Playtube is published: accent fill, the version, what to do,
// and the download's state on the right; ✕ does what the update row does.
class UpdateBanner : public ui::Box {
public:
    explicit UpdateBanner(std::function<void()> onPress) : ui::Box(ui::Axis::Row) {
        name = "update banner";
        focusable = true;
        drawsOwnFocus = true;
        width = ui::SizeSpec::fixed(1100);
        padding = {ui::theme().spacingL, ui::theme().spacingL, ui::theme().spacingL, ui::theme().spacingL};
        cornerRadius = ui::theme().radius;
        crossAlign = ui::CrossAlign::Center;
        gap = ui::theme().spacingL;
        auto* texts = add<ui::Box>(ui::Axis::Column);
        texts->width = ui::SizeSpec::fill();
        texts->gap = ui::theme().spacingS / 2;
        title_ = texts->add<ui::Label>("", ui::FontStyle{ui::theme().fontTitle, ui::FontWeight::SemiBold});
        title_->width = ui::SizeSpec::fill();
        text_ = texts->add<ui::Label>("", ui::FontStyle{ui::theme().fontBody, ui::FontWeight::Regular});
        text_->width = ui::SizeSpec::fill();
        text_->maxLines = 2;
        state_ = add<ui::Label>("", ui::FontStyle{ui::theme().fontBody, ui::FontWeight::SemiBold});
        refresh();
        bind(platform::Action::Confirm, ui::tr("ui.select"), [onPress = std::move(onPress)] {
            onPress();
            return true;
        });
    }
    void set(const std::string& title, const std::string& text, const std::string& state) {
        title_->setText(title);
        text_->setText(text);
        state_->setText(state);
    }
    void onFocusChanged(bool focused) override {
        focused_ = focused;
        refresh();
    }
    void onThemeChanged() override {
        refresh();
        ui::Box::onThemeChanged();
    }
    // focused: a white line inside its edge (the fill stays the accent, as the focused rows' is)
    void draw(NVGcontext* vg) const override {
        ui::Box::draw(vg);
        if (!focused_) {
            return;
        }
        const float inset = ui::theme().focusRingWidth / 2 + 2;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset, inset, frame.width - 2 * inset, frame.height - 2 * inset, cornerRadius - inset);
        nvgStrokeColor(vg, ui::nvgColor(ui::theme().onAccent));
        nvgStrokeWidth(vg, ui::theme().focusRingWidth);
        nvgStroke(vg);
    }

private:
    void refresh() {
        background = ui::theme().accent;
        title_->color = ui::theme().onAccent;
        text_->color = ui::withAlpha(ui::theme().onAccent, 0xD8);
        state_->color = ui::theme().onAccent;
        title_->markDirty();
        text_->markDirty();
        state_->markDirty();
        markDirty();
    }
    bool focused_ = false;
    ui::Label* title_;
    ui::Label* text_;
    ui::Label* state_;
};

ui::Label* addHeading(ui::Box& column, const std::string& text) {
    auto* heading = column.add<ui::Label>(text, ui::FontStyle{ui::theme().fontHeading, ui::FontWeight::SemiBold});
    heading->margin = {ui::theme().spacingL, 0, 0, 0};
    return heading;
}

class SettingsScreen : public ShellScreen {
public:
    explicit SettingsScreen(AppSession& session) : ShellScreen(session) {
        auto* scroll = add<ui::VScroll>();
        scroll->policy = ui::VScroll::Policy::Nearest;
        auto content = std::make_unique<ui::Box>(ui::Axis::Column);
        content->padding = pagePadding();
        content->gap = ui::theme().spacingM;
        ui::Box* column = content.get();
        column->add<NavBar>(session_, Tab::Settings);
        banner_ = column->add<UpdateBanner>([this] { pressUpdate(); });
        banner_->margin = {ui::theme().spacingS, 0, 0, 0};

        addHeading(*column, ui::tr("settings.display"));
        accent_ = column->add<ui::ValueRow>(ui::tr("settings.accent"), accentName(session_.settings().accent),
                                            [this] { chooseAccent(); });
        language_ = column->add<ui::ValueRow>(ui::tr("settings.language"), "", [this] { chooseLanguage(); });
        logo_ = column->add<ui::ValueRow>(ui::tr("settings.logo"), logoName(session_.settings().logo), [this] {
            Settings& settings = session_.settings();
            settings.logo = settings.logo == "youtube" ? "playtube" : "youtube";
            session_.saveSettings();
            logo_->setValue(logoName(settings.logo));
            session_.applyLogo();
        });
        refreshLanguage();
        remembered = accent_;  // the first setting, not the bar above it

        AppSession* shared = &session_;
        addHeading(*column, ui::tr("settings.playback"));
        quality_ = column->add<ui::ValueRow>(ui::tr("settings.quality"), heightName(session_.settings().maxHeight),
                                             [this] { chooseQuality(); });
        sponsor_ = column->add<ui::ValueRow>("SponsorBlock", onOff(session_.settings().sponsorBlock), [this] {
            session_.settings().sponsorBlock = !session_.settings().sponsorBlock;
            session_.saveSettings();
            sponsor_->setValue(onOff(session_.settings().sponsorBlock));
        });
        autoplay_ = column->add<ui::ValueRow>(ui::tr("settings.autoplay"), onOff(session_.settings().autoplay),
                                              [this] {
            session_.settings().autoplay = !session_.settings().autoplay;
            session_.saveSettings();
            autoplay_->setValue(onOff(session_.settings().autoplay));
        });
        subtitleSize_ = column->add<ui::ValueRow>(ui::tr("settings.subtitle_size"),
                                                  subtitleSizeName(session_.settings().subtitleSize),
                                                  [this] { chooseSubtitleSize(); });
        subtitleBackground_ = column->add<ui::ValueRow>(ui::tr("settings.subtitle_background"),
                                                        onOff(session_.settings().subtitleBackground), [this] {
            session_.settings().subtitleBackground = !session_.settings().subtitleBackground;
            session_.saveSettings();
            subtitleBackground_->setValue(onOff(session_.settings().subtitleBackground));
        });
        dislikes_ = column->add<ui::ValueRow>(ui::tr("settings.dislikes"), onOff(session_.settings().dislikes), [this] {
            session_.settings().dislikes = !session_.settings().dislikes;
            session_.saveSettings();
            dislikes_->setValue(onOff(session_.settings().dislikes));
        });
        dearrow_ = column->add<ui::ValueRow>(ui::tr("settings.dearrow"), onOff(session_.settings().dearrow), [this] {
            session_.settings().dearrow = !session_.settings().dearrow;
            session_.saveSettings();
            dearrow_->setValue(onOff(session_.settings().dearrow));
        });
        watchHistory_ = column->add<ui::ValueRow>(ui::tr("settings.watch_history"),
                                                  onOff(session_.settings().watchHistory), [this] {
            session_.settings().watchHistory = !session_.settings().watchHistory;
            session_.saveSettings();
            watchHistory_->setValue(onOff(session_.settings().watchHistory));
        });
        column->add<ui::ValueRow>(ui::tr("settings.sponsor.categories"), "", [shared] {
            ui::Application::current()->push(makeSponsorCategories(*shared));
        });
        invidious_ = column->add<ui::ValueRow>(ui::tr("settings.invidious"), invidiousName(),
                                               [this] { editInvidious(); });

        addHeading(*column, ui::tr("shell.tab.home"));
        column->add<ui::ValueRow>(ui::tr("settings.topics"), "", [shared] {
            ui::Application::current()->push(makeTopics(*shared));
        });
        shorts_ = column->add<ui::ValueRow>(ui::tr("settings.shorts"), onOff(session_.settings().shorts), [this] {
            session_.settings().shorts = !session_.settings().shorts;
            session_.saveSettings();
            shorts_->setValue(onOff(session_.settings().shorts));
        });

        addHeading(*column, ui::tr("settings.account"));
        if (session_.account().signedIn()) {
            column->add<ui::ValueRow>(ui::tr("settings.account.youtube"), core::withoutEmoji(session_.account().name()),
                                      nullptr);
            column->add<ui::ValueRow>(ui::tr("settings.signout"), "", [this] { confirmSignOut(); });
        } else {
            column->add<ui::ValueRow>(ui::tr("shell.signin"), "", [shared] {
                openSignIn(*shared, [shared] { openTab(*shared, Tab::Settings); });
            });
        }

        addHeading(*column, ui::tr("settings.about"));
        column->add<ui::ValueRow>(ui::tr("settings.version"), core::version(), nullptr);
        update_ = column->add<ui::ValueRow>(ui::tr("settings.update"), "", [this] { pressUpdate(); });
        refreshUpdate();
        if (session_.update) {
            remembered = banner_;  // the screen opens on the banner
        }
        column->add<ui::ValueRow>(ui::tr("settings.licenses"), "", [shared] {
            ui::Application::current()->push(makeLicenses(*shared));
        });
        scroll->setContent(std::move(content));
    }

    void onShow() override {
        ShellScreen::onShow();
        refreshUpdate();  // the check at start may have answered since
    }

private:
    // ---- software update

    void refreshUpdate() {
        if (downloading_) {
            return;  // the progress shows there
        }
        banner_->visible = session_.update.has_value();
        if (session_.update) {
            banner_->set(ui::tr("settings.update.banner", {session_.update->version}),
                         ui::tr(ready_ ? "settings.update.banner_ready" : "settings.update.banner_text"),
                         ui::tr(ready_ ? "settings.update.ready" : "settings.update.download"));
        }
        banner_->markDirty();
        if (ready_) {
            update_->setValue(ui::tr("settings.update.ready"));
        } else if (session_.update) {
            update_->setValue(ui::tr("settings.update.available", {session_.update->version}));
        } else if (session_.updateChecked) {
            update_->setValue(ui::tr("settings.update.latest"));
        } else {
            update_->setValue(ui::tr("settings.update.check"));
        }
    }

    void pressUpdate() {
        if (downloading_) {
            return;
        }
        if (!session_.update) {
            session_.updateChecked = false;  // asked again
            update_->setValue(ui::tr("settings.update.checking"));
            session_.checkForUpdate([this, token = token_] {
                if (token.cancelled()) {
                    return;
                }
                refreshUpdate();
                if (!session_.updateChecked) {  // asked by the user, so the failure is told
                    ui::Application::current()->toast(ui::tr("settings.update.unreachable"));
                }
            });
            return;
        }
        const Release release = *session_.update;
        if (ready_) {
            showInstall(release);
            return;
        }
        const std::string megabytes = std::to_string((release.size + 999999) / 1000000);
        ui::Application::current()->showModal(ui::makeDialog(
            ui::tr("settings.update.title", {release.version}), ui::tr("settings.update.explain", {megabytes}),
            {{ui::tr("settings.update.download"), [this, release] { later([this, release] { download(release); }); }},
             {ui::tr("ui.cancel"), {}}}));
    }

    // the package to /data/pkg, its progress on the row; then how to install it
    void download(const Release& release) {
        const auto path = core::packagePath(packageFileName(release.version));
        if (!path) {
            ui::Application::current()->toast(ui::tr("settings.update.failed"));
            return;
        }
        downloading_ = true;
        showProgress(0);
        const net::HttpClient* http = &session_.http();
        const std::string target = *path;
        const core::CancelToken token = token_;
        ui::Application* app = ui::Application::current();
        auto percent = std::make_shared<std::atomic<int>>(0);
        runTransfer<core::Result<core::Unit>>(
            [http, release, target, token, app, percent, this] {
                return downloadPackage(
                    *http, release, target,
                    [app, percent, token, this](long long received, long long total) {
                        const int now = total > 0 ? static_cast<int>(received * 100 / total) : 0;
                        if (now != percent->exchange(now) && app != nullptr) {
                            app->mainQueue().post([this, token, now] {
                                if (!token.cancelled() && downloading_) {
                                    showProgress(now);
                                }
                            });
                        }
                    },
                    token);
            },
            [this, release](core::Result<core::Unit> done) {
                downloading_ = false;
                if (!done.ok()) {
                    PF_LOG_WARN("update: %s", done.error().message.c_str());
                    refreshUpdate();
                    ui::Application::current()->toast(ui::tr("settings.update.failed"));
                    return;
                }
                ready_ = true;
                refreshUpdate();
                showInstall(release);
            });
    }

    void showInstall(const Release& release) {
        ui::Application::current()->showModal(
            ui::makeDialog(ui::tr("settings.update.title", {release.version}),
                           ui::tr("settings.update.install", {packageFileName(release.version)}),
                           {{ui::tr("ui.ok"), {}}}));
    }

    void showProgress(int percent) {
        const std::string progress = ui::tr("settings.update.progress", {std::to_string(percent)});
        update_->setValue(progress);
        if (session_.update) {
            banner_->set(ui::tr("settings.update.banner", {session_.update->version}),
                         ui::tr("settings.update.banner_text"), progress);
        }
    }

    static std::string logoName(const std::string& logo) { return logo == "youtube" ? "YouTube" : "Playtube"; }

    static std::string subtitleSizeName(const std::string& size) { return ui::tr("settings.subtitle_size." + size); }

    void chooseSubtitleSize() {
        static const char* const kSizes[] = {"small", "normal", "large", "larger"};
        std::vector<std::string> names;
        int selected = 1;
        for (int i = 0; i < 4; ++i) {
            names.push_back(subtitleSizeName(kSizes[i]));
            selected = session_.settings().subtitleSize == kSizes[i] ? i : selected;
        }
        ui::Application::current()->showModal(
            ui::makeChoiceDialog(ui::tr("settings.subtitle_size"), names, selected, [this](int index) {
                session_.settings().subtitleSize = kSizes[index];
                session_.saveSettings();
                subtitleSize_->setValue(subtitleSizeName(kSizes[index]));
            }));
    }

    static std::string heightName(int height) { return std::to_string(height) + "p"; }

    std::string invidiousName() const {
        const std::string& address = session_.settings().invidious;
        return address.empty() ? ui::tr("settings.invidious.none") : address;
    }

    void chooseQuality() {
        std::vector<std::string> names;
        int selected = 0;
        for (int i = 0; i < 4; ++i) {
            names.push_back(heightName(kHeights[i]));
            if (session_.settings().maxHeight == kHeights[i]) {
                selected = i;
            }
        }
        ui::Application::current()->showModal(
            ui::makeChoiceDialog(ui::tr("settings.quality"), names, selected, [this](int index) {
                session_.settings().maxHeight = kHeights[index];
                session_.saveSettings();
                quality_->setValue(heightName(kHeights[index]));
            }));
    }

    void editInvidious() {
        platform::TextInputRequest request;
        request.title = ui::tr("settings.invidious");
        request.initial = session_.settings().invidious;
        request.kind = platform::TextInputRequest::Kind::Url;
        request.maxLength = 200;
        ui::Application::current()->requestText(request, [this, token = token_](std::optional<std::string> text) {
            if (token.cancelled() || !text) {
                return;
            }
            const std::string address = cleanInvidious(*text);
            if (address.empty() && !core::trimmed(*text).empty()) {
                ui::Application::current()->toast(ui::tr("settings.invidious.invalid"));
                return;
            }
            session_.settings().invidious = address;
            session_.saveSettings();
            invidious_->setValue(invidiousName());
        });
    }

    void confirmSignOut() {
        AppSession* shared = &session_;
        ui::Application::current()->showModal(ui::makeDialog(
            ui::tr("settings.signout"), ui::tr("settings.signout.confirm"),
            {{ui::tr("settings.signout"),
              [shared] {
                  const std::string token = shared->account().forget();
#ifndef PLAYTUBE_DEV
                  shared->submit([shared, token] { shared->auth().revoke(token); });
#else
                  // the development app shares the installed app's imported sign-in: revoking it here would sign
                  // that one out too
                  (void)token;
#endif
                  openTab(*shared, Tab::Settings);
              }},
             {ui::tr("ui.cancel"), [] {}}}));
    }

    void chooseAccent() {
        std::vector<std::string> names;
        int selected = 0;
        const auto& palette = ui::accentPalette();
        for (std::size_t i = 0; i < palette.size(); ++i) {
            names.push_back(accentName(palette[i].id));
            if (session_.settings().accent == palette[i].id) {
                selected = static_cast<int>(i);
            }
        }
        ui::Application::current()->showModal(ui::makeChoiceDialog(ui::tr("settings.accent"), names, selected, [this](int index) {
            const ui::AccentChoice& choice = ui::accentPalette()[static_cast<std::size_t>(index)];
            session_.settings().accent = choice.id;
            session_.saveSettings();
            ui::applyAccent(choice);  // the halo and every view follow on the next frame
            accent_->setValue(accentName(choice.id));
        }));
    }

    void chooseLanguage() {
        std::vector<std::string> names{ui::tr("settings.language.system")};
        std::vector<std::string> tags{""};
        int selected = 0;
        for (const auto& language : uiLanguages()) {
            if (session_.settings().language == language.first) {
                selected = static_cast<int>(tags.size());
            }
            names.push_back(language.second);
            tags.push_back(language.first);
        }
        ui::Application::current()->showModal(ui::makeChoiceDialog(ui::tr("settings.language"), names, selected,
                                                                   [this, tags](int index) {
            session_.settings().language = tags[static_cast<std::size_t>(index)];
            session_.saveSettings();
            refreshLanguage();
        }));
    }

    // a language other than the one the app started with applies at the next start, and says so
    void refreshLanguage() {
        const std::string& language = session_.settings().language;
        const std::string name = languageName(language);
        language_->setValue(language == session_.languageAtStart ? name
                                                                 : ui::tr("settings.language.next_start", {name}));
    }

    ui::ValueRow* accent_;
    ui::ValueRow* language_;
    ui::ValueRow* quality_;
    ui::ValueRow* sponsor_;
    ui::ValueRow* autoplay_;
    ui::ValueRow* invidious_;
    ui::ValueRow* update_ = nullptr;
    ui::ValueRow* subtitleSize_ = nullptr;
    ui::ValueRow* dislikes_ = nullptr;
    ui::ValueRow* watchHistory_ = nullptr;
    ui::ValueRow* dearrow_ = nullptr;
    ui::ValueRow* shorts_ = nullptr;
    ui::ValueRow* logo_ = nullptr;
    ui::ValueRow* subtitleBackground_ = nullptr;
    bool downloading_ = false;
    bool ready_ = false;  // the package is in /data/pkg
    UpdateBanner* banner_ = nullptr;
};

// The SponsorBlock categories, each on or off.
class SponsorCategoriesScreen : public ShellScreen {
public:
    explicit SponsorCategoriesScreen(AppSession& session) : ShellScreen(session) {
        auto* column = add<ui::Box>(ui::Axis::Column);
        column->width = ui::SizeSpec::fill();
        column->padding = pagePadding();
        column->gap = ui::theme().spacingM;
        column->add<ui::Label>(ui::tr("settings.sponsor.categories"),
                               ui::FontStyle{ui::theme().fontLogo, ui::FontWeight::Bold});
        for (const std::string& id : sponsorCategoryIds()) {
            const std::size_t index = rows_.size();
            rows_.push_back(column->add<ui::ValueRow>(categoryName(id), onOff(chosen(id)), [this, id, index] {
                toggle(id);
                rows_[index]->setValue(onOff(chosen(id)));
            }));
        }
    }

private:
    bool chosen(const std::string& id) const {
        const auto& list = session_.settings().sponsorCategories;
        return std::find(list.begin(), list.end(), id) != list.end();
    }

    // kept in the order of sponsorCategoryIds()
    void toggle(const std::string& id) {
        const bool on = !chosen(id);
        std::vector<std::string> list;
        for (const std::string& known : sponsorCategoryIds()) {
            if (known == id ? on : chosen(known)) {
                list.push_back(known);
            }
        }
        session_.settings().sponsorCategories = std::move(list);
        session_.saveSettings();
    }

    std::vector<ui::ValueRow*> rows_;
};

// The home topics: ✕ on one offers to move it or remove it; add one with the keyboard; or go back to the defaults.
class TopicsScreen : public ShellScreen {
public:
    explicit TopicsScreen(AppSession& session) : ShellScreen(session) {
        auto* scroll = add<ui::VScroll>();
        scroll->policy = ui::VScroll::Policy::Nearest;
        auto content = std::make_unique<ui::Box>(ui::Axis::Column);
        content->padding = pagePadding();
        content->gap = ui::theme().spacingM;
        content->add<ui::Label>(ui::tr("settings.topics"), ui::FontStyle{ui::theme().fontLogo, ui::FontWeight::Bold});
        list_ = content->add<ui::Box>(ui::Axis::Column);
        list_->gap = ui::theme().spacingM;
        auto* buttons = content->add<ui::Box>(ui::Axis::Row);
        buttons->gap = ui::theme().spacingM;
        buttons->margin.top = ui::theme().spacingL;
        setStart(buttons->add<ui::Button>(ui::tr("settings.topics.add"), [this] { addOne(); }));
        buttons->add<ui::Button>(ui::tr("settings.topics.reset"), [this] {
            later([this] {
                session_.settings().customTopics = false;
                session_.settings().homeTopics.clear();
                session_.saveSettings();
                refresh();
            });
        });
        scroll->setContent(std::move(content));
        refresh();
    }

private:
    std::vector<std::string> topics() const { return homeTopics(session_); }

    // the defaults become the user's list as soon as it changes
    std::vector<std::string>& editable() {
        Settings& settings = session_.settings();
        if (!settings.customTopics) {
            settings.homeTopics = defaultTopics();
            settings.customTopics = true;
        }
        return settings.homeTopics;
    }

    // the list again; the focus on the topic at `focusIndex` when given (a moved topic keeps it)
    void refresh(std::size_t focusIndex = SIZE_MAX) {
        moveFocusOff(list_);
        list_->clearChildren();
        const auto names = topics();
        ui::View* focused = nullptr;
        for (std::size_t i = 0; i < names.size(); ++i) {
            auto* row = list_->add<ui::ValueRow>(names[i], "", [this, i] { edit(i); });
            if (i == focusIndex) {
                focused = row;
            }
        }
        ui::Application* app = ui::Application::current();
        if (focused != nullptr && app != nullptr) {
            app->focus().focus(focused);
        }
        focusIfEmpty();
    }

    void edit(std::size_t index) {
        const auto names = topics();
        if (index >= names.size()) {
            return;
        }
        std::vector<std::pair<std::string, std::function<void()>>> buttons;
        if (index > 0) {
            buttons.push_back({ui::tr("settings.topics.up"), [this, index] { move(index, index - 1); }});
        }
        if (index + 1 < names.size()) {
            buttons.push_back({ui::tr("settings.topics.down"), [this, index] { move(index, index + 1); }});
        }
        buttons.push_back({ui::tr("settings.topics.remove"), [this, index] {
                               auto& list = editable();
                               list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
                               session_.saveSettings();
                               refresh();
                           }});
        buttons.push_back({ui::tr("ui.cancel"), [] {}});
        ui::Application::current()->showModal(ui::makeDialog(names[index], "", std::move(buttons)));
    }

    void move(std::size_t from, std::size_t to) {
        auto& list = editable();
        std::swap(list[from], list[to]);
        session_.saveSettings();
        refresh(to);
    }

    void addOne() {
        Settings shown = session_.settings();
        shown.homeTopics = topics();
        if (topicsFull(shown)) {
            ui::Application::current()->toast(ui::tr("settings.topics.full"));
            return;
        }
        platform::TextInputRequest request;
        request.title = ui::tr("settings.topics.add");
        request.maxLength = 60;
        ui::Application::current()->requestText(request, [this, token = token_](std::optional<std::string> text) {
            if (token.cancelled() || !text) {
                return;
            }
            Settings candidate = session_.settings();
            if (!candidate.customTopics) {
                candidate.homeTopics = defaultTopics();
            }
            if (addTopic(candidate, *text)) {  // the defaults become the user's list only when one is added
                session_.settings().homeTopics = candidate.homeTopics;
                session_.settings().customTopics = true;
                session_.saveSettings();
                refresh();
            }
        });
    }

    ui::Box* list_;
};

// A paragraph of the licences: focusable so that the list scrolls with the pad.
class Paragraph : public ui::Label {
public:
    explicit Paragraph(std::string text) : ui::Label(std::move(text), ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Regular}) {
        focusable = true;
        width = ui::SizeSpec::fill();
        maxLines = 0;
        color = ui::theme().textSecondary;
    }
};

// One licence: the texts of a component, one focusable paragraph per block (the pad scrolls through them).
class LicenseTextScreen : public ShellScreen {
public:
    // `paths`: the entry's files, full paths
    LicenseTextScreen(AppSession& session, const LicenseEntry& entry, const std::vector<std::string>& paths)
        : ShellScreen(session) {
        auto* scroll = add<ui::VScroll>();
        scroll->policy = ui::VScroll::Policy::Nearest;
        auto content = std::make_unique<ui::Box>(ui::Axis::Column);
        content->padding = pagePadding();
        content->gap = ui::theme().spacingL;
        content->add<ui::Label>(entry.name, ui::FontStyle{ui::theme().fontLogo, ui::FontWeight::Bold});
        auto* meta = content->add<ui::Label>(entry.version + " \xC2\xB7 " + entry.license,
                                             ui::FontStyle{ui::theme().fontBody, ui::FontWeight::Regular});
        meta->color = ui::theme().textSecondary;
        meta->visible = !entry.version.empty();
        for (const auto& path : paths) {
            std::string text;
            if (core::readSmallFile(path, kMaxLicensesBytes, text) != core::ReadStatus::Ok) {
                text = ui::tr("settings.licenses.missing");
            }
            std::size_t start = 0;
            while (start < text.size()) {
                std::size_t end = text.find("\n\n", start);
                if (end == std::string::npos) {
                    end = text.size();
                }
                const std::string paragraph = text.substr(start, end - start);
                if (paragraph.find_first_not_of(" \n") != std::string::npos) {
                    content->add<Paragraph>(paragraph);
                }
                start = end + 2;
            }
        }
        scroll->setContent(std::move(content));
    }
};

// The components and their licences; ✕ shows a licence's texts.
class LicensesScreen : public ShellScreen {
public:
    explicit LicensesScreen(AppSession& session) : ShellScreen(session) {
        auto* scroll = add<ui::VScroll>();
        scroll->policy = ui::VScroll::Policy::Nearest;
        auto content = std::make_unique<ui::Box>(ui::Axis::Column);
        content->padding = pagePadding();
        content->gap = ui::theme().spacingM;
        content->add<ui::Label>(ui::tr("settings.licenses"), ui::FontStyle{ui::theme().fontLogo, ui::FontWeight::Bold});
        auto* intro = content->add<ui::Label>(ui::tr("settings.licenses.intro"),
                                              ui::FontStyle{ui::theme().fontBody, ui::FontWeight::Regular});
        intro->color = ui::theme().textSecondary;
        intro->width = ui::SizeSpec::fixed(1100);
        intro->maxLines = 0;
        AppSession* shared = &session_;
        LicenseEntry playtube{"Playtube", core::version(), "GPL-3.0-or-later", {"GPL-3.0.txt"}, {}};
        std::vector<LicenseEntry> entries{playtube};
        std::string manifest;
        if (core::readSmallFile(session_.licensesDir + "/manifest.txt", kMaxLicensesBytes, manifest) ==
            core::ReadStatus::Ok) {
            for (auto& entry : parseLicenseManifest(manifest)) {
                entries.push_back(std::move(entry));
            }
        }
        for (const auto& entry : entries) {
            content->add<ui::ValueRow>(entry.name, entry.license, [shared, entry] {
                std::vector<std::string> paths;
                for (const auto& file : entry.files) {
                    paths.push_back(shared->licensesDir + "/" + file);
                }
                ui::Application::current()->push(std::make_unique<LicenseTextScreen>(*shared, entry, paths));
            });
            if (&entry == &entries.front()) {
                // after Playtube: the copyright lines and credits the licences ask to reproduce (FreeType), the
                // patches' authors, where the sources are
                LicenseEntry notices{ui::tr("settings.licenses.notices"), "", "", {}, {}};
                content->add<ui::ValueRow>(notices.name, "", [shared, notices] {
                    ui::Application::current()->push(std::make_unique<LicenseTextScreen>(
                        *shared, notices, std::vector<std::string>{shared->noticesFile}));
                });
            }
        }
        scroll->setContent(std::move(content));
    }
};

}  // namespace

std::unique_ptr<ui::Screen> makeSettings(AppSession& session) { return std::make_unique<SettingsScreen>(session); }
std::unique_ptr<ui::Screen> makeLicenses(AppSession& session) { return std::make_unique<LicensesScreen>(session); }
std::unique_ptr<ui::Screen> makeSponsorCategories(AppSession& session) {
    return std::make_unique<SponsorCategoriesScreen>(session);
}
std::unique_ptr<ui::Screen> makeTopics(AppSession& session) { return std::make_unique<TopicsScreen>(session); }

}  // namespace playtube::app
