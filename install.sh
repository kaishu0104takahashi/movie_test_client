#!/bin/bash

# エラーが発生したらそこで処理を停止する
set -e

echo "=================================================="
echo " 映像伝送システム (Client/Server共通) 依存ライブラリのインストール"
echo "=================================================="

echo "[1/7] パッケージリストを更新しています..."
sudo apt-get update

echo "[2/7] ビルドツール (CMake, g++など) をインストールしています..."
sudo apt-get install -y build-essential cmake pkg-config

echo "[3/7] FFmpeg 開発用ライブラリ (通信・エンコード・デコード用) をインストールしています..."
sudo apt-get install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

echo "[4/7] カメラ制御 (V4L2) と ロギング (spdlog) のライブラリをインストールしています..."
sudo apt-get install -y libv4l-dev libspdlog-dev

echo "[5/7] SDL2 開発用ライブラリ (画面描画・テキスト合成用) をインストールしています..."
sudo apt-get install -y libsdl2-dev libsdl2-ttf-dev

echo "[6/7] ドキュメント自動生成ツール (Doxygen, Graphviz) をインストールしています..."
sudo apt-get install -y doxygen graphviz

echo "[7/7] リソース監視用ツール (bpftrace, ethtool, iw) をインストールしています..."
sudo apt-get install -y bpftrace ethtool iw

sudo apt install vnstat -y

echo "=================================================="
echo " すべてのインストールが完了しました！"
echo " 以下のコマンドでビルドを開始してください："
echo "   mkdir build && cd build"
echo "   cmake .."
echo "   make -j4"
echo "=================================================="