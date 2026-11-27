#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "app/shell/cards.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/video_pages.hpp"
#include "support/shell_harness.hpp"
#include "ui/image_view.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

void open(Shell& shell, app::Tab tab) {
    shell.application->replace(app::makeTab(shell.session, tab));
    shell.frames(10);
}

// searches for a term (not next pages)
int searches(Shell& shell) {
    int count = 0;
    for (const auto& request : shell.youtube.requestsTo("/youtubei/v1/search")) {
        count += request.body.find("\"query\"") != std::string::npos ? 1 : 0;
    }
    return count;
}

int cards(Shell& shell) {
    int count = 0;
    std::vector<ui::View*> stack{&shell.application->root()};
    while (!stack.empty()) {
        ui::View* view = stack.back();
        stack.pop_back();
        if (!view->visible) {
            continue;
        }
        if (dynamic_cast<app::VideoCard*>(view) != nullptr) {
            ++count;
        }
        for (const auto& child : view->children()) {
            stack.push_back(child.get());
        }
    }
    return count;
}

}  // namespace

TEST_CASE("home signed out: a row per topic, no signed-in feed") {
    Shell shell;
    open(shell, app::Tab::Home);
    for (const char* topic : {"Music", "Video games", "News", "Sports", "Science"}) {
        CHECK_MESSAGE(shows(*shell.application, topic), topic);
    }
    CHECK(shows(*shell.application, "Shorts"));  // the first topic's Shorts, after its row
    CHECK_FALSE(shows(*shell.application, "For you"));
    CHECK(shell.youtube.requestsTo("/youtubei/v1/search").size() == 6);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/browse").empty());
    CHECK(cards(shell) > 0);
    CHECK(shows(*shell.application, "Lofi Girl"));
}

TEST_CASE("home signed in: recommendations and subscriptions first, then the user's topics") {
    Shell shell;
    signIn(shell);
    shell.session.settings().customTopics = true;
    shell.session.settings().homeTopics = {"Cuisine"};
    open(shell, app::Tab::Home);
    CHECK(shows(*shell.application, "For you"));
    CHECK(shows(*shell.application, "Subscriptions"));
    CHECK(shows(*shell.application, "Cuisine"));
    CHECK_FALSE(shows(*shell.application, "Music"));
    const auto browses = shell.youtube.requestsTo("/youtubei/v1/browse");
    REQUIRE(browses.size() == 2);
    CHECK(browses[0].authorization == "Bearer demo-access-token");
    CHECK(shows(*shell.application, "Vid\xC3\xA9o 1"));
}

TEST_CASE("home: when nothing came, the reason and Retry") {
    Shell shell;
    shell.session.settings().customTopics = true;
    shell.session.settings().homeTopics = {"Cuisine"};
    shell.youtube.failNext("/youtubei/v1/search", 500, 2);  // the topic's row and its Shorts
    open(shell, app::Tab::Home);
    CHECK(shows(*shell.application, "YouTube did not answer. Try again in a moment."));
    CHECK(cards(shell) == 0);
    pressAnywhere(shell, ui::tr("ui.retry"));
    shell.frames(5);
    CHECK_FALSE(shows(*shell.application, "YouTube did not answer. Try again in a moment."));
    CHECK(cards(shell) > 0);
}

TEST_CASE("search: results, suggestions, the term kept in recent searches") {
    Shell shell;
    open(shell, app::Tab::Search);
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(field != nullptr);
    REQUIRE(shell.application->focus().focus(field));
    typeText(shell, "Lofi");  // as the console keyboard writes it
    CHECK(searches(shell) == 1);
    CHECK(cards(shell) == 5);  // 2, then the next page asked for at once (the focus is near the end)
    auto* grid = findView<ui::Grid>(shell.application->root());
    REQUIRE(grid != nullptr);
    CHECK(grid->scrollOffset() == doctest::Approx(0));  // the first row in view
    CHECK(shows(*shell.application, "lofi coffee shop"));  // a suggestion
    CHECK(findPressable(shell.application->root(), "lofi") == nullptr);  // not the term itself
    CHECK_FALSE(shows(*shell.application, "Recent searches"));
    REQUIRE_FALSE(shell.session.settings().recentSearches.empty());
    CHECK(shell.session.settings().recentSearches.front() == "Lofi");
    pressAnywhere(shell, "lofi coffee shop");
    CHECK(searches(shell) == 2);
    CHECK(shell.session.settings().recentSearches.front() == "lofi coffee shop");
}

TEST_CASE("search: a pasted link plays instead of searching; recent searches show while empty") {
    Shell shell;
    shell.session.settings().recentSearches = {"chill"};
    open(shell, app::Tab::Search);
    CHECK(shows(*shell.application, "Recent searches"));
    CHECK(shows(*shell.application, "chill"));
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(shell.application->focus().focus(field));
    typeText(shell, "https://youtu.be/dQw4w9WgXcQ");
    CHECK(searches(shell) == 0);
    CHECK(shell.session.settings().recentSearches == std::vector<std::string>{"chill"});
}

