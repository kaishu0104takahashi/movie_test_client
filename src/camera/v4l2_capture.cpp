/**
 * @file v4l2_capture.cpp
 * @brief V4L2ドライバを使ってカメラデバイスを直接操作する実装
 * @author sawada
 * @date 2026-01-24
 */

 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/ioctl.h>
 #include <sys/mman.h>
 #include <poll.h>
 #include <linux/videodev2.h>
 
 #include <cstdint>
 #include <string>
 #include <cstring>
 #include <cerrno>
 #include <system_error>
 #include <climits>
 
 #include "camera/v4l2_capture.hpp"
 #include "logger/logger.hpp"
 
 // プロトタイプ宣言
 static int xioctl(int fd, unsigned long req, void* arg);
 static int xpoll(struct pollfd* poll_fds, nfds_t nfds, int timeout);
 
 /**
  * @brief ioctl（OSへの命令）の安全なラッパー関数
  * @details OSの割り込み（EINTR）によって命令が中断された場合、成功するまで自動的に再試行します。
  */
 static int xioctl(int fd, unsigned long req, void* arg)
 {
     int ret;
     do {
         ret = ioctl(fd, req, arg);
     } while(ret == -1 && errno == EINTR);
     return ret;
 }
 
 /**
  * @brief poll（データ到着待ち）の安全なラッパー関数
  * @details ioctl同様、OSの割り込みで待機が中断された場合に自動再試行します。
  */
 static int xpoll(struct pollfd* poll_fds, nfds_t nfds, int timeout)
 {
     int ret;
     do {
         ret = poll(poll_fds, nfds, timeout);
     } while (ret == -1 && errno == EINTR);
     return ret;
 }
 
 // =====================================================================
 // コンストラクタ（初期化）
 // =====================================================================
 V4L2Capture::V4L2Capture(std::string device_file_name, std::uint16_t width, std::uint16_t height, frame_format fmt)
     : device_file_name_(std::move(device_file_name))
     , width_(width)
     , height_(height)
     , fmt_(fmt)
     , device_fd_(open_device())  // デバイスファイル（/dev/video0等）を開く
 {
     try {
         // カメラに対して解像度とフォーマットを指示
         set_frame_format();
     }
     catch(const std::system_error& e) {
         throw;
     }
 
     spdlog::info("initialized: {} ({}x{})", device_file_name_, width_, height_);
 
     try {
         // 映像を受け取るためのメモリ（バッファ）をOSに準備させる
         request_capture_buffer();
     }
     catch (const std::system_error& e) {
         cleanup_buffers();
         throw;
     }
 }
 
 // =====================================================================
 // デストラクタ（後片付け）
 // =====================================================================
 V4L2Capture::~V4L2Capture()
 {
     try {
         stream_off(); // 映像の送信を止める
     }
     catch(const std::system_error& e) {
         // 終了時のエラーは無視して進める
     }
 
     cleanup_buffers(); // OSから借りたメモリを返却する
     close_device();    // カメラを閉じる
 }
 
 // =====================================================================
 // 映像フレームの取得
 // =====================================================================
 bool V4L2Capture::capture_frame(V4L2Capture::Frame& frame)
 {
     pollfd poll_fd{};
     poll_fd.fd = device_fd_;
     poll_fd.events = POLLIN; // データが読み込み可能になるのを待つ設定
 
     // 最大1秒(1000ms)待機する
     int ret = xpoll(&poll_fd, 1, 1000);
 
     if (ret == 0) {
         // タイムアウト（新しい映像が来ていない）
         frame.valid_size = 0;
         return false;
     } else if (ret < 0) {
         throw std::system_error(errno, std::generic_category(), "poll failed");
     }
 
     v4l2_buffer buf{};
     buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     buf.memory = V4L2_MEMORY_MMAP;
 
     // OSのバッファ列から、データが書き込まれたバッファを1つ取り出す (DeQueue)
     if (xioctl(device_fd_, VIDIOC_DQBUF, &buf) < 0) {
         throw std::system_error(errno, std::generic_category(), "Failed to dequeue buffer");
     }
 
     if (buf.index >= buffers_.size()) {
         throw std::out_of_range("Buffer index out of bounds from driver");
     }
 
     // データの実体があるメモリアドレスを取得
     const std::uint8_t* start_ptr = static_cast<const std::uint8_t*>(buffers_[buf.index].start);
     size_t data_size = buf.bytesused; // 今回届いたデータのサイズ
 
     // 自分の箱（frame.data）が小さければ、届いたサイズに合わせて広げる
     if (data_size > frame.data.size()) {
         frame.data.resize(data_size);
     }
 
     // フォーマットに応じたデータの取り出し処理
     if(fmt_ == frame_format::MJPEG) {
         try {
             // MJPEGの場合、ゴミデータを取り除いて純粋なJPEGのみを取り出す
             store_clean_mjpeg(frame, start_ptr, data_size);            
         }
         catch (const std::exception& e) {
             spdlog::warn("MJPEG cleanup failed: {}", e.what());
             throw std::runtime_error("Failed to cleanup mjpeg data");
         }
     }
     else {
         // YUYVやH.264パススルーの場合は、生データをそのまま箱にコピーする
         std::memcpy(frame.data.data(), start_ptr, data_size);
         frame.valid_size = data_size;
     }
 
     // フレーム情報（幅・高さなど）を更新
     frame.width = width_;
     frame.height = height_;
     frame.bytesperline = bytesperline_;
     frame.fmt = fmt_;
 
     // 読み終わったバッファを、再びOSの「空きバッファ列」に返却する (EnQueue)
     if (xioctl(device_fd_, VIDIOC_QBUF, &buf) < 0) {
         throw std::system_error(errno, std::generic_category(), "VIDIOC_QBUF failed");
     }
 
     return true;
 }
 
 // =====================================================================
 // ストリーム制御
 // =====================================================================
 void V4L2Capture::stream_on()
 {
     v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     if (xioctl(device_fd_, VIDIOC_STREAMON, &type) < 0) {
         throw std::system_error(errno, std::generic_category(), "VIDIOC_STREAMON failed");
     }
 }
 
 void V4L2Capture::stream_off()
 {
     v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     if (xioctl(device_fd_, VIDIOC_STREAMOFF, &type) < 0) {
         throw std::system_error(errno, std::generic_category(), "Failed to stream off");
     }
 }
 
 // =====================================================================
 // フォーマットと解像度の再設定
 // =====================================================================
 void V4L2Capture::reconfigure(frame_format fmt)
 {
     constexpr int MAX_RETRY = 5;
     constexpr int DELAY_US = 5000;
 
     bool is_pass_fmt_check;
 
     // 指定されたフォーマットにカメラが対応しているか、何度かテスト(Try)する
     for (int i = 0; i < MAX_RETRY; ++i) {
         is_pass_fmt_check = try_format(width_, height_, static_cast<std::uint32_t>(fmt));
 
         if(!is_pass_fmt_check && (errno == EBUSY || errno == EAGAIN)) {
             usleep(DELAY_US); // カメラが忙しければ少し待つ
         } else {
             break;
         }
     }
 
     if(!is_pass_fmt_check) {
         throw std::runtime_error("not supprted fmt");
     }
 
     // カメラの設定を一旦リセットして再構築する
     stream_off();
     cleanup_buffers();
     fmt_ = fmt;
     set_frame_format();
     request_capture_buffer();
     stream_on();
 
     // カメラの明るさが安定するまで少し映像を捨てる
     constexpr int DROP_FRAM_NUM = 30;
     drop_frame(DROP_FRAM_NUM);
 }
 
 // =====================================================================
 // デバイスファイルの開閉
 // =====================================================================
 int V4L2Capture::open_device()
 {
     // O_NONBLOCK: 処理がブロックされない（止まらない）モードで開く
     int device_fd = ::open(device_file_name_.c_str(), O_RDWR | O_NONBLOCK);
     if (device_fd < 0) {
         throw std::system_error(errno, std::generic_category(), "Failed to open device: " + device_file_name_);
     }
     return device_fd;
 }
 
 void V4L2Capture::close_device()
 {
     if (device_fd_ < 0) {
         return;
     }
     ::close(device_fd_);
     device_fd_ = -1;
 }
 
 // =====================================================================
 // メモリマッピング（ゼロコピーで取得するための準備）
 // =====================================================================
 void V4L2Capture::request_capture_buffer()
 {
     v4l2_requestbuffers req{};
     req.count = CAPTURE_BUFFER_COUNT; // 4つのバッファを要求
     req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     req.memory = V4L2_MEMORY_MMAP; // メモリマップドI/Oを指定（最速）
 
     // OSにバッファの作成を依頼
     if (xioctl(device_fd_, VIDIOC_REQBUFS, &req) < 0) {
         throw std::system_error(errno, std::generic_category(), "VIDEOC_QUERYBUF failed");
     }
 
     buffers_.resize(req.count);
 
     // 作成されたバッファを、プログラムから直接触れるようにマッピング（mmap）する
     for (size_t i = 0; i < buffers_.size(); ++i) {
         v4l2_buffer buf{};
         buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;
         buf.index  = i;
 
         if (xioctl(device_fd_, VIDIOC_QUERYBUF, &buf) < 0) {
             throw std::system_error(errno, std::generic_category(), "VIDEOC_QUERYBUF failed");
         }
 
         buffers_[i].length = buf.length;
         // MAP_SHARED: デバイスとメモリを共有する（データコピーの手間を省く）
         buffers_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, device_fd_, buf.m.offset);
 
         if (buffers_[i].start == MAP_FAILED) {
             throw std::system_error(errno, std::generic_category(), "MMAP failed");
         }
 
         // バッファをOSの「書き込み待ち列」にセットする
         if (xioctl(device_fd_, VIDIOC_QBUF, &buf) < 0) {
             throw std::system_error(errno, std::generic_category(), "VIDIOC_QBUF failed");
         }
     }
 }
 
 // =====================================================================
 // カメラの初期設定（フォーマット指定）
 // =====================================================================
 void V4L2Capture::set_frame_format()
 {
     v4l2_format fmt{};
     fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     fmt.fmt.pix.width = width_;
     fmt.fmt.pix.height = height_;
     fmt.fmt.pix.pixelformat = static_cast<std::uint32_t>(fmt_);
     fmt.fmt.pix.field = V4L2_FIELD_NONE; // インターレース（走査線）処理なし
 
     // カメラに設定を書き込む
     if (xioctl(device_fd_, VIDIOC_S_FMT, &fmt) < 0) {
         throw std::system_error(errno, std::generic_category(), "invalid format");
     }
 
     // カメラ側が「そのサイズ無理だから近いサイズにしたよ」と修正してくる場合があるため、実際の幅・高さを保存
     if (width_ != fmt.fmt.pix.width) {
         width_ = fmt.fmt.pix.width;
     }
     if (height_ != fmt.fmt.pix.height) {
         height_ = fmt.fmt.pix.height;
     }
 
     spdlog::info("set format {}x{}", width_, height_);
 
     bytesperline_ = fmt.fmt.pix.bytesperline;
 }
 
 // =====================================================================
 // メモリマッピングの解除（掃除）
 // =====================================================================
 void V4L2Capture::cleanup_buffers()
 {
     for (auto& buf : buffers_) {
         if (buf.start && buf.start != MAP_FAILED) {
             munmap(buf.start, buf.length); // マッピングを解除
         }
     }
     buffers_.clear();
 
     // OSにバッファ数を0にして破棄するよう依頼
     v4l2_requestbuffers req{};
     req.count = 0;
     req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     req.memory = V4L2_MEMORY_MMAP;
 
     if (xioctl(device_fd_, VIDIOC_REQBUFS, &req) < 0) {
         spdlog::warn("cleanup_buffers: REQBUFS(0) failed. errno={}", errno);
     }
 }
 
 // =====================================================================
 // サポート確認用（TRY_FMT）
 // =====================================================================
 bool V4L2Capture::try_format(std::uint16_t width, std::uint16_t height, uint32_t pixfmt)
 {
     v4l2_format fmt{}; 
     fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     fmt.fmt.pix.width = width;
     fmt.fmt.pix.height = height;
     fmt.fmt.pix.pixelformat = pixfmt;
     fmt.fmt.pix.field = V4L2_FIELD_ANY; // ドライバに任せる
 
     // 実際に適用はせず「この設定イケる？」とお伺いだけ立てる
     if (xioctl(device_fd_, VIDIOC_TRY_FMT, &fmt) < 0) {
         spdlog::warn("try_format: ioctl failed. errno={}", errno);
         return false;
     }
 
     bool match_fmt = (fmt.fmt.pix.pixelformat == pixfmt);
     bool match_w   = (fmt.fmt.pix.width == width);
     bool match_h   = (fmt.fmt.pix.height == height);
 
     // カメラが修正して返してきた場合（非サポート）は false
     if (!match_fmt || !match_w || !match_h) {
         return false;
     }
 
     return true;
 }
 
 // =====================================================================
 // 白飛び対策（露出調整のためのフレーム破棄）
 // =====================================================================
 void V4L2Capture::drop_frame(int drop_frame_num)
 {
     struct v4l2_control ctrl;
 
     // 一旦、手動露出モード(MANUAL)に切り替えて、基準となる数値を設定する
     ctrl.id = V4L2_CID_EXPOSURE_AUTO;
     ctrl.value = V4L2_EXPOSURE_MANUAL; 
     xioctl(device_fd_, VIDIOC_S_CTRL, &ctrl);
 
     ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
     ctrl.value = 156;
     xioctl(device_fd_, VIDIOC_S_CTRL, &ctrl);
 
     usleep(1000 * 100); // 0.1秒待機
 
     // 再度オート露出(AUTO)に戻すことで、カメラの露出計算を強制リセットさせる
     ctrl.id = V4L2_CID_EXPOSURE_AUTO;
     ctrl.value = V4L2_EXPOSURE_AUTO;
     xioctl(device_fd_, VIDIOC_S_CTRL, &ctrl);
 
     // 露出が安定するまでの間、指定された回数だけ映像を引き抜いてそのまま捨てる
     for (int i = 0; i < drop_frame_num; ++i) {
         v4l2_buffer buf{};
         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;
 
         pollfd poll_fd;
         poll_fd.fd = device_fd_;
         poll_fd.events = POLLIN;
 
         if (xpoll(&poll_fd, 1, 1000) <= 0) break;
         if (xioctl(device_fd_, VIDIOC_DQBUF, &buf) < 0) break; // 引き抜く
         if (xioctl(device_fd_, VIDIOC_QBUF, &buf) < 0) break;  // そのままOSに返す
     }
 }
 
 // =====================================================================
 // MJPEGのパース（綺麗な画像の切り出し）
 // =====================================================================
 void V4L2Capture::store_clean_mjpeg(Frame& frame, const uint8_t *buf_ptr, size_t buf_size)
 {
     size_t start = SIZE_MAX;
     size_t end = SIZE_MAX;
 
     // JPEG画像の「始まり」の合図である SOIマーカー (Start of Image: FF D8) を探す
     for (size_t i = 0; i + 1 < buf_size; ++i) {
         if (buf_ptr[i] == 0xFF && buf_ptr[i + 1] == 0xD8) {
             start = i;
             break;
         }
     }
 
     // JPEG画像の「終わり」の合図である EOIマーカー (End of Image: FF D9) を探す
     for (size_t i = start + 1; i + 1 < buf_size; ++i) {
         if (buf_ptr[i] == 0xFF && buf_ptr[i + 1] == 0xD9) {
             end = i + 2; // D9の1バイト先までが画像データなので +2
             break;
         }
     }
 
     // マーカーが見つからない場合、そのフレームは壊れている（破棄する）
     if (start == SIZE_MAX || end == SIZE_MAX || end <= start) {
         throw std::runtime_error("Invalid MJPEG frame (no JPEG markers)");
     }
 
     size_t jpeg_size = end - start;
 
     // 必要に応じて箱（frame.data）のサイズを広げる
     if (jpeg_size > frame.data.size()) {
         frame.data.resize(jpeg_size);
     }
 
     // 発見した開始位置から、純粋なJPEGデータだけをコピーする
     std::memcpy(frame.data.data(), buf_ptr + start, jpeg_size);
     frame.valid_size = jpeg_size;
 }