cmake_minimum_required(VERSION 3.20)

project(movie_test_client LANGUAGES CXX)

#  【ルール設定】 C++の「2020年版の最新ルール」で作ってね、という指示
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === パッケージの検索 ===

# 1. FFmpegライブラリ群の検索 (pkg-configを使用)
#【外部業者の手配】 FFmpegやSDL2など「自分では作れない超便利な外部ツール」を探してきて！という指示
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)

# 2. その他の必須ライブラリ
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Threads REQUIRED)

# === ソースファイルの登録 ===
# ※これから作るファイルをここに追記していきます
# 【自分の材料】 今回自分が書いたコード（材料）はこれだよ、というリスト
set(SOURCE_FILES
    src/main.cpp
    src/camera/v4l2_capture.cpp
    src/stream/h264_encoder.cpp
    src/stream/pass_through_encoder.cpp
    src/stream/udp_streamer.cpp
    src/stream/stream_thread.cpp
)

# === 実行ファイルの作成 ===
#【調理開始】 上の材料を使って、「movie_test_client」という名前のアプリを作ってね！
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# === インクルードパス（ヘッダファイルの場所）の指定 ===
#【設計図の場所】 .hpp（ヘッダファイル＝設計図）は「include」フォルダに入ってるから読んでね
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# === ライブラリのリンク（合体） ===
#【合体】 最後に、②で呼んだ外部ツール（FFmpeg等）を、④で作ったアプリにガチャンと連結させて！
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    PkgConfig::FFMPEG   # ← これがFFmpegを合体させる魔法の一行
    SDL2::SDL2
    spdlog::spdlog
    fmt::fmt
    Threads::Threads
)