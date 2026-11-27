#include "youtube/playback.hpp"

#include <cstdio>

namespace playtube::youtube {

std::optional<std::size_t> segmentToSkip(double position, const std::vector<Segment>& segments,
                                         const std::vector<bool>& skipped) {
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const Segment& segment = segments[i];
        if (segment.end - segment.start < 1 || (i < skipped.size() && skipped[i])) {
            continue;
        }
        if (position >= segment.start && position < segment.end - 0.5) {
            return i;
        }
    }
    return std::nullopt;
}

const std::vector<double>& playbackSpeeds() {
    static const std::vector<double> speeds{0.5, 0.75, 1, 1.25, 1.5, 2};
    return speeds;
}

std::string captionUrl(const CaptionTrack& track) {
    std::string url = track.url;
    const std::size_t at = url.find("&fmt=");
    if (at != std::string::npos) {
        const std::size_t end = url.find('&', at + 1);
        url.erase(at, end == std::string::npos ? std::string::npos : end - at);
    }
    return url + "&fmt=vtt";
}

namespace {

std::string seconds(double value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.3f", value < 0 ? 0.0 : value);
    return text;
}

// The base address with the reports' common fields: version 2 and the playback's nonce
std::string withNonce(const std::string& base, const std::string& nonce) {
    return base + (base.find('?') == std::string::npos ? "?" : "&") + "ver=2&cpn=" + nonce;
}

}  // namespace

std::string playbackNonce(const unsigned char (&random)[16]) {
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string nonce;
    for (unsigned char byte : random) {
        nonce += kAlphabet[byte & 63];
    }
    return nonce;
}

std::string playbackReportUrl(const std::string& base, const std::string& nonce, double position) {
    return withNonce(base, nonce) + "&cmt=" + seconds(position);
}

std::string watchtimeReportUrl(const std::string& base, const std::string& nonce, double from, double to,
                               bool paused) {
    return withNonce(base, nonce) + "&cmt=" + seconds(to) + "&st=" + seconds(from) + "&et=" + seconds(to) +
           "&state=" + (paused ? "paused" : "playing");
}

}  // namespace playtube::youtube
