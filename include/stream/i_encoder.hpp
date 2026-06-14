/**
 * @file i_encoder.hpp
 * @brief エンコーダの「親クラス（インターフェース）」
 */

 #ifndef I_ENCODER_HPP_
 #define I_ENCODER_HPP_
 
 #include <vector>
 #include <cstdint>
 
 class IEncoder {
 public:
     // 継承される親クラスのデストラクタには必ず virtual をつける（メモリリーク防止）
     virtual ~IEncoder() = default;
 
     // 子クラスは絶対にこの関数を実装しなさい、というルール
     virtual bool encode_frame(const std::vector<uint8_t>& in_data, std::vector<uint8_t>& out_data) = 0;
 };
 
 #endif