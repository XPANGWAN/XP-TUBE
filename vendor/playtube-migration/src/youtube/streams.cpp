#include "youtube/streams.hpp"

#include <algorithm>

namespace playtube::youtube {

namespace {

bool starts(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

bool h264(const Format& f) { return starts(f.mimeType, "video/mp4") && f.mimeType.find("avc1") != std::string::npos; }
bool muxed(const Format& f) { return h264(f) && f.mimeType.find("mp4a") != std::string::npos; }

// 30 fps H.264, or its 60 fps (298, 299): a video may offer 60 fps only, or 30 fps only in lower sizes
bool separateVideo(const Format& f, bool sixty) {
    switch (f.itag) {
        case 137: case 136: case 135: case 134: case 133: case 160: return !sixty && h264(f) && f.height > 0;
        case 299: case 298: return sixty && h264(f) && f.height > 0;
        default: return false;
    }
}

// stereo AAC (itag 140), not its volume-levelled copy; one per language when YouTube offers dubbed tracks
bool stereoAac(const Format& f) { return f.itag == 140 && !f.drc && starts(f.mimeType, "audio/mp4"); }

// the highest at most `maxHeight`, else the smallest
const Format* bestVideo(const PlayerReply& reply, int maxHeight, bool sixty) {
    const Format* video = nullptr;
    for (const Format& f : reply.formats) {
        if (!separateVideo(f, sixty)) {
            continue;
        }
        const bool fits = f.height <= maxHeight;
        const bool chosenFits = video != nullptr && video->height <= maxHeight;
        if (video == nullptr || (fits && (!chosenFits || f.height > video->height)) ||
            (!fits && !chosenFits && f.height < video->height)) {
            video = &f;
        }
    }
    return video;
}

Streams base(const PlayerReply& reply) {
    Streams streams;
    streams.live = reply.live;
    streams.title = reply.title;
    streams.channelId = reply.channelId;
    streams.captions = reply.captions;
    streams.storyboardSpec = reply.storyboardSpec;
    streams.durationSeconds = reply.durationSeconds;
    return streams;
}

}  // namespace

std::optional<Streams> chooseStreams(const PlayerReply& reply, int maxHeight) {
    if (!reply.playable) {
        return std::nullopt;
    }
    if (reply.live) {
        if (reply.hls.empty()) {
            return std::nullopt;
        }
        Streams streams = base(reply);
        streams.hls = reply.hls;
        return streams;
    }
    // the higher resolution within the quality; at the same height, 30 fps (lighter to decode)
    const Format* thirty = bestVideo(reply, maxHeight, false);
    const Format* sixty = bestVideo(reply, maxHeight, true);
    const Format* video = thirty;
    const bool sixtyFits = sixty != nullptr && sixty->height <= maxHeight;
    if (sixty != nullptr &&
        (thirty == nullptr || (sixtyFits && (thirty->height > maxHeight || sixty->height > thirty->height)))) {
        video = sixty;
    }
    const Format* audio = nullptr;
    std::vector<AudioTrack> tracks;
    for (const Format& f : reply.formats) {
        if (!stereoAac(f)) {
            continue;
        }
        if (audio == nullptr || (!audio->defaultAudio && f.defaultAudio)) {
            audio = &f;
        }
        if (!f.audioTrackId.empty()) {
            tracks.push_back({f.audioTrackId, f.audioTrackName, f.url, f.defaultAudio});
        }
    }
    if (video != nullptr && audio != nullptr) {
        Streams streams = base(reply);
        streams.video = video->url;
        streams.audio = audio->url;
        streams.height = video->height;
        if (tracks.size() > 1) {
            std::stable_sort(tracks.begin(), tracks.end(), [](const AudioTrack& a, const AudioTrack& b) {
                return a.original != b.original ? a.original : a.name < b.name;
            });
            streams.audioTracks = std::move(tracks);
            streams.audioTrack = audio->audioTrackId;
        }
        return streams;
    }
    if (auto single = muxedStreams(reply)) {
        return single;
    }
    if (!reply.hls.empty()) {
        Streams streams = base(reply);
        streams.hls = reply.hls;
        return streams;
    }
    return std::nullopt;
}

std::optional<Streams> muxedStreams(const PlayerReply& reply) {
    if (!reply.playable) {
        return std::nullopt;
    }
    for (const Format& f : reply.formats) {
        if (muxed(f)) {
            Streams streams = base(reply);
            streams.video = f.url;
            streams.height = f.height;
            return streams;
        }
    }
    return std::nullopt;
}

namespace {

// the number after "expire=" (a query parameter) or "/expire/" (a path segment); 0 when absent or not a number
std::int64_t expiryOf(const std::string& url) {
    for (const char* key : {"expire=", "/expire/"}) {
        std::size_t at = 0;
        while ((at = url.find(key, at)) != std::string::npos) {
            const bool parameter = key[0] == 'e';
            if (parameter && at > 0 && url[at - 1] != '?' && url[at - 1] != '&') {
                at += 1;  // "...xexpire=": another parameter's name
                continue;
            }
            std::size_t i = at + std::char_traits<char>::length(key);
            std::int64_t value = 0;
            int digits = 0;
            while (i < url.size() && url[i] >= '0' && url[i] <= '9' && digits < 12) {
                value = value * 10 + (url[i] - '0');
                ++i;
                ++digits;
            }
            return digits > 0 ? value : 0;
        }
    }
    return 0;
}

}  // namespace

std::int64_t expiresAt(const Streams& streams) {
    std::int64_t earliest = 0;
    for (const std::string* url : {&streams.video, &streams.audio, &streams.hls}) {
        const std::int64_t expiry = expiryOf(*url);
        if (expiry > 0 && (earliest == 0 || expiry < earliest)) {
            earliest = expiry;
        }
    }
    return earliest;
}

}  // namespace playtube::youtube
