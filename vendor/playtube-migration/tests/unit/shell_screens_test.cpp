#include <doctest/doctest.h>

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/settings.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/nav_bar.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/screensaver.hpp"
#include "app/shell/session.hpp"
#include "core/data_dir.hpp"
#include "core/task.hpp"
#include "core/version.hpp"
#include "support/files.hpp"
#include "support/shell_harness.hpp"
#include "ui/application.hpp"
#include "ui/i18n.hpp"
#include "ui/label.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

struct SettingsShell {
    Shell shell;
    SettingsShell() {
        shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
        shell.frames(10);
    }
};

}  // namespace

TEST_CASE("the app opens on Home with four tabs and the settings' gear beside the account; each replaces the screen") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(10);
    for (const char* tab : {"Home", "Search", "Subscriptions", "Library"}) {
        CHECK_MESSAGE(shows(*shell.application, tab), tab);
    }
    CHECK_FALSE(shows(*shell.application, "Settings"));  // no text tab any more
    pressAnywhere(shell, "Library");
    shell.frames(5);
    CHECK(shell.application->depth() == 1);
    ui::View* gear = anyNamed(shell.application->root(), "settings");
    REQUIRE(gear != nullptr);
    REQUIRE(shell.application->focus().focus(gear));
    shell.press(Action::Confirm);
    shell.frames(5);
    CHECK(shell.application->depth() == 1);
    CHECK(shows(*shell.application, "Accent colour"));
    // on the settings, Up from the first row lands on the gear (the screen shown)
    shell.press(Action::Up);
    CHECK(shell.application->focus().focused() == anyNamed(shell.application->root(), "settings"));
}

TEST_CASE("the accent colour applies at once and is saved") {
    SettingsShell s;
    CHECK(shows(*s.shell.application, "Accent colour"));
    CHECK(shows(*s.shell.application, "White"));  // the default
    pressAnywhere(s.shell, "Accent colour");
    REQUIRE(s.shell.application->hasModal());
    pressAnywhere(s.shell, "PlayStation blue");
    CHECK_FALSE(s.shell.application->hasModal());
    CHECK(ui::theme().accent.r == 0x00);
    CHECK(ui::theme().accent.b == 0xD1);
    CHECK(shows(*s.shell.application, "PlayStation blue"));
    CHECK(app::loadSettings(s.shell.dir.path + "/settings.json").accent == "playstation");
}

TEST_CASE("a new language is saved and applies at the next start") {
    SettingsShell s;
    CHECK(shows(*s.shell.application, "System"));
    pressAnywhere(s.shell, "Language");
    pressAnywhere(s.shell, "Deutsch");
    CHECK(s.shell.session.settings().language == "de");
    CHECK(shows(*s.shell.application, "Deutsch (at next start)"));
    pressAnywhere(s.shell, "Language");
    pressAnywhere(s.shell, "System");
    CHECK(shows(*s.shell.application, "System"));
}

TEST_CASE("about shows the version, and the licences page opens each licence") {
    SettingsShell s;
    CHECK(shows(*s.shell.application, core::version()));
    pressAnywhere(s.shell, "Third-party licences");
    s.shell.frames(10);
    CHECK(s.shell.application->depth() == 2);
    CHECK(shows(*s.shell.application, "FFmpeg"));
    CHECK(shows(*s.shell.application, "LGPL-3.0-or-later"));
    pressAnywhere(s.shell, "FFmpeg");
    s.shell.frames(10);
    CHECK(s.shell.application->depth() == 3);
    CHECK(showsSomewhere(*s.shell.application, "GNU LESSER GENERAL PUBLIC LICENSE"));
}

TEST_CASE("the licences page opens the copyright notices, with the FreeType credit") {
    SettingsShell s;
    pressAnywhere(s.shell, "Third-party licences");
    s.shell.frames(10);
    pressAnywhere(s.shell, "Copyright notices");
    s.shell.frames(10);
    CHECK(s.shell.application->depth() == 3);
    CHECK(showsSomewhere(*s.shell.application,
                         "Portions of this software are copyright \xC2\xA9 2024 The FreeType Project"));
    CHECK(showsSomewhere(*s.shell.application, "Copyright (C) 1999-2000, 2017 Dov Grobgeld"));
}

