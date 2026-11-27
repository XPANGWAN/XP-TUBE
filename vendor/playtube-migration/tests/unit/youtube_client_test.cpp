#include <doctest/doctest.h>

#include <chrono>
#include <string>

#include "net/http.hpp"
#include "support/fake_youtube.hpp"
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
    endpoints.youtube = endpoints.suggest = endpoints.sponsor = endpoints.oauthRevoke = endpoints.dislikes =
        endpoints.stats = yt.baseUrl();
    return endpoints;
}

}  // namespace

TEST_CASE("client: search, next page, channel and video page, with the web client's headers") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto page = client.search("lofi");
    REQUIRE(page.ok());
    CHECK(page.value().items.size() == 2);
    CHECK(page.value().items[0].thumbnail.rfind(yt.baseUrl() + "/img/", 0) == 0);  // pictures served by the fake
    const auto more = client.searchMore(page.value().continuation);
    REQUIRE(more.ok());
    CHECK(more.value().items.size() == 3);
    const auto searches = yt.requestsTo("/youtubei/v1/search");
    REQUIRE(searches.size() == 2);
    CHECK(searches[0].clientName == "WEB");
    CHECK(searches[0].clientHeader == "1");
    CHECK(searches[0].userAgent.find("Chrome/") != std::string::npos);
    CHECK(searches[0].body.find(R"("query":"lofi")") != std::string::npos);
    CHECK(searches[0].body.find(R"("hl":"fr")") != std::string::npos);
    CHECK(searches[1].body.find(page.value().continuation) != std::string::npos);

    const auto channel = client.channel("UCSJ4gkVC6NrvII8umztf0Ow");
    REQUIRE(channel.ok());
    CHECK(channel.value().channel.id == "UCSJ4gkVC6NrvII8umztf0Ow");
    CHECK(channel.value().channel.name == "Lofi Girl");
    CHECK(channel.value().channel.avatar.rfind(yt.baseUrl() + "/img/", 0) == 0);
    CHECK(channel.value().channel.banner.rfind(yt.baseUrl() + "/img/", 0) == 0);
    for (const youtube::Video& video : channel.value().videos.items) {
        CHECK(video.thumbnail.rfind(yt.baseUrl() + "/img/", 0) == 0);
    }
    const auto next = client.next("rFZHOHl-L8A");
    REQUIRE(next.ok());
    CHECK(next.value().related.size() == 4);
}

TEST_CASE("client: suggestions and SponsorBlock segments; a video without segments has none") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto suggestions = client.suggestions("lofi");
    REQUIRE(suggestions.ok());
    CHECK(suggestions.value().size() == 16);
    CHECK(yt.requestsTo("/complete/search").size() == 1);
    const auto segments = client.segments("dQw4w9WgXcQ", {"sponsor", "selfpromo"});
    REQUIRE(segments.ok());
    CHECK(segments.value().size() == 1);
    const auto none = client.segments("aaaaaaaaaaa", {"sponsor"});
    REQUIRE(none.ok());
    CHECK(none.value().empty());
}

TEST_CASE("client: streams come from the first client that answers, with its user agent and the visitor id") {
    FakeYouTube yt;
    yt.setPlayer("ANDROID_VR", yt.playerReply({137, 136, 140, 18}));
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto streams = client.streams("dQw4w9WgXcQ", 720);
    REQUIRE(streams.ok());
    CHECK(streams.value().client == "ANDROID_VR");
    CHECK(streams.value().video == yt.baseUrl() + "/media/136?clen=200000");
    CHECK(streams.value().audio == yt.baseUrl() + "/media/140?clen=200000");
    CHECK(streams.value().height == 720);
    const auto players = yt.requestsTo("/youtubei/v1/player");
    REQUIRE(players.size() == 2);
    CHECK(players[0].clientName == "VISIONOS");
    CHECK(players[0].clientHeader == "101");
    CHECK(players[0].visitorId == "CgtkZW1vVmlzaXRvcg%3D%3D");
    CHECK(players[1].clientName == "ANDROID_VR");
    CHECK(players[1].clientHeader == "28");
    CHECK(streams.value().userAgent == players[1].userAgent);
    CHECK(yt.requestsTo("/sw.js_data").size() == 1);
    // the file was checked with the same user agent
    const auto media = yt.requestsTo("/media/136");
    REQUIRE_FALSE(media.empty());
    CHECK(media[0].userAgent == players[1].userAgent);

    REQUIRE(client.streams("dQw4w9WgXcQ", 720).ok());
    CHECK(yt.requestsTo("/sw.js_data").size() == 1);  // visitor id kept
}

