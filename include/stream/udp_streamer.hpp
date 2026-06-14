/**
 * @file udp_streamer.hpp
 * @brief H.264データをMPEG-TSに梱包してUDP送信する部品
 */

 #ifndef UDP_STREAMER_HPP_
 #define UDP_STREAMER_HPP_
 
 #include <string>
 #include <vector>
 #include <cstdint>
 
 extern "C" {
 #include <libavformat/avformat.h>
 }
 
 class UdpStreamer {
 public:
     // ① 準備：送り先のIPアドレス（Tailscale）とポート番号を指定する
     UdpStreamer(const std::string& dest_ip, int port);
     
     // ② 後片付け：通信を綺麗に閉じる
     ~UdpStreamer();
 
     // ③ メイン処理：圧縮済みのH.264データを渡すと、ネットワークに発射する
     bool send_packet(const std::vector<uint8_t>& h264_data);
 
 private:
     AVFormatContext* fmt_ctx_ = nullptr; // 通信と梱包の「ルールブック」
     AVStream* out_stream_ = nullptr;     // 映像の通り道
     int frame_count_ = 0;                // 送った枚数を数えるカウンター
 };
 
 #endif