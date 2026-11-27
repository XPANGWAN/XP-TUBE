#include <doctest/doctest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include "core/file_io.hpp"
#include "net/http.hpp"
#include "support/fake_youtube.hpp"
#include "youtube/account.hpp"
#include "youtube/auth.hpp"
#include "youtube/client.hpp"

using namespace playtube;
using namespace playtube::youtube;
using playtube::test::FakeYouTube;

namespace {

net::HttpClient testHttp() {
    net::HttpConfig config;
    config.caFile = test::fixturePath("tls/ca.crt");
    return net::HttpClient(config);
}

Endpoints at(const FakeYouTube& yt) {
    Endpoints endpoints;
    endpoints.youtube = endpoints.suggest = endpoints.sponsor = endpoints.oauthRevoke = endpoints.dislikes = yt.baseUrl();
    return endpoints;
}

struct TempDir {
    std::string path;
    TempDir() {
        char pattern[] = "/tmp/playtube-account-XXXXXX";
        path = mkdtemp(pattern);
    }
    ~TempDir() {
        (void)std::system(("rm -rf '" + path + "'").c_str());
    }
};

Token tokens(const char* access, const char* refresh) {
    Token token;
    token.accessToken = access;
    token.refreshToken = refresh;
    token.expiresIn = 3599;
    return token;
}

}  // namespace

TEST_CASE("sign-in by code: the code, pending polls, then the tokens") {
    FakeYouTube yt;
    yt.setPendingPolls(2);
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    const auto code = auth.start();
    REQUIRE(code.ok());
    CHECK(code.value().userCode == "ABC-DEF-GHI");
    const auto asked = yt.requestsTo("/o/oauth2/device/code");
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].body.find("861556708454-d6dlm3lh05idd8npek18k6be8ba3oc68.apps.googleusercontent.com") !=
          std::string::npos);
    for (int i = 0; i < 2; ++i) {
        const auto pending = auth.poll(code.value().deviceCode);
        REQUIRE(pending.ok());
        CHECK(pending.value().pending);
    }
    const auto token = auth.poll(code.value().deviceCode);
    REQUIRE(token.ok());
    CHECK_FALSE(token.value().pending);
    CHECK(token.value().accessToken == "demo-access-token");
    CHECK(token.value().refreshToken == "demo-refresh-token");
    const auto polls = yt.requestsTo("/o/oauth2/token");
    REQUIRE(polls.size() == 3);
    CHECK(polls[0].body.find("demo-device-code") != std::string::npos);
}

TEST_CASE("account: the access token is reused until it expires, then renewed") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    TempDir dir;
    Account account(dir.path + "/account.json");
    CHECK_FALSE(account.signedIn());
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    CHECK(account.signedIn());
    CHECK(account.name() == "D\xC3\xA9mo");
    const auto first = account.accessToken(auth, 1000 + 60);
    REQUIRE(first.ok());
    CHECK(first.value() == "first-access");
    CHECK(yt.requestsTo("/o/oauth2/token").empty());
    const auto renewed = account.accessToken(auth, 1000 + 3599);
    REQUIRE(renewed.ok());
    CHECK(renewed.value() == "fresh-access-1");
    const auto refreshes = yt.requestsTo("/o/oauth2/token");
    REQUIRE(refreshes.size() == 1);
    CHECK(refreshes[0].body.find("demo-refresh-token") != std::string::npos);
}

TEST_CASE("account: a refused token is renewed once and the request runs again") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    const Client client(http, at(yt), "fr", "FR");
    TempDir dir;
    Account account(dir.path + "/account.json");
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    yt.failNext("/youtubei/v1/browse", 401);
    const auto feed = account.withToken(auth, 1010, [&](const std::string& token) {
        return client.feed("FEsubscriptions", token);
    });
    REQUIRE(feed.ok());
    CHECK_FALSE(feed.value().items.empty());
    const auto browses = yt.requestsTo("/youtubei/v1/browse");
    REQUIRE(browses.size() == 2);
    CHECK(browses[0].authorization == "Bearer first-access");
    CHECK(browses[1].authorization == "Bearer fresh-access-1");
    CHECK(yt.requestsTo("/o/oauth2/token").size() == 1);
}