TEST_CASE("client: refused separate streams fall back to the muxed 360p, then to the next client") {
    FakeYouTube yt;
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140, 18}));
    yt.refuseMedia(137);
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto muxed = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(muxed.ok());
    CHECK(muxed.value().client == "VISIONOS");
    CHECK(muxed.value().video == yt.baseUrl() + "/media/18?clen=200000");
    CHECK(muxed.value().audio.empty());

    yt.refuseMedia(18);
    yt.setPlayer("ANDROID_VR", yt.playerReply({135, 140}));
    const auto vr = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(vr.ok());
    CHECK(vr.value().client == "ANDROID_VR");
    CHECK(vr.value().video == yt.baseUrl() + "/media/135?clen=200000");
}

TEST_CASE("client: IOS and ANDROID give their muxed 360p only (their separate streams stop after a few MB)") {
    FakeYouTube yt;
    yt.setPlayer("IOS", yt.playerReply({137, 140, 18}));
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto streams = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(streams.ok());
    CHECK(streams.value().client == "IOS");
    CHECK(streams.value().video == yt.baseUrl() + "/media/18?clen=200000");
    CHECK(streams.value().audio.empty());
}

TEST_CASE("client: when every client refuses with a visitor id kept from before, a new one is tried once") {
    FakeYouTube yt;
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140}));
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    REQUIRE(client.streams("dQw4w9WgXcQ", 1080).ok());
    yt.setPlayer("VISIONOS", test::readFixture("youtube/player_refused.json"));
    CHECK_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
    CHECK(yt.requestsTo("/sw.js_data").size() == 2);
    // the four clients, then the first one alone with the new id (the bot check refused them all: they rest)
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 1 + 4 + 1);
    CHECK_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());  // the new id did not help: none asked for a while
    CHECK(yt.requestsTo("/sw.js_data").size() == 2);
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 1 + 4 + 1 + 1);
}

TEST_CASE("client: clients refused by the bot check rest: a blocked connection costs one request a video") {
    FakeYouTube yt;  // refuses every client: "pas un robot"
    const auto http = testHttp();
    Client client(http, at(yt), "fr", "FR");
    auto now = std::chrono::steady_clock::now();
    client.setClock([&now] { return now; });
    const auto first = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE_FALSE(first.ok());
    CHECK(first.error().kind == core::ErrorKind::Blocked);
    CHECK(first.error().message == "Connectez-vous pour confirmer que vous n'\xC3\xAAtes pas un robot");
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4);

    // another video: the first client alone, once more with a new visitor id, then no new id for half an hour
    CHECK(client.streams("yD1EYj8xct0", 1080).error().kind == core::ErrorKind::Blocked);
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 2);
    CHECK(yt.requestsTo("/sw.js_data").size() == 2);
    CHECK_FALSE(client.streams("yD1EYj8xct0", 1080).ok());
    CHECK_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 2 + 2);
    CHECK(yt.requestsTo("/sw.js_data").size() == 2);
    CHECK(yt.requestsTo("/youtubei/v1/player").back().clientName == "VISIONOS");

    now += std::chrono::minutes(11);  // the rest is over: every client is asked again
    CHECK_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 2 + 2 + 4);
    CHECK(yt.requestsTo("/sw.js_data").size() == 2);

    now += std::chrono::minutes(31);  // and a new visitor id may be tried again
    CHECK_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
    CHECK(yt.requestsTo("/sw.js_data").size() == 3);
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 2 + 2 + 4 + 4 + 1);
}

