#include "support/fake_youtube.hpp"

#include <chrono>
#include <thread>
#include <utility>

#include <stb/stb_image_write.h>

#include <cstdint>
#include <vector>

#include "core/json.hpp"
#include "support/files.hpp"
#include "net/crypto.hpp"

namespace playtube::test {

namespace {

std::string youtubeFixture(const char* name) { return readFixture(std::string("youtube/") + name); }

void writeToString(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

// A 64x36 JPEG: a diagonal gradient whose colours come from the picture's path (the same path, the same picture).
std::string pictureFor(const std::string& path) {
    std::uint32_t hash = 2166136261u;
    for (char c : path) {
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    }
    const int width = 64;
    const int height = 36;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 3));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto t = static_cast<std::uint32_t>((x + y) * 255 / (width + height));
            unsigned char* p = &pixels[static_cast<std::size_t>((y * width + x) * 3)];
            p[0] = static_cast<unsigned char>(((hash & 0xffu) * (255u - t) + 40u * t) / 255u);
            p[1] = static_cast<unsigned char>((((hash >> 8) & 0xffu) * (255u - t) + 30u * t) / 255u);
            p[2] = static_cast<unsigned char>((((hash >> 16) & 0xffu) * (255u - t) + 60u * t) / 255u);
        }
    }
    std::string out;
    stbi_write_jpg_to_func(writeToString, &out, width, height, 3, pixels.data(), 90);
    return out;
}

std::string mimeOf(int itag) {
    switch (itag) {
        case 140: return R"(audio/mp4; codecs=\"mp4a.40.2\")";
        case 18: return R"(video/mp4; codecs=\"avc1.42001E, mp4a.40.2\")";
        default: return R"(video/mp4; codecs=\"avc1.640028\")";
    }
}

int heightOf(int itag) {
    switch (itag) {
        case 137: return 1080;
        case 136: return 720;
        case 135: return 480;
        case 134: case 18: return 360;
        case 133: return 240;
        default: return 0;
    }
}

// The first or last continuation token written in a recorded reply.
std::string tokenIn(const std::string& text, bool last) {
    const std::string key = "\"token\":\"";
    const std::size_t at = last ? text.rfind(key) : text.find(key);
    if (at == std::string::npos) {
        return {};
    }
    const std::size_t start = at + key.size();
    return text.substr(start, text.find('"', start) - start);
}

// The recorded video page has no comment section: the one of another recorded page is added at its end.
std::string nextWithComments() {
    std::string page = youtubeFixture("next.json");
    const std::size_t end = page.rfind('}');
    return page.substr(0, end) + ",\"commentSection\":" + youtubeFixture("next_comments.json") + "}";
}

}  // namespace

void FakeYouTube::setChapters(std::vector<std::pair<int, std::string>> chapters) {
    std::lock_guard<std::mutex> lock(mutex_);
    chapters_ = std::move(chapters);
}

void FakeYouTube::hideLikeCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    likeCountHidden_ = true;
}

void FakeYouTube::publishRelease(std::string version, std::string package) {
    std::lock_guard<std::mutex> lock(mutex_);
    releaseVersion_ = std::move(version);
    releasePackage_ = std::move(package);
}

