#!/bin/bash

# エラーが発生したらそこで処理を停止する
set -e

echo "=================================================="
echo " 映像伝送 Client (送信側) 依存ライブラリのインストール"
echo "=================================================="

echo "[1/4] パッケージリストを更新しています..."
sudo apt-get update

echo "[2/4] ビルドツール (CMake, g++など) をインストールしています..."
sudo apt-get install -y build-essential cmake pkg-config

echo "[3/4] FFmpeg 開発用ライブラリ (通信・エンコード用) をインストールしています..."
sudo apt-get install -y libavcodec-dev libavformat-dev libavutil-dev

echo "[4/4] カメラ制御 (V4L2) と ロギング (spdlog) のライブラリをインストールしています..."
sudo apt-get install -y libv4l-dev libspdlog-dev

echo "=================================================="
echo " すべてのインストールが完了しました！"
echo " 以下のコマンドでビルドを開始してください："
echo "   mkdir build && cd build"
echo "   cmake .."
echo "   make"
echo "=================================================="