TEST_CASE("client: a client refused in the bot check's words rests too, whatever its reply's shape") {
    // seen 2026-11-20 on the console: IOS and ANDROID's refusals carry more than VISIONOS's and were asked again
    FakeYouTube yt;
    const std::string mobile = R"({"playabilityStatus":{"status":"LOGIN_REQUIRED",
        "reason":"Connectez-vous pour confirmer que vous n'\u00eates pas un robot",
        "errorScreen":{"playerErrorMessageRenderer":{"subreason":{"simpleText":"Connectez-vous"}}}}})";
    yt.setPlayer("IOS", mobile);
    yt.setPlayer("ANDROID", mobile);
    const auto http = testHttp();
    Client client(http, at(yt), "fr", "FR");
    auto now = std::chrono::steady_clock::now();
    client.setClock([&now] { return now; });
    REQUIRE_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4);
    CHECK(client.streams("yD1EYj8xct0", 1080).error().kind == core::ErrorKind::Blocked);
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 2);  // the first client alone, twice (a new id)
    CHECK_FALSE(client.streams("yD1EYj8xct0", 1080).ok());
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 2 + 1);
    CHECK(yt.requestsTo("/youtubei/v1/player").back().clientName == "VISIONOS");
}

TEST_CASE("client: a resting client that YouTube accepts again plays at once") {
    FakeYouTube yt;
    const auto http = testHttp();
    Client client(http, at(yt), "fr", "FR");
    REQUIRE_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140}));  // the block lifts
    const auto streams = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(streams.ok());
    CHECK(streams.value().client == "VISIONOS");
    CHECK(streams.value().height == 1080);
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4 + 1);
}

TEST_CASE("client: a refusal that is not the bot check does not rest the clients") {
    FakeYouTube yt;
    const std::string gone = R"({"playabilityStatus":{"status":"ERROR","reason":"Vid\u00e9o non disponible"}})";
    for (const char* name : {"VISIONOS", "ANDROID_VR", "IOS", "ANDROID"}) {
        yt.setPlayer(name, gone);
    }
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto refused = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().kind == core::ErrorKind::Server);
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4);
    yt.setPlayer("ANDROID_VR", yt.playerReply({137, 140}));
    const auto other = client.streams("yD1EYj8xct0", 1080);  // every client is asked for another video
    REQUIRE(other.ok());
    CHECK(other.value().client == "ANDROID_VR");
}

TEST_CASE("client: HD streams refused with a visitor id kept for hours: a new id is tried before the 360p") {
    FakeYouTube yt;
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140, 18}));
    yt.setPlayer("ANDROID", yt.playerReply({18}));
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    REQUIRE(client.streams("dQw4w9WgXcQ", 1080).ok());
    yt.expireVisitor();  // VISIONOS's files refused, ANDROID_VR "not a robot", ANDROID's 360p still served
    const auto streams = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(streams.ok());
    CHECK(streams.value().client == "VISIONOS");
    CHECK(streams.value().height == 1080);
    CHECK(yt.requestsTo("/sw.js_data").size() == 2);

    // HD refused whatever the id: one new id is tried within a call, then the 360p is kept (no loop)
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140}));
    yt.refuseMedia(137);
    const auto fallback = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(fallback.ok());
    CHECK(fallback.value().client == "ANDROID");
    CHECK(fallback.value().height == 360);
    CHECK(yt.requestsTo("/sw.js_data").size() == 3);
    REQUIRE(client.streams("dQw4w9WgXcQ", 1080).ok());  // the new id did not help: none asked for a while
    CHECK(yt.requestsTo("/sw.js_data").size() == 3);
}

TEST_CASE("client: the 360p asked for is no setback: the visitor id is kept") {
    FakeYouTube yt;
    yt.setPlayer("ANDROID", yt.playerReply({18}));
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    REQUIRE(client.streams("dQw4w9WgXcQ", 360).ok());
    REQUIRE(client.streams("dQw4w9WgXcQ", 360).ok());
    CHECK(yt.requestsTo("/sw.js_data").size() == 1);
}