TEST_CASE("settings are saved and read back by a new session; pictures are fetched over https only") {
    ShellDir dir;
    {
        app::AppSession session(testHttp(), dir.path + "/settings.json");
        session.settings().accent = "green";
        CHECK(session.saveSettings());
    }
    app::AppSession session(testHttp(), dir.path + "/settings.json");
    CHECK(session.settings().accent == "green");
    CHECK_FALSE(session.fetchImage("http://example.com/a.jpg", {}).ok());
    app::AppSession noFile(testHttp());
    CHECK_FALSE(noFile.saveSettings());
}

TEST_CASE("a tab opens with the focus on its own tab") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(10);
    pressAnywhere(shell, "Library");
    shell.frames(5);
    ui::View* focused = shell.application->focus().focused();
    REQUIRE(focused != nullptr);
    std::vector<std::string> labels;
    texts(*focused, labels);
    REQUIRE_FALSE(labels.empty());
    CHECK(labels.front() == "Library");
}

TEST_CASE("software update: a newer version shows in the settings, its package downloads and is checked") {
    SettingsShell s;
    const std::string package(150000, 'k');
    s.shell.youtube.publishRelease("9.0.0", package);
    s.shell.session.updateUrl = s.shell.youtube.baseUrl() + "/github/latest";
    core::setDataRoot(s.shell.dir.path);
    s.shell.session.checkForUpdate();
    CHECK(s.shell.session.update.has_value());
    s.shell.application->replace(app::makeTab(s.shell.session, app::Tab::Settings));
    s.shell.frames(5);
    CHECK(showsSomewhere(*s.shell.application, "Version 9.0.0 available"));
    pressAnywhere(s.shell, "Software update");
    REQUIRE(s.shell.application->hasModal());
    pressAnywhere(s.shell, "Download");
    s.shell.frames(5);
    CHECK(showsSomewhere(*s.shell.application, "Ready to install"));
    const std::string path = s.shell.dir.path + "/pkg/Playtube-9.0.0.pkg";
    REQUIRE(std::filesystem::exists(path));
    CHECK(std::filesystem::file_size(path) == package.size());
    s.shell.press(Action::Back);
    core::setDataRoot({});
}

TEST_CASE("software update: a banner at the top of the settings, focused first, downloads the package") {
    SettingsShell s;
    s.shell.youtube.publishRelease("9.0.0", std::string(150000, 'k'));
    s.shell.session.updateUrl = s.shell.youtube.baseUrl() + "/github/latest";
    core::setDataRoot(s.shell.dir.path);
    s.shell.session.checkForUpdate();
    s.shell.application->replace(app::makeTab(s.shell.session, app::Tab::Settings));
    s.shell.frames(5);
    CHECK(shows(*s.shell.application, "Playtube 9.0.0 is available"));
    const ui::View* focused = s.shell.application->focus().focused();
    REQUIRE(focused != nullptr);
    CHECK(focused->name == "update banner");
    s.shell.press(Action::Confirm);
    s.shell.frames(3);
    REQUIRE(s.shell.application->hasModal());
    pressAnywhere(s.shell, "Download");
    s.shell.frames(5);
    REQUIRE(s.shell.application->hasModal());  // how to install it
    s.shell.press(Action::Confirm);
    s.shell.frames(3);
    CHECK(shows(*s.shell.application, ui::tr("settings.update.banner_ready")));
    // its longer text wraps beside "Ready to install" and the banner grows to hold its lines (it used to overflow)
    const ui::View* banner = anyNamed(s.shell.application->root(), "update banner");
    REQUIRE(banner != nullptr);
    const ui::Rect box = banner->absoluteFrame();
    std::vector<const ui::View*> pending{banner};
    while (!pending.empty()) {
        const ui::View* view = pending.back();
        pending.pop_back();
        for (const auto& child : view->children()) {
            pending.push_back(child.get());
        }
        if (const auto* label = dynamic_cast<const ui::Label*>(view)) {  // its lines, as drawn
            const ui::Rect at = label->absoluteFrame();
            const float drawn = static_cast<float>(label->lines().size()) * label->style.size * 1.25f;
            CHECK(at.y + std::max(at.height, drawn) <= box.y + box.height - banner->padding.bottom + 0.01f);
        }
    }
    CHECK(std::filesystem::exists(s.shell.dir.path + "/pkg/Playtube-9.0.0.pkg"));
    s.shell.press(Action::Confirm);  // the banner again: how to install it, nothing downloaded twice
    s.shell.frames(3);
    CHECK(s.shell.application->hasModal());
    s.shell.press(Action::Back);
    core::setDataRoot({});
}