TEST_CASE("account: a revoked sign-in signs out") {
    FakeYouTube yt;
    yt.revokeRefreshToken("demo-refresh-token");
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    TempDir dir;
    Account account(dir.path + "/account.json");
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    const auto token = account.accessToken(auth, 1000 + 4000);
    REQUIRE_FALSE(token.ok());
    CHECK(token.error().kind == core::ErrorKind::Unauthorized);
    CHECK_FALSE(account.signedIn());
    Account reloaded(dir.path + "/account.json");
    reloaded.load();
    CHECK_FALSE(reloaded.signedIn());
}

TEST_CASE("account: a failed renewal that is not a refusal keeps the sign-in") {
    FakeYouTube yt;
    yt.failNextRefresh("internal_failure");
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    TempDir dir;
    Account account(dir.path + "/account.json");
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    const auto token = account.accessToken(auth, 1000 + 4000);
    REQUIRE_FALSE(token.ok());
    CHECK(token.error().kind != core::ErrorKind::Unauthorized);
    CHECK(account.signedIn());
    const auto retried = account.accessToken(auth, 1000 + 4000);
    REQUIRE(retried.ok());
    CHECK(retried.value() == "fresh-access-1");
}

TEST_CASE("account: signing out during a renewal stays signed out") {
    FakeYouTube yt;
    yt.setTokenDelay(400);
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    TempDir dir;
    const std::string path = dir.path + "/account.json";
    Account account(path);
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    std::thread renewing([&] { (void)account.accessToken(auth, 1000 + 4000); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    account.signOut(auth);
    renewing.join();
    CHECK_FALSE(account.signedIn());
    CHECK(test::readFile(path).find("refresh") == std::string::npos);
    CHECK_FALSE(account.accessToken(auth, 1000 + 4000).ok());
}

TEST_CASE("account: forgetting signs out at once and hands back the token to revoke") {
    TempDir dir;
    Account account(dir.path + "/account.json");
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    CHECK(account.forget() == "demo-refresh-token");
    CHECK_FALSE(account.signedIn());
    CHECK(account.name().empty());
    CHECK(test::readFile(dir.path + "/account.json").find("demo-refresh-token") == std::string::npos);
    CHECK(account.forget().empty());
}

TEST_CASE("account: without a file nothing is written") {
    Account account("");
    account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    CHECK(account.signedIn());
}

TEST_CASE("account: kept in a private file, without the access token; sign-out revokes and forgets") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Auth auth(http, at(yt));
    TempDir dir;
    const std::string path = dir.path + "/account.json";
    {
        Account account(path);
        account.signIn(tokens("first-access", "demo-refresh-token"), "D\xC3\xA9mo", 1000);
    }
    struct stat info {};
    REQUIRE(stat(path.c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0600);
    const std::string text = test::readFile(path);
    CHECK(text.find("demo-refresh-token") != std::string::npos);
    CHECK(text.find("first-access") == std::string::npos);

    Account account(path);
    account.load();
    CHECK(account.signedIn());
    CHECK(account.name() == "D\xC3\xA9mo");
    account.signOut(auth);
    CHECK_FALSE(account.signedIn());
    const auto revoked = yt.requestsTo("/revoke");
    REQUIRE(revoked.size() == 1);
    CHECK(revoked[0].body.find("demo-refresh-token") != std::string::npos);
    CHECK(test::readFile(path).find("demo-refresh-token") == std::string::npos);
}

TEST_CASE("account: the previous app's sign-in is imported once") {
    TempDir dir;
    const std::string old = dir.path + "/config.json";
    REQUIRE(core::writeFileAtomic(old, R"({"setting":{"yt_refresh_token":"old-refresh-token","x":1}})"));
    const std::string path = dir.path + "/account.json";
    {
        Account account(path);
        account.load();
        CHECK(account.importFromOld(old));
        CHECK(account.signedIn());
    }
    CHECK(test::readFile(old).find("old-refresh-token") != std::string::npos);  // left as it was
    Account account(path);
    account.load();
    CHECK(account.signedIn());
    FakeYouTube yt;
    const auto http = testHttp();
    account.signOut(Auth(http, at(yt)));
    CHECK_FALSE(account.importFromOld(old));  // never again, even signed out
    CHECK_FALSE(account.signedIn());

    Account fresh(dir.path + "/other.json");
    CHECK_FALSE(fresh.importFromOld(dir.path + "/missing.json"));
    CHECK_FALSE(fresh.importFromOld(dir.path + "/missing.json"));
    REQUIRE(core::writeFileAtomic(dir.path + "/top.json", R"({"yt_refresh_token":"top-level"})"));
    Account top(dir.path + "/top-account.json");
    CHECK(top.importFromOld(dir.path + "/top.json"));
}