TEST_CASE("client: no network is a network error, not a refusal") {
    const auto http = testHttp();
    Endpoints nowhere;
    nowhere.youtube = "https://127.0.0.1:9";
    const Client client(http, nowhere, "fr", "FR");
    const auto streams = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE_FALSE(streams.ok());
    CHECK(streams.error().kind == core::ErrorKind::Network);
}

TEST_CASE("client: every client refuses: YouTube's reason, or Invidious when set") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto refused = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message == "Connectez-vous pour confirmer que vous n'\xC3\xAAtes pas un robot");
    CHECK(yt.requestsTo("/youtubei/v1/player").size() == 4);
    CHECK(yt.requestsTo("/api/v1/videos/dQw4w9WgXcQ").empty());

    const auto invidious = client.streams("dQw4w9WgXcQ", 1080, yt.baseUrl());
    REQUIRE(invidious.ok());
    CHECK(invidious.value().client == "Invidious");
    CHECK(invidious.value().height == 1080);
    const auto asked = yt.requestsTo("/api/v1/videos/dQw4w9WgXcQ");
    REQUIRE(asked.size() == 1);
}

TEST_CASE("client: a live video plays its HLS playlist") {
    FakeYouTube yt;
    yt.setPlayer("VISIONOS", test::readFixture("youtube/player_live.json"));
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto streams = client.streams("jfKfPfyJRdk", 1080);
    REQUIRE(streams.ok());
    CHECK(streams.value().live);
    CHECK(streams.value().hls.rfind("https://", 0) == 0);
    CHECK(streams.value().video.empty());
}

TEST_CASE("client: signed-in feeds through the TV client with the token; an expired token is reported") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto feed = client.feed("FEsubscriptions", "demo-access-token");
    REQUIRE(feed.ok());
    CHECK_FALSE(feed.value().items.empty());
    const auto browses = yt.requestsTo("/youtubei/v1/browse");
    REQUIRE(browses.size() == 1);
    CHECK(browses[0].clientName == "TVHTML5");
    CHECK(browses[0].clientHeader == "7");
    CHECK(browses[0].authorization == "Bearer demo-access-token");
    CHECK(browses[0].body.find(R"("browseId":"FEsubscriptions")") != std::string::npos);
    const auto name = client.accountName("demo-access-token");
    REQUIRE(name.ok());
    CHECK(name.value() == "D\xC3\xA9mo");

    yt.failNext("/youtubei/v1/browse", 401);
    const auto expired = client.feed("FEwhat_to_watch", "old-token");
    REQUIRE_FALSE(expired.ok());
    CHECK(expired.error().kind == core::ErrorKind::Unauthorized);
}

TEST_CASE("client: a server error or a damaged reply is an error, not an empty page") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    yt.failNext("/youtubei/v1/search", 500);
    const auto failed = client.search("lofi");
    REQUIRE_FALSE(failed.ok());
    CHECK(failed.error().kind == core::ErrorKind::Server);
    core::CancelToken cancel;
    cancel.cancel();
    const auto cancelled = client.search("lofi", cancel);
    REQUIRE_FALSE(cancelled.ok());
    CHECK(cancelled.error().kind == core::ErrorKind::Cancelled);
}

TEST_CASE("client: a video added to watch later through the TV client with the token") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto added = client.addToPlaylist("WL", "dQw4w9WgXcQ", "demo-access-token");
    REQUIRE(added.ok());
    const auto requests = yt.requestsTo("/youtubei/v1/browse/edit_playlist");
    REQUIRE(requests.size() == 1);
    CHECK(requests[0].clientName == "TVHTML5");
    CHECK(requests[0].authorization == "Bearer demo-access-token");
    CHECK(requests[0].body.find(R"("playlistId":"WL")") != std::string::npos);
    CHECK(requests[0].body.find(R"("addedVideoId":"dQw4w9WgXcQ")") != std::string::npos);
    CHECK(requests[0].body.find(R"("action":"ACTION_ADD_VIDEO")") != std::string::npos);
    yt.failNext("/youtubei/v1/browse/edit_playlist", 401);
    const auto expired = client.addToPlaylist("WL", "dQw4w9WgXcQ", "old");
    REQUIRE_FALSE(expired.ok());
    CHECK(expired.error().kind == core::ErrorKind::Unauthorized);
}

