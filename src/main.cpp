#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "stream/stream_thread.hpp"
#include "stream/control_receiver.hpp"

std::atomic<bool> keep_running(true);
void signal_handler(int) {
    keep_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::cout << "--- 映像伝送 Client 起動 (マルチスレッド完全版) ---" << std::endl;

    // 映像配信用（コックピット側）のIPアドレス
    std::string server_ip = "192.168.77.234"; 
    int server_port = 1234;

    // 車両内制御マイコンのIPアドレス
    std::string vehicle_ip = "192.168.77.99";

    // ログの別ターミナル表示をONにするかどうかのフラグ
    bool show_terminal_log = false;

    // ★修正箇所：廃止された5678番ポートを削り、5005番のみで起動する
    ControlReceiver ctrl_receiver(5005, vehicle_ip, 5005, show_terminal_log);
    
    try {
        StreamThread stream(server_ip, server_port, 1920, 1080, 30, EncodeMode::Camera_PassThrough);
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