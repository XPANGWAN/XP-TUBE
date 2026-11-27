#include "player/webp.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <memory>

namespace playtube::player {

namespace {

struct CodecContextFree {
    void operator()(AVCodecContext* context) const { avcodec_free_context(&context); }
};
struct FrameFree {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
struct PacketFree {
    void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};
struct ScaleFree {
    void operator()(SwsContext* scale) const { sws_freeContext(scale); }
};

constexpr std::size_t kMaxBytes = 8u * 1024u * 1024u;

}  // namespace

bool decodeWebp(const std::string& bytes, int& width, int& height, std::vector<std::uint8_t>& rgba, int maxSide) {
    if (bytes.size() < 16 || bytes.size() > kMaxBytes || bytes.compare(0, 4, "RIFF") != 0 ||
        bytes.compare(8, 4, "WEBP") != 0) {
        return false;
    }
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_WEBP);
    if (codec == nullptr) {
        return false;
    }
    std::unique_ptr<AVCodecContext, CodecContextFree> context(avcodec_alloc_context3(codec));
    std::unique_ptr<AVPacket, PacketFree> packet(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameFree> frame(av_frame_alloc());
    if (!context || !packet || !frame) {
        return false;
    }
    context->thread_count = 1;
    if (avcodec_open2(context.get(), codec, nullptr) < 0) {
        return false;
    }
    // FFmpeg reads up to AV_INPUT_BUFFER_PADDING_SIZE past the data: a padded copy
    std::vector<std::uint8_t> padded(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    std::copy(bytes.begin(), bytes.end(), padded.begin());
    packet->data = padded.data();
    packet->size = static_cast<int>(bytes.size());
    if (avcodec_send_packet(context.get(), packet.get()) < 0 || avcodec_receive_frame(context.get(), frame.get()) < 0) {
        return false;
    }
    if (frame->width <= 0 || frame->height <= 0 || frame->width > maxSide || frame->height > maxSide) {
        return false;
    }
    std::unique_ptr<SwsContext, ScaleFree> scale(
        sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), frame->width,
                       frame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!scale) {
        return false;
    }
    width = frame->width;
    height = frame->height;
    rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    std::uint8_t* planes[4] = {rgba.data(), nullptr, nullptr, nullptr};
    int strides[4] = {width * 4, 0, 0, 0};
    return sws_scale(scale.get(), frame->data, frame->linesize, 0, frame->height, planes, strides) == height;
}

}  // namespace playtube::player
