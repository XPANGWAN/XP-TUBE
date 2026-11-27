#include <doctest/doctest.h>

#include <cstdlib>
#include <string>

#include "app/shell/session.hpp"
#include "core/file_io.hpp"
#include "support/fake_youtube.hpp"

using namespace playtube;
using playtube::test::FakeYouTube;

TEST_CASE("the YouTube language and region follow the UI language") {
    CHECK(app::youtubeLocale("fr").hl == "fr");
    CHECK(app::youtubeLocale("fr").gl == "FR");
    CHECK(app::youtubeLocale("en").gl == "US");
    CHECK(app::youtubeLocale("pt").hl == "pt");
    CHECK(app::youtubeLocale("pt").gl == "BR");  // the translations are Brazilian Portuguese
    CHECK(app::youtubeLocale("zh-Hant").hl == "zh-TW");
    CHECK(app::youtubeLocale("zh-Hant").gl == "TW");
    CHECK(app::youtubeLocale("zh-Hans").hl == "zh-CN");
    CHECK(app::youtubeLocale("").hl == "en");
    CHECK(app::youtubeLocale("xx").gl == "US");
}

TEST_CASE("the session reaches YouTube and keeps the account in its file") {
    FakeYouTube yt;
    net::HttpConfig config;
    config.caFile = test::fixturePath("tls/ca.crt");
    char folder[] = "/tmp/playtube-session-XXXXXX";
    REQUIRE(mkdtemp(folder) != nullptr);
    const std::string accountPath = std::string(folder) + "/account.json";
    {
        app::AppSession session(config, {}, accountPath);
        youtube::Endpoints endpoints;
        endpoints.youtube = endpoints.suggest = endpoints.sponsor = endpoints.oauthRevoke = endpoints.dislikes = yt.baseUrl();
        session.startYoutube("fr", endpoints);
        const auto page = session.youtube().search("lofi");
        REQUIRE(page.ok());
        CHECK_FALSE(page.value().items.empty());
        CHECK(yt.requestsTo("/youtubei/v1/search")[0].body.find(R"("gl":"FR")") != std::string::npos);
        CHECK_FALSE(session.account().signedIn());
        const auto refused = session.signedIn<youtube::VideoPage>(
            [&](const std::string& token) { return session.youtube().feed("FEsubscriptions", token); });
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().kind == core::ErrorKind::Unauthorized);

        youtube::Token token;
        token.accessToken = "demo-access-token";
        token.refreshToken = "demo-refresh-token";
        token.expiresIn = 3599;
        session.account().signIn(token, "D\xC3\xA9mo", session.now());
        const auto feed = session.signedIn<youtube::VideoPage>(
            [&](const std::string& access) { return session.youtube().feed("FEsubscriptions", access); });
        REQUIRE(feed.ok());
        CHECK(yt.requestsTo("/youtubei/v1/browse").back().authorization == "Bearer demo-access-token");
    }
    app::AppSession again(config, {}, accountPath);
    CHECK(again.account().signedIn());
    CHECK(again.account().name() == "D\xC3\xA9mo");
    (void)std::system(("rm -rf '" + std::string(folder) + "'").c_str());
}

TEST_CASE("the YouTube locale from a system tag with a region") {
    CHECK(app::youtubeLocale("fr-FR").hl == "fr");
    CHECK(app::youtubeLocale("fr-FR").gl == "FR");
    CHECK(app::youtubeLocale("fr_CA").gl == "CA");
    CHECK(app::youtubeLocale("en-GB").hl == "en");
    CHECK(app::youtubeLocale("en-GB").gl == "GB");
    CHECK(app::youtubeLocale("pt-BR").gl == "BR");
    CHECK(app::youtubeLocale("zh-TW").hl == "zh-TW");
    CHECK(app::youtubeLocale("zh-CN").hl == "zh-CN");
    CHECK(app::youtubeLocale("de").gl == "DE");
}

TEST_CASE("the previous Playtube: its sign-in and recent searches imported once, its file left as it is") {
    char folder[] = "/tmp/playtube-import-XXXXXX";
    REQUIRE(mkdtemp(folder) != nullptr);
    const std::string dir = folder;
    const std::string old = dir + "/config.json";
    const std::string oldText =
        R"({"setting":{"yt_refresh_token":"old-refresh-token","yt_search_history":["lofi","jazz"]},"users":[]})";
    REQUIRE(core::writeFileAtomic(old, oldText));
    net::HttpConfig config;
    {
        app::AppSession session(config, dir + "/settings.json", dir + "/account.json");
        session.settings().recentSearches = {"mine"};
        session.importPrevious(old);
        CHECK(session.account().signedIn());
        CHECK(session.settings().recentSearches == std::vector<std::string>{"mine", "lofi", "jazz"});
    }
    CHECK(test::readFile(old) == oldText);
    {
        app::AppSession session(config, dir + "/settings.json", dir + "/account.json");
        CHECK(session.settings().recentSearches == std::vector<std::string>{"mine", "lofi", "jazz"});
        session.account().forget();
        session.importPrevious(old);  // signed out since: still never again
        CHECK_FALSE(session.account().signedIn());
    }
    (void)std::system(("rm -rf '" + dir + "'").c_str());
}

TEST_CASE("a sign-in without its name (imported) gets the account name") {
    FakeYouTube yt;
    net::HttpConfig config;
    config.caFile = test::fixturePath("tls/ca.crt");
    app::AppSession session(config);
    youtube::Endpoints endpoints;
    endpoints.youtube = endpoints.suggest = endpoints.sponsor = endpoints.oauthRevoke = endpoints.dislikes = yt.baseUrl();
    session.startYoutube("fr", endpoints);
    session.synchronous = true;
    youtube::Token token;
    token.accessToken = "demo-access-token";
    token.refreshToken = "demo-refresh-token";
    token.expiresIn = 3599;
    session.account().signIn(token, "", session.now());
    session.completeAccount();
    CHECK(session.account().name() == "D\xC3\xA9mo");
    CHECK(yt.requestsTo("/youtubei/v1/account/accounts_list").size() == 1);
    session.completeAccount();  // known now: not asked again
    CHECK(yt.requestsTo("/youtubei/v1/account/accounts_list").size() == 1);
}

TEST_CASE("the previous Playtube's sign-in never replaces the one already here") {
    char folder[] = "/tmp/playtube-import2-XXXXXX";
    REQUIRE(mkdtemp(folder) != nullptr);
    const std::string dir = folder;
    REQUIRE(core::writeFileAtomic(dir + "/config.json", R"({"setting":{"yt_refresh_token":"old-refresh-token"}})"));
    net::HttpConfig config;
    app::AppSession session(config, {}, dir + "/account.json");
    youtube::Token token;
    token.accessToken = "a";
    token.refreshToken = "current-refresh-token";
    token.expiresIn = 3599;
    session.account().signIn(token, "D\xC3\xA9mo", session.now());
    session.importPrevious(dir + "/config.json");
    CHECK(test::readFile(dir + "/account.json").find("current-refresh-token") != std::string::npos);
    CHECK(test::readFile(dir + "/account.json").find("old-refresh-token") == std::string::npos);
    (void)std::system(("rm -rf '" + dir + "'").c_str());
}
