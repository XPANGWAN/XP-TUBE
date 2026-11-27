#include <doctest/doctest.h>

#include <string>

#include "app/shell/cards.hpp"
#include "app/shell/screens.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;
using namespace playtube::test;

TEST_CASE("channel: □ on a search result opens its channel page with its videos") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Search));
    shell.frames(5);
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(shell.application->focus().focus(field));
    typeText(shell, "lofi");
    auto* card = findView<app::VideoCard>(shell.application->root());
    REQUIRE(card != nullptr);
    REQUIRE(shell.application->focus().focus(card));
    shell.press(Action::Context);
    shell.frames(5);
    const auto browses = shell.youtube.requestsTo("/youtubei/v1/browse");
    REQUIRE(browses.size() == 1);
    CHECK(browses[0].body.find("UCSJ4gkVC6NrvII8umztf0Ow") != std::string::npos);
    CHECK(shows(*shell.application, "Lofi Girl"));
    CHECK(showsSomewhere(*shell.application, "@LofiGirl"));
    CHECK(shows(*shell.application, "Play the latest video"));
    CHECK(shows(*shell.application, "8h of halloween lofi spooky ambience to get chills to"));  // emoji left out
    shell.press(Action::Back);
    CHECK(findView<ui::TextField>(shell.application->root()) != nullptr);  // back on the search
}

TEST_CASE("channel: a card without its channel asks the video page for it") {
    Shell shell;
    signIn(shell);
    shell.session.settings().customTopics = true;
    shell.session.settings().homeTopics.clear();
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(5);
    auto* card = findView<app::VideoCard>(shell.application->root());  // a TV tile: no channel id
    REQUIRE(card != nullptr);
    REQUIRE(shell.application->focus().focus(card));
    shell.press(Action::Context);
    shell.frames(5);
    int webPages = 0;  // the video page that tells the channel (the TV one tells the subscription)
    for (const auto& request : shell.youtube.requestsTo("/youtubei/v1/next")) {
        webPages += request.clientName == "WEB" ? 1 : 0;
    }
    CHECK(webPages == 1);
    CHECK(shows(*shell.application, "Lofi Girl"));
}

TEST_CASE("channel: its playlists on their own page, one opens with its videos") {
    Shell shell;
    app::openChannel(shell.session, "UCSJ4gkVC6NrvII8umztf0Ow");
    shell.frames(5);
    pressAnywhere(shell, "Playlists");
    shell.frames(5);
    CHECK(shows(*shell.application, "Lofi House 2026"));
    CHECK(shows(*shell.application, "198\xC2\xA0vid\xC3\xA9os"));
    auto* card = findView<app::VideoCard>(*shell.application->top());  // the playlists' page, not the channel's
    REQUIRE(card != nullptr);
    REQUIRE(shell.application->focus().focus(card));
    shell.press(Action::Confirm);
    shell.frames(5);
    CHECK(shows(*shell.application, "Play all"));
    const auto browsed = shell.youtube.requestsTo("/youtubei/v1/browse");
    REQUIRE_FALSE(browsed.empty());
    CHECK(browsed.back().clientName == "TVHTML5");
    CHECK(browsed.back().body.find(R"("browseId":"VLPLaklDK5Yxuss")") != std::string::npos);
}

TEST_CASE("channel: a failed page says why and tries again") {
    Shell shell;
    shell.youtube.failNext("/youtubei/v1/browse", 500);
    app::openChannel(shell.session, "UCSJ4gkVC6NrvII8umztf0Ow");
    shell.frames(5);
    CHECK(shows(*shell.application, "YouTube did not answer. Try again in a moment."));
    pressAnywhere(shell, ui::tr("ui.retry"));
    shell.frames(5);
    CHECK(shows(*shell.application, "Lofi Girl"));
}

TEST_CASE("channel: its Shorts tab; a Short plays upright, ↓ goes to the next one of the tab") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    app::openChannel(shell.session, "UCSJ4gkVC6NrvII8umztf0Ow");
    shell.frames(5);
    pressAnywhere(shell, "Shorts");
    shell.frames(5);
    const auto browses = shell.youtube.requestsTo("/youtubei/v1/browse");
    REQUIRE(browses.size() == 2);
    CHECK(browses[1].body.find("EgZzaG9ydHPyBgUKA5oBAA==") != std::string::npos);
    CHECK(showsSomewhere(*shell.application, "in this economy"));
    auto* card = findView<app::VideoCard>(shell.application->root());
    REQUIRE(card != nullptr);
    REQUIRE(shell.application->focus().focus(card));
    const std::size_t depth = shell.application->depth();
    shell.press(Action::Confirm);
    shell.frames(10);
    CHECK(shell.application->depth() == depth + 1);  // the Shorts player
    const std::size_t asked = shell.youtube.requestsTo("/youtubei/v1/player").size();
    shell.press(Action::Down);  // the next Short, in place of this one
    shell.frames(10);
    CHECK(shell.application->depth() == depth + 1);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/player").size() > asked);
    shell.press(Action::Back);
}
