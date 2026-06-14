/**
 * @file h264_encoder.hpp
 * @brief カメラの生映像をH.264に圧縮する部品
 */

 #ifndef H264_ENCODER_HPP_
 #define H264_ENCODER_HPP_
 
 #include <vector>
 #include <string>
 #include <cstdint>
 
 // 親クラスを読み込む
 #include "stream/i_encoder.hpp"
 
 extern "C" {
 #include <libavcodec/avcodec.h>
 #include <libavutil/opt.h>
 #include <libavutil/imgutils.h>
 #include <libswscale/swscale.h>
 }
 
 // 親クラス(IEncoder)を継承する
 class H264Encoder : public IEncoder {
 public:
     H264Encoder(int width, int height, int fps, const std::string& encoder_name);
     ~H264Encoder() override;
 
     // 親のルールに従って実装していることの宣言(override)
     bool encode_frame(const std::vector<uint8_t>& yuyv_data, std::vector<uint8_t>& out_h264_data) override;
 
 private:
     int width_;
     int height_;
     int frame_count_ = 0;
 
     AVCodecContext* codec_ctx_ = nullptr;
     AVFrame* frame_yuyv_ = nullptr;
     AVFrame* frame_yuv420p_ = nullptr;
     AVPacket* pkt_ = nullptr;
     SwsContext* sws_ctx_ = nullptr;
 };
 
 #endif