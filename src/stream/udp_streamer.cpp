
    // =========================================================================
    // ★修正箇所：送信ペースの調整（マイクロバースト対策）
    // bitrate=10000000 (10Mbps) を指定し、パケット間に微小な待機時間を設ける
    // buffer_size も 5MB (5242880) に拡張
    // =========================================================================
    //std::string url = "udp://" + dest_ip + ":" + std::to_string(port) + 
    //                  "?pkt_size=1128&buffer_size=5242880&bitrate=10000000";
    #include "stream/udp_streamer.hpp"
    #include <stdexcept>
    #include <iostream>
    
    UdpStreamer::UdpStreamer(const std::string& dest_ip, int port) {
        avformat_network_init();
    
        // =========================================================================
        // ★修正箇所：送信ペースの調整（マイクロバースト対策）
        // bitrate制限を外し、バッファを5MBにして大渋滞を回避する
        // =========================================================================
        std::string url = "udp://" + dest_ip + ":" + std::to_string(port) + 
                          "?pkt_size=1128&buffer_size=524288";//値を10/1にしている
    
        avformat_alloc_output_context2(&fmt_ctx_, nullptr, "mpegts", url.c_str());
        if (!fmt_ctx_) {
            throw std::runtime_error("エラー: MPEG-TS 出力設定に失敗");
        }
    
        out_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        out_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        out_stream_->codecpar->codec_id = AV_CODEC_ID_H264;
    
        AVDictionary* options = nullptr;
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "preset", "ultrafast", 0);
    
        if (avio_open(&fmt_ctx_->pb, url.c_str(), AVIO_FLAG_WRITE) < 0) {
            av_dict_free(&options);
            throw std::runtime_error("エラー: 送信ポートを開けません");
        }
        
        if (avformat_write_header(fmt_ctx_, &options) < 0) {
            av_dict_free(&options);
            throw std::runtime_error("エラー: 通信ヘッダの書き込みに失敗しました");
        }
        
        av_dict_free(&options);
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
    
    bool UdpStreamer::send_packet(AVPacket* pkt) {
        if (!pkt || pkt->size == 0) return false;
    
        pkt->pts = frame_count_ * (90000 / 30); 
        pkt->dts = pkt->pts;
        pkt->stream_index = out_stream_->index;
    
        int ret = av_interleaved_write_frame(fmt_ctx_, pkt);
    
        av_packet_unref(pkt);
        frame_count_++;
    
        return (ret >= 0);
    }