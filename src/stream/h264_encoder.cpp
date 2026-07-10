#include "stream/h264_encoder.hpp"
#include <stdexcept>
#include <cstring>

H264Encoder::H264Encoder(int width, int height, int fps, const std::string& encoder_name)
    : width_(width), height_(height) {
    
    const AVCodec* codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    if (!codec) throw std::runtime_error("エラー: エンコーダが見つかりません");

    codec_ctx_ = avcodec_alloc_context3(codec);
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->time_base = {1, fps};
    codec_ctx_->framerate = {fps, 1};
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // ★ 修正箇所：目標ビットレートを4G回線に合わせて500kbpsに絞る
    codec_ctx_->bit_rate = 500000;

    AVDictionary *opt = nullptr;
    av_dict_set(&opt, "preset", "ultrafast", 0);
    av_dict_set(&opt, "tune", "zerolatency", 0);

    if (avcodec_open2(codec_ctx_, codec, &opt) < 0) {
        throw std::runtime_error("エラー: エンコーダの起動に失敗しました");
    }
    av_dict_free(&opt);

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

    sws_ctx_ = sws_getContext(width, height, AV_PIX_FMT_YUYV422,
                              width, height, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
}

H264Encoder::~H264Encoder() {
    if (sws_ctx_) sws_freeContext(sws_ctx_);
    if (frame_yuv420p_) av_frame_free(&frame_yuv420p_);
    if (frame_yuyv_) av_frame_free(&frame_yuyv_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool H264Encoder::send_frame(const uint8_t* in_data, size_t size) {
    int expected_size = width_ * height_ * 2;
    if (size < expected_size || !in_data) return false;

    std::memcpy(frame_yuyv_->data[0], in_data, expected_size);

    sws_scale(sws_ctx_, frame_yuyv_->data, frame_yuyv_->linesize, 0, height_,
              frame_yuv420p_->data, frame_yuv420p_->linesize);

    frame_yuv420p_->pts = frame_count_++;

    // エンコーダに「入れる」だけ
    int ret = avcodec_send_frame(codec_ctx_, frame_yuv420p_);
    return (ret == 0);
}

bool H264Encoder::receive_packet(AVPacket* out_pkt) {
    // エンコーダから「取り出す」だけ
    int ret = avcodec_receive_packet(codec_ctx_, out_pkt);
    return (ret == 0);
}