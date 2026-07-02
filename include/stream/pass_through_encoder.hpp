/**
 * @file pass_through_encoder.hpp
 * @brief 生のH.264ストリームを解析（パース）して正しく送信するクラス
 */
#ifndef PASS_THROUGH_ENCODER_HPP_
#define PASS_THROUGH_ENCODER_HPP_

#include "stream/i_encoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

class PassThroughEncoder : public IEncoder {
public:
    PassThroughEncoder();
    ~PassThroughEncoder() override;

    bool encode_frame(const std::vector<uint8_t>& in_data, std::vector<uint8_t>& out_data) override;

private:
    AVCodecParserContext* parser_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVPacket* pkt_ = nullptr;
};

#endif