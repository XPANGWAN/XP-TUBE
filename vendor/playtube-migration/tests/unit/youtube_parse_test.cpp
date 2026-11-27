#include <doctest/doctest.h>

#include <string>

#include "support/files.hpp"
#include "youtube/parse.hpp"

using namespace playtube;
using namespace playtube::youtube;

namespace {

std::string fixture(const char* name) { return test::readFixture(std::string("youtube/") + name); }

bool https(const std::string& url) { return url.rfind("https://", 0) == 0; }

}  // namespace

TEST_CASE("search: videos with their channel, views, date and duration; a live video; the next page") {
    const auto page = parseSearch(fixture("search.json"));
    REQUIRE(page.ok());
    REQUIRE(page.value().items.size() == 2);
    const Video& live = page.value().items[0];
    CHECK(live.id == "rFZHOHl-L8A");
    CHECK(live.title == "lofi hip hop radio \xF0\x9F\x93\x9A beats to relax/study to");
    CHECK(live.channelName == "Lofi Girl");
    CHECK(live.channelId == "UCSJ4gkVC6NrvII8umztf0Ow");
    CHECK(live.live);
    CHECK(live.durationText.empty());
    CHECK(https(live.thumbnail));
    const Video& video = page.value().items[1];
    CHECK(video.id == "n61ULEU7CO0");
    CHECK(video.durationText == "6:10:58");
    CHECK(video.durationSeconds == 6 * 3600 + 10 * 60 + 58);
    CHECK(video.viewsText == "58\xE2\x80\xAF" "138\xE2\x80\xAF" "619\xC2\xA0vues");
    CHECK(video.publishedText == "il y a 4 ans");
    CHECK_FALSE(video.live);
    CHECK_FALSE(page.value().continuation.empty());

    const auto more = parseSearch(fixture("search_continuation.json"));
    REQUIRE(more.ok());
    REQUIRE(more.value().items.size() == 3);
    CHECK(more.value().items[0].id == "gUbNlN_SqpE");
    CHECK_FALSE(more.value().continuation.empty());
}

TEST_CASE("channel: header and videos, with the next page of videos") {
    const auto page = parseChannel(fixture("channel.json"));
    REQUIRE(page.ok());
    const Channel& channel = page.value().channel;
    CHECK(channel.name == "Lofi Girl");
    CHECK(channel.handle == "@LofiGirl");
    CHECK(channel.subscribersText.find("abonn") != std::string::npos);
    CHECK(https(channel.avatar));
    CHECK(https(channel.banner));
    CHECK_FALSE(channel.description.empty());
    REQUIRE(page.value().videos.items.size() == 4);
    const Video& first = page.value().videos.items[0];
    CHECK(first.id == "vadHXNBy4jQ");
    CHECK(first.title == "8h of halloween lofi \xF0\x9F\x8E\x83 spooky ambience to get chills to");
    CHECK(first.durationText == "8:00:00");
    CHECK(first.viewsText == "223\xC2\xA0k\xC2\xA0vues");
    CHECK(first.publishedText == "il y a 2 jours");
    CHECK(https(first.thumbnail));
    CHECK_FALSE(page.value().videos.continuation.empty());
}

TEST_CASE("the video page: related videos and the channel that published it") {
    const auto next = parseNext(fixture("next.json"));
    REQUIRE(next.ok());
    REQUIRE(next.value().related.size() == 4);
    CHECK(next.value().related[0].id == "E2vONfzoyRI");
    CHECK(next.value().related[0].live);
    CHECK(next.value().related[0].channelName == "Lofi Girl");
    CHECK(next.value().related[2].durationText == "2:42:43");
    CHECK_FALSE(next.value().related[2].live);
    CHECK(next.value().owner.id == "UCSJ4gkVC6NrvII8umztf0Ow");
    CHECK(next.value().owner.name == "Lofi Girl");
    CHECK(https(next.value().owner.avatar));
}

