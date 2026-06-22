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

    // コックピット側（Server）のTailscale IPアドレス
    std::string server_ip = "100.111.56.107"; 
    int server_port = 1234;
    
    try {
        // 解像度の履歴（コメントアウト）
        //StreamThread stream(server_ip, server_port, 960, 720, 30, EncodeMode::Hardware_Pi4);
        //StreamThread stream(server_ip, server_port, 800, 600, 30, EncodeMode::Hardware_Pi4);
        
        // =================================================================================
        // ★ 変更箇所：ラズパイ5へのシステム移行（モードの切り替え）
        // =================================================================================
        // 第6引数のモード指定を EncodeMode::Hardware_Pi4 から EncodeMode::Software_Pi5 に
        // 
        // 【システム的な動作の連鎖】
        // 1. ここで Software_Pi5 を指定する。
        // 2. stream_thread.cpp 内の三項演算子により、エンコーダ名が "libx264" に確定する。
        // 3. h264_encoder.cpp の初期化処理で libx264 が起動し、すでに記述されている
        //    超低遅延パラメータ（preset=ultrafast, tune=zerolatency）が自動的に適用される。
        // =================================================================================
        /*Hardware_Pi4,Software_Pi5,Camera_PassThrough このどれかを使用。include/stream/stream_thread.hppを参照*/
        StreamThread stream(server_ip, server_port, 640, 360, 30, EncodeMode::Software_Pi5);
        
        // スレッドで配信スタート
        stream.start();

        std::cout << "(終了するには Ctrl+C を押してください)\n" << std::endl;

        // ③ メインスレッドの仕事は「終了の合図が来るまで待機する」
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