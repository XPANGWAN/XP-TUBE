#pragma once

#include <initializer_list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "support/fake_server.hpp"

namespace playtube::test {

// YouTube, SponsorBlock, suggestions and Invidious stand-ins over TLS (test CA), answering from the recorded
// fixtures. The player refuses every client unless given a reply for it.
class FakeYouTube {
public:
    struct Request {
        std::string method;
        std::string path;
        std::string target;  // the path with its query
        std::string clientName;  // context.client.clientName of an InnerTube body
        std::string clientHeader;  // X-YouTube-Client-Name
        std::string userAgent;
        std::string visitorId;
        std::string authorization;
        std::string range;
        std::string body;
    };

    FakeYouTube();
    std::string baseUrl() const { return server_.baseUrl(); }

    void setPlayer(const std::string& clientName, std::string reply);
    // A player reply offering these itags (137/136/135/134/133 H.264, 140 AAC, 18 muxed 360p), served by /media.
    std::string playerReply(std::initializer_list<int> itags) const;
    void refuseMedia(int itag);           // its file answers 403
    void servePlayableMedia();            // /media/* answers the test clip (a real video with sound)
    void refuseDeepRanges(int itag);      // its first bytes answer, a range further on answers 403
    // The visitor id given last goes stale, as YouTube's do after hours: with it, VISIONOS's files answer 403 and
    // ANDROID_VR is refused ("not a robot"). Each /sw.js_data gives a new id.
    void expireVisitor();
    // the next `times` requests to `path` answer `status`
    void failNext(const std::string& path, int status, int times = 1);
    void setPendingPolls(int count);                     // sign-in: polls answered "pending" before the tokens
    void revokeRefreshToken(const std::string& token);   // renewing with it is refused
    void failNextRefresh(const std::string& error);      // the next renewal answers 400 with this error
    void setTokenDelay(int milliseconds);                // the token endpoint answers this late
    // GitHub's latest release at /github/latest: `version`, its package (`package`, digest included) at
    // /github/Playtube.pkg; an empty version answers 404.
    void publishRelease(std::string version, std::string package);
    // The video page (WEB next) gets these chapters: (start in ms, title) pairs.
    void setChapters(std::vector<std::pair<int, std::string>> chapters);
    // The signed-in video state (TV next) without its like count, as YouTube sometimes answers.
    void hideLikeCount();
    // README screenshots: the signed-in home's feeds from `dir` (recommended.json, subscriptions.json: TV tiles)
    // and their pictures from dir/img/<path> (else the generated ones).
    void setShowcase(std::string dir);
    std::vector<Request> requests() const;
    std::vector<Request> requestsTo(const std::string& path) const;

private:
    void record(const httplib::Request& req);
    std::string servedHere(std::string reply) const;  // pictures of the reply fetched from this server
    bool failed(const std::string& path, httplib::Response& res);
    std::string showcaseFeed(const std::string& browseId) const;  // "" outside the showcase
    static std::string visitorId(int n);  // the id the n-th /sw.js_data gives (the fixture's first)

    FakeServer server_{TlsMode::CaSigned};
    mutable std::mutex mutex_;
    std::vector<Request> requests_;
    std::string showcase_;
    std::map<std::string, std::string> players_;
    std::set<int> refused_;
    std::set<int> refusedDeep_;
    int visitorsGiven_ = 0;
    std::set<std::string> expiredVisitors_;
    std::string media_;
    std::map<std::string, std::pair<int, int>> failures_;  // status, times left
    int pendingPolls_ = 0;
    int refreshed_ = 0;
    std::set<std::string> revoked_;
    std::string refreshError_;
    int tokenDelayMs_ = 0;
    std::vector<std::pair<int, std::string>> chapters_;
    bool likeCountHidden_ = false;
    std::string releaseVersion_;
    std::string releasePackage_;
};

}  // namespace playtube::test
