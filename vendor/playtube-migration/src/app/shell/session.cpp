#include "app/shell/session.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

#include "app/shell/chrome.hpp"
#include "core/file_io.hpp"
#include "core/json.hpp"
#include "core/log.hpp"
#include "core/version.hpp"
#include "platform/platform.hpp"
#include "ui/application.hpp"

namespace playtube::app {

namespace {

constexpr std::size_t kMaxImageBytes = 8u * 1024u * 1024u;

}  // namespace

YoutubeLocale youtubeLocale(const std::string& uiLanguage) {
    static const std::pair<const char*, YoutubeLocale> table[] = {
        {"fr", {"fr", "FR"}}, {"de", {"de", "DE"}}, {"es", {"es", "ES"}}, {"pt", {"pt", "BR"}},
        {"cs", {"cs", "CZ"}}, {"tr", {"tr", "TR"}}, {"ru", {"ru", "RU"}}, {"uk", {"uk", "UA"}},
        {"vi", {"vi", "VN"}}, {"ja", {"ja", "JP"}}, {"ko", {"ko", "KR"}}, {"zh-Hans", {"zh-CN", "SG"}},
        {"zh-Hant", {"zh-TW", "TW"}},
    };
    for (const auto& entry : table) {
        if (uiLanguage == entry.first) {
            return entry.second;
        }
    }
    // a system tag: "fr-FR", "fr_CA", "zh-TW"
    std::string tag = uiLanguage;
    std::replace(tag.begin(), tag.end(), '_', '-');
    const std::size_t dash = tag.find('-');
    const std::string language = tag.substr(0, dash);
    const std::string region = dash == std::string::npos ? std::string() : tag.substr(dash + 1);
    if (language == "zh") {
        if (region == "TW" || region == "HK" || region == "MO") {
            return {"zh-TW", region};
        }
        return {"zh-CN", region.empty() ? "SG" : region};
    }
    for (const auto& entry : table) {
        if (language == entry.first) {
            return {entry.second.hl, region.size() == 2 ? region : entry.second.gl};
        }
    }
    return {"en", region.size() == 2 ? region : "US"};
}

AppSession::AppSession(net::HttpConfig http, std::string settingsPath, std::string accountPath)
    : caFile_(http.caFile),
      http_(std::move(http)),
      settingsPath_(std::move(settingsPath)),
      account_(std::move(accountPath)) {
    if (!settingsPath_.empty()) {
        settings_ = loadSettings(settingsPath_);
    }
    account_.load();
    languageAtStart = settings_.language;
}

void AppSession::startYoutube(const std::string& uiLanguage, youtube::Endpoints endpoints) {
    const YoutubeLocale locale = youtubeLocale(uiLanguage);
    youtube_ = std::make_unique<youtube::Client>(http_, endpoints, locale.hl, locale.gl);
    auth_ = std::make_unique<youtube::Auth>(http_, std::move(endpoints));
}

void AppSession::importPrevious(const std::string& oldConfigPath) {
    if (account_.lookedForOld()) {
        return;
    }
    account_.importFromOld(oldConfigPath);
    std::string text;
    if (core::readSmallFile(oldConfigPath, 1024 * 1024, text) != core::ReadStatus::Ok) {
        return;
    }
    const auto doc = core::json::Document::parse(text);
    if (!doc) {
        return;
    }
    const auto terms = core::json::strings(core::json::object(doc->root(), "setting"), "yt_search_history");
    Settings imported = settings_;
    for (const std::string& term : terms) {  // after the searches made here, in their order
        if (imported.recentSearches.size() >= 8) {
            break;
        }
        if (std::find(imported.recentSearches.begin(), imported.recentSearches.end(), term) ==
            imported.recentSearches.end()) {
            imported.recentSearches.push_back(term);
        }
    }
    if (imported.recentSearches != settings_.recentSearches) {
        settings_.recentSearches = std::move(imported.recentSearches);
        saveSettings();
    }
}

void AppSession::completeAccount() {
    if (!account_.signedIn() || !account_.name().empty()) {
        return;
    }
    submit([this] {
        const auto name =
            signedIn<std::string>([this](const std::string& token) { return youtube_->accountName(token); });
        if (name.ok()) {
            account_.setName(name.value());
        }
    });
}

std::int64_t AppSession::now() const { return static_cast<std::int64_t>(std::time(nullptr)); }

int AppSession::utcOffsetMinutes(std::int64_t utc) const {
    return fixedClock ? fixedUtcOffset : platform::utcOffsetMinutes(utc);
}

bool AppSession::saveSettings() { return !settingsPath_.empty() && app::saveSettings(settingsPath_, settings_); }

void AppSession::submit(std::function<void()> job) {
    ui::Application* app = ui::Application::current();
    if (synchronous || app == nullptr) {
        job();
    } else {
        app->pool().submit(std::move(job));
    }
}

core::Result<std::string> AppSession::fetchImage(const std::string& url, const core::CancelToken& token) const {
    if (url.rfind("https://", 0) != 0) {
        return core::makeError(core::ErrorKind::BadResponse, "pictures are fetched over https only");
    }
    net::HttpRequest request;
    request.url = url;
    request.maxBodyBytes = kMaxImageBytes;
    auto response = http_.send(request, {}, token);
    if (!response.ok()) {
        return response.error();
    }
    if (response.value().status != 200) {
        return core::makeError(core::ErrorKind::BadResponse, "HTTP " + std::to_string(response.value().status));
    }
    return std::move(response.value().body);
}

AppSession::~AppSession() { stopTransfers(); }

core::WorkerPool& AppSession::transfers() {
    if (!transfers_) {
        transfers_ = std::make_unique<core::WorkerPool>(1);
    }
    return *transfers_;
}

void AppSession::stopTransfers() {
    if (!transfers_) {
        return;
    }
    if (!transfers_->finish(std::chrono::seconds(3))) {
        // a transfer that does not hear its cancel: left running rather than waited for (the app is closing)
        PF_LOG_WARN("exit: a transfer still running after 3 s, left behind");
        static_cast<void>(transfers_.release());
        return;
    }
    transfers_.reset();
}

void AppSession::branding(const std::string& videoId, std::function<void(const Branding&)> ready) {
    if (!settings_.dearrow || videoId.empty() || !youtube_) {
        return;
    }
    const auto known = brandings_.find(videoId);
    if (known != brandings_.end()) {
        ready(known->second);
        return;
    }
    auto& waiting = brandingWaiting_[videoId];
    waiting.push_back(std::move(ready));
    if (waiting.size() > 1) {
        return;  // already asked
    }
    const youtube::Client* client = youtube_.get();
    const std::string id = videoId;
    const auto job = [client, id] {
        Branding out;
        auto answer = client->branding(id);
        if (answer.ok()) {
            out.title = answer.value().title;
            if (answer.value().thumbnailTime >= 0) {
                out.thumbnail = client->brandingThumbnail(id, answer.value().thumbnailTime);
            }
        } else {
            PF_LOG_DEBUG("dearrow: %s: %s", id.c_str(), answer.error().message.c_str());
        }
        return out;
    };
    const auto apply = [this, id](Branding found) {
        if (brandings_.size() >= kMaxBrandings) {
            brandings_.clear();
        }
        brandings_[id] = found;
        auto callbacks = std::move(brandingWaiting_[id]);
        brandingWaiting_.erase(id);
        for (const auto& callback : callbacks) {
            callback(found);
        }
    };
    ui::Application* app = ui::Application::current();
    if (synchronous || app == nullptr) {
        apply(job());
        return;
    }
    core::runAsync<Branding>(app->pool(), app->mainQueue(), job, apply, core::CancelToken{});
}

void AppSession::checkForUpdate(std::function<void()> done) {
    if (updateChecked || updateUrl.empty()) {
        if (done) {
            done();
        }
        return;
    }
    updateChecked = true;
    const net::HttpClient* http = &http_;
    const std::string url = updateUrl;
    const auto apply = [this, done](core::Result<Release> release) {
        if (release.ok() && newerVersion(release.value().version, core::version())) {
            PF_LOG_INFO("update: %s is available", release.value().version.c_str());
            update = release.value();
        } else if (!release.ok()) {
            PF_LOG_INFO("update: not checked: %s", release.error().message.c_str());
            updateChecked = false;  // asked again from the settings
        }
        if (ui::Application* current = ui::Application::current()) {
            current->requestRedraw();  // the settings tab's dot
        }
        if (done) {
            done();
        }
    };
    ui::Application* app = ui::Application::current();
    if (synchronous || app == nullptr) {
        apply(fetchLatestRelease(*http, url));
        return;
    }
    core::runAsync<core::Result<Release>>(app->pool(), app->mainQueue(),
                                          [http, url] { return fetchLatestRelease(*http, url); }, apply,
                                          core::CancelToken{});
}

void AppSession::applyLogo() {
    ++logoGeneration_;
    if (settings_.logo != "youtube" || youtubeLogoUrl.empty()) {
        setLogoPicture(nullptr);
        return;
    }
    const unsigned generation = logoGeneration_;
    const net::HttpClient* http = &http_;
    const std::string url = youtubeLogoUrl;
    const int height = static_cast<int>(ui::theme().fontLogo);  // read here: the theme belongs to the main thread
    const auto job = [http, url, height]() -> std::shared_ptr<const ui::SvgPicture> {
        if (url.rfind("https://", 0) != 0) {
            return nullptr;
        }
        net::HttpRequest request;
        request.url = url;
        request.maxBodyBytes = 64 * 1024;
        const auto response = http->send(request);
        if (!response.ok() || response.value().status != 200) {
            PF_LOG_INFO("logo: not fetched: %s",
                        response.ok() ? ("HTTP " + std::to_string(response.value().status)).c_str()
                                      : response.error().message.c_str());
            return nullptr;
        }
        auto picture = ui::rasterizeSvg(response.value().body, height);
        if (!picture) {
            PF_LOG_INFO("logo: not drawn (its file changed?)");
            return nullptr;
        }
        return std::make_shared<const ui::SvgPicture>(std::move(*picture));
    };
    const auto apply = [this, generation](std::shared_ptr<const ui::SvgPicture> picture) {
        if (generation == logoGeneration_ && settings_.logo == "youtube") {
            setLogoPicture(std::move(picture));
            if (ui::Application* app = ui::Application::current()) {
                app->requestRedraw();
            }
        }
    };
    ui::Application* app = ui::Application::current();
    if (synchronous || app == nullptr) {
        apply(job());
        return;
    }
    core::runAsync<std::shared_ptr<const ui::SvgPicture>>(app->pool(), app->mainQueue(), job, apply,
                                                          core::CancelToken{});
}

}  // namespace playtube::app
