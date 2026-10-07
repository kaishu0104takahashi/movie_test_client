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

    // 映像配信用 兼 距離センサ通知先（コックピット側）のIPアドレス
    //実車用削除禁止
    std::string server_ip = "219.112.66.122";
    //教室用削除禁止
    //std::string server_ip = "192.168.77.234"; 
    int server_port = 1234;

    // 車両内制御マイコンのIPアドレス
    //実車用
    //std::string vehicle_ip = "192.168.1.18";
    
    //教室用 削除禁止
    std::string vehicle_ip = "192.168.77.99";

    // ログの別ターミナル表示をONにするかどうかのフラグ
    bool show_terminal_log = false;

    // 操作信号と距離センサの受信用モジュールを起動
    ControlReceiver ctrl_receiver(5005, vehicle_ip, 5005, 
                                  3000, server_ip, 3000, 
                                  show_terminal_log);
    
    try {
        StreamThread stream(server_ip, server_port, 1920, 1080, 30, EncodeMode::Camera_PassThrough);
        stream.start();
        std::cout << "(終了するには Ctrl+C を押してください)\n" << std::endl;

        // ★★★ ここが修正ポイント ★★★
        while (keep_running) {
            // ControlReceiver から最新の操作状態（カメラフラグ含む）を取得
            VehicleControlState state = ctrl_receiver.get_current_state();
            
            // カメラフラグが 1 なら映像配信をON、0ならOFFにする
            stream.set_active(state.cam_on == 1);
            
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