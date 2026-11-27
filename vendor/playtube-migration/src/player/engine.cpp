#include "player/engine.hpp"

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/stream_cb.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "core/log.hpp"
#include "player/repeats.hpp"

namespace playtube::player {

struct Engine::Impl {
    mpv_handle* mpv = nullptr;
    mpv_render_context* render = nullptr;
    std::function<void()> wake;
    std::vector<EngineEvent> pending;
    bool loaded = false;      // a file is loaded and no picture was shown yet
    bool shownFirst = false;  // FirstFrame sent for the current file
    std::string error;
    double renderMs = 0;
    std::vector<std::uint8_t> aligned;  // the picture drawn on 64-byte rows, when the frame's are not
    Repeats repeats;                    // mpv's messages already in the log

    ~Impl() {
        if (render != nullptr) {
            mpv_render_context_free(render);  // before the core goes away
        }
        if (mpv != nullptr) {
            mpv_terminate_destroy(mpv);
        }
    }

    double number(const char* name) const {
        double value = 0;
        return mpv_get_property(mpv, name, MPV_FORMAT_DOUBLE, &value) >= 0 ? value : 0;
    }
    std::int64_t integer(const char* name) const {
        std::int64_t value = 0;
        return mpv_get_property(mpv, name, MPV_FORMAT_INT64, &value) >= 0 ? value : 0;
    }
    bool flag(const char* name) const {
        int value = 0;
        return mpv_get_property(mpv, name, MPV_FORMAT_FLAG, &value) >= 0 && value != 0;
    }
    std::string text(const char* name) const {
        char* value = mpv_get_property_string(mpv, name);
        std::string out = value != nullptr ? value : "";
        mpv_free(value);
        return out;
    }
    void command(std::initializer_list<const char*> words) {
        std::vector<const char*> args(words);
        args.push_back(nullptr);
        const int rc = mpv_command_async(mpv, 0, args.data());
        if (rc < 0) {
            PF_LOG_WARN("mpv: %s failed: %s", args[0], mpv_error_string(rc));
        }
    }
};

namespace {

void setOption(mpv_handle* mpv, const char* name, const std::string& value) {
    const int rc = mpv_set_option_string(mpv, name, value.c_str());
    if (rc == MPV_ERROR_OPTION_NOT_FOUND && optionMayBeMissing(name)) {
        PF_LOG_DEBUG("mpv: option %s not in this build", name);
    } else if (rc < 0) {
        PF_LOG_WARN("mpv: option %s refused: %s", name, mpv_error_string(rc));
    }
}

}  // namespace

bool optionMayBeMissing(const char* name) { return std::strcmp(name, "ytdl") == 0; }

std::vector<std::pair<const char*, const char*>> hardeningOptions() {
    return {{"config", "no"},
            {"load-scripts", "no"},
            {"ytdl", "no"},
            {"input-default-bindings", "no"},
            {"embeddedfonts", "no"},
            {"access-references", "no"},
            {"load-unsafe-playlists", "no"}};
}

std::unique_ptr<Engine> Engine::create(const EngineOptions& options, std::function<void()> wake) {
    auto impl = std::make_unique<Impl>();
    impl->wake = std::move(wake);
    impl->mpv = mpv_create();
    if (impl->mpv == nullptr) {
        PF_LOG_ERROR("mpv: cannot create the player");
        return nullptr;
    }
    mpv_handle* mpv = impl->mpv;
    for (const auto& option : hardeningOptions()) {
        setOption(mpv, option.first, option.second);
    }
    setOption(mpv, "terminal", "no");
    setOption(mpv, "idle", "yes");
    setOption(mpv, "vo", "libmpv");
    if (options.audioOnly) {
        setOption(mpv, "vid", "no");
        setOption(mpv, "audio-display", "no");
    }
    if (options.loop) {
        // mpv starts the file again itself: a seek sent after the end would fail (the file is closed by then)
        setOption(mpv, "loop-file", "inf");
    }
    setOption(mpv, "hwdec", "no");
    setOption(mpv, "osd-level", "0");
    setOption(mpv, "cache", "yes");
    setOption(mpv, "demuxer-max-bytes", "64MiB");
    setOption(mpv, "demuxer-readahead-secs", "20");
    // the picture is converted at its own size: swscale's fast paths (SIMD) instead of its accurate C path,
    // which cost 110 ms per 1080p picture on the PS4 (3 ms with these)
    setOption(mpv, "sws-fast", "yes");
    setOption(mpv, "sws-scaler", "fast-bilinear");
    if (options.decoderThreads > 0) {
        setOption(mpv, "vd-lavc-threads", std::to_string(options.decoderThreads));
    }
    if (!options.audioOutput.empty()) {
        setOption(mpv, "ao", options.audioOutput);
    }
    if (!options.fontsDir.empty()) {
        setOption(mpv, "sub-fonts-dir", options.fontsDir);
        setOption(mpv, "sub-font", "Noto Sans");
        setOption(mpv, "sub-font-provider", "none");
    }
    if (!options.userAgent.empty()) {
        setOption(mpv, "user-agent", options.userAgent);
    }
    if (options.subtitleScale > 0 && options.subtitleScale != 1.0) {
        char scale[16];
        std::snprintf(scale, sizeof(scale), "%.2f", options.subtitleScale);
        setOption(mpv, "sub-scale", scale);
    }
    if (options.subtitleBackground) {
        // libass' opaque box (BorderStyle 3) in a translucent black, for text subtitles and the ASS ones alike
        // (mpv 0.36 has no sub-border-style)
        setOption(mpv, "sub-ass-force-style", "BorderStyle=3,BackColour=&H60000000,OutlineColour=&H60000000,Outline=6,Shadow=0");
        setOption(mpv, "sub-back-color", "0.0/0.0/0.0/0.62");
        setOption(mpv, "sub-border-color", "0.0/0.0/0.0/0.62");
    }
    if (options.reconnect) {
        setOption(mpv, "stream-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=5");
    }
    setOption(mpv, "tls-verify", options.verifyTls ? "yes" : "no");
    if (!options.caFile.empty()) {
        setOption(mpv, "tls-ca-file", options.caFile);
    }
    if (!options.authorization.empty()) {
        // a node list: the header value contains commas, which the string form would split
        std::string header = "Authorization: " + options.authorization;
        mpv_node item;
        item.format = MPV_FORMAT_STRING;
        item.u.string = header.data();
        mpv_node_list list;
        list.num = 1;
        list.values = &item;
        list.keys = nullptr;
        mpv_node node;
        node.format = MPV_FORMAT_NODE_ARRAY;
        node.u.list = &list;
        if (mpv_set_option(mpv, "http-header-fields", MPV_FORMAT_NODE, &node) < 0) {
            PF_LOG_WARN("mpv: the authorization header was refused");
        }
    }
    mpv_request_log_messages(mpv, "warn");
    const int rc = mpv_initialize(mpv);
    if (rc < 0) {
        PF_LOG_ERROR("mpv: initialisation failed: %s", mpv_error_string(rc));
        return nullptr;
    }
    mpv_render_param params[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW)},
                                 {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&impl->render, mpv, params) < 0) {
        PF_LOG_ERROR("mpv: cannot create the software renderer");
        return nullptr;
    }
    Impl* raw = impl.get();
    mpv_set_wakeup_callback(
        mpv, [](void* data) { static_cast<Impl*>(data)->wake(); }, raw);
    mpv_render_context_set_update_callback(
        impl->render, [](void* data) { static_cast<Impl*>(data)->wake(); }, raw);
    return std::unique_ptr<Engine>(new Engine(std::move(impl)));
}