TEST_CASE("player: direct formats up to 1080p, AAC audio, captions, HLS") {
    const auto reply = parsePlayer(fixture("player_visionos.json"));
    REQUIRE(reply.ok());
    const PlayerReply& p = reply.value();
    CHECK(p.playable);
    CHECK(p.title == "Rick Astley - Never Gonna Give You Up (Official Video) (4K Remaster)");
    CHECK(p.channelId == "UCuAXFkgsw1L7xaCfnd5JJOw");
    CHECK(p.durationSeconds == 213);
    CHECK_FALSE(p.live);
    bool video1080 = false;
    bool aac = false;
    for (const Format& f : p.formats) {
        video1080 = video1080 || (f.itag == 137 && f.height == 1080 && https(f.url));
        aac = aac || (f.itag == 140 && f.height == 0 && f.mimeType.rfind("audio/mp4", 0) == 0);
    }
    CHECK(video1080);
    CHECK(aac);
    CHECK(https(p.hls));
    REQUIRE(p.captions.size() >= 3);
    bool english = false;
    bool automatic = false;
    for (const CaptionTrack& t : p.captions) {
        english = english || (t.language == "en" && !t.automatic && t.name == "Anglais");
        automatic = automatic || t.automatic;
        CHECK(https(t.url));
    }
    CHECK(english);
    CHECK(automatic);
}

TEST_CASE("player: a refused video says why; a live video has its HLS playlist") {
    const auto refused = parsePlayer(fixture("player_refused.json"));
    REQUIRE(refused.ok());
    CHECK_FALSE(refused.value().playable);
    CHECK(refused.value().reason == "Connectez-vous pour confirmer que vous n'\xC3\xAAtes pas un robot");
    const auto live = parsePlayer(fixture("player_live.json"));
    REQUIRE(live.ok());
    CHECK(live.value().playable);
    CHECK(live.value().live);
    CHECK(https(live.value().hls));
}

TEST_CASE("signed-in feeds: TV tiles with channel, views, date, duration") {
    const auto page = parseTiles(fixture("tv_recommended.json"));
    REQUIRE(page.ok());
    REQUIRE(page.value().items.size() == 16);
    const Video& first = page.value().items[0];
    CHECK(first.id == "demoVideo01");
    CHECK(first.title == "Vid\xC3\xA9o 1");
    CHECK(first.channelName == "Cha\xC3\xAEne 1");
    CHECK(first.viewsText == "2\xC2\xA0M de vues");
    CHECK(first.publishedText == "il y a 2 jours");
    CHECK(first.durationText == "1:01:07");
    CHECK(first.durationSeconds == 3667);
    CHECK(https(first.thumbnail));
    CHECK(first.percentWatched == -1);                    // never watched
    CHECK(page.value().items[1].percentWatched == 17);    // demoVideo02: started
    CHECK(page.value().items[6].percentWatched == 100);   // demoVideo07: watched
    for (const char* name : {"tv_subscriptions.json", "tv_history.json", "tv_watch_later.json"}) {
        INFO(name);
        const auto feed = parseTiles(fixture(name));
        REQUIRE(feed.ok());
        CHECK_FALSE(feed.value().items.empty());
    }
}

TEST_CASE("suggestions, sponsor segments, visitor data, account name") {
    const auto suggestions = parseSuggestions(fixture("suggestions.json"));
    REQUIRE(suggestions.ok());
    REQUIRE(suggestions.value().size() == 16);
    CHECK(suggestions.value()[0] == "lofi");
    CHECK(suggestions.value()[1] == "lofi coffee shop");

    const auto segments = parseSegments(fixture("sponsorblock.json"));
    REQUIRE(segments.ok());
    REQUIRE(segments.value().size() == 1);
    CHECK(segments.value()[0].category == "sponsor");
    CHECK(segments.value()[0].start == doctest::Approx(0));
    CHECK(segments.value()[0].end == doctest::Approx(5));

    CHECK(parseVisitorData(fixture("sw_js_data.txt")) == "CgtkZW1vVmlzaXRvcg%3D%3D");
    CHECK(parseAccountName(fixture("tv_accounts_list.json")) == "D\xC3\xA9mo");
}

