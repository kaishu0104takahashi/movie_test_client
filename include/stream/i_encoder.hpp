/**
 * @file i_encoder.hpp
 * @brief エンコーダの「親クラス（インターフェース）」
 */
#ifndef I_ENCODER_HPP_
#define I_ENCODER_HPP_

#include <cstdint>
#include <cstddef>

extern "C" {
#include <libavcodec/avcodec.h>
}

class IEncoder {
public:
    virtual ~IEncoder() = default;

    // ★ 変更：入れる（send）と 出す（receive）を完全に分離する
    virtual bool send_frame(const uint8_t* in_data, size_t size) = 0;
    virtual bool receive_packet(AVPacket* out_pkt) = 0;
};

#endif