Engine::Engine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Engine::~Engine() = default;

void Engine::loadPlaylist(const std::string& url, double startSeconds) { open(url, startSeconds, {}, true); }

void Engine::load(const std::string& url, double startSeconds, const std::string& audioUrl) {
    open(url, startSeconds, audioUrl, false);
}

void Engine::open(const std::string& url, double startSeconds, const std::string& audioUrl, bool playlist) {
    // a file opens no other file, except a playlist its segments (hardening, see hardeningOptions)
    mpv_set_property_string(impl_->mpv, "access-references", playlist ? "yes" : "no");
    // "start" and the audio before loadfile: the loadfile argument list changed between mpv versions
    mpv_set_property_string(impl_->mpv, "start", startSeconds > 0 ? std::to_string(startSeconds).c_str() : "none");
    // a list option: the node form, so that commas or colons in the address are not taken as separators
    mpv_node item;
    item.format = MPV_FORMAT_STRING;
    item.u.string = const_cast<char*>(audioUrl.c_str());
    mpv_node_list list;
    list.num = audioUrl.empty() ? 0 : 1;
    list.values = &item;
    list.keys = nullptr;
    mpv_node node;
    node.format = MPV_FORMAT_NODE_ARRAY;
    node.u.list = &list;
    if (mpv_set_property(impl_->mpv, "audio-files", MPV_FORMAT_NODE, &node) < 0) {
        PF_LOG_WARN("mpv: the separate audio was refused");
    }
    impl_->loaded = false;
    impl_->shownFirst = false;
    impl_->error.clear();
    impl_->command({"loadfile", url.c_str(), "replace"});
}

void Engine::addSubtitle(const std::string& url, bool select) {
    impl_->command({"sub-add", url.c_str(), select ? "select" : "auto"});
}