TEST_CASE("search: no result and a failed search say so") {
    Shell shell;
    open(shell, app::Tab::Search);
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(shell.application->focus().focus(field));
    shell.youtube.failNext("/youtubei/v1/search", 503);
    typeText(shell, "lofi");
    CHECK(shows(*shell.application, "YouTube did not answer. Try again in a moment."));
    pressAnywhere(shell, ui::tr("ui.retry"));
    CHECK(cards(shell) >= 2);
}

TEST_CASE("Up from the content lands on the current tab, not on the last tab visited in the bar") {
    Shell shell;
    open(shell, app::Tab::Home);
    auto* bar = findView<app::NavBar>(shell.application->root());
    REQUIRE(bar != nullptr);
    REQUIRE(shell.application->focus().focus(bar->currentTab()));
    shell.press(Action::Right);  // onto "Search", not opened
    shell.press(Action::Right);  // "Subscriptions"
    auto* card = findView<app::VideoCard>(shell.application->root());
    REQUIRE(card != nullptr);
    REQUIRE(shell.application->focus().focus(card));
    shell.press(Action::Up);
    CHECK(shell.application->focus().focused() == bar->currentTab());
}

TEST_CASE("search: filters ask YouTube with their params; the button counts them; Shorts and channels show") {
    Shell shell;
    open(shell, app::Tab::Search);
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(field != nullptr);
    REQUIRE(shell.application->focus().focus(field));
    typeText(shell, "lofi");
    pressAnywhere(shell, "Filters");
    REQUIRE(shell.application->hasModal());
    pressAnywhere(shell, "Type: All");
    shell.frames(3);
    pressAnywhere(shell, "Shorts");
    shell.frames(5);
    bool shortsAsked = false;
    for (const auto& r : shell.youtube.requestsTo("/youtubei/v1/search")) {
        shortsAsked = shortsAsked || r.body.find("\"params\":\"EgIQCQ==\"") != std::string::npos;
    }
    CHECK(shortsAsked);
    CHECK(findPressable(shell.application->root(), "Filters \xC2\xB7 1") != nullptr);
}

TEST_CASE("Shorts off in the settings: no Shorts row on the home nor tab on a channel; search keeps them") {
    Shell shell;
    shell.session.settings().shorts = false;
    open(shell, app::Tab::Home);
    CHECK(shows(*shell.application, "Music"));
    CHECK_FALSE(shows(*shell.application, "Shorts"));
    CHECK(shell.youtube.requestsTo("/youtubei/v1/search").size() == 5);  // the topics only
    app::openChannel(shell.session, "UCSJ4gkVC6NrvII8umztf0Ow");
    shell.frames(5);
    CHECK(shows(*shell.application, "Lofi Girl"));
    CHECK(findPressable(shell.application->root(), "Shorts") == nullptr);
    shell.press(Action::Back);
    shell.frames(3);
    open(shell, app::Tab::Search);
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(field != nullptr);
    REQUIRE(shell.application->focus().focus(field));
    typeText(shell, "lofi");
    pressAnywhere(shell, "Filters");
    pressAnywhere(shell, "Type: All");
    shell.frames(3);
    pressAnywhere(shell, "Shorts");
    shell.frames(5);
    CHECK(showsSomewhere(*shell.application, "Lofi Girl Sad Cat Dance"));  // a Short, in search
}

TEST_CASE("dropShorts leaves the other videos in order") {
    youtube::VideoPage page;
    for (int i = 0; i < 5; ++i) {
        youtube::Video video;
        video.id = std::to_string(i);
        video.isShort = i % 2 == 1;
        page.items.push_back(video);
    }
    app::dropShorts(page);
    REQUIRE(page.items.size() == 3);
    CHECK(page.items[0].id == "0");
    CHECK(page.items[1].id == "2");
    CHECK(page.items[2].id == "4");
}

TEST_CASE("DeArrow on: cards show its titles and thumbnails; off (the default): nothing is asked") {
    Shell shell;
    app::AppSession* session = &shell.session;
    app::setBrandingSource([session](const std::string& id,
                                     std::function<void(const std::string&, const std::string&)> ready) {
        session->branding(id, [ready](const app::AppSession::Branding& b) { ready(b.title, b.thumbnail); });
    });
    CHECK_FALSE(shell.session.settings().dearrow);
    open(shell, app::Tab::Home);
    CHECK(shell.youtube.requestsTo("/api/branding").empty());
    CHECK_FALSE(showsSomewhere(*shell.application, "Granting Wishes for 100 People with Cancer"));

    shell.session.settings().dearrow = true;
    open(shell, app::Tab::Search);
    open(shell, app::Tab::Home);
    shell.frames(10);
    CHECK_FALSE(shell.youtube.requestsTo("/api/branding").empty());
    CHECK(showsSomewhere(*shell.application, "Granting Wishes for 100 People with Cancer"));
    std::vector<ui::ImageView*> pictures;
    findViews<ui::ImageView>(shell.application->root(), pictures);
    bool dearrowPicture = false;
    for (const ui::ImageView* picture : pictures) {
        dearrowPicture = dearrowPicture || picture->url().find("/api/v1/getThumbnail?videoID=") != std::string::npos;
    }
    CHECK(dearrowPicture);  // its picture (WebP), YouTube's kept as the fallback
    app::setBrandingSource({});
}
