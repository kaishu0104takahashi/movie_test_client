#include "stream/h264_encoder.hpp"
#include <stdexcept>
#include <cstring>

H264Encoder::H264Encoder(int width, int height, int fps, const std::string& encoder_name)
    : width_(width), height_(height) {
    
    // 指定された名前（例: libx264）に対応するエンコーダを検索して取得する
    const AVCodec* codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    if (!codec) throw std::runtime_error("エラー: エンコーダが見つかりません");

    // エンコーダの設定情報（コンテキスト）を格納するためのメモリを割り当てる
    codec_ctx_ = avcodec_alloc_context3(codec);
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    
    // タイムベース（時間の基本単位）とフレームレートを設定する
    codec_ctx_->time_base = {1, fps};
    codec_ctx_->framerate = {fps, 1};
    
    // H.264エンコードにおいて標準的に用いられるピクセルフォーマット（YUV420P）を指定する
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // 目標とする映像ビットレートを5Mbps（5,000,000 bps）に設定し、データ量と画質を制御する
    codec_ctx_->bit_rate = 5000000;
    
    // GOP (Group of Pictures) サイズの設定。フレームレートと同値にすることで、
    // 1秒に1回必ずキーフレーム（Iフレーム）を挿入し、パケットロス発生時の映像復帰を早める機能を持たせる
    codec_ctx_->gop_size = fps;

    // リアルタイムストリーミング用のエンコーダオプションを設定する
    // ultrafast: 圧縮効率よりも処理速度を最優先する
    // zerolatency: エンコーダ内部でのバッファリングを無効化し、遅延を最小限に抑える
    AVDictionary *opt = nullptr;
    av_dict_set(&opt, "preset", "ultrafast", 0);
    av_dict_set(&opt, "tune", "zerolatency", 0);

    // 設定したオプションとコンテキストを用いて、実際にエンコーダを初期化して起動する
    if (avcodec_open2(codec_ctx_, codec, &opt) < 0) {
        throw std::runtime_error("エラー: エンコーダの起動に失敗しました");
    }
    av_dict_free(&opt);

    // カメラから取得する非圧縮の生データ（YUYV422形式）を格納するためのフレーム構造体を確保する
    frame_yuyv_ = av_frame_alloc();
    frame_yuyv_->format = AV_PIX_FMT_YUYV422;
    frame_yuyv_->width = width;
    frame_yuyv_->height = height;
    av_frame_get_buffer(frame_yuyv_, 0);

    // エンコーダに渡すための変換後データ（YUV420P形式）を格納するフレーム構造体を確保する
    frame_yuv420p_ = av_frame_alloc();
    frame_yuv420p_->format = AV_PIX_FMT_YUV420P;
    frame_yuv420p_->width = width;
    frame_yuv420p_->height = height;
    av_frame_get_buffer(frame_yuv420p_, 0);

    // 生データ（YUYV422）からエンコード用データ（YUV420P）へのピクセルフォーマット変換、
    // および必要に応じたスケーリング処理を行うための変換コンテキストを生成する
    sws_ctx_ = sws_getContext(width, height, AV_PIX_FMT_YUYV422,
                              width, height, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
}

H264Encoder::~H264Encoder() {
    // 確保した各種コンテキストとフレームのメモリを安全に解放し、メモリリークを防ぐ
    if (sws_ctx_) sws_freeContext(sws_ctx_);
    if (frame_yuv420p_) av_frame_free(&frame_yuv420p_);
    if (frame_yuyv_) av_frame_free(&frame_yuyv_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool H264Encoder::send_frame(const uint8_t* in_data, size_t size) {
    // 入力データのサイズが、指定解像度におけるYUYV422の想定サイズと一致するか検証する
    int expected_size = width_ * height_ * 2;
    if (size < expected_size || !in_data) return false;

    // 入力された生データを、変換前用のフレーム（frame_yuyv_）のデータ領域にコピーする
    std::memcpy(frame_yuyv_->data[0], in_data, expected_size);

    // libswscaleを用いて、YUYV422からYUV420Pへのフォーマット変換を実行し、frame_yuv420p_に出力する
    sws_scale(sws_ctx_, frame_yuyv_->data, frame_yuyv_->linesize, 0, height_,
              frame_yuv420p_->data, frame_yuv420p_->linesize);

    // フレームに表示タイミングを示すタイムスタンプ（PTS）を付与し、カウントを進める
    frame_yuv420p_->pts = frame_count_++;

    // フォーマット変換されたフレームをエンコーダに送信し、圧縮処理を要求する
    int ret = avcodec_send_frame(codec_ctx_, frame_yuv420p_);
    return (ret == 0);
}

bool H264Encoder::receive_packet(AVPacket* out_pkt) {
    // エンコーダから、圧縮処理が完了したパケット（H.264データ）を取り出す
    int ret = avcodec_receive_packet(codec_ctx_, out_pkt);
    return (ret == 0);
}