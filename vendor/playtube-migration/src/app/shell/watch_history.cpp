#include "app/shell/watch_history.hpp"

#include <deque>
#include <mutex>
#include <random>
#include <utility>

#include "app/shell/session.hpp"
#include "core/log.hpp"
#include "youtube/playback.hpp"

namespace playtube::app {

std::vector<WatchReport> WatchSegments::update(double position, bool paused) {
    std::vector<WatchReport> out;
    if (!started_) {
        started_ = true;
        paused_ = paused;
        from_ = last_ = position;
        out.push_back(WatchReport{true, position, position, paused});
        return out;
    }
    if (live_) {
        return out;
    }
    if (position < last_ - 1 || position > last_ + kJump) {  // a seek: the part watched before it
        if (auto before = part(paused_)) {
            out.push_back(*before);
        }
        from_ = last_ = position;
        paused_ = paused;
        return out;
    }
    last_ = position;
    if (paused != paused_) {
        paused_ = paused;
        if (paused) {
            if (auto before = part(true)) {
                out.push_back(*before);
            }
            return out;
        }
    }
    if (last_ - from_ >= kPart) {
        if (auto watched = part(false)) {
            out.push_back(*watched);
        }
    }
    return out;
}

std::optional<WatchReport> WatchSegments::flush() {
    if (!started_ || live_) {
        return std::nullopt;
    }
    return part(paused_);
}

std::optional<WatchReport> WatchSegments::part(bool paused) {
    if (last_ - from_ < 0.5) {
        return std::nullopt;
    }
    WatchReport report{false, from_, last_, paused};
    from_ = last_;
    return report;
}

struct WatchHistory::Shared {
    std::mutex mutex;
    std::deque<WatchReport> pending;  // in order; one job sends them, one at a time (the start first)
    bool sending = false;             // that job is running
    // the sending job's own (one at a time)
    std::string nonce;
    std::optional<youtube::PlaybackTracking> tracking;
    bool failed = false;
};

WatchHistory::WatchHistory(AppSession& session, std::string videoId)
    : session_(session), videoId_(std::move(videoId)), shared_(std::make_shared<Shared>()) {
    std::random_device device;
    unsigned char random[16];
    for (unsigned char& byte : random) {
        byte = static_cast<unsigned char>(device() & 0xFF);
    }
    shared_->nonce = youtube::playbackNonce(random);
}

void WatchHistory::update(double position, bool paused) {
    for (const WatchReport& report : segments_.update(position, paused)) {
        send(report);
    }
}

void WatchHistory::flush() {
    if (const auto report = segments_.flush()) {
        send(*report);
    }
}

namespace {

// One report: the addresses asked first when they are not known yet. False when it failed.
bool sendOne(AppSession& session, const std::string& id, const std::string& nonce,
             std::optional<youtube::PlaybackTracking>& tracking, const WatchReport& report) {
    if (!tracking) {
        auto where = session.signedIn<youtube::PlaybackTracking>(
            [&](const std::string& token) { return session.youtube().playbackTracking(id, token, nonce); });
        if (!where.ok()) {
            PF_LOG_WARN("watch history: %s: %s", id.c_str(), where.error().message.c_str());
            return false;
        }
        tracking = std::move(where.value());
    }
    std::string url;
    if (report.start) {
        url = youtube::playbackReportUrl(tracking->playbackUrl, nonce, report.to);
    } else if (!tracking->watchtimeUrl.empty()) {
        url = youtube::watchtimeReportUrl(tracking->watchtimeUrl, nonce, report.from, report.to, report.paused);
    }
    if (url.empty()) {
        return true;
    }
    auto sent = session.signedIn<core::Unit>(
        [&](const std::string& token) { return session.youtube().report(url, token); });
    if (!sent.ok()) {
        PF_LOG_WARN("watch history: %s: %s", id.c_str(), sent.error().message.c_str());
        return false;
    }
    if (report.start) {
        PF_LOG_INFO("watch history: %s reported", id.c_str());
    }
    return true;
}

}  // namespace

// Queued; a single job of the session's pool sends a video's reports (a slow answer never holds more than one of
// its threads).
void WatchHistory::send(const WatchReport& report) {
    if (!report.start && !kReportParts) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(shared_->mutex);
        if (shared_->failed) {
            return;
        }
        shared_->pending.push_back(report);
        if (shared_->sending) {
            return;
        }
        shared_->sending = true;
    }
    AppSession* session = &session_;
    std::shared_ptr<Shared> shared = shared_;
    const std::string id = videoId_;
    session_.submit([session, shared, id] {
        for (;;) {
            WatchReport next;
            {
                std::lock_guard<std::mutex> lock(shared->mutex);
                if (shared->pending.empty() || shared->failed) {
                    shared->pending.clear();
                    shared->sending = false;
                    return;
                }
                next = shared->pending.front();
                shared->pending.pop_front();
            }
            if (!sendOne(*session, id, shared->nonce, shared->tracking, next)) {
                std::lock_guard<std::mutex> lock(shared->mutex);
                shared->failed = true;
            }
        }
    });
}

}  // namespace playtube::app
