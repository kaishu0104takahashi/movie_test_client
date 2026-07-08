#!/usr/bin/env python3
import subprocess
import datetime
import sys
import os

try:
    import pygame
except ImportError:
    print("エラー: pygame がインストールされていません。'pip install pygame' を実行してください。")
    sys.exit(1)

def get_traffic_gb(interface="wwan0"):
    try:
        # 1. vnstatコマンドを裏で実行して、結果のテキストを取得
        result = subprocess.check_output(["vnstat", "-i", interface, "-m"], text=True)

        # 2. 今月の「年-月」（例: "2026-07"）の文字列を作る
        current_month = datetime.datetime.now().strftime("%Y-%m")

        # 3. 取得したテキストを1行ずつチェック
        for line in result.split("\n"):

            # 今月のデータ行を見つけたら処理開始
            if current_month in line:

                # 縦線「|」で区切ってバラバラにする
                parts = line.split("|")

                if len(parts) >= 3:
                    # 3つ目の要素（total）の空白を消して取り出す
                    total_str = parts[2].strip()
                    # 数値と単位に分ける（例: "1.34" と "MiB"）

                    value_str, unit = total_str.split()
                    value = float(value_str)

                    # 4. 単位に合わせてギガバイト（GB）に計算
                    if unit in ["KiB", "KB"]:
                        return value / (1024 * 1024)  # キロからギガへ
                    elif unit in ["MiB", "MB"]:
                        return value / 1024           # メガからギガへ
                    elif unit in ["GiB", "GB"]:
                        return value                  # ギガならそのまま
                    elif unit in ["TiB", "TB"]:
                        return value * 1024           # テラからギガへ
        return 0.0 

    except FileNotFoundError:
        return -1.0 # エラーを判定するための特別な値
    except Exception as e:
        return -2.0 # エラーを判定するための特別な値

# ==========================================
# ウィンドウ表示用関数 (pygame版 - フレームあり)
# ==========================================
def show_window_pygame(used_gb, remaining_gb, limit_gb):
    pygame.init()
    
    # ウィンドウのサイズを設定 (幅450 x 高さ250)
    width, height = 450, 250
    # pygame.NOFRAMEを削除し、標準のフレーム（タイトルバー等）を復活させました
    screen = pygame.display.set_mode((width, height))
    pygame.display.set_caption("通信量チェック")

    # フォント設定
    font_paths = [
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttf",
        "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
        "/usr/share/fonts/truetype/vlgothic/VL-Gothic-Regular.ttf"
    ]
    font_path = None
    for path in font_paths:
        if os.path.exists(path):
            font_path = path
            break
    if font_path:
        font = pygame.font.Font(font_path, 28)
        btn_font = pygame.font.Font(font_path, 20)
    else:
        font = pygame.font.SysFont("notosanscjkjp, ipagothic, sans-serif", 28)
        btn_font = pygame.font.SysFont("notosanscjkjp, ipagothic, sans-serif", 20)

    # 色の定義
    bg_color = (240, 240, 240) 
    text_color = (30, 30, 30)  
    btn_color = (220, 50, 50)       # 赤色に変更
    btn_text_color = (255, 255, 255) # 赤い背景に合わせてテキストを白色に変更

    # 表示するテキストのリストを作成
    if used_gb == -1.0:
        lines = ["エラー: vnstat がインストールされていません。"]
    elif used_gb == -2.0:
        lines = ["エラー: 通信量の取得に失敗しました。"]
    else:
        lines = [
            "📡 今月の 4GPi 総通信量:",
            f"  {used_gb:.3f} GB",
            "",
            "🔋 今月の残り通信可能量:",
            f"  {remaining_gb:.3f} GB / {limit_gb:.1f} GB"
        ]

    # 独自閉じるボタンの定義
    btn_width, btn_height = 80, 40 
    btn_x = (width - btn_width) // 2
    btn_y = height - btn_height - 20
    button_rect = pygame.Rect(btn_x, btn_y, btn_width, btn_height) 
    button_text = btn_font.render("閉じる", True, btn_text_color)
    button_text_rect = button_text.get_rect(center=button_rect.center)

    running = True

    while running:
        # イベント処理
        for event in pygame.event.get():
            # OS標準の「×」ボタンが押されたときの処理
            if event.type == pygame.QUIT:
                running = False
            # 画面内のクリック処理
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1: # 左クリック
                    # クリック位置が赤いボタンの内側ならアプリを終了
                    if button_rect.collidepoint(event.pos):
                        running = False

        # 背景の塗りつぶし
        screen.fill(bg_color)

        # テキストの描画処理
        y_offset = 30 
        for line in lines:
            if line: 
                text_surface = font.render(line, True, text_color)
                text_rect = text_surface.get_rect(center=(width // 2, y_offset))
                screen.blit(text_surface, text_rect)
            y_offset += 35 

        # 閉じるボタンの描画
        pygame.draw.rect(screen, btn_color, button_rect)
        screen.blit(button_text, button_text_rect)

        # 画面の更新
        pygame.display.flip()
        
        # CPU使用率を抑えるための待機処理
        pygame.time.wait(100)

    # ループを抜けたら終了
    pygame.quit()

# ==========================================
# メイン処理
# ==========================================
if __name__ == "__main__":
    LIMIT_GB = 50.0  # 1ヶ月の制限値
    
    # 今月の使用量（GB）を取得 (今回は eth0 を指定)
    used_gb = get_traffic_gb("eth0")
    
    # 残りの容量を計算 (エラーのマイナス値でない場合のみ計算)
    if used_gb >= 0:
        remaining_gb = LIMIT_GB - used_gb
        if remaining_gb < 0:
            remaining_gb = 0.0 # 0以下にならないよう安全策
    else:
        remaining_gb = 0.0
    
    # pygameでウィンドウを表示
    show_window_pygame(used_gb, remaining_gb, LIMIT_GB)