#include "stream/udp_streamer.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>

UdpStreamer::UdpStreamer(const std::string& dest_ip, int port) {
    // ネットワーク機能の初期化
    avformat_network_init();

    // 送り先のURL（例: udp://100.76.x.x:1234?pkt_size=1316）
    // pkt_size=1316 は「UDPの壁にぶつからない安全なサイズに切り刻んでね」というFFmpegへの重要なお願いです
    std::string url = "udp://" + dest_ip + ":" + std::to_string(port) + "?pkt_size=1316";

    // MPEG-TS形式で出力する設定を作成
    avformat_alloc_output_context2(&fmt_ctx_, nullptr, "mpegts", url.c_str());
    if (!fmt_ctx_) {
        throw std::runtime_error("エラー: MPEG-TS 出力設定の作成に失敗しました");
    }

    // 映像用のストリーム（通り道）を1つ追加する
    out_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
    if (!out_stream_) {
        throw std::runtime_error("エラー: 出力ストリームの作成に失敗しました");
    }

    // この通り道には「H.264」の映像が流れるよ、と宣言する
    out_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    out_stream_->codecpar->codec_id = AV_CODEC_ID_H264;

    // 宛先への扉（UDPポート）を開く
    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, url.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error("エラー: 宛先ポートを開けませんでした -> " + url);
        }
    }

    // 通信開始の合図（ヘッダ）を送る
    if (avformat_write_header(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("エラー: 通信ヘッダの書き込みに失敗しました");
    }
}

UdpStreamer::~UdpStreamer() {
    if (fmt_ctx_) {
        // 終了の合図を送って扉を閉める
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

    // FFmpeg用の空のパケット（カプセル）を用意
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) return false;

    // 受け取ったH.264データをカプセルの中にコピーする
    av_new_packet(pkt, h264_data.size());
    std::memcpy(pkt->data, h264_data.data(), h264_data.size());

    // タイムスタンプ（再生の順番とタイミング）を設定する ※30fps想定の簡易設定
    pkt->pts = frame_count_ * 90000 / 30; 
    pkt->dts = pkt->pts;
    pkt->stream_index = out_stream_->index;

    // ネットワークに向けて発射！（ここで自動的にMPEG-TSに切り刻まれます）
    int ret = av_interleaved_write_frame(fmt_ctx_, pkt);

    // 撃ち終わった空のカプセルを処分する
    av_packet_free(&pkt);
    frame_count_++;

    return (ret >= 0);
}