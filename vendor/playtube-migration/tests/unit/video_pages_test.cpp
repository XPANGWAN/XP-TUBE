#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "app/shell/cards.hpp"
#include "app/shell/video_pages.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

youtube::VideoPage page(const std::string& prefix, int count, std::string continuation) {
    youtube::VideoPage result;
    for (int i = 0; i < count; ++i) {
        youtube::Video video;
        video.id = prefix + std::to_string(i);
        video.title = "Video " + video.id;
        result.items.push_back(video);
    }
    result.continuation = std::move(continuation);
    return result;
}

class GridScreen : public app::ShellScreen {
public:
    explicit GridScreen(app::AppSession& session) : ShellScreen(session) {
        auto* column = add<ui::Box>(ui::Axis::Column);
        column->width = ui::SizeSpec::fill();
        column->height = ui::SizeSpec::fill();
        grid = app::addVideoGrid(*column);
        pages = std::make_unique<app::VideoPages>(
            *this, *grid, [this](const std::string& token, const core::CancelToken&) { return fetch(token); },
            [this](const youtube::Video& video) { pressed = video.id; }, nullptr);
    }
    std::function<core::Result<youtube::VideoPage>(const std::string&)> fetch;
    ui::Grid* grid = nullptr;
    std::unique_ptr<app::VideoPages> pages;
    std::string pressed;
};

struct GridShell {
    Shell shell;
    GridScreen* screen = nullptr;
    int calls = 0;
    GridShell() {
        auto owned = std::make_unique<GridScreen>(shell.session);
        screen = owned.get();
        shell.application->replace(std::move(owned));
        shell.frames(3);
    }
};

}  // namespace

TEST_CASE("video meta: views and date joined, either may be missing") {
    youtube::Video video;
    video.viewsText = "12 k vues";
    video.publishedText = "il y a 2 jours";
    CHECK(app::videoMeta(video) == "12 k vues \xC2\xB7 il y a 2 jours");
    video.publishedText.clear();
    CHECK(app::videoMeta(video) == "12 k vues");
    video.viewsText.clear();
    CHECK(app::videoMeta(video).empty());
}

TEST_CASE("video pages: the next page is asked for once near the end, then appended") {
    GridShell g;
    g.screen->fetch = [&](const std::string& token) -> core::Result<youtube::VideoPage> {
        ++g.calls;
        CHECK(token == (g.calls == 1 ? "c1" : "c2"));
        return page("p" + std::to_string(g.calls) + "-", 12, "c" + std::to_string(g.calls + 1));
    };
    g.screen->pages->show(page("first-", 12, "c1"));
    g.shell.frames(3);
    REQUIRE(g.shell.application->focus().focus(g.screen->grid));
    g.shell.frames(2);
    g.shell.press(Action::Down);  // index 4: within 8 of the end
    CHECK(g.calls == 1);
    CHECK(g.screen->pages->count() == 24);
    g.shell.press(Action::Right);
    g.shell.press(Action::Right);
    CHECK(g.calls == 1);  // not near the new end yet
    g.shell.press(Action::Confirm);
    CHECK(g.screen->pressed == "first-6");
}

TEST_CASE("video pages: a page for a replaced list is dropped; a failed page is asked again") {
    GridShell g;
    g.screen->fetch = [&](const std::string&) -> core::Result<youtube::VideoPage> {
        ++g.calls;
        if (g.calls == 1) {
            g.screen->pages->show(page("other-", 10, "o1"));  // the list changed before the reply came
            return page("late-", 12, "c2");
        }
        if (g.calls == 2) {
            return core::makeError(core::ErrorKind::Network, "offline");
        }
        return page("more-", 5, "");
    };
    g.screen->pages->show(page("first-", 12, "c1"));
    g.shell.frames(3);
    REQUIRE(g.shell.application->focus().focus(g.screen->grid));
    g.shell.frames(2);
    g.shell.press(Action::Down);
    CHECK(g.calls == 1);
    CHECK(g.screen->pages->count() == 10);  // the late page was not added to the other list
    CHECK(g.screen->grid->focusedIndex() == 0);  // the new list starts at its first card
    g.shell.press(Action::Right);
    CHECK(g.calls == 1);
    g.shell.press(Action::Right);  // index 2: within 8 of the end
    CHECK(g.calls == 2);
    CHECK(g.screen->pages->count() == 10);
    g.shell.press(Action::Right);
    CHECK(g.calls == 3);
    CHECK(g.screen->pages->count() == 15);
    g.shell.press(Action::Right);
    CHECK(g.calls == 3);  // no continuation: the end
}
