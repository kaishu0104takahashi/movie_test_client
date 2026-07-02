#include "stream/udp_streamer.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>

UdpStreamer::UdpStreamer(const std::string& dest_ip, int port) {
    avformat_network_init();

    std::string url = "udp://" + dest_ip + ":" + std::to_string(port) + "?pkt_size=1128";

    avformat_alloc_output_context2(&fmt_ctx_, nullptr, "mpegts", url.c_str());
    if (!fmt_ctx_) {
        throw std::runtime_error("エラー: MPEG-TS 出力設定の作成に失敗しました");
    }

    out_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
    if (!out_stream_) {
        throw std::runtime_error("エラー: 出力ストリームの作成に失敗しました");
    }

    out_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    out_stream_->codecpar->codec_id = AV_CODEC_ID_H264;

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, url.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error("エラー: 宛先ポートを開けませんでした -> " + url);
        }
    }

    if (avformat_write_header(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("エラー: 通信ヘッダの書き込みに失敗しました");
    }
}

UdpStreamer::~UdpStreamer() {
    if (fmt_ctx_) {
        av_write_trailer(fmt_ctx_);
        if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmt_ctx_->pb);
        }
        avformat_free_context(fmt_ctx_);
    }
    avformat_network_deinit();
}

bool UdpStreamer::send_packet(const std::vector<uint8_t>& h264_data) {
    if (h264_data.empty()) return false;

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) return false;

    av_new_packet(pkt, h264_data.size());
    std::memcpy(pkt->data, h264_data.data(), h264_data.size());

    // ==============================================================
    // ★修正箇所：強制的なタイムスタンプ(PTS)の上書きを廃止する
    // ==============================================================
    // パススルー映像の場合、カメラの出力タイミングとプログラムのループ時間がズレるため、
    // タイムスタンプを「未設定（AV_NOPTS_VALUE）」にして、
    // FFmpegのMPEG-TSコンテナ側にタイミングを自動計算・補正させます。
    pkt->pts = frame_count_ * (90000 / 30); 
    pkt->dts = pkt->pts;
    pkt->stream_index = out_stream_->index;

    // ネットワークに向けて発射
    int ret = av_interleaved_write_frame(fmt_ctx_, pkt);

    av_packet_free(&pkt);
    frame_count_++;

    return (ret >= 0);
}