FakeYouTube::FakeYouTube() {
    auto& http = server_.http();
    http.Get("/votes", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.set_content(R"({"id":"x","likes":19500302,"dislikes":521783,"rating":4.9,"viewCount":1,"deleted":false})",
                        "application/json");
    });
    http.Get(R"(/api/stats/(playback|watchtime))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.status = 204;
    });
    http.Get("/api/branding", [this](const httplib::Request& req, httplib::Response& res) {  // DeArrow
        record(req);
        res.set_content(readFixture("dearrow/branding.json"), "application/json");
    });
    http.Get("/api/v1/getThumbnail", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.set_content(readFixture("dearrow/thumbnail.webp"), "image/webp");
    });
    http.Get("/logo.svg", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.set_content(readFixture("svg/logo.svg"), "image/svg+xml");
    });
    http.Get("/github/latest", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (releaseVersion_.empty()) {
            res.status = 404;
            return;
        }
        net::Sha256Stream digest;
        digest.update(releasePackage_.data(), releasePackage_.size());
        res.set_content(R"({"tag_name":")" + releaseVersion_ + R"(","assets":[{"name":"IV0001-PLTB.pkg","size":)" +
                            std::to_string(releasePackage_.size()) + R"(,"digest":"sha256:)" +
                            net::hexLower(digest.finish()) + R"(","browser_download_url":")" + server_.baseUrl() +
                            R"(/github/Playtube.pkg"}]})",
                        "application/json");
    });
    http.Get("/github/Playtube.pkg", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mutex_);
        res.set_content(releasePackage_, "application/octet-stream");
    });
    http.Get("/sw.js_data", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        std::lock_guard<std::mutex> lock(mutex_);
        ++visitorsGiven_;
        std::string data = youtubeFixture("sw_js_data.txt");
        const std::string first = visitorId(1);
        data.replace(data.find(first), first.size(), visitorId(visitorsGiven_));
        res.set_content(data, "application/json");
    });
    http.Get(R"(/img/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        std::string showcase;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            showcase = showcase_;
        }
        const std::string path = req.matches[1];
        std::string file;
        if (!showcase.empty() && path.find("..") == std::string::npos &&
            !(file = readFile(showcase + "/img/" + path.substr(0, path.find('?')))).empty()) {
            res.set_content(file, "image/jpeg");
            return;
        }
        res.set_content(pictureFor(path), "image/jpeg");
    });
    http.Post(R"(/youtubei/v1/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        if (failed(req.path, res)) {
            return;
        }
        const std::string endpoint = req.matches[1];
        const auto doc = core::json::Document::parse(req.body);
        const auto root = doc ? doc->root() : nullptr;
        const bool more = core::json::field(root, "continuation") != nullptr;
        const bool signedIn = !req.get_header_value("Authorization").empty();
        const std::string client = core::json::string(
            core::json::field(core::json::field(root, "context"), "client"), "clientName");
        const std::string browseId = core::json::string(root, "browseId");
        std::string reply;
        if (endpoint == "search" && core::json::string(root, "params") == "EgIQCQ==") {
            reply = youtubeFixture("search_shorts.json");  // the Shorts filter
        } else if (endpoint == "search") {
            reply = youtubeFixture(more ? "search_continuation.json" : "search.json");
        } else if (endpoint == "next" && client == "TVHTML5" && signedIn) {
            reply = youtubeFixture("tv_next.json");
            std::lock_guard<std::mutex> lock(mutex_);
            if (likeCountHidden_) {  // the key renamed: the reader finds no count
                const std::size_t at = reply.find("\"likeCountText\"");
                if (at != std::string::npos) {
                    reply.replace(at, 15, "\"likeCountNone\"");
                }
            }
        } else if (endpoint == "next" && more) {
            // comments: the first page, its next page, else a comment's replies
            const std::string token = core::json::string(root, "continuation");
            reply = youtubeFixture(token == tokenIn(youtubeFixture("next_comments.json"), false) ? "comments.json"
                                   : token == tokenIn(youtubeFixture("comments.json"), true)    ? "comments_more.json"
                                                                                             : "comments_replies.json");
        } else if (endpoint == "next") {
            reply = nextWithComments();
            std::lock_guard<std::mutex> lock(mutex_);
            if (!chapters_.empty()) {
                std::string list;
                for (const auto& [startMs, title] : chapters_) {
                    list += std::string(list.empty() ? "" : ",") + R"({"chapterRenderer":{"title":{"simpleText":")" +
                            title + R"("},"timeRangeStartMillis":)" + std::to_string(startMs) + "}}";
                }
                // first, so that it wins over the page's own (a reader takes the first of two equal keys)
                reply = R"({"playerOverlays":{"playerOverlayRenderer":{"decoratedPlayerBarRenderer":{)"
                        R"("decoratedPlayerBarRenderer":{"playerBar":{"multiMarkersPlayerBarRenderer":{"markersMap":)"
                        R"([{"key":"DESCRIPTION_CHAPTERS","value":{"chapters":[)" +
                        list + "]}}]}}}}}}," + reply.substr(reply.find('{') + 1);
            }
        } else if (endpoint == "live_chat/get_live_chat") {
            reply = youtubeFixture("live_chat.json");
        } else if (endpoint == "account/accounts_list") {
            reply = youtubeFixture("tv_accounts_list.json");
        } else if (endpoint == "browse/edit_playlist" && signedIn) {
            reply = R"({"status":"STATUS_SUCCEEDED"})";
        } else if ((endpoint.rfind("like/", 0) == 0 || endpoint.rfind("subscription/", 0) == 0) && signedIn) {
            reply = "{}";
        } else if (endpoint == "playlist/get_add_to_playlist" && signedIn) {
            reply = youtubeFixture("tv_add_to_playlist.json");
        } else if (endpoint == "browse" && core::json::string(root, "params") == "EgZzaG9ydHPyBgUKA5oBAA==") {
            reply = youtubeFixture("channel_shorts.json");  // a channel's Shorts tab
        } else if (endpoint == "browse" && core::json::string(root, "params") == "EglwbGF5bGlzdHPyBgQKAkIA") {
            reply = youtubeFixture("channel_playlists.json");
        } else if (endpoint == "browse" && client == "TVHTML5" && browseId.rfind("VL", 0) == 0 && browseId != "VLWL") {
            reply = youtubeFixture("tv_playlist.json");  // any playlist, signed in or not
        } else if (endpoint == "browse" && client == "TVHTML5" && more && !signedIn) {
            reply = youtubeFixture("tv_playlist.json");  // a public playlist's next page
        } else if (endpoint == "browse" && signedIn && !showcaseFeed(browseId).empty()) {
            reply = showcaseFeed(browseId);
        } else if (endpoint == "browse" && signedIn) {
            reply = youtubeFixture(browseId == "FEsubscriptions"          ? "tv_subscriptions.json"
                                   : browseId == "FEhistory"               ? "tv_history.json"
                                   : browseId == "VLWL"                    ? "tv_watch_later.json"
                                   : browseId == "FEplaylist_aggregation" ? "tv_playlists.json"
                                                                           : "tv_recommended.json");
        } else if (endpoint == "browse") {
            reply = youtubeFixture("channel.json");
        } else if (endpoint == "player" && client == "TVHTML5" && signedIn &&
                   core::json::integer(core::json::object(core::json::object(root, "playbackContext"),
                                                          "contentPlaybackContext"),
                                       "signatureTimestamp", 0) < 10000000) {
            // as YouTube (seen 2026-11-20): without the TV player's own 8-digit signature timestamp, refused
            reply = R"({"playabilityStatus":{"status":"UNPLAYABLE","reason":"La page doit \u00eatre actualis\u00e9e."}})";
        } else if (endpoint == "player" && client == "TVHTML5" && signedIn) {
            // the account's reporting addresses (the streams come from the other clients)
            const std::string id = core::json::string(root, "videoId");
            const std::string query = "?ns=yt&el=leanback&docid=" + id + "&len=212";
            reply = R"({"playabilityStatus":{"status":"OK"},"playbackTracking":{"videostatsPlaybackUrl":{"baseUrl":")" +
                    baseUrl() + "/api/stats/playback" + query + R"("},"videostatsWatchtimeUrl":{"baseUrl":")" +
                    baseUrl() + "/api/stats/watchtime" + query + R"("}}})";
        } else if (endpoint == "player") {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto found = players_.find(client);
            reply = found != players_.end() ? found->second : youtubeFixture("player_refused.json");
            if (expiredVisitors_.count(req.get_header_value("X-Goog-Visitor-Id")) != 0) {
                if (client == "ANDROID_VR") {
                    reply = youtubeFixture("player_refused.json");
                } else if (client == "VISIONOS") {
                    for (std::size_t at = 0; (at = reply.find("/media/", at)) != std::string::npos;) {
                        reply.replace(at, 7, "/stale/");  // files that answer 403
                    }
                }
            }
        } else {
            res.status = 404;
            return;
        }
        res.set_content(servedHere(reply), "application/json");
    });
    http.Get("/complete/search", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.set_content(youtubeFixture("suggestions.json"), "text/javascript");
    });
    http.Get("/api/skipSegments", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        if (req.get_param_value("videoID") == "dQw4w9WgXcQ") {
            res.set_content(youtubeFixture("sponsorblock.json"), "application/json");
        } else {
            res.status = 404;
            res.set_content("Not Found", "text/plain");
        }
    });
    http.Get(R"(/api/v1/videos/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        if (failed(req.path, res)) {
            return;
        }
        res.set_content(youtubeFixture("invidious_video.json"), "application/json");
    });
    http.Get(R"(/stale/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.status = 403;
    });
    http.Get(R"(/media/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        std::lock_guard<std::mutex> lock(mutex_);
        const int itag = std::stoi(req.matches[1]);
        const std::string range = req.get_header_value("Range");
        const bool deep = !range.empty() && range.rfind("bytes=0-", 0) != 0;
        if (refused_.count(itag) != 0 || (deep && refusedDeep_.count(itag) != 0)) {
            res.status = 403;
            return;
        }
        // as long as its clen; ranges answered
        res.set_content(media_.empty() ? std::string(200000, 'x') : media_, "video/mp4");
    });
    // the test clip as an HLS playlist (tests/fixtures/media/hls): a live that plays
    http.Get(R"(/hls/([A-Za-z0-9]+\.(m3u8|ts)))", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        const bool playlist = req.matches[2] == "m3u8";
        res.set_content(readFixture("media/hls/" + std::string(req.matches[1])),
                        playlist ? "application/vnd.apple.mpegurl" : "video/mp2t");
    });
    http.Post("/o/oauth2/device/code", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        res.set_content(youtubeFixture("oauth_device_code.json"), "application/json");
    });
    http.Post("/o/oauth2/token", [this](const httplib::Request& req, httplib::Response& res) {
        record(req);
        const auto doc = core::json::Document::parse(req.body);
        const auto root = doc ? doc->root() : nullptr;
        int delay = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            delay = tokenDelayMs_;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        std::lock_guard<std::mutex> lock(mutex_);
        if (core::json::string(root, "grant_type") == "refresh_token" && !refreshError_.empty()) {
            res.status = 400;
            res.set_content(R"({"error":")" + std::exchange(refreshError_, {}) + "\"}", "application/json");
        } else if (core::json::string(root, "grant_type") == "refresh_token") {
            if (revoked_.count(core::json::string(root, "refresh_token")) != 0) {
                res.status = 400;
                res.set_content(R"({"error":"invalid_grant"})", "application/json");
                return;
            }
            res.set_content(R"({"access_token":"fresh-access-)" + std::to_string(++refreshed_) +
                                R"(","expires_in":3599,"token_type":"Bearer"})",
                            "application/json");
        } else if (pendingPolls_ > 0) {
            --pendingPolls_;
            res.status = 428;
            res.set_content(youtubeFixture("oauth_pending.json"), "application/json");
        } else {
            res.set_content(youtubeFixture("oauth_token.json"), "application/json");
        }
    });
    http.Post("/revoke", [this](const httplib::Request& req, httplib::Response&) { record(req); });
    server_.start();
}

