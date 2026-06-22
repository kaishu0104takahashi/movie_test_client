#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "stream/stream_thread.hpp"

// メインスレッド用（プログラム全体）の安全装置
std::atomic<bool> keep_running(true);
void signal_handler(int) {
    keep_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::cout << "--- 映像伝送 Client 起動 (マルチスレッド完全版) ---" << std::endl;

    // IP
    std::string server_ip = "100.111.56.107"; 
    int server_port = 1234;
    
    try {
        //IPの変更
        //StreamThread stream(server_ip, server_port, 960, 720, 30, EncodeMode::Hardware_Pi4);
        //StreamThread stream(server_ip, server_port, 800, 600, 30, EncodeMode::Hardware_Pi4);
        StreamThread stream(server_ip, server_port, 640, 360, 30, EncodeMode::Hardware_Pi4);
        
        // スレッドで配信スタート
        stream.start();

        std::cout << "(終了するには Ctrl+C を押してください)\n" << std::endl;

        // ③ メインスレッドの仕事は「終了の合図が来るまで待機する」だけ
        while (keep_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\n終了シグナルを受信。スレッドを停止します..." << std::endl;
        
        // ④ スレッドを安全に止める
        stream.stop();

    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}