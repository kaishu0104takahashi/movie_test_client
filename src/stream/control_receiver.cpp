#include "stream/control_receiver.hpp"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdlib>

ControlReceiver::ControlReceiver(int local_car_port, const std::string& vehicle_ip, int target_car_port,
                                 int local_dist_port, const std::string& server_ip, int target_dist_port,
                                 bool enable_logging)
    : local_car_port_(local_car_port), vehicle_ip_(vehicle_ip), target_car_port_(target_car_port),
      local_dist_port_(local_dist_port), server_ip_(server_ip), target_dist_port_(target_dist_port),
      enable_logging_(enable_logging) {
    
    // ターミナルの自動起動（ロギング有効時のみ）
    if (enable_logging_) {
        std::ofstream ofs("control_log.txt", std::ios::trunc);
        ofs << "--- Control Data Log Started ---\n";
        ofs.close();

        int ret = std::system("lxterminal -e 'tail -f control_log.txt' &");
        (void)ret; 
    }

    // スレッドの起動
    car_thread_ = std::thread(&ControlReceiver::car_receive_loop, this);
    dist_thread_ = std::thread(&ControlReceiver::dist_receive_loop, this); // 距離センサ中継用スレッド起動
    
    if (enable_logging_) {
        log_thread_ = std::thread(&ControlReceiver::logging_loop, this);
    }
}

ControlReceiver::~ControlReceiver() {
    keep_running_ = false;
    if (car_thread_.joinable()) car_thread_.join();
    if (dist_thread_.joinable()) dist_thread_.join();
    if (log_thread_.joinable()) log_thread_.join();
}

VehicleControlState ControlReceiver::get_current_state() {
    std::lock_guard<std::mutex> lock(mtx_);
    return state_;
}

void ControlReceiver::car_receive_loop() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{}, target_addr{};
    
    // 受信用の設定
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_car_port_);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

    struct timeval tv = {0, 100000}; // 100ms timeout
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // 転送先（マイコン）の設定
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_car_port_);
    inet_pton(AF_INET, vehicle_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[8];
    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 8) {
            // 受信した「8バイトの統合データ」を変形させずにそのままマイコンへ転送（リレー）
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));

            // ログ確認用に内部で数値を保持
            {
                std::lock_guard<std::mutex> lock(mtx_);
                state_.steer = (buf[0] - 126.0f) / 126.0f;
                state_.throttle = (buf[1] - 126.0f) / 126.0f;
                state_.brake = (buf[2] - 126.0f) / 126.0f;
                state_.horn = buf[3];
                state_.cruise_set = buf[4];
                state_.cam_on = buf[5];
                state_.target_speed = buf[6];
            }
        }
    }
    close(sock);
}

void ControlReceiver::dist_receive_loop() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{}, target_addr{};
    
    // 距離フラグ受信用（ポート3000）の設定
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_dist_port_);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

    struct timeval tv = {0, 100000}; // 100ms timeout
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // 転送先（ras5-server）の設定
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_dist_port_);
    inet_pton(AF_INET, server_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[1];
    while (keep_running_) {
        // 車体から1バイトの距離フラグを受信
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 1) {
            // 受信したフラグ（1または0）をそのまま ras5-server へUDP送信
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));

            // ログ確認用に内部状態を更新
            {
                std::lock_guard<std::mutex> lock(mtx_);
                state_.distance_alert = buf[0];
            }
        }
    }
    close(sock);
}

void ControlReceiver::logging_loop() {
    while (keep_running_) {
        VehicleControlState current;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            current = state_;
        }

        std::ofstream ofs("control_log.txt", std::ios::app);
        if (ofs.is_open()) {
            ofs << "STR: " << current.steer 
                << " | THR: " << current.throttle 
                << " | BRK: " << current.brake 
                << " | HRN: " << current.horn 
                << " | CRS: " << (current.cruise_set ? "ON" : "OFF") 
                << " | SPD: " << current.target_speed << "km/h"
                << " | CAM: " << (current.cam_on ? "ON" : "OFF") 
                << " | DIST: " << current.distance_alert
                << "\n";
            ofs.close();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}