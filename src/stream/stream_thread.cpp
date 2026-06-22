#include "stream/stream_thread.hpp"
#include "stream/pass_through_encoder.hpp" // 新しく作った子クラスを読み込む
#include <iostream>
#include <chrono>
#include <memory> // スマートポインタ（std::unique_ptr）を使うための道具

StreamThread::StreamThread(const std::string& server_ip, int server_port, int width, int height, int fps, EncodeMode mode)
    : server_ip_(server_ip), server_port_(server_port), width_(width), height_(height), fps_(fps), mode_(mode) {
}

StreamThread::~StreamThread() {
    stop();
}

void StreamThread::start() {
    stop_flag_.store(false);
    worker_ = std::thread(&StreamThread::thread_loop, this); 
}

void StreamThread::stop() {
    stop_flag_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void StreamThread::thread_loop() {
    try {
        std::cout << "[裏方スレッド] 部品を初期化中..." << std::endl;

        // モードに応じてカメラのフォーマットを切り替える
        V4L2Capture::frame_format cam_fmt = (mode_ == EncodeMode::Camera_PassThrough) 
                                            ? V4L2Capture::frame_format::H264 
                                            : V4L2Capture::frame_format::YUV422;

        V4L2Capture camera("/dev/video0", width_, height_, cam_fmt);
        UdpStreamer streamer(server_ip_, server_port_);

        // =======================================================
        // 継承の魔法（ポリモーフィズム）が発動する部分
        // =======================================================
        // 親クラス（IEncoder）のポインタ型として「箱」だけ用意する
        std::unique_ptr<IEncoder> encoder;

        // スイッチ（mode_）に応じて、その箱に「子クラス」の実体を突っ込む
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
        std::vector<uint8_t> h264_data;

        camera.stream_on();
        std::cout << "[裏方スレッド] >>> 配信開始！宛先: " << server_ip_ << ":" << server_port_ << " <<<" << std::endl;

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            if (camera.capture_frame(frame) && frame.valid_size > 0) {
                
                // 司令塔は中身がどっちの子クラスか全く知らない！
                // ただ「親のルール(encode_frame)を実行しろ」と命令するだけ
                if (encoder->encode_frame(frame.data, h264_data)) {
                    streamer.send_packet(h264_data);
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        camera.stream_off();
        std::cout << "[裏方スレッド] --- 配信を安全に終了しました ---" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n[裏方スレッド 致命的エラー] " << e.what() << std::endl;
    }
}