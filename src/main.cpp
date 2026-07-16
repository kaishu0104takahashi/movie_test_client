#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "stream/stream_thread.hpp"
#include "stream/control_receiver.hpp" // ★ 追加

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

    // ★追加箇所：車両内制御マイコンのIPアドレス
    std::string vehicle_ip = "192.168.77.99";

    // ログの別ターミナル表示をONにするかどうかのフラグ
    bool show_terminal_log = false;

    // ★追加箇所：操作信号の受信用モジュールを起動し、指定IPへ転送させる
    // (自機の5005,5678番で受信し、vehicle_ipの5005,5678番へ中継する)
    ControlReceiver ctrl_receiver(5005, 5678, vehicle_ip, 5005, 5678, show_terminal_log);
    
    try {
        StreamThread stream(server_ip, server_port, 1920, 1080, 30, EncodeMode::Software_Pi5);
        
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