/**
 * @file stream_thread.hpp
 * @brief 映像取得・エンコード・送信のパイプラインを別スレッドで管理する司令塔
 */
#ifndef STREAM_THREAD_HPP_
#define STREAM_THREAD_HPP_

#include <string>
#include <thread>
#include <atomic>

#include "camera/v4l2_capture.hpp"
#include "stream/h264_encoder.hpp"
#include "stream/udp_streamer.hpp"

enum class EncodeMode {
    Hardware_Pi4,
    Software_Pi5,
    Camera_PassThrough
};

class StreamThread {
public:
    StreamThread(const std::string& server_ip, int server_port, int width, int height, int fps, EncodeMode mode);
    ~StreamThread();

    StreamThread(const StreamThread&) = delete;
    StreamThread& operator=(const StreamThread&) = delete;

    void start();
    void stop();

private:
    std::string server_ip_;
    int server_port_;
    int width_;
    int height_;
    int fps_;
    EncodeMode mode_;

    std::thread worker_;
    std::atomic<bool> stop_flag_{false};

    void thread_loop();
};

#endif