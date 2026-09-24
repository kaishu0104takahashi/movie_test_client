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
    int cruise_set = 0;
    int cam_on = 0;
    int target_speed = 0;
    int distance_alert = 0; // 追加: 距離センサのフラグ保持用
};

class ControlReceiver {
public:
    // コックピット操作の中継と、距離センサの中継の両方を処理できるよう引数を追加
    ControlReceiver(int local_car_port, const std::string& vehicle_ip, int target_car_port,
                    int local_dist_port, const std::string& server_ip, int target_dist_port,
                    bool enable_logging);
    ~ControlReceiver();

    // 最新の操作状態を取得（ログ出力用）
    VehicleControlState get_current_state();

private:
    int local_car_port_;
    std::string vehicle_ip_;
    int target_car_port_;
    
    int local_dist_port_;
    std::string server_ip_;
    int target_dist_port_;
    
    bool enable_logging_;

    std::atomic<bool> keep_running_{true};
    std::thread car_thread_;
    std::thread dist_thread_; // 追加: 距離センサ中継用スレッド
    std::thread log_thread_;
    
    VehicleControlState state_;
    std::mutex mtx_;

    void car_receive_loop();
    void dist_receive_loop(); // 追加: 距離センサ中継用ループ
    void logging_loop();
};

#endif