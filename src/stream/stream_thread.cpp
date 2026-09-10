#include "stream/stream_thread.hpp"
#include "stream/pass_through_encoder.hpp"
#include <iostream>
#include <chrono>
#include <memory>

StreamThread::StreamThread(const std::string& server_ip, int server_port, int width, int height, int fps, EncodeMode mode)
    : server_ip_(server_ip), server_port_(server_port), width_(width), height_(height), fps_(fps), mode_(mode) {
}

StreamThread::~StreamThread() { stop(); }

void StreamThread::start() {
    stop_flag_.store(false);
    worker_ = std::thread(&StreamThread::thread_loop, this); 
}

void StreamThread::stop() {
    stop_flag_.store(true);
    if (worker_.joinable()) worker_.join();
}

// ★追加: 外部からフラグを更新するための関数
void StreamThread::set_active(bool active) {
    active_.store(active, std::memory_order_relaxed);
}

void StreamThread::thread_loop() {
    try {
        std::cout << "[裏方スレッド] 部品を初期化中..." << std::endl;

        V4L2Capture::frame_format cam_fmt = (mode_ == EncodeMode::Camera_PassThrough) 
                                            ? V4L2Capture::frame_format::H264 
                                            : V4L2Capture::frame_format::YUV422;

        V4L2Capture camera("/dev/video0", width_, height_, cam_fmt);
        UdpStreamer streamer(server_ip_, server_port_);

        std::unique_ptr<IEncoder> encoder;
        if (mode_ == EncodeMode::Camera_PassThrough) {
            std::cout << "[裏方] パススルーエンコーダをセット！" << std::endl;
            encoder = std::make_unique<PassThroughEncoder>();
        } else {
            std::string enc_name = (mode_ == EncodeMode::Hardware_Pi4) ? "h264_v4l2m2m" : "libx264";
            std::cout << "[裏方] FFmpegエンコーダ（" << enc_name << "）をセット！" << std::endl;
            encoder = std::make_unique<H264Encoder>(width_, height_, fps_, enc_name);
        }

        V4L2Capture::Frame frame;
        frame.data.resize(width_ * height_ * 2);

        AVPacket* pkt = av_packet_alloc();

        bool current_active = false;
        std::cout << "[裏方スレッド] >>> 配信準備完了！宛先: " << server_ip_ << ":" << server_port_ << " (カメラON待機中) <<<" << std::endl;

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            // 現在設定されているべきカメラ状態を取得
            bool target_active = active_.load(std::memory_order_relaxed);

            // 状態が切り替わった瞬間のみ、カメラの起動/停止を実行する
            if (target_active != current_active) {
                if (target_active) {
                    camera.stream_on();
                    std::cout << "[裏方スレッド] カメラ配信を [開始] しました！" << std::endl;
                } else {
                    camera.stream_off();
                    std::cout << "[裏方スレッド] カメラ配信を [停止] しました。" << std::endl;
                }
                current_active = target_active;
            }

            // カメラがONの時だけキャプチャと送信を行う
            if (current_active) {
                if (camera.capture_frame(frame) && frame.valid_size > 0) {
                    if (encoder->send_frame(frame.data.data(), frame.valid_size)) {
                        while (encoder->receive_packet(pkt)) {
                            streamer.send_packet(pkt);
                        }
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } 
            // カメラがOFFの時は負荷を極限まで下げるために長めに休む
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        // プログラム終了時、もしカメラがONなら安全にOFFにする
        if (current_active) {
            camera.stream_off();
        }
        
        av_packet_free(&pkt); 
        std::cout << "[裏方スレッド] --- 配信スレッドを安全に終了しました ---" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n[裏方スレッド 致命的エラー] " << e.what() << std::endl;
    }
}