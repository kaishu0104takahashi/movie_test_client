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
    std::cout << "--- 映像伝送 Client 起動 (4GPi最適化・操作受信対応版) ---" << std::endl;

    // ★ログの別ターミナル表示をONにするかどうかのフラグ
    // 本番稼働時（確認が不要になったら）ここを false に変更してください。
    bool show_terminal_log = true;

    // 操作信号の受信用モジュールを起動（5005番と5678番）
    ControlReceiver ctrl_receiver(5005, 5678, show_terminal_log);

    std::string server_ip = "192.168.77.234"; 
    int server_port = 1234;
    
    try {
        /* Hardware_Pi4, Software_Pi5, Camera_PassThrough のいずれかを使用 */
        StreamThread stream(server_ip, server_port, 1920, 1080, 30, EncodeMode::Software_Pi5);
        stream.start();

        while (keep_running) {
            // 将来的には、ここで ctrl_receiver.get_current_state() を呼び出して
            // 最新の操作値を取得し、モーター制御基板（GPIOやシリアル）へ送る処理を追記します。
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        stream.stop();
    } catch (const std::exception& e) {
        std::cerr << "エラーが発生しました: " << e.what() << std::endl;
    }

    std::cout << "Client を終了します。" << std::endl;
    return 0;
}