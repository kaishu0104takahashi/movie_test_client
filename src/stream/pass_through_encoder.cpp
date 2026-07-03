#include "stream/pass_through_encoder.hpp"
#include <cstring>

PassThroughEncoder::PassThroughEncoder() {
    internal_pkt_ = av_packet_alloc();
}

PassThroughEncoder::~PassThroughEncoder() {
    av_packet_free(&internal_pkt_);
}

bool PassThroughEncoder::send_frame(const uint8_t* in_data, size_t size) {
    if (size == 0 || !in_data) return false;

    av_packet_unref(internal_pkt_);
    av_new_packet(internal_pkt_, size);
    std::memcpy(internal_pkt_->data, in_data, size);

    // 超軽量キーフレーム検出器
    bool is_keyframe = false;
    for (size_t i = 0; i < size - 4; ++i) {
        if (in_data[i] == 0 && in_data[i+1] == 0 && in_data[i+2] == 1) {
            uint8_t nal_type = in_data[i+3] & 0x1F;
            if (nal_type == 5 || nal_type == 7 || nal_type == 8) {
                is_keyframe = true;
                break;
            }
        }
    }
    if (is_keyframe) {
        internal_pkt_->flags |= AV_PKT_FLAG_KEY;
    }

    has_data_ = true;
    return true;
}

bool PassThroughEncoder::receive_packet(AVPacket* out_pkt) {
    if (!has_data_) return false;
    av_packet_move_ref(out_pkt, internal_pkt_); // データを転送して内部を空にする
    has_data_ = false;
    return true;
}