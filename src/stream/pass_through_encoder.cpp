#include "stream/pass_through_encoder.hpp"
#include <stdexcept>
#include <iostream>

PassThroughEncoder::PassThroughEncoder() {
    // 1. 解析用（パース用）のコーデックを探す
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        throw std::runtime_error("エラー: H.264パーサー用のコーデックが見つかりません");
    }

    // 2. パーサー（解析機）を初期化
    parser_ = av_parser_init(codec->id);
    if (!parser_) {
        throw std::runtime_error("エラー: H.264パーサーの初期化に失敗しました");
    }

    codec_ctx_ = avcodec_alloc_context3(codec);
    pkt_ = av_packet_alloc();
}

PassThroughEncoder::~PassThroughEncoder() {
    if (parser_) av_parser_close(parser_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
    if (pkt_) av_packet_free(&pkt_);
}

bool PassThroughEncoder::encode_frame(const std::vector<uint8_t>& in_data, std::vector<uint8_t>& out_data) {
    if (in_data.empty()) return false;

    // ★ここが魔法の処理：
    // FFmpegのパーサーに生のバイト列を流し込み、NALユニット（フレーム境界）を正確に認識させる
    int parsed_len = av_parser_parse2(
        parser_, codec_ctx_,
        &pkt_->data, &pkt_->size,
        in_data.data(), in_data.size(),
        AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0
    );

    // 意味のある1フレームとしてパース（切り出し）が完了したら、送信用の配列にコピー
    if (pkt_->size > 0) {
        out_data.assign(pkt_->data, pkt_->data + pkt_->size);
        return true;
    }

    // まだ1フレーム分揃っていない場合（細切れで届いた場合）はスキップ
    return false; 
}