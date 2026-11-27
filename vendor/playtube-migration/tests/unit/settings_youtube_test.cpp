#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "app/settings.hpp"
#include "app/shell/screens.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

void openSettings(Shell& shell) {
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    shell.frames(10);
}

void typeInEditor(Shell& shell, const std::string& text) {
    REQUIRE(shell.application->hasModal());
    Event event;
    event.type = Event::Type::Text;
    event.text = text;
    shell.application->handle(event);
    shell.press(Action::Confirm);
    shell.frames(3);
}

}  // namespace

TEST_CASE("settings: maximum quality and SponsorBlock, kept in settings.json") {
    Shell shell;
    openSettings(shell);
    CHECK(shows(*shell.application, "1080p"));
    pressAnywhere(shell, "Maximum playback quality");
    REQUIRE(shell.application->hasModal());
    shell.press(Action::Up);  // from 1080p to 720p
    shell.press(Action::Confirm);
    CHECK(shell.session.settings().maxHeight == 720);
    CHECK(shows(*shell.application, "720p"));
    pressAnywhere(shell, "SponsorBlock");
    CHECK_FALSE(shell.session.settings().sponsorBlock);
    CHECK(app::loadSettings(shell.dir.path + "/settings.json").maxHeight == 720);
}

TEST_CASE("settings: SponsorBlock categories switch on and off, in their order") {
    Shell shell;
    openSettings(shell);
    pressAnywhere(shell, "Segments to skip");
    pressAnywhere(shell, "Intros");
    CHECK(shell.session.settings().sponsorCategories ==
          std::vector<std::string>{"sponsor", "selfpromo", "interaction", "intro"});
    pressAnywhere(shell, "Sponsors");
    CHECK(shell.session.settings().sponsorCategories == std::vector<std::string>{"selfpromo", "interaction", "intro"});
}

TEST_CASE("settings: home topics moved, removed, added, reset") {
    Shell shell;
    openSettings(shell);
    pressAnywhere(shell, "Home topics");
    CHECK(shows(*shell.application, "Music"));
    pressAnywhere(shell, "Music");
    REQUIRE(shell.application->hasModal());
    pressAnywhere(shell, "Move down");
    CHECK(shell.session.settings().customTopics);
    REQUIRE(shell.session.settings().homeTopics.size() == 5);
    CHECK(shell.session.settings().homeTopics[0] == "Video games");
    CHECK(shell.session.settings().homeTopics[1] == "Music");
    {
        std::vector<std::string> labels;
        texts(*shell.application->focus().focused(), labels);
        REQUIRE_FALSE(labels.empty());
        CHECK(labels.front() == "Music");  // the moved topic keeps the focus
    }
    pressAnywhere(shell, "Science");
    pressAnywhere(shell, "Remove");
    CHECK(shell.session.settings().homeTopics.size() == 4);
    pressAnywhere(shell, "Add a topic");
    typeInEditor(shell, "Cooking");
    CHECK(shell.session.settings().homeTopics.back() == "Cooking");
    CHECK(shows(*shell.application, "Cooking"));
    pressAnywhere(shell, "Restore the default topics");
    CHECK_FALSE(shell.session.settings().customTopics);
    CHECK(app::homeTopics(shell.session).front() == "Music");
}

TEST_CASE("settings: an Invidious address must be https") {
    Shell shell;
    openSettings(shell);
    pressAnywhere(shell, "Invidious instance");
    typeInEditor(shell, "http://plain.example");
    CHECK(shell.session.settings().invidious.empty());
    pressAnywhere(shell, "Invidious instance");
    typeInEditor(shell, "https://inv.example/");
    CHECK(shell.session.settings().invidious == "https://inv.example");
    CHECK(shows(*shell.application, "https://inv.example"));
}

TEST_CASE("settings: the account, and signing out forgets it (and revokes it in the installed app)") {
    Shell shell;
    signIn(shell);
    openSettings(shell);
    CHECK(shows(*shell.application, "YouTube account"));
    CHECK(shows(*shell.application, "D\xC3\xA9mo"));
    pressAnywhere(shell, "Sign out");
    REQUIRE(shell.application->hasModal());
    shell.press(Action::Confirm);  // the dialog's "Sign out"
    shell.frames(5);
    CHECK_FALSE(shell.session.account().signedIn());
    const auto revoked = shell.youtube.requestsTo("/revoke");
#ifdef PLAYTUBE_DEV
    CHECK(revoked.empty());  // the development app shares the installed app's sign-in: never revoked from here
#else
    REQUIRE(revoked.size() == 1);
    CHECK(revoked[0].body.find("demo-refresh-token") != std::string::npos);
#endif
    CHECK_FALSE(shows(*shell.application, "YouTube account"));
}

TEST_CASE("settings: adding a topic already there changes nothing, not even the defaults") {
    Shell shell;
    openSettings(shell);
    pressAnywhere(shell, "Home topics");
    pressAnywhere(shell, "Add a topic");
    typeInEditor(shell, "Music");
    CHECK_FALSE(shell.session.settings().customTopics);
    CHECK(shell.session.settings().homeTopics.empty());
}