TEST_CASE("sign-in by code: the code, a pending poll, the tokens") {
    const auto code = parseDeviceCode(fixture("oauth_device_code.json"));
    REQUIRE(code.ok());
    CHECK(code.value().userCode == "ABC-DEF-GHI");
    CHECK(code.value().verificationUrl == "https://www.google.com/device");
    CHECK(code.value().interval == 5);
    const auto pending = parseToken(fixture("oauth_pending.json"));
    REQUIRE(pending.ok());
    CHECK(pending.value().pending);
    CHECK_FALSE(pending.value().slowDown);
    const auto slower = parseToken(R"({"error":"slow_down"})");
    REQUIRE(slower.ok());
    CHECK(slower.value().pending);
    CHECK(slower.value().slowDown);
    const auto token = parseToken(fixture("oauth_token.json"));
    REQUIRE(token.ok());
    CHECK(token.value().accessToken == "demo-access-token");
    CHECK(token.value().refreshToken == "demo-refresh-token");
    CHECK(token.value().expiresIn == 3599);
    CHECK_FALSE(token.value().pending);
}

TEST_CASE("Invidious: the same streams as YouTube's player") {
    const auto reply = parseInvidious(fixture("invidious_video.json"), "https://invidious.example");
    REQUIRE(reply.ok());
    CHECK(reply.value().playable);
    CHECK(reply.value().durationSeconds == 213);
    bool video1080 = false;
    bool aac = false;
    bool muxed = false;
    for (const Format& f : reply.value().formats) {
        video1080 = video1080 || (f.itag == 137 && f.height == 1080);
        aac = aac || (f.itag == 140 && f.height == 0);
        muxed = muxed || (f.itag == 18 && f.height == 360);
    }
    CHECK(video1080);
    CHECK(aac);
    CHECK(muxed);
    REQUIRE(reply.value().captions.size() == 1);
    CHECK(reply.value().captions[0].url.rfind("https://invidious.example/api/v1/captions/", 0) == 0);
}

TEST_CASE("unknown or damaged replies give what was understood, never a crash") {
    CHECK_FALSE(parseSearch("not json").ok());
    const auto empty = parseSearch(R"({"contents":{"somethingNewRenderer":{"items":[1,2,3]}}})");
    REQUIRE(empty.ok());
    CHECK(empty.value().items.empty());
    const auto partial = parseSearch(
        R"({"contents":[{"videoRenderer":{"videoId":"abcdefghijk","title":{"runs":[{"text":"T"}]}}},)"
        R"({"futureRenderer":{}},{"videoRenderer":{"title":{"runs":[{"text":"no id"}]}}}]})");
    REQUIRE(partial.ok());
    REQUIRE(partial.value().items.size() == 1);
    CHECK(partial.value().items[0].title == "T");
    CHECK(parseVisitorData("garbage").empty());
    CHECK_FALSE(parseSegments("Not Found").ok());
}

TEST_CASE("numbers out of range give nothing rather than overflowing") {
    CHECK(parseDuration("99999:99999:99999:99999:99999") == 0);
    CHECK(parseDuration("1:00:00:00") == 0);
    const auto player = parsePlayer(
        R"({"playabilityStatus":{"status":"OK"},"videoDetails":{"lengthSeconds":"99999999999999999999999"},)"
        R"("streamingData":{"adaptiveFormats":[{"itag":137,"url":"https://m.example/137","height":1080,)"
        R"("contentLength":"99999999999999999999999999"}]}})");
    REQUIRE(player.ok());
    CHECK(player.value().durationSeconds >= 0);
    REQUIRE(player.value().formats.size() == 1);
    CHECK(player.value().formats[0].contentLength >= 0);
    const auto code = parseDeviceCode(R"({"device_code":"d","user_code":"u","interval":0,"expires_in":-5})");
    REQUIRE(code.ok());
    CHECK(code.value().interval >= 1);
    CHECK(code.value().interval <= 60);
    CHECK(code.value().expiresIn > 0);
}

TEST_CASE("token errors: only a revoked or refused sign-in is a refusal") {
    for (const char* error : {"invalid_grant", "access_denied", "expired_token"}) {
        INFO(error);
        const auto token = parseToken(std::string(R"({"error":")") + error + "\"}");
        REQUIRE_FALSE(token.ok());
        CHECK(token.error().kind == core::ErrorKind::Unauthorized);
    }
    for (const char* error : {"invalid_client", "internal_failure", "rate_limit_exceeded"}) {
        INFO(error);
        const auto token = parseToken(std::string(R"({"error":")") + error + "\"}");
        REQUIRE_FALSE(token.ok());
        CHECK(token.error().kind != core::ErrorKind::Unauthorized);
    }
}