void Engine::setPaused(bool paused) {
    int value = paused ? 1 : 0;
    mpv_set_property(impl_->mpv, "pause", MPV_FORMAT_FLAG, &value);
}

void Engine::seekTo(double seconds) {
    const std::string target = std::to_string(seconds);
    impl_->command({"seek", target.c_str(), "absolute"});
}

void Engine::setAudioTrack(int mpvId) {
    mpv_set_property_string(impl_->mpv, "aid", mpvId > 0 ? std::to_string(mpvId).c_str() : "no");
}

void Engine::setVolume(int percent) {
    double value = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    mpv_set_property(impl_->mpv, "volume", MPV_FORMAT_DOUBLE, &value);
}

void Engine::setSpeed(double speed) {
    double value = speed < 0.5 ? 0.5 : speed > 2.0 ? 2.0 : speed;
    mpv_set_property(impl_->mpv, "speed", MPV_FORMAT_DOUBLE, &value);
}

double Engine::speed() const {
    double value = 1.0;
    mpv_get_property(impl_->mpv, "speed", MPV_FORMAT_DOUBLE, &value);
    return value;
}

void Engine::setSubtitleTrack(int mpvId) {
    mpv_set_property_string(impl_->mpv, "sid", mpvId > 0 ? std::to_string(mpvId).c_str() : "no");
}

Engine::State Engine::state() const {
    State s;
    s.position = impl_->number("time-pos");
    s.duration = impl_->number("duration");
    s.paused = impl_->flag("pause");
    s.buffering = impl_->flag("paused-for-cache");
    s.subtitleTrack = static_cast<int>(impl_->integer("sid"));
    s.tracks = static_cast<int>(impl_->integer("track-list/count"));
    s.speed = impl_->number("speed");
    s.error = impl_->error;
    return s;
}