TEST_CASE("software update: no banner when Playtube is up to date; asked from the settings it says so") {
    SettingsShell s;
    CHECK_FALSE(s.shell.session.update.has_value());
    CHECK_FALSE(showsSomewhere(*s.shell.application, "is available"));
    s.shell.youtube.publishRelease("0.1.0", "old");
    s.shell.session.updateUrl = s.shell.youtube.baseUrl() + "/github/latest";
    pressAnywhere(s.shell, "Software update");
    s.shell.frames(3);
    CHECK_FALSE(s.shell.session.update.has_value());
    CHECK(showsSomewhere(*s.shell.application, "Playtube is up to date"));
}

TEST_CASE("the screensaver: the time, large, and the date on black; it moves every minute; any button closes it") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(5);
    const auto asked = shell.youtube.requests().size();
    shell.application->showModal(app::makeScreensaver(shell.session));
    shell.frames(5);
    CHECK(app::Screensaver::shown());
    CHECK(showsSomewhere(*shell.application, "21:45"));        // the fixed clock of the tests
    CHECK(showsSomewhere(*shell.application, "Saturday 17"));
    CHECK(shell.youtube.requests().size() == asked);           // no videos, nothing asked
    const ui::View* saver = anyNamed(shell.application->root(), "screensaver");
    REQUIRE(saver != nullptr);
    const float before = saver->padding.left + saver->padding.top;
    app::Screensaver::tick(app::Screensaver::kMoveMs + 1);
    shell.frames(2);
    CHECK(saver->padding.left + saver->padding.top != before);  // somewhere else
    shell.press(Action::Right);
    CHECK_FALSE(shell.application->hasModal());
    CHECK_FALSE(app::Screensaver::shown());
}

TEST_CASE("logo: chosen in the settings, the YouTube logo is fetched and shown in the header; back to Playtube's") {
    SettingsShell s;
    s.shell.session.youtubeLogoUrl = s.shell.youtube.baseUrl() + "/logo.svg";
    CHECK(shows(*s.shell.application, "Playtube"));
    CHECK_FALSE(app::logoPictureShown());
    pressAnywhere(s.shell, "Logo");
    s.shell.frames(3);
    CHECK(s.shell.session.settings().logo == "youtube");
    CHECK(app::loadSettings(s.shell.dir.path + "/settings.json").logo == "youtube");
    CHECK(s.shell.youtube.requestsTo("/logo.svg").size() == 1);
    CHECK(app::logoPictureShown());
    pressAnywhere(s.shell, "Logo");
    s.shell.frames(3);
    CHECK_FALSE(app::logoPictureShown());
    CHECK(s.shell.session.settings().logo == "playtube");
}

TEST_CASE("logo: nothing fetched while Playtube's is chosen; a file it cannot draw keeps Playtube's") {
    Shell shell;
    shell.session.youtubeLogoUrl = shell.youtube.baseUrl() + "/logo.svg";
    shell.session.applyLogo();
    CHECK(shell.youtube.requestsTo("/logo.svg").empty());
    shell.session.settings().logo = "youtube";
    shell.session.youtubeLogoUrl = shell.youtube.baseUrl() + "/github/latest";  // not an SVG
    shell.session.applyLogo();
    CHECK_FALSE(app::logoPictureShown());
    shell.session.settings().logo = "playtube";
    shell.session.applyLogo();
}
