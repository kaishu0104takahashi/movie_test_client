#ifndef H264_ENCODER_HPP_
#define H264_ENCODER_HPP_

#include <string>
#include <cstdint>

#include "stream/i_encoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class H264Encoder : public IEncoder {
public:
    H264Encoder(int width, int height, int fps, const std::string& encoder_name);
    ~H264Encoder() override;

    bool send_frame(const uint8_t* in_data, size_t size) override;
    bool receive_packet(AVPacket* out_pkt) override;

private:
    int width_;
    int height_;
    int frame_count_ = 0;

    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* frame_yuyv_ = nullptr;
    AVFrame* frame_yuv420p_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
};

#endif