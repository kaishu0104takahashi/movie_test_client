#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "stream/stream_thread.hpp"

std::atomic<bool> keep_running(true);
void signal_handler(int) {
    keep_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::cout << "--- 映像伝送 Client 起動 (4GPi最適化版) ---" << std::endl;

    //std::string server_ip = "219.112.66.122";
    std::string server_ip = "192.168.77.234"; 
    int server_port = 1234;
    
    try {
        // ★ 修正箇所：Software_Pi5 を指定し、安定の 320x240 に変更
        // 1. ここで Software_Pi5 を指定する。
        // 2. stream_thread.cpp 内の三項演算子により、エンコーダ名が "libx264" に確定する。
        // 3. h264_encoder.cpp の初期化処理で libx264 が起動し、すでに記述されている
        //    超低遅延パラメータ（preset=ultrafast, tune=zerolatency）が自動的に適用される。
        // =================================================================================
        
        /*Hardware_Pi4,Software_Pi5,Camera_PassThrough このどれかを使用。include/stream/stream_thread.hppを参照*/
        StreamThread stream(server_ip, server_port, 1920, 1080, 30, EncodeMode::Camera_PassThrough); //論外
        //StreamThread stream(server_ip, server_port, 1280, 720, 30, EncodeMode::Camera_PassThrough);//かなり重い
        //StreamThread stream(server_ip, server_port, 800, 600, 30, EncodeMode::Camera_PassThrough);//かなり重い
        //StreamThread stream(server_ip, server_port, 640, 480, 30, EncodeMode::Camera_PassThrough);
        //StreamThread stream(server_ip, server_port, 320, 240, 30, EncodeMode::Camera_PassThrough);
        
        stream.start();
        std::cout << "(終了するには Ctrl+C を押してください)\n" << std::endl;

        while (keep_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\n終了シグナルを受信。スレッドを停止します..." << std::endl;
        stream.stop();
    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