void FakeYouTube::setPendingPolls(int count) {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingPolls_ = count;
}

void FakeYouTube::failNextRefresh(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    refreshError_ = error;
}

void FakeYouTube::setTokenDelay(int milliseconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    tokenDelayMs_ = milliseconds;
}

void FakeYouTube::revokeRefreshToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    revoked_.insert(token);
}

std::string FakeYouTube::servedHere(std::string reply) const {
    for (const std::string host : {"https://i.ytimg.com/", "https://i9.ytimg.com/", "https://yt3.ggpht.com/",
                                   "https://yt3.googleusercontent.com/"}) {
        const std::string local = baseUrl() + "/img/";
        for (std::size_t at = reply.find(host); at != std::string::npos; at = reply.find(host, at + local.size())) {
            reply.replace(at, host.size(), local);
        }
    }
    for (std::size_t at = reply.find("\"//yt3.ggpht.com/"); at != std::string::npos;
         at = reply.find("\"//yt3.ggpht.com/", at + 1)) {
        reply.replace(at + 1, 16, baseUrl() + "/img/");
    }
    return reply;
}

void FakeYouTube::setShowcase(std::string dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    showcase_ = std::move(dir);
}

std::string FakeYouTube::showcaseFeed(const std::string& browseId) const {
    std::string dir;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dir = showcase_;
    }
    const char* name = browseId == "FEwhat_to_watch" ? "recommended.json"
                       : browseId == "FEsubscriptions" ? "subscriptions.json"
                                                        : nullptr;
    return dir.empty() || name == nullptr ? std::string() : readFile(dir + "/" + name);
}

