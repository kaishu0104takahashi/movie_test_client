#ifndef CONTROL_RECEIVER_HPP_
#define CONTROL_RECEIVER_HPP_

#include <thread>
#include <atomic>
#include <mutex>
#include <string>

// 車両側で受信・保持する操作データの構造体
struct VehicleControlState {
    float steer = 0.0f;
    float throttle = -1.0f;
    float brake = -1.0f;
    int horn = 0;
    int cam_on = 0;
};

class ControlReceiver {
public:
    // コンストラクタ（ポート番号と、別ターミナル出力のON/OFFフラグを受け取る）
    ControlReceiver(int car_port, int cam_port, bool enable_logging);
    ~ControlReceiver();

    // 最新の操作状態を取得（モーター制御スレッド等から呼ばれる用）
    VehicleControlState get_current_state();

private:
    int car_port_;
    int cam_port_;
    bool enable_logging_;

    std::atomic<bool> keep_running_{true};
    std::thread car_thread_;
    std::thread cam_thread_;
    std::thread log_thread_;
    
    VehicleControlState state_;
    std::mutex mtx_;

    void car_receive_loop();
    void cam_receive_loop();
    void logging_loop(); // ログファイルへの書き込みを行うループ
};

#endif