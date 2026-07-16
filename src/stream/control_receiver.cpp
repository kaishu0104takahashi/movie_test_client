#include "stream/control_receiver.hpp"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdlib>

ControlReceiver::ControlReceiver(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port, bool enable_logging)
    : local_car_port_(local_car_port), local_cam_port_(local_cam_port), target_ip_(target_ip), target_car_port_(target_car_port), target_cam_port_(target_cam_port), enable_logging_(enable_logging) {
    
    // ターミナルの自動起動（ロギング有効時のみ）
    if (enable_logging_) {
        std::ofstream ofs("control_log.txt", std::ios::trunc);
        ofs << "--- Control Data Log Started ---\n";
        ofs.close();

        int ret = std::system("lxterminal -e 'tail -f control_log.txt' &");
        (void)ret; // 戻り値の未使用警告を回避
    }

    // スレッドの起動
    car_thread_ = std::thread(&ControlReceiver::car_receive_loop, this);
    cam_thread_ = std::thread(&ControlReceiver::cam_receive_loop, this);
    
    if (enable_logging_) {
        log_thread_ = std::thread(&ControlReceiver::logging_loop, this);
    }
}

ControlReceiver::~ControlReceiver() {
    keep_running_ = false;
    if (car_thread_.joinable()) car_thread_.join();
    if (cam_thread_.joinable()) cam_thread_.join();
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
    inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[8];
    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len > 0) {
            // ★ 受信した「生のバイトデータ」を変形させずにそのままマイコンへ転送（リレー）
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));

            // ログ確認用に内部で数値を計算して保持（※マイコンへ送るデータには影響しません）
            if (len >= 4) {
                std::lock_guard<std::mutex> lock(mtx_);
                state_.steer = (buf[0] - 126.0f) / 126.0f;
                state_.throttle = (buf[1] - 126.0f) / 126.0f;
                state_.brake = (buf[2] - 126.0f) / 126.0f;
                state_.horn = buf[3];
            }
        }
    }
    close(sock);
}

void ControlReceiver::cam_receive_loop() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{}, target_addr{};
    
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_cam_port_);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

    struct timeval tv = {0, 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_cam_port_);
    inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[8];
    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len > 0) {
            // ★ カメラON/OFFデータも同様に転送
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));

            if (len >= 1) {
                std::lock_guard<std::mutex> lock(mtx_);
                state_.cam_on = buf[0];
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
                << " | CAM: " << (current.cam_on ? "ON" : "OFF") 
                << "\n";
            ofs.close();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}