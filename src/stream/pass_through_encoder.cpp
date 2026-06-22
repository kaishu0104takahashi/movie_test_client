#include "stream/pass_through_encoder.hpp"

bool PassThroughEncoder::encode_frame(const std::vector<uint8_t>& in_data, std::vector<uint8_t>& out_data) {
    if (in_data.empty()) {
        return false;
    }

    // カメラから取得した時点で既にH.264データ（NALユニット）になっているため、
    // 複雑なエンコード計算は一切行わず、そのまま送信用のvectorにコピーする。
    out_data = in_data;

    return true;
}