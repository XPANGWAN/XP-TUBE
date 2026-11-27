#include <doctest/doctest.h>

#include <string>

#include "app/shell/cards.hpp"
#include "app/shell/screens.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

void open(Shell& shell, app::Tab tab) {
    shell.application->replace(app::makeTab(shell.session, tab));
    shell.frames(10);
}

}  // namespace

TEST_CASE("subscriptions and library signed out: an invitation that signs in and comes back") {
    Shell shell;
    open(shell, app::Tab::Subscriptions);
    CHECK(shows(*shell.application, "Sign in to see the latest videos from your subscriptions."));
    CHECK(shell.youtube.requestsTo("/youtubei/v1/browse").empty());
    open(shell, app::Tab::Library);
    CHECK(shows(*shell.application, "Sign in to see your history, your playlists and Watch later."));
    shell.press(Action::Down);  // from the tab to the invitation's button
    shell.press(Action::Confirm);
    shell.frames(6 * 60);  // the code, then the first poll gives the tokens
    shell.frames(10);
    CHECK(shell.session.account().signedIn());
    CHECK(shows(*shell.application, "History"));  // back on the library
    CHECK(shows(*shell.application, "Watch later"));
}

TEST_CASE("subscriptions signed in: the feed with the token, more pages") {
    Shell shell;
    signIn(shell);
    open(shell, app::Tab::Subscriptions);
    const auto browses = shell.youtube.requestsTo("/youtubei/v1/browse");
    REQUIRE_FALSE(browses.empty());
    CHECK(browses[0].body.find("FEsubscriptions") != std::string::npos);
    CHECK(browses[0].authorization == "Bearer demo-access-token");
    REQUIRE(findView<app::VideoCard>(shell.application->root()) != nullptr);
    // the next page, with the token too
    bool more = false;
    for (const auto& request : shell.youtube.requestsTo("/youtubei/v1/browse")) {
        more = more || (request.body.find("\"continuation\"") != std::string::npos &&
                        request.authorization == "Bearer demo-access-token");
    }
    if (!more) {
        auto* grid = findView<ui::Grid>(shell.application->root());
        REQUIRE(grid != nullptr);
        REQUIRE(shell.application->focus().focus(grid));
        for (int i = 0; i < 4; ++i) {
            shell.press(Action::Down);
        }
        for (const auto& request : shell.youtube.requestsTo("/youtubei/v1/browse")) {
            more = more || request.body.find("\"continuation\"") != std::string::npos;
        }
    }
    CHECK(more);
}

TEST_CASE("library signed in: history and watch later rows") {
    Shell shell;
    signIn(shell);
    open(shell, app::Tab::Library);
    CHECK(shows(*shell.application, "History"));
    CHECK(shows(*shell.application, "Watch later"));
    const auto browses = shell.youtube.requestsTo("/youtubei/v1/browse");
    REQUIRE(browses.size() >= 2);
    CHECK(browses[0].body.find("FEhistory") != std::string::npos);
    CHECK(browses[1].body.find("VLWL") != std::string::npos);
}

TEST_CASE("a revoked sign-in on a feed: the tab comes back signed out, with the invitation") {
    Shell shell;
    signIn(shell);
    shell.youtube.failNext("/youtubei/v1/browse", 401);
    shell.youtube.revokeRefreshToken("demo-refresh-token");
    open(shell, app::Tab::Subscriptions);
    shell.frames(5);
    CHECK_FALSE(shell.session.account().signedIn());
    CHECK(shows(*shell.application, "Sign in to see the latest videos from your subscriptions."));
    CHECK(shows(*shell.application, "Sign in"));  // the bar too: no account initial any more
}

TEST_CASE("home signed in: a revoked sign-in brings the home back signed out") {
    Shell shell;
    signIn(shell);
    shell.youtube.failNext("/youtubei/v1/browse", 401);
    shell.youtube.revokeRefreshToken("demo-refresh-token");
    shell.session.settings().customTopics = true;
    shell.session.settings().homeTopics = {"Cuisine"};
    open(shell, app::Tab::Home);
    shell.frames(5);
    CHECK_FALSE(shell.session.account().signedIn());
    CHECK_FALSE(shows(*shell.application, "For you"));
    CHECK(shows(*shell.application, "Cuisine"));
}

TEST_CASE("Up from the sign-in invitation lands on the current tab") {
    Shell shell;
    open(shell, app::Tab::Library);
    auto* bar = findView<app::NavBar>(shell.application->root());
    REQUIRE(bar != nullptr);
    shell.press(Action::Down);
    REQUIRE(shell.application->focus().focused() != bar->currentTab());
    shell.press(Action::Up);
    CHECK(shell.application->focus().focused() == bar->currentTab());
}
