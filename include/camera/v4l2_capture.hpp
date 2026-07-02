/**
 * @file v4l2_capture.hpp
 * @brief Linux標準のカメラ操作機能（V4L2）を使って映像を取得する部品
 * @author sawada
 * @date 2026-01-24
 */

 #ifndef V4L2_CAPTURE_HPP_
 #define V4L2_CAPTURE_HPP_
 
 #include <linux/videodev2.h>
 #include <cstdint>
 #include <atomic>
 #include <string>
 #include <vector>
 #include <stdexcept>
 
 /**
  * @class V4L2Capture
  * @brief カメラデバイス（/dev/video0 など）を直接操作し、映像を高速に引き抜くクラス
  * @note 複数のスレッドから同時に操作する（スレッドセーフ）ことは想定していないため、
  * 必ず専属のカメラ用スレッドからのみ呼び出すこと。
  */
 class V4L2Capture {
 public:
     /**
      * @enum frame_format
      * @brief カメラに要求する映像のデータ形式（V4L2の定数と紐付け）
      */
     enum class frame_format : std::uint32_t {
         YUV422 = V4L2_PIX_FMT_YUYV,   // 非圧縮の生データ（低遅延・高負荷）
         MJPEG  = V4L2_PIX_FMT_MJPEG,  // JPEG圧縮データ（USB帯域の節約用）
         H264   = V4L2_PIX_FMT_H264    // H.264圧縮データ（超低負荷なパススルー用）
     };
 
     /**
      * @struct Frame
      * @brief カメラから取得した1コマ（フレーム）の画像データを格納する箱
      */
     struct Frame {
         std::uint16_t width;          // 画像の幅
         std::uint16_t height;         // 画像の高さ
         
         size_t valid_size;            // 実際に箱に入っている有効なデータのサイズ（バイト）
 
         std::uint32_t bytesperline;   // 1行あたりのデータ量
 
         frame_format fmt;             // 取得したデータの形式（YUYV, MJPEG, H264）
 
         // 映像データを格納する可変長の配列（毎回メモリ確保すると遅いので、事前に大きく確保しておく）
         std::vector<std::uint8_t> data;
     };
 
     // コンストラクタ：カメラを開いて初期設定を行う
     V4L2Capture(std::string device_file_name, std::uint16_t width, std::uint16_t height, frame_format fmt);
 
     // デストラクタ：カメラを安全に閉じる
     ~V4L2Capture();
 
     /**
      * @brief カメラから最新の1フレームを取得して Frame の箱に入れる
      * @return true: 取得成功 / false: データがまだ来ていない、または失敗
      */
     bool capture_frame(Frame& frame);
 
     /**
      * @brief 映像のストリーミング（データ送信）をカメラに要求する
      */
     void stream_on();
 
     /**
      * @brief 映像のストリーミングを停止する
      */
     void stream_off();
 
     /**
      * @brief 動作中に解像度やフォーマットを変更する
      * @note 呼び出し中にエラーが起きた場合はカメラを再起動して復帰させる必要がある
      * capture_frame() が実行されていないタイミングでのみ呼ぶこと。
      */
     void reconfigure(frame_format fmt);
 
 private:
     // OS(カーネル)のメモリ領域をプログラム(ユーザー空間)に直接マッピングするための情報
     struct mmap_buffer {
         void* start = nullptr;  // メモリの先頭アドレス
         size_t length = 0;      // メモリのサイズ
     };
 
     std::string device_file_name_; // "/dev/video0" などのデバイス名
     std::uint16_t width_;          // 現在設定されている幅
     std::uint16_t height_;         // 現在設定されている高さ
     std::uint32_t bytesperline_;   // 1行あたりのバイト数
     frame_format fmt_;             // 現在設定されているフォーマット
     int device_fd_;                // カメラデバイスを操作するための「鍵（ファイルディスクリプタ）」
 
     // 映像をスムーズに取得するためにカメラとOS間で使い回すバッファの数
     static constexpr std::uint16_t CAPTURE_BUFFER_COUNT = 4;
     std::vector<struct mmap_buffer> buffers_;
 
     // 内部用の初期化・終了処理
     int open_device();
     void close_device();
 
     // OSにメモリマップ用のバッファを要求する
     void request_capture_buffer();
     // カメラの解像度とフォーマットを設定する
     void set_frame_format();
     // 確保したバッファ（メモリ）をOSに返却して掃除する
     void cleanup_buffers();
 
     // 指定したフォーマットや解像度を、接続されているカメラがサポートしているかテストする
     bool try_format(std::uint16_t width, std::uint16_t height, uint32_t pixfmt);
 
     /**
      * @brief 起動直後の映像を数フレーム捨てる処理
      * @details カメラ起動直後はオート露出(明るさ調整)が効かず「白飛び」しやすいため、
      * 安定するまでの数コマを意図的に読み飛ばします。
      */
     void drop_frame(int drop_frame_num);
 
     /**
      * @brief MJPEGデータから純粋なJPEG画像だけを綺麗に切り出す
      * @details カメラから送られてくるMJPEGデータにはゴミ（不要なパディング）が混ざることがあるため、
      * JPEGの「開始記号（SOI）」と「終了記号（EOI）」を探して中身だけを抽出します。
      */
     void store_clean_mjpeg(Frame& frame, const uint8_t *buf_ptr, size_t buf_size);
 };
 
 #endif