TEST_CASE("client: the probe asks for bytes deep in the file, where a PO-token stream refuses") {
    FakeYouTube yt;
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140, 18}));
    yt.refuseDeepRanges(137);  // the first bytes answer, further on 403 (as mpv's "Range: bytes=0-" gets)
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto muxed = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(muxed.ok());
    CHECK(muxed.value().client == "VISIONOS");
    CHECK(muxed.value().video == yt.baseUrl() + "/media/18?clen=200000");
    const auto probes = yt.requestsTo("/media/137");
    REQUIRE_FALSE(probes.empty());
    CHECK(probes[0].range == "bytes=160000-160000");  // 80 % of clen

    yt.refuseDeepRanges(18);
    yt.setPlayer("ANDROID_VR", yt.playerReply({136, 140}));
    const auto vr = client.streams("dQw4w9WgXcQ", 1080);
    REQUIRE(vr.ok());
    CHECK(vr.value().client == "ANDROID_VR");
}

TEST_CASE("client: a refused audio stream is a refused client too") {
    FakeYouTube yt;
    yt.setPlayer("VISIONOS", yt.playerReply({137, 140}));
    yt.refuseDeepRanges(140);
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    CHECK_FALSE(client.streams("dQw4w9WgXcQ", 1080).ok());
}

TEST_CASE("client: comments from the video page's continuation, their next page and a comment's replies") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto video = client.next("dQw4w9WgXcQ");
    REQUIRE(video.ok());
    REQUIRE_FALSE(video.value().commentsContinuation.empty());
    const auto page = client.comments(video.value().commentsContinuation);
    REQUIRE(page.ok());
    REQUIRE(page.value().items.size() == 4);
    CHECK(page.value().items[0].author == "@auteur1");
    const auto more = client.comments(page.value().continuation);
    REQUIRE(more.ok());
    CHECK(more.value().items.size() == 4);
    CHECK(more.value().items[0].author != "@auteur1");
    const auto replies = client.comments(page.value().items[0].repliesContinuation);
    REQUIRE(replies.ok());
    REQUIRE_FALSE(replies.value().items.empty());
    CHECK(replies.value().items[0].text.rfind("R\xC3\xA9ponse", 0) == 0);
    for (const auto& request : yt.requestsTo("/youtubei/v1/next")) {
        CHECK(request.clientName == "WEB");
        CHECK(request.authorization.empty());
    }
}

