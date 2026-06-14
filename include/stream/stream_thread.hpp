/**
 * @file stream_thread.hpp
 * @brief 映像取得・エンコード・送信のパイプラインを別スレッドで管理する司令塔
 */

 #ifndef STREAM_THREAD_HPP_
 #define STREAM_THREAD_HPP_
 
 #include <string>
 #include <thread>
 #include <atomic>
 
 // 外部から隠蔽（カプセル化）して、この中で部品を使う
 #include "camera/v4l2_capture.hpp"
 #include "stream/h264_encoder.hpp"
 #include "stream/udp_streamer.hpp"
 
 // 動作モードの定義もここに移す
 enum class EncodeMode {
     Hardware_Pi4,
     Software_Pi5,
     Camera_PassThrough
 };
 
 class StreamThread {
 public:
     // 初期化：必要な設定をすべて受け取る
     StreamThread(const std::string& server_ip, int server_port, int width, int height, int fps, EncodeMode mode);
     
     // 後片付け：スレッドが確実に止まることを保証する
     ~StreamThread();
 
     // コピー禁止のおまじない（スレッドやハードウェアを扱うクラスの鉄則）
     StreamThread(const StreamThread&) = delete;
     StreamThread& operator=(const StreamThread&) = delete;
 
     // 外部から操作するためのシンプルなスイッチ
     void start();
     void stop();
 
 private:
     // 設定の保管庫
     std::string server_ip_;
     int server_port_;
     int width_;
     int height_;
     int fps_;
     EncodeMode mode_;
 
     // スレッド（裏方）と安全装置
     std::thread worker_;
     std::atomic<bool> stop_flag_{false};
 
     // 実際に裏で回り続けるメインループ
     void thread_loop();
 };
 
 #endif