void FakeYouTube::setPlayer(const std::string& clientName, std::string reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_[clientName] = std::move(reply);
}

std::string FakeYouTube::playerReply(std::initializer_list<int> itags) const {
    std::size_t length = 200000;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        length = media_.empty() ? length : media_.size();
    }
    const std::string clen = "?clen=" + std::to_string(length);
    std::string formats;
    for (int itag : itags) {
        formats += std::string(formats.empty() ? "" : ",") + R"({"itag":)" + std::to_string(itag) + R"(,"url":")" +
                   baseUrl() + "/media/" + std::to_string(itag) + clen + R"(","mimeType":")" + mimeOf(itag) +
                   "\"" +
                   (heightOf(itag) > 0 ? R"(,"height":)" + std::to_string(heightOf(itag)) : std::string()) + "}";
    }
    return R"({"playabilityStatus":{"status":"OK"},"videoDetails":{"title":"Demo","lengthSeconds":"213"},)"
           R"("streamingData":{"adaptiveFormats":[)" + formats + "]}}";
}

void FakeYouTube::servePlayableMedia() {
    std::lock_guard<std::mutex> lock(mutex_);
    media_ = readFixture("media/clip.mp4");
}

void FakeYouTube::refuseDeepRanges(int itag) {
    std::lock_guard<std::mutex> lock(mutex_);
    refusedDeep_.insert(itag);
}

