#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "app/shell/chat_panel.hpp"
#include "app/shell/screens.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

youtube::ChatMessage message(const std::string& id, const std::string& author, const std::string& text,
                             const std::string& amount = {}) {
    youtube::ChatMessage m;
    m.id = id;
    m.author = author;
    m.text = text;
    m.amount = amount;
    return m;
}

}  // namespace

TEST_CASE("live chat panel: the messages with their authors, the next page after the wait, removals, ◯ closes") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(3);
    std::vector<std::string> asked;
    std::vector<youtube::ChatPage> pages(3);
    pages[0].messages = {message("a", "@one", "hello"), message("b", "@two", "\xF0\x9F\x99\x8F"),  // emoji only
                         message("c", "@three", "thanks", "5,00 \xE2\x82\xAC")};
    pages[0].continuation = "second";
    pages[0].waitMs = 10000;  // kept to 3 s
    pages[1].messages = {message("d", "@four", "it rains")};
    pages[1].removed = {"a"};
    pages[1].continuation = "third";
    pages[1].waitMs = 500;  // at least 1 s
    pages[2].continuation = "";  // the stream ended
    auto* panel = new app::ChatPanel("first", [&](const std::string& token, app::ChatPanel::Done done) {
        asked.push_back(token);
        done(pages[asked.size() - 1]);
    });
    shell.application->showModal(std::unique_ptr<ui::View>(panel));
    shell.frames(3);
    REQUIRE(asked == std::vector<std::string>{"first"});
    CHECK(panel->count() == 2);
    CHECK(showsSomewhere(*shell.application, "@one"));
    CHECK(showsSomewhere(*shell.application, "hello"));
    CHECK(showsSomewhere(*shell.application, "@three \xC2\xB7 5,00 \xE2\x82\xAC"));
    CHECK_FALSE(showsSomewhere(*shell.application, "@two"));
    shell.frames(60 * 2);  // 2 s: not yet
    CHECK(asked.size() == 1);
    shell.frames(60 * 1 + 10);  // past 3 s
    REQUIRE(asked.size() == 2);
    CHECK(asked[1] == "second");
    CHECK(panel->count() == 2);  // "hello" removed, "it rains" added
    CHECK_FALSE(showsSomewhere(*shell.application, "hello"));
    CHECK(showsSomewhere(*shell.application, "it rains"));
    shell.frames(60 * 1 + 10);
    CHECK(asked.size() == 3);
    shell.frames(60 * 5);
    CHECK(asked.size() == 3);  // ended: nothing more asked
    shell.press(Action::Back);
    CHECK_FALSE(shell.application->hasModal());
}

TEST_CASE("live chat panel: a failed page says so and is asked again after 10 s; closed, nothing more is asked") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(3);
    int asked = 0;
    shell.application->showModal(std::make_unique<app::ChatPanel>(
        "first", [&](const std::string&, app::ChatPanel::Done done) {
            ++asked;
            done(core::makeError(core::ErrorKind::Network, "down"));
        }));
    shell.frames(3);
    CHECK(asked == 1);
    CHECK(showsSomewhere(*shell.application, "The chat did not answer. Trying again\xE2\x80\xA6"));
    shell.frames(60 * 10 + 10);
    CHECK(asked == 2);
    shell.press(Action::Back);
    shell.frames(60 * 11);
    CHECK(asked == 2);
}
