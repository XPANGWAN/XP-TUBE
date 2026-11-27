#include <doctest/doctest.h>

#include <string>

#include "app/shell/screens.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;
using namespace playtube::test;

TEST_CASE("sign-in by code from the bar: the code, waiting, then signed in with the account's feeds") {
    Shell shell;
    shell.youtube.setPendingPolls(2);
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(5);
    CHECK_FALSE(shows(*shell.application, "For you"));
    pressAnywhere(shell, "Sign in");
    shell.frames(5);
    CHECK(shows(*shell.application, "ABC-DEF-GHI"));
    CHECK(shows(*shell.application, "https://www.google.com/device"));
    CHECK(shows(*shell.application, "Waiting for your code\xE2\x80\xA6"));
    shell.frames(4 * 60);  // less than the 5 s interval
    CHECK(shell.youtube.requestsTo("/o/oauth2/token").empty());
    shell.frames(2 * 60);
    CHECK(shell.youtube.requestsTo("/o/oauth2/token").size() == 1);  // pending
    shell.frames(10 * 60);
    CHECK(shell.youtube.requestsTo("/o/oauth2/token").size() == 3);  // pending, pending, the tokens
    shell.frames(5);
    CHECK(shell.session.account().signedIn());
    CHECK(shell.session.account().name() == "D\xC3\xA9mo");
    CHECK(shows(*shell.application, "For you"));  // back on the home, now signed in
    CHECK(shows(*shell.application, "D"));         // the account's initial on the bar
    CHECK_FALSE(shows(*shell.application, "Sign in"));
}

TEST_CASE("sign-in: leaving the screen stops the polls") {
    Shell shell;
    shell.youtube.setPendingPolls(100);
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(5);
    pressAnywhere(shell, "Sign in");
    shell.frames(6 * 60);
    CHECK(shell.youtube.requestsTo("/o/oauth2/token").size() == 1);
    shell.press(Action::Back);
    shell.frames(20 * 60);
    CHECK(shell.youtube.requestsTo("/o/oauth2/token").size() == 1);
    CHECK_FALSE(shell.session.account().signedIn());
}
