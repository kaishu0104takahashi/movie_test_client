#ifndef PASS_THROUGH_ENCODER_HPP_
#define PASS_THROUGH_ENCODER_HPP_

#include "stream/i_encoder.hpp"

class PassThroughEncoder : public IEncoder {
public:
    PassThroughEncoder();
    ~PassThroughEncoder() override;

    bool send_frame(const uint8_t* in_data, size_t size) override;
    bool receive_packet(AVPacket* out_pkt) override;

private:
    AVPacket* internal_pkt_ = nullptr;
    bool has_data_ = false;
};

#endif