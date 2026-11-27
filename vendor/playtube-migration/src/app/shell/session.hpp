#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/progress.hpp"
#include "app/settings.hpp"
#include "app/update.hpp"
#include "core/result.hpp"
#include "core/task.hpp"
#include "net/http.hpp"
#include "youtube/account.hpp"
#include "youtube/auth.hpp"
#include "youtube/client.hpp"

namespace playtube::app {

// YouTube's `hl` (language) and `gl` (region) for a UI language tag.
struct YoutubeLocale {
    std::string hl;
    std::string gl;
};
YoutubeLocale youtubeLocale(const std::string& uiLanguage);

// What the screens share: the HTTP client, settings.json, YouTube and the account. Main thread, except
// fetchImage(), youtube(), auth() and signedIn() (any thread).
class AppSession {
public:
    // settings.json and account.json are read here (without a path: defaults, nothing written).
    AppSession(net::HttpConfig http, std::string settingsPath = {}, std::string accountPath = {});
    ~AppSession();
    AppSession(const AppSession&) = delete;
    AppSession& operator=(const AppSession&) = delete;

    const net::HttpClient& http() const { return http_; }
    const std::string& caFile() const { return caFile_; }  // the certificates trusted (also for the player)
    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    bool saveSettings();

    // Work for the worker pool that nothing waits for; inline in test mode.
    void submit(std::function<void()> job);

    // At the first start: the previous Playtube's sign-in and recent searches, from its config.json (only read),
    // once ever.
    void importPrevious(const std::string& oldConfigPath);

    // Signed in without the account's name (an imported sign-in): asks for it in the background.
    void completeAccount();

    // Once the UI language is known (YouTube answers in it).
    void startYoutube(const std::string& uiLanguage, youtube::Endpoints endpoints = {});
    const youtube::Client& youtube() const { return *youtube_; }
    const youtube::Auth& auth() const { return *auth_; }
    youtube::Account& account() { return account_; }
    std::int64_t now() const;  // seconds since 1970
    // The time shown (the player's "Se termine à", the header clock): now(), or fixedClock in tests so that
    // screenshots do not change with the time; the local time's distance to UTC the same way.
    std::int64_t clockNow() const { return fixedClock ? *fixedClock : now(); }
    int utcOffsetMinutes(std::int64_t utc) const;
    std::optional<std::int64_t> fixedClock;
    int fixedUtcOffset = 0;

    // `call(accessToken)` signed in, the token renewed when needed; ErrorKind::Unauthorized when signed out.
    template <typename T, typename F>
    core::Result<T> signedIn(F call, const core::CancelToken& cancel = {}) {
        return account_.withToken(*auth_, now(), call, cancel);
    }

    // A picture by its absolute URL (thumbnails, avatars).
    core::Result<std::string> fetchImage(const std::string& url, const core::CancelToken& token) const;

    std::string languageAtStart;  // settings().language when the app started (the UI's language until a restart)
    std::string noticesFile;      // THIRD_PARTY_LICENSES: copyright lines, credits, where the sources are
    std::string licensesDir;      // resources/licenses: the manifest and the licence texts

    bool synchronous = false;  // test mode: network jobs run inline (deterministic screenshots)

    // One thread of its own for the long transfers (the update package), so that they never hold the pool the
    // pages and pictures need. stopTransfers() (when the app closes) waits up to 3 s for its job to end.
    core::WorkerPool& transfers();
    void stopTransfers();

    // The videos the user queued (△ "Ajouter à la file" on a card): they play next, before "À suivre"; not kept
    // across a restart.
    std::vector<youtube::Video>& watchQueue() { return watchQueue_; }

    // The ids of the videos played since Playtube started (rememberPlayed): what plays next by itself is not one
    // of them. Not kept across a restart.
    std::vector<std::string>& played() { return played_; }

    // DeArrow (setting `dearrow`, off by default): `ready` gets the video's title and thumbnail address on the main
    // thread, at once when known, else once asked (a video DeArrow has nothing for gets empty ones). Nothing is
    // asked while the setting is off. The last kMaxBrandings are kept in memory.
    struct Branding {
        std::string title;
        std::string thumbnail;
    };
    static constexpr std::size_t kMaxBrandings = 2000;
    void branding(const std::string& videoId, std::function<void(const Branding&)> ready);

    // Where videos were left (the profile's progress.json; in memory until setProgressFile)
    ProgressStore& progress() { return progress_; }
    void setProgressFile(const std::string& path) { progress_ = ProgressStore(path); }

    // The header's logo of the settings: the YouTube logo fetched (YouTube's own file, never shipped with Playtube)
    // and drawn, or Playtube's; the text logo stays while it comes or when it cannot.
    std::string youtubeLogoUrl =
        "https://www.gstatic.com/youtube/img/branding/youtubelogo/svg/youtubelogo_dark.svg";
    void applyLogo();

    // Software update: where the latest release is asked for, and the newer one found (checked at start).
    std::string updateUrl = kLatestReleaseUrl;
    std::optional<Release> update;
    bool updateChecked = false;
    // Asks GitHub once (again after a failure); `done` on the main thread afterwards.
    void checkForUpdate(std::function<void()> done = {});

private:
    std::string caFile_;
    net::HttpClient http_;
    std::string settingsPath_;
    Settings settings_;
    youtube::Account account_;
    std::unique_ptr<youtube::Client> youtube_;
    std::unique_ptr<youtube::Auth> auth_;
    std::unique_ptr<core::WorkerPool> transfers_;
    ProgressStore progress_;
    unsigned logoGeneration_ = 0;
    std::vector<youtube::Video> watchQueue_;
    std::vector<std::string> played_;
    std::unordered_map<std::string, Branding> brandings_;
    std::unordered_map<std::string, std::vector<std::function<void(const Branding&)>>> brandingWaiting_;
};

}  // namespace playtube::app