TEST_CASE("client: the account's rating and subscription, rating a video and subscribing, with the TV client") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto state = client.videoState("dQw4w9WgXcQ", "demo-access-token");
    REQUIRE(state.ok());
    CHECK(state.value().rating == Rating::None);
    CHECK(state.value().channelId == "UCuAXFkgsw1L7xaCfnd5JJOw");
    const auto stateRequests = yt.requestsTo("/youtubei/v1/next");
    REQUIRE(stateRequests.size() == 1);
    CHECK(stateRequests[0].clientName == "TVHTML5");
    CHECK(stateRequests[0].authorization == "Bearer demo-access-token");

    REQUIRE(client.rate("dQw4w9WgXcQ", Rating::Like, "demo-access-token").ok());
    REQUIRE(client.rate("dQw4w9WgXcQ", Rating::Dislike, "demo-access-token").ok());
    REQUIRE(client.rate("dQw4w9WgXcQ", Rating::None, "demo-access-token").ok());
    for (const char* path : {"/youtubei/v1/like/like", "/youtubei/v1/like/dislike", "/youtubei/v1/like/removelike"}) {
        INFO(path);
        const auto requests = yt.requestsTo(path);
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].clientName == "TVHTML5");
        CHECK(requests[0].authorization == "Bearer demo-access-token");
        CHECK(requests[0].body.find(R"("target":{"videoId":"dQw4w9WgXcQ"})") != std::string::npos);
    }
    yt.failNext("/youtubei/v1/like/like", 401);
    const auto expired = client.rate("dQw4w9WgXcQ", Rating::Like, "old");
    REQUIRE_FALSE(expired.ok());
    CHECK(expired.error().kind == core::ErrorKind::Unauthorized);

    REQUIRE(client.setSubscribed("UCuAXFkgsw1L7xaCfnd5JJOw", true, "demo-access-token").ok());
    REQUIRE(client.setSubscribed("UCuAXFkgsw1L7xaCfnd5JJOw", false, "demo-access-token").ok());
    for (const char* path : {"/youtubei/v1/subscription/subscribe", "/youtubei/v1/subscription/unsubscribe"}) {
        INFO(path);
        const auto requests = yt.requestsTo(path);
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].authorization == "Bearer demo-access-token");
        CHECK(requests[0].body.find(R"("channelIds":["UCuAXFkgsw1L7xaCfnd5JJOw"])") != std::string::npos);
    }
}

TEST_CASE("client: the account's playlists, where to save a video, saving it, a channel's and a playlist's videos") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto mine = client.feed("FEplaylist_aggregation", "demo-access-token");
    REQUIRE(mine.ok());
    REQUIRE(mine.value().items.size() == 6);
    CHECK(mine.value().items[0].playlist);

    const auto choices = client.playlistChoices("dQw4w9WgXcQ", "demo-access-token");
    REQUIRE(choices.ok());
    REQUIRE(choices.value().size() == 5);
    const auto asked = yt.requestsTo("/youtubei/v1/playlist/get_add_to_playlist");
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].body.find(R"("videoIds":["dQw4w9WgXcQ"])") != std::string::npos);
    REQUIRE(client.addToPlaylist(choices.value()[1].id, "dQw4w9WgXcQ", "demo-access-token").ok());
    const auto saved = yt.requestsTo("/youtubei/v1/browse/edit_playlist");
    REQUIRE(saved.size() == 1);
    CHECK(saved[0].body.find(R"("playlistId":"PLdemo02xxxxxxxxxxxxxxxxxxxxxxxxxx")") != std::string::npos);

    const auto channel = client.channelPlaylists("UCSJ4gkVC6NrvII8umztf0Ow");
    REQUIRE(channel.ok());
    REQUIRE(channel.value().items.size() == 4);
    const auto browsed = yt.requestsTo("/youtubei/v1/browse");
    CHECK(browsed.back().clientName == "WEB");
    CHECK(browsed.back().body.find(R"("params":"EglwbGF5bGlzdHPyBgQKAkIA")") != std::string::npos);

    const auto open = client.playlist("PLaklDK5Yxuss", "");
    REQUIRE(open.ok());
    CHECK(open.value().items.size() == 6);
    CHECK_FALSE(open.value().continuation.empty());
    const auto request = yt.requestsTo("/youtubei/v1/browse").back();
    CHECK(request.clientName == "TVHTML5");
    CHECK(request.authorization.empty());
    CHECK(request.body.find(R"("browseId":"VLPLaklDK5Yxuss")") != std::string::npos);
    REQUIRE(client.feedMore(open.value().continuation, "").ok());
}