TEST_CASE("player: the bot check is told from an age gate, a private video and any other refusal") {
    const auto bot = parsePlayer(fixture("player_refused.json"));
    REQUIRE(bot.ok());
    CHECK_FALSE(bot.value().playable);
    CHECK(bot.value().botCheck);
    const auto age = parsePlayer(R"({"playabilityStatus":{"status":"LOGIN_REQUIRED",
        "reason":"Connectez-vous pour confirmer votre \u00e2ge","desktopLegacyAgeGateReason":1}})");
    REQUIRE(age.ok());
    CHECK_FALSE(age.value().botCheck);
    const auto priv = parsePlayer(R"({"playabilityStatus":{"status":"LOGIN_REQUIRED",
        "messages":["Cette vid\u00e9o est priv\u00e9e."]}})");
    REQUIRE(priv.ok());
    CHECK_FALSE(priv.value().botCheck);
    const auto gone = parsePlayer(R"({"playabilityStatus":{"status":"ERROR","reason":"Vid\u00e9o non disponible"}})");
    REQUIRE(gone.ok());
    CHECK_FALSE(gone.value().botCheck);
}

TEST_CASE("player: the original audio track is marked") {
    const auto reply = parsePlayer(
        R"({"playabilityStatus":{"status":"OK"},"streamingData":{"adaptiveFormats":[)"
        R"({"itag":140,"url":"https://m.example/de","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"",)"
        R"("audioTrack":{"displayName":"Deutsch","id":"de.3","audioIsDefault":false}},)"
        R"({"itag":140,"url":"https://m.example/en","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"",)"
        R"("audioTrack":{"displayName":"English original","id":"en.4","audioIsDefault":true}},)"
        R"({"itag":140,"url":"https://m.example/plain","mimeType":"audio/mp4; codecs=\"mp4a.40.2\""}]}})");
    REQUIRE(reply.ok());
    REQUIRE(reply.value().formats.size() == 3);
    CHECK_FALSE(reply.value().formats[0].defaultAudio);
    CHECK(reply.value().formats[1].defaultAudio);
    CHECK(reply.value().formats[2].defaultAudio);
    CHECK(reply.value().formats[0].audioTrackId == "de.3");
    CHECK(reply.value().formats[0].audioTrackName == "Deutsch");
    CHECK(reply.value().formats[2].audioTrackId.empty());
    const auto drc = parsePlayer(
        R"({"playabilityStatus":{"status":"OK"},"streamingData":{"adaptiveFormats":[)"
        R"({"itag":140,"url":"https://m.example/drc","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"","isDrc":true}]}})");
    REQUIRE(drc.ok());
    CHECK(drc.value().formats[0].drc);
}

TEST_CASE("thumbnails: plain JPEG addresses (the signed ones answer WebP)") {
    CHECK(jpegThumbnail("https://i.ytimg.com/vi/abc/hq720.jpg?sqp=-oay&rs=AOn") ==
          "https://i.ytimg.com/vi/abc/hq720.jpg");
    CHECK(jpegThumbnail("https://i.ytimg.com/vi/abc/hq720_custom_2.jpg?sqp=x") ==
          "https://i.ytimg.com/vi/abc/hqdefault.jpg");
    CHECK(jpegThumbnail("https://i.ytimg.com/vi_webp/abc/maxresdefault.webp") ==
          "https://i.ytimg.com/vi/abc/hqdefault.jpg");
    CHECK(jpegThumbnail("https://i.ytimg.com/vi/abc/hqdefault.jpg") == "https://i.ytimg.com/vi/abc/hqdefault.jpg");
    // a thumbnail localised for the reply's language exists only signed (WebP): the original's JPEG instead
    CHECK(jpegThumbnail("https://i.ytimg.com/vi_lc/abc/hq720_fr.jpg?sqp=x&rs=y") ==
          "https://i.ytimg.com/vi/abc/hq720.jpg");
    CHECK(jpegThumbnail("https://i.ytimg.com/vi_lc/abc/hqdefault_fr.jpg?sqp=x") ==
          "https://i.ytimg.com/vi/abc/hqdefault.jpg");
    CHECK(jpegThumbnail("https://yt3.ggpht.com/abc=s88") == "https://yt3.ggpht.com/abc=s88");  // not a video's
    const auto page = parseSearch(fixture("search.json"));
    REQUIRE(page.ok());
    for (const Video& video : page.value().items) {
        CHECK(video.thumbnail.find('?') == std::string::npos);
    }
}

TEST_CASE("comments: the first page's continuation in the video page; threads with author, text, likes, replies") {
    const auto next = parseNext(fixture("next_comments.json"));
    REQUIRE(next.ok());
    CHECK_FALSE(next.value().commentsContinuation.empty());

    const auto page = parseComments(fixture("comments.json"));
    REQUIRE(page.ok());
    REQUIRE(page.value().items.size() == 4);
    const Comment& first = page.value().items[0];
    CHECK(first.author == "@auteur1");
    CHECK(first.avatar == "https://yt3.ggpht.com/demo-avatar-1=s88-c-k-c0x00ffffff-no-rj");
    CHECK(first.text.rfind("Premier commentaire de d\xC3\xA9monstration", 0) == 0);
    CHECK(first.text.find('\n') != std::string::npos);
    CHECK(first.publishedText == "il y a 6 ans (modifi\xC3\xA9)");
    CHECK(first.likesText == "104\xC2\xA0k");
    CHECK(first.repliesText == "611");
    CHECK_FALSE(first.repliesContinuation.empty());
    CHECK(page.value().items[1].author == "@auteur2");
    CHECK_FALSE(page.value().continuation.empty());

    const auto more = parseComments(fixture("comments_more.json"));
    REQUIRE(more.ok());
    CHECK(more.value().items.size() == 4);
    CHECK_FALSE(more.value().continuation.empty());

    const auto replies = parseComments(fixture("comments_replies.json"));
    REQUIRE(replies.ok());
    REQUIRE(replies.value().items.size() == 4);
    CHECK(replies.value().items[0].text.rfind("R\xC3\xA9ponse ", 0) == 0);
    CHECK(replies.value().items[0].likesText == "6,9\xC2\xA0k");
    CHECK(replies.value().items[0].repliesText.empty());
    CHECK(replies.value().items[0].repliesContinuation.empty());
    CHECK_FALSE(replies.value().continuation.empty());  // "Afficher plus de réponses"
}

TEST_CASE("comments: a thread without its text in the entities is left out; damaged replies give nothing") {
    const auto page = parseComments(
        R"({"onResponseReceivedEndpoints":[{"appendContinuationItemsAction":{"continuationItems":[)"
        R"({"commentThreadRenderer":{"commentViewModel":{"commentViewModel":{"commentKey":"k1"}}}},)"
        R"({"commentThreadRenderer":{"commentViewModel":{"commentViewModel":{"commentKey":"k2"}}}}]}}],)"
        R"("frameworkUpdates":{"entityBatchUpdate":{"mutations":[{"entityKey":"k2","payload":{"commentEntityPayload":)"
        R"({"properties":{"content":{"content":"Bonjour"}},"author":{"displayName":"@a"}}}}]}}})");
    REQUIRE(page.ok());
    REQUIRE(page.value().items.size() == 1);
    CHECK(page.value().items[0].text == "Bonjour");
    CHECK(page.value().continuation.empty());
    CHECK_FALSE(parseComments("not json").ok());
}

TEST_CASE("the signed-in video page: the rating, the likes and the subscription to its channel") {
    const auto state = parseVideoState(fixture("tv_next.json"));
    REQUIRE(state.ok());
    CHECK(state.value().rating == Rating::None);
    CHECK(state.value().likesText == "19\xC2\xA0M");
    CHECK_FALSE(state.value().subscribed);
    CHECK(state.value().channelId == "UCuAXFkgsw1L7xaCfnd5JJOw");
    const auto liked = parseVideoState(
        R"({"x":{"likeButtonRenderer":{"likeStatus":"LIKE","likeCountText":{"simpleText":"20 M"}}},)"
        R"("y":{"subscribeButtonRenderer":{"subscribed":true,"channelId":"UCabc"}}})");
    REQUIRE(liked.ok());
    CHECK(liked.value().rating == Rating::Like);
    CHECK(liked.value().subscribed);
    CHECK(parseVideoState(R"({"likeButtonRenderer":{"likeStatus":"DISLIKE"}})").value().rating == Rating::Dislike);
}

TEST_CASE("playlists: the account's, a channel's, in search results (mixes left out), and where to save a video") {
    const auto mine = parseTiles(fixture("tv_playlists.json"));
    REQUIRE(mine.ok());
    REQUIRE(mine.value().items.size() == 6);
    const Video& later = mine.value().items[0];
    CHECK(later.playlist);
    CHECK(later.id == "WL");
    CHECK(later.title == "\xC3\x80 regarder plus tard");
    CHECK(later.videoCountText == "584\xC2\xA0vid\xC3\xA9os");
    CHECK(later.durationText.empty());
    CHECK(later.publishedText == "Mise \xC3\xA0 jour aujourd'hui");
    CHECK(https(later.thumbnail));
    CHECK(mine.value().items[1].id == "PLdemo01xxxxxxxxxxxxxxxxxxxxxxxxxx");
    CHECK_FALSE(mine.value().continuation.empty());

    const auto videos = parseTiles(fixture("tv_playlist.json"));
    REQUIRE(videos.ok());
    CHECK(videos.value().items.size() == 6);
    CHECK(videos.value().items[0].id == "jICmkF585LA");
    CHECK_FALSE(videos.value().items[0].playlist);

    const auto channel = parseChannelPlaylists(fixture("channel_playlists.json"));
    REQUIRE(channel.ok());
    REQUIRE(channel.value().items.size() == 4);
    const Video& house = channel.value().items[0];
    CHECK(house.playlist);
    CHECK(house.id == "PLaklDK5Yxuss");
    CHECK(house.title == "Lofi House 2026");
    CHECK(house.videoCountText == "198\xC2\xA0vid\xC3\xA9os");
    CHECK(house.thumbnail == "https://i.ytimg.com/pl_c/PLaklDK5Yxuss/studio_square_thumbnail.jpg");

    const auto search = parseSearch(fixture("search_playlists.json"));
    REQUIRE(search.ok());
    REQUIRE_FALSE(search.value().items.empty());
    const Video& found = search.value().items[0];
    CHECK(found.playlist);
    CHECK(found.id == "PLe0XBeaAl1KElgUjwGuNtut0AdjryI4DD");
    CHECK(found.channelName == "Auteur 1");
    CHECK(found.videoCountText == "17\xC2\xA0vid\xC3\xA9os");
    CHECK(found.thumbnail == "https://i.ytimg.com/vi/CFGLoQIhmow/hq720.jpg");
    const auto mix = parseSearch(
        R"({"contents":[{"lockupViewModel":{"contentType":"LOCKUP_CONTENT_TYPE_PLAYLIST","contentId":"RDabc",)"
        R"("metadata":{"lockupMetadataViewModel":{"title":{"content":"Mix"}}}}}]})");
    REQUIRE(mix.ok());
    CHECK(mix.value().items.empty());

    const auto choices = parseAddToPlaylist(fixture("tv_add_to_playlist.json"));
    REQUIRE(choices.ok());
    REQUIRE(choices.value().size() == 5);
    CHECK(choices.value()[0].id == "WL");
    CHECK(choices.value()[0].title == "\xC3\x80 regarder plus tard");
    CHECK_FALSE(choices.value()[0].contains);
    CHECK(choices.value()[1].title == "Playlist 2");
    const auto holding = parseAddToPlaylist(
        R"({"x":[{"playlistAddToOptionRenderer":{"playlistId":"PLa","title":{"simpleText":"A"},)"
        R"("containsSelectedVideos":"ALL"}}]})");
    REQUIRE(holding.ok());
    CHECK(holding.value()[0].contains);
}

TEST_CASE("the video page's chapters: in order from 0, titles as YouTube writes them") {
    const auto next = parseNext(fixture("next_chapters.json"));  // recorded 2026-11-11, titles replaced
    REQUIRE(next.ok());
    const auto& chapters = next.value().chapters;
    REQUIRE(chapters.size() == 6);
    CHECK(chapters[0].start == 0);
    CHECK(chapters[0].title == "Chapitre 1");
    CHECK(chapters[1].start == doctest::Approx(117));
    CHECK(chapters[5].title == "Chapitre 6");
    const auto none = parseNext(fixture("next.json"));
    REQUIRE(none.ok());
    CHECK(none.value().chapters.empty());
}

TEST_CASE("Shorts: a search for them and a channel's tab; their upright first frame as the picture") {
    const auto search = parseSearch(fixture("search_shorts.json"));
    REQUIRE(search.ok());
    REQUIRE(search.value().items.size() == 6);
    const Video& first = search.value().items[0];
    CHECK(first.isShort);
    CHECK(first.title == "Lofi Girl Sad Cat Dance");
    CHECK(first.viewsText == "1\xC2\xA0M de vues");
    CHECK(first.thumbnail == "https://i.ytimg.com/vi/" + first.id + "/frame0.jpg");
    const auto channel = parseChannel(fixture("channel_shorts.json"));
    REQUIRE(channel.ok());
    CHECK(channel.value().videos.items.size() == 6);
    CHECK(channel.value().videos.items[0].isShort);
    const auto plain = parseSearch(fixture("search.json"));  // a plain search's Shorts shelf is left out
    REQUIRE(plain.ok());
    for (const Video& video : plain.value().items) {
        CHECK_FALSE(video.isShort);
    }
}

TEST_CASE("live chat: the video page gives its first page; a page's messages, Super Chats, removals and the next") {
    const auto next = parseNext(fixture("next.json"));  // a live video (Lofi Girl's stream)
    REQUIRE(next.ok());
    CHECK_FALSE(next.value().liveChat.empty());
    CHECK(parseNext(fixture("next_chapters.json")).value().liveChat.empty());  // not a live

    const auto page = parseLiveChat(fixture("live_chat.json"));  // recorded 2026-11-13, authors and texts replaced
    REQUIRE(page.ok());
    REQUIRE(page.value().messages.size() == 3);  // the emoji-only message and YouTube's welcome left out
    CHECK(page.value().messages[0].author == "@viewer-one");
    CHECK(page.value().messages[0].text == "bonne soirée à tous ");
    CHECK(page.value().messages[0].amount.empty());
    CHECK(page.value().messages[1].author == "@viewer-three");
    CHECK(page.value().messages[1].amount == "5,00\xC2\xA0\xE2\x82\xAC");
    CHECK(page.value().messages[1].text == "merci pour le live");
    CHECK(page.value().messages[2].id == "ChwKGkNNVGVfX19fNQ");
    CHECK(page.value().removed == std::vector<std::string>{"ChwKGkNNVGVfX19fMA"});
    CHECK(page.value().continuation.rfind("0ofMyAOZ", 0) == 0);
    CHECK(page.value().waitMs == 10000);
    CHECK_FALSE(parseLiveChat("{}").ok());
    CHECK_FALSE(parseLiveChat("not json").ok());
}

TEST_CASE("DeArrow: the best trusted title and thumbnail; the original first or a disliked one keeps YouTube's") {
    const auto branding = parseBranding(fixture("../dearrow/branding.json"));  // recorded 2026-11-15
    REQUIRE(branding.ok());
    CHECK(branding.value().title == "Granting Wishes for 100 People with Cancer");
    CHECK(branding.value().thumbnailTime == doctest::Approx(219.059131));

    const auto original = parseBranding(R"({"titles":[{"title":"Theirs","original":true,"votes":5,"locked":false},
        {"title":"Another","original":false,"votes":2,"locked":false}],"thumbnails":[{"original":true,"votes":3}]})");
    REQUIRE(original.ok());
    CHECK(original.value().title.empty());
    CHECK(original.value().thumbnailTime < 0);

    const auto voted = parseBranding(R"({"titles":[{"title":"Bad","original":false,"votes":-2,"locked":false},
        {"title":"The >iPhone story","original":false,"votes":0,"locked":false}],
        "thumbnails":[{"timestamp":12.5,"original":false,"votes":-1,"locked":true}]})");
    REQUIRE(voted.ok());
    CHECK(voted.value().title == "The iPhone story");  // the disliked one skipped, the ">" marker removed
    CHECK(voted.value().thumbnailTime == doctest::Approx(12.5));  // locked by a moderator

    const auto none = parseBranding(R"({"titles":[],"thumbnails":[],"casualVotes":[],"videoDuration":null})");
    REQUIRE(none.ok());
    CHECK(none.value().title.empty());
    CHECK(none.value().thumbnailTime < 0);
    CHECK_FALSE(parseBranding("[]").ok());
}
