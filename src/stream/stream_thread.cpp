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

        // ★ FFmpegの空パケットを1つだけ用意し、これを無限に使い回す（究極のゼロコピー）
        AVPacket* pkt = av_packet_alloc();

        camera.stream_on();
        std::cout << "[裏方スレッド] >>> 配信開始！宛先: " << server_ip_ << ":" << server_port_ << " <<<" << std::endl;

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            if (camera.capture_frame(frame) && frame.valid_size > 0) {
                
                // 1. まず生データをエンコーダに投げ込む (send)
                if (encoder->send_frame(frame.data.data(), frame.valid_size)) {
                    
                    // 2. ★超重要：エンコーダ内に溜まったデータを「空になるまで(while)」全て取り出す！
                    while (encoder->receive_packet(pkt)) {
                        streamer.send_packet(pkt);
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        camera.stream_off();
        av_packet_free(&pkt); // 最後に箱を解体
        std::cout << "[裏方スレッド] --- 配信を安全に終了しました ---" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n[裏方スレッド 致命的エラー] " << e.what() << std::endl;
    }
}