std::string FakeYouTube::visitorId(int n) {
    return n <= 1 ? "CgtkZW1vVmlzaXRvcg%3D%3D" : "CgtkZW1vVmlzaXRvcg" + std::to_string(n) + "%3D%3D";
}

void FakeYouTube::expireVisitor() {
    std::lock_guard<std::mutex> lock(mutex_);
    expiredVisitors_.insert(visitorId(visitorsGiven_));
}

void FakeYouTube::refuseMedia(int itag) {
    std::lock_guard<std::mutex> lock(mutex_);
    refused_.insert(itag);
}

void FakeYouTube::failNext(const std::string& path, int status, int times) {
    std::lock_guard<std::mutex> lock(mutex_);
    failures_[path] = {status, times};
}

std::vector<FakeYouTube::Request> FakeYouTube::requests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return requests_;
}

std::vector<FakeYouTube::Request> FakeYouTube::requestsTo(const std::string& path) const {
    std::vector<Request> out;
    for (const Request& r : requests()) {
        if (r.path == path) {
            out.push_back(r);
        }
    }
    return out;
}

void FakeYouTube::record(const httplib::Request& req) {
    Request r;
    r.method = req.method;
    r.path = req.path;
    r.target = req.target;
    r.clientHeader = req.get_header_value("X-YouTube-Client-Name");
    r.userAgent = req.get_header_value("User-Agent");
    r.visitorId = req.get_header_value("X-Goog-Visitor-Id");
    r.authorization = req.get_header_value("Authorization");
    r.range = req.get_header_value("Range");
    r.body = req.body;
    if (const auto doc = core::json::Document::parse(req.body)) {
        r.clientName = core::json::string(
            core::json::field(core::json::field(doc->root(), "context"), "client"), "clientName");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    requests_.push_back(std::move(r));
}

bool FakeYouTube::failed(const std::string& path, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = failures_.find(path);
    if (found == failures_.end()) {
        return false;
    }
    res.status = found->second.first;
    if (--found->second.second <= 0) {
        failures_.erase(found);
    }
    return true;
}

}  // namespace playtube::test
