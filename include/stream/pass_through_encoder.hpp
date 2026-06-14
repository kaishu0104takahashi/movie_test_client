/**
 * @file pass_through_encoder.hpp
 * @brief すでにH.264化されたデータをそのまま流すだけの子クラス
 */

 #ifndef PASS_THROUGH_ENCODER_HPP_
 #define PASS_THROUGH_ENCODER_HPP_
 
 #include "stream/i_encoder.hpp"
 
 class PassThroughEncoder : public IEncoder {
 public:
     PassThroughEncoder() = default;
     ~PassThroughEncoder() override = default;
 
     // 親のルールを実装（データを右から左へコピーするだけ）
     bool encode_frame(const std::vector<uint8_t>& in_data, std::vector<uint8_t>& out_data) override {
         out_data = in_data;
         return true;
     }
 };
 
 #endif