TEST_CASE("client: where to report watching (TV client, signed in), and a report sent with the account's token") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto tracking = client.playbackTracking("dQw4w9WgXcQ", "demo-access-token", "AbCdEfGh01234567");
    REQUIRE(tracking.ok());
    CHECK(tracking.value().playbackUrl == yt.baseUrl() + "/api/stats/playback?ns=yt&el=leanback&docid=dQw4w9WgXcQ&len=212");
    CHECK(tracking.value().watchtimeUrl.rfind(yt.baseUrl() + "/api/stats/watchtime?", 0) == 0);
    const auto players = yt.requestsTo("/youtubei/v1/player");
    REQUIRE(players.size() == 1);
    CHECK(players[0].clientName == "TVHTML5");
    CHECK(players[0].authorization == "Bearer demo-access-token");
    // the TV player's own signature timestamp (8 digits) and the playback's nonce: without the timestamp YouTube
    // answers UNPLAYABLE "La page doit être actualisée" and no address (seen 2026-11-20)
    CHECK(players[0].body.find(R"("playbackContext":{"contentPlaybackContext":{"signatureTimestamp":2)") !=
          std::string::npos);
    CHECK(players[0].body.find(R"("cpn":"AbCdEfGh01234567")") != std::string::npos);

    REQUIRE(client.report(tracking.value().playbackUrl + "&ver=2&cpn=abc", "demo-access-token").ok());
    const auto reports = yt.requestsTo("/api/stats/playback");
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].authorization == "Bearer demo-access-token");
    CHECK(reports[0].target.find("&cpn=abc") != std::string::npos);
}

TEST_CASE("client: the token goes to YouTube's reporting addresses only") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    for (const std::string& url : {yt.baseUrl() + "/votes?videoId=x",                      // not a report
                                   std::string("https://example.com/api/stats/playback?x"),  // not YouTube
                                   "http" + yt.baseUrl().substr(5) + "/api/stats/playback?x"}) {  // not https
        CHECK_FALSE(client.report(url, "demo-access-token").ok());
    }
    CHECK(yt.requests().empty());
    CHECK_FALSE(client.playbackTracking("dQw4w9WgXcQ", "", "AbCdEfGh01234567").ok());  // signed out: no addresses
}

TEST_CASE("client: a live chat's page with the web client") {
    FakeYouTube yt;
    const auto http = testHttp();
    const Client client(http, at(yt), "fr", "FR");
    const auto page = client.liveChat("0ofMyAOEARpe");
    REQUIRE(page.ok());
    CHECK(page.value().messages.size() == 3);
    const auto asked = yt.requestsTo("/youtubei/v1/live_chat/get_live_chat");
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].clientName == "WEB");
    CHECK(asked[0].body.find(R"("continuation":"0ofMyAOEARpe")") != std::string::npos);
    CHECK(asked[0].authorization.empty());
}

TEST_CASE("client: DeArrow's branding of a video and its thumbnail's address") {
    FakeYouTube yt;
    const auto http = testHttp();
    Endpoints endpoints = at(yt);
    endpoints.dearrow = endpoints.dearrowThumbnails = yt.baseUrl();
    const Client client(http, endpoints, "fr", "FR");
    const auto branding = client.branding("lVylRtlPOIE");
    REQUIRE(branding.ok());
    CHECK(branding.value().title == "Granting Wishes for 100 People with Cancer");
    const auto asked = yt.requestsTo("/api/branding");
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].target == "/api/branding?videoID=lVylRtlPOIE");
    CHECK(asked[0].authorization.empty());
    // the time as DeArrow's API gave it: its pictures are kept under that exact text, a rounded one is not found
    const std::string thumbnails = yt.baseUrl() + "/api/v1/getThumbnail?videoID=lVylRtlPOIE&time=";
    CHECK(client.brandingThumbnail("lVylRtlPOIE", branding.value().thumbnailTime) == thumbnails + "219.059131");
    CHECK(client.brandingThumbnail("lVylRtlPOIE", 1267.3677896666668) == thumbnails + "1267.3677896666668");
    CHECK(client.brandingThumbnail("lVylRtlPOIE", 1000) == thumbnails + "1000");
    CHECK(client.brandingThumbnail("lVylRtlPOIE", 12.5) == thumbnails + "12.5");
    CHECK(client.brandingThumbnail("lVylRtlPOIE", 0) == thumbnails + "0");
}