std::vector<EngineEvent> Engine::drainEvents() {
    for (;;) {
        mpv_event* event = mpv_wait_event(impl_->mpv, 0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
        case MPV_EVENT_COMMAND_REPLY:
            if (event->error < 0) {
                PF_LOG_WARN("mpv: a command failed: %s", mpv_error_string(event->error));
            }
            break;
        case MPV_EVENT_LOG_MESSAGE: {
            const auto* message = static_cast<mpv_event_log_message*>(event->data);
            std::string line = message->text != nullptr ? message->text : "";
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            // a message some videos repeat every few seconds is written once, then with its count
            const std::string text = std::string("mpv [") + (message->prefix != nullptr ? message->prefix : "") +
                                     "]: " + line;
            if (const auto written = impl_->repeats.line(text)) {
                PF_LOG_WARN("%s", written->c_str());
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            impl_->loaded = true;
            impl_->pending.push_back(EngineEvent::FileLoaded);
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            impl_->pending.push_back(EngineEvent::Seeked);
            break;
        case MPV_EVENT_END_FILE: {
            const auto* end = static_cast<mpv_event_end_file*>(event->data);
            if (end->reason == MPV_END_FILE_REASON_EOF) {
                impl_->pending.push_back(EngineEvent::EndOfFile);
            } else if (end->reason == MPV_END_FILE_REASON_ERROR) {
                impl_->error = mpv_error_string(end->error);
                PF_LOG_WARN("player: the file cannot be played: %s", impl_->error.c_str());
                impl_->pending.push_back(EngineEvent::Error);
            }
            break;
        }
        default:
            break;
        }
    }
    std::vector<EngineEvent> out;
    out.swap(impl_->pending);
    return out;
}

bool Engine::renderIfDue(VideoFrame& frame) {
    if ((mpv_render_context_update(impl_->render) & MPV_RENDER_UPDATE_FRAME) == 0) {
        return false;
    }
    const auto width = static_cast<int>(impl_->integer("dwidth"));
    const auto height = static_cast<int>(impl_->integer("dheight"));
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        return false;
    }
    // mpv wants a stride and a pointer on 64 bytes: a width whose rows are not (1080 px: 4320 bytes) goes through
    // its unaligned path, which ended Playtube on the console. Such a picture is drawn in an aligned buffer, then
    // its rows are packed into the frame.
    const std::size_t tight = static_cast<std::size_t>(width) * 4;
    const std::size_t stride = (tight + 63) / 64 * 64;
    frame.pixels.resize(tight * static_cast<std::size_t>(height));
    std::uint8_t* target = frame.pixels.data();
    if (stride != tight || reinterpret_cast<std::uintptr_t>(target) % 64 != 0) {
        impl_->aligned.resize(stride * static_cast<std::size_t>(height) + 64);
        const auto base = reinterpret_cast<std::uintptr_t>(impl_->aligned.data());
        target = impl_->aligned.data() + (64 - base % 64) % 64;
    }
    int size[2] = {width, height};
    char format[] = "rgb0";
    std::size_t strideParam = stride;
    int block = 0;  // never wait for the picture's display time: the frame loop must keep drawing the UI
    mpv_render_param params[] = {{MPV_RENDER_PARAM_SW_SIZE, size},
                                 {MPV_RENDER_PARAM_SW_FORMAT, format},
                                 {MPV_RENDER_PARAM_SW_STRIDE, &strideParam},
                                 {MPV_RENDER_PARAM_SW_POINTER, target},
                                 {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
                                 {MPV_RENDER_PARAM_INVALID, nullptr}};
    const auto start = std::chrono::steady_clock::now();
    const int rc = mpv_render_context_render(impl_->render, params);
    impl_->renderMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (rc < 0) {
        PF_LOG_WARN("mpv: render failed: %s", mpv_error_string(rc));
        return false;
    }
    if (target != frame.pixels.data()) {
        for (int y = 0; y < height; ++y) {
            std::memcpy(frame.pixels.data() + static_cast<std::size_t>(y) * tight,
                        target + static_cast<std::size_t>(y) * stride, tight);
        }
    }
    frame.width = width;
    frame.height = height;
    ++frame.serial;
    if (impl_->loaded && !impl_->shownFirst) {
        impl_->shownFirst = true;
        impl_->pending.push_back(EngineEvent::FirstFrame);
    }
    return true;
}

Engine::Stats Engine::stats() const {
    Stats s;
    s.decoderDrops = static_cast<long>(impl_->integer("decoder-frame-drop-count"));
    s.outputDrops = static_cast<long>(impl_->integer("frame-drop-count"));
    s.renderMs = impl_->renderMs;
    return s;
}

std::string Engine::tag(const char* name) const {
    // the file's tags, the key compared without case ("title", "Title", "TITLE")
    std::string out;
    mpv_node node;
    if (mpv_get_property(impl_->mpv, "metadata", MPV_FORMAT_NODE, &node) < 0) {
        return out;
    }
    if (node.format == MPV_FORMAT_NODE_MAP && node.u.list != nullptr) {
        for (int i = 0; i < node.u.list->num && out.empty(); ++i) {
            const char* key = node.u.list->keys[i];
            const mpv_node& value = node.u.list->values[i];
            if (key != nullptr && value.format == MPV_FORMAT_STRING && value.u.string != nullptr &&
                strcasecmp(key, name) == 0) {
                out = value.u.string;
            }
        }
    }
    mpv_free_node_contents(&node);
    return out.size() > 512 ? out.substr(0, 512) : out;
}

std::vector<Engine::Track> Engine::tracks() const {
    std::vector<Track> out;
    mpv_node node;
    if (mpv_get_property(impl_->mpv, "track-list", MPV_FORMAT_NODE, &node) < 0) {
        return out;
    }
    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list != nullptr) {
        for (int i = 0; i < node.u.list->num && out.size() < 256; ++i) {
            const mpv_node& entry = node.u.list->values[i];
            if (entry.format != MPV_FORMAT_NODE_MAP || entry.u.list == nullptr) {
                continue;
            }
            Track track;
            std::string type;
            std::string file;
            for (int k = 0; k < entry.u.list->num; ++k) {
                const char* key = entry.u.list->keys[k];
                const mpv_node& value = entry.u.list->values[k];
                if (std::strcmp(key, "type") == 0 && value.format == MPV_FORMAT_STRING) {
                    type = value.u.string;
                } else if (std::strcmp(key, "id") == 0 && value.format == MPV_FORMAT_INT64) {
                    track.id = static_cast<int>(value.u.int64);
                } else if (std::strcmp(key, "title") == 0 && value.format == MPV_FORMAT_STRING) {
                    track.title = value.u.string;
                } else if (std::strcmp(key, "lang") == 0 && value.format == MPV_FORMAT_STRING) {
                    track.language = value.u.string;
                } else if (std::strcmp(key, "selected") == 0 && value.format == MPV_FORMAT_FLAG) {
                    track.selected = value.u.flag != 0;
                } else if (std::strcmp(key, "external-filename") == 0 && value.format == MPV_FORMAT_STRING) {
                    file = value.u.string;
                }
            }
            if (type != "audio" && type != "sub") {
                continue;
            }
            track.audio = type == "audio";
            if (track.title.empty() && !file.empty()) {
                const std::size_t slash = file.rfind('/');
                track.title = slash == std::string::npos ? file : file.substr(slash + 1);
            }
            out.push_back(std::move(track));
        }
    }
    mpv_free_node_contents(&node);
    return out;
}

std::string Engine::describe() const {
    return impl_->text("video-codec") + " " + impl_->text("width") + "x" + impl_->text("height") + " " +
           impl_->text("file-format");
}

}  // namespace playtube::player
