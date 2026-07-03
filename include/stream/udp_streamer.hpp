/**
 * @file udp_streamer.hpp
 * @brief H.264パケットをMPEG-TSに梱包してUDP送信する部品
 */
#ifndef UDP_STREAMER_HPP_
#define UDP_STREAMER_HPP_

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class UdpStreamer {
public:
    UdpStreamer(const std::string& dest_ip, int port);
    ~UdpStreamer();

    // ★ 変更：std::vectorではなく、FFmpegのパケット(AVPacket)のポインタを直接受け取る
    bool send_packet(AVPacket* pkt);

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    AVStream* out_stream_ = nullptr;
    int frame_count_ = 0;
};

#endif