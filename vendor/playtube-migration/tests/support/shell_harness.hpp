#pragma once

#include <doctest/doctest.h>

#include <unistd.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/shell/cards.hpp"
#include "app/shell/nav_bar.hpp"
#include "app/shell/player_settings.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/session.hpp"
#include "core/task.hpp"
#include "support/fake_youtube.hpp"
#include "support/files.hpp"
#include "support/fixed_metrics.hpp"
#include "ui/application.hpp"
#include "ui/i18n.hpp"
#include "ui/icon_button.hpp"
#include "ui/label.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

// The shell on an Application with network jobs inline (as in the screenshot tests), YouTube answered by the
// fake, and helpers to find what the screens show.
namespace playtube::test {

using platform::Action;
using platform::Event;

struct ShellDir {
    std::string path;
    ShellDir() {
        char tmpl[] = "/tmp/playtube-screens-XXXXXX";
        REQUIRE(mkdtemp(tmpl) != nullptr);
        path = tmpl;
    }
    ~ShellDir() { std::filesystem::remove_all(path); }
};

inline net::HttpConfig testHttp() {
    net::HttpConfig config;
    config.caFile = test::fixturePath("tls/ca.crt");
    return config;
}

// The shell on an Application with network jobs inline (as in the screenshot tests).
struct Shell {
    ShellDir dir;
    core::WorkerPool pool{1};
    core::MainQueue mainQueue;
    test::FixedMetrics metrics;
    FakeYouTube youtube;
    app::AppSession session{testHttp(), dir.path + "/settings.json", dir.path + "/account.json"};
    std::unique_ptr<ui::Application> application;
    Shell() {
        youtube::Endpoints endpoints;
        endpoints.youtube = endpoints.suggest = endpoints.sponsor = endpoints.oauthRevoke = endpoints.dislikes =
            endpoints.stats = endpoints.dearrow = endpoints.dearrowThumbnails = youtube.baseUrl();
        session.startYoutube("en", endpoints);
        ui::translator().load(std::string(PLAYTUBE_SOURCE_DIR) + "/resources/i18n", "en");
        session.synchronous = true;
        session.fixedClock = 1792266300;  // 2026-10-17 19:45 UTC
        session.fixedUtcOffset = 120;
        session.licensesDir = std::string(PLAYTUBE_SOURCE_DIR) + "/resources/licenses";
        session.noticesFile = std::string(PLAYTUBE_SOURCE_DIR) + "/THIRD_PARTY_LICENSES";
        application =
            std::make_unique<ui::Application>(ui::Application::Services{pool, mainQueue, metrics, nullptr});
        app::setQueueAdder([this](const youtube::Video& video) { app::addToQueue(session, video); });
        app::setShortsOpener([this](std::vector<youtube::Video> shorts, std::size_t index) {
            app::openShorts(session, std::make_shared<const std::vector<youtube::Video>>(std::move(shorts)), index);
        });
    }
    ~Shell() {
        app::setShortsOpener({});
        app::setQueueAdder({});
        ui::applyAccent(ui::accentPalette().front());
    }
    void frames(int count = 5) {
        for (int i = 0; i < count; ++i) {
            application->update(1000.0f / 60.0f);
        }
    }
    void press(Action action) {
        Event event;
        event.type = Event::Type::ActionDown;
        event.action = action;
        application->handle(event);
        frames();
    }
};

// Every visible label text under `view`.
inline void texts(const ui::View& view, std::vector<std::string>& out) {
    if (!view.visible) {
        return;
    }
    if (const auto* label = dynamic_cast<const ui::Label*>(&view)) {
        out.push_back(label->text());
    }
    for (const auto& child : view.children()) {
        texts(*child, out);
    }
}

inline bool shows(ui::Application& app, const std::string& text) {
    std::vector<std::string> all;
    texts(app.root(), all);
    for (const std::string& t : all) {
        if (t == text) {
            return true;
        }
    }
    return false;
}

// the view named `name` in `view`'s tree (hidden ones too), else null
inline ui::View* anyNamed(ui::View& view, const std::string& name) {
    if (view.name == name) {
        return &view;
    }
    for (const auto& child : view.children()) {
        if (ui::View* found = anyNamed(*child, name)) {
            return found;
        }
    }
    return nullptr;
}

inline bool showsSomewhere(ui::Application& app, const std::string& part) {
    std::vector<std::string> all;
    texts(app.root(), all);
    for (const std::string& t : all) {
        if (t.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// the visible button, tab, value row or player setting whose first label is `text`, or the round icon named `text`
inline ui::View* findPressable(ui::View& view, const std::string& text) {
    if (!view.visible) {
        return nullptr;
    }
    if (const auto* icon = dynamic_cast<const ui::IconButton*>(&view)) {
        return icon->label() == text ? &view : nullptr;
    }
    if (dynamic_cast<ui::Button*>(&view) != nullptr || dynamic_cast<ui::ValueRow*>(&view) != nullptr ||
        dynamic_cast<app::NavTab*>(&view) != nullptr || dynamic_cast<app::PlayerSettingLine*>(&view) != nullptr) {
        std::vector<std::string> labels;
        texts(view, labels);
        if (!labels.empty() && labels.front() == text) {
            return &view;
        }
    }
    for (const auto& child : view.children()) {
        if (ui::View* found = findPressable(*child, text)) {
            return found;
        }
    }
    return nullptr;
}

inline void pressAnywhere(Shell& shell, const std::string& text) {
    ui::View* view = findPressable(shell.application->root(), text);
    REQUIRE_MESSAGE(view != nullptr, text);
    REQUIRE(shell.application->focus().focus(view));
    shell.press(Action::Confirm);
    shell.frames(3);
}

// Types `text` into the focused text field (opens the editor, types, confirms).
inline void typeText(Shell& shell, const std::string& text) {
    shell.press(Action::Confirm);
    REQUIRE(shell.application->hasModal());
    Event event;
    event.type = Event::Type::Text;
    event.text = text;
    shell.application->handle(event);
    shell.press(Action::Confirm);
    shell.frames(3);
}

// The first view of type T under `view` (depth first), else nullptr.
// every view of type T under `view`, in order
template <typename T>
void findViews(ui::View& view, std::vector<T*>& out) {
    if (T* found = dynamic_cast<T*>(&view)) {
        out.push_back(found);
    }
    for (const auto& child : view.children()) {
        findViews<T>(*child, out);
    }
}

template <typename T>
T* findView(ui::View& view) {
    if (auto* found = dynamic_cast<T*>(&view)) {
        return found;
    }
    for (const auto& child : view.children()) {
        if (T* found = findView<T>(*child)) {
            return found;
        }
    }
    return nullptr;
}

// Signs the shell's account in with the fake's tokens.
inline void signIn(Shell& shell) {
    youtube::Token token;
    token.accessToken = "demo-access-token";
    token.refreshToken = "demo-refresh-token";
    token.expiresIn = 3599;
    shell.session.account().signIn(token, "D\xC3\xA9mo", shell.session.now());
}

}  // namespace playtube::test
