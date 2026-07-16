#ifndef CONTROL_RECEIVER_HPP_
#define CONTROL_RECEIVER_HPP_

#include <thread>
#include <atomic>
#include <mutex>
#include <string>

// ターミナルでのログ確認用に保持する操作データの構造体
struct VehicleControlState {
    float steer = 0.0f;
    float throttle = -1.0f;
    float brake = -1.0f;
    int horn = 0;
    int cam_on = 0;
};

class ControlReceiver {
public:
    // コンストラクタ（自機の受信ポート、転送先IP、転送先ポート、ログ出力フラグを指定）
    ControlReceiver(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port, bool enable_logging);
    ~ControlReceiver();

    // 最新の操作状態を取得（ログ出力用）
    VehicleControlState get_current_state();

private:
    int local_car_port_;
    int local_cam_port_;
    std::string target_ip_;
    int target_car_port_;
    int target_cam_port_;
    bool enable_logging_;

    std::atomic<bool> keep_running_{true};
    std::thread car_thread_;
    std::thread cam_thread_;
    std::thread log_thread_;
    
    VehicleControlState state_;
    std::mutex mtx_;

    void car_receive_loop();
    void cam_receive_loop();
    void logging_loop();
};

#endif