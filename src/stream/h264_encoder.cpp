#include "stream/h264_encoder.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>

H264Encoder::H264Encoder(int width, int height, int fps, const std::string& encoder_name)
    : width_(width), height_(height) {
    
    // 1. 指定された名前のエンコーダ（ラズパイのハードウェア等）を探す
    const AVCodec* codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    if (!codec) {
        throw std::runtime_error("エラー: エンコーダが見つかりません -> " + encoder_name);
    }

    // 2. エンコーダの「設定データ」を作成
    codec_ctx_ = avcodec_alloc_context3(codec);
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->time_base = {1, fps};
    codec_ctx_->framerate = {fps, 1};
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P; // H.264が好きな標準フォーマット

    codec_ctx_->bit_rate = 1000000; // 2Mbps (数値を上げると高画質、下げると低遅延・軽量)
    
    // ★遠隔操作EV用 超低遅延のおまじない（ソフトウェアモードの時に効きます）
    AVDictionary *opt = nullptr;
    av_dict_set(&opt, "preset", "ultrafast", 0);
    av_dict_set(&opt, "tune", "zerolatency", 0);

    // 3. 設定を適用してエンコーダを起動！
    if (avcodec_open2(codec_ctx_, codec, &opt) < 0) {
        throw std::runtime_error("エラー: エンコーダの起動に失敗しました");
    }
    av_dict_free(&opt);

    // 4. データを入れる「箱（フレームとパケット）」を準備する
    frame_yuyv_ = av_frame_alloc();
    frame_yuyv_->format = AV_PIX_FMT_YUYV422;
    frame_yuyv_->width = width;
    frame_yuyv_->height = height;
    av_frame_get_buffer(frame_yuyv_, 0);

    frame_yuv420p_ = av_frame_alloc();
    frame_yuv420p_->format = AV_PIX_FMT_YUV420P;
    frame_yuv420p_->width = width;
    frame_yuv420p_->height = height;
    av_frame_get_buffer(frame_yuv420p_, 0);

    pkt_ = av_packet_alloc();

    // 5. 色の変換器（YUYV → YUV420P）を準備する
    sws_ctx_ = sws_getContext(width, height, AV_PIX_FMT_YUYV422,
                              width, height, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
}

H264Encoder::~H264Encoder() {
    // 使い終わったらメモリを綺麗に掃除する（C++の鉄則）
    if (sws_ctx_) sws_freeContext(sws_ctx_);
    if (frame_yuv420p_) av_frame_free(&frame_yuv420p_);
    if (frame_yuyv_) av_frame_free(&frame_yuyv_);
    if (pkt_) av_packet_free(&pkt_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool H264Encoder::encode_frame(const std::vector<uint8_t>& yuyv_data, std::vector<uint8_t>& out_h264_data) {
    // 1. カメラから来た生データをFFmpegの箱（frame_yuyv_）にコピー
    int expected_size = width_ * height_ * 2;
    if (yuyv_data.size() < expected_size) return false;
    std::memcpy(frame_yuyv_->data[0], yuyv_data.data(), expected_size);

    // 2. YUYV形式 を H.264が処理できる YUV420P形式 に変換（sws_scale）
    sws_scale(sws_ctx_, frame_yuyv_->data, frame_yuyv_->linesize, 0, height_,
              frame_yuv420p_->data, frame_yuv420p_->linesize);
    
    frame_yuv420p_->pts = frame_count_++; // フレームに番号を振る

    // 3. 変換した画像をエンコーダに投げ込む！
    int ret = avcodec_send_frame(codec_ctx_, frame_yuv420p_);
    if (ret < 0) return false;

    // 4. エンコーダから圧縮済みのデータ（H.264）を取り出す！
    out_h264_data.clear();
    ret = avcodec_receive_packet(codec_ctx_, pkt_);
    if (ret >= 0) {
        // パケットのデータをC++のvector（配列）にコピーして渡す
        out_h264_data.assign(pkt_->data, pkt_->data + pkt_->size);
        av_packet_unref(pkt_); // パケットをリセット
        return true;           // 圧縮成功！
    }

    return false; // まだ圧縮データが出てこない時（バッファリング中など）
}