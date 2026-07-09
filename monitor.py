import sys
import os
import subprocess
import time
import csv
import statistics
import fcntl
import re
from datetime import datetime

# ==========================================
# ⚙️ 設定エリア
# ==========================================
TARGET_PROCESS = "movie_test_client" # 監視したいプログラム名
LOG_DIR = "./log"                        # ログファイルの保存先

# --- 回線品質テスト設定 (パッシブ計測) ---
PHYSICAL_IFACE = "eth0"              # 物理インターフェース (有線ならeth0, Wi-Fiならwlan0)
TAILSCALE_TARGET_IP = "100.67.192.60"  # 相手のTailscale IP (空欄 "" にするとスキップ)
# ==========================================

# --------------------------------------------------
# [1] ログ解析サマリー (Analysis Mode)
# --------------------------------------------------
if len(sys.argv) > 1:
    log_file = sys.argv[1]
    if not os.path.exists(log_file):
        print(f"エラー: ファイル '{log_file}' が見つかりません。")
        sys.exit(1)

    cpu_list, mem_mb_list, mem_pct_list, rx_speed_list, tx_speed_list = [], [], [], [], []
    start_time, end_time = None, None
    total_rx_mb, total_tx_mb = 0.0, 0.0
    proc_name = TARGET_PROCESS

    with open(log_file, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not start_time:
                start_time = row['Timestamp']
            end_time = row['Timestamp']
            proc_name = row['Process_Name']
            
            cpu_list.append(float(row['CPU_Percent']))
            mem_mb_list.append(float(row['Mem_RSS_MB']))
            mem_pct_list.append(float(row['Mem_Percent']))
            rx_speed_list.append(float(row['RX_Speed_KBps']))
            tx_speed_list.append(float(row['TX_Speed_KBps']))
            total_rx_mb = float(row['Total_RX_MB'])
            total_tx_mb = float(row['Total_TX_MB'])

    if not cpu_list:
        print("データがありません。")
        sys.exit(0)

    total_mb = total_rx_mb + total_tx_mb
    total_gb = total_mb / 1024.0

    print("==================================================")
    print("📊 ログ解析サマリー (Analysis Mode)")
    print("==================================================")
    print(f"▶ 対象ファイル: {log_file}")
    print(f"▶ 監視対象    : {proc_name}")
    print(f"▶ 記録期間    : {start_time} ～ {end_time}")
    print("--------------------------------------------------")
    print("📈 リソース使用状況")
    print(f"   CPU使用率  : 平均 {statistics.mean(cpu_list):.1f} % / 最大 {max(cpu_list):.1f} %")
    print(f"   メモリ使用 : 平均 {statistics.mean(mem_mb_list):.1f} MB ({statistics.mean(mem_pct_list):.1f} %) / 最大 {max(mem_mb_list):.1f} MB")
    print("--------------------------------------------------")
    print("📡 通信パフォーマンス")
    print(f"   平均送信速度 : {statistics.mean(tx_speed_list)/1024:.2f} MB/s")
    print(f"   平均受信速度 : {statistics.mean(rx_speed_list)/1024:.2f} MB/s")
    print("--------------------------------------------------")
    print("📱 総データ消費量 (送信＋受信)")
    print(f"   合計 : {total_gb:.3f} GB ( {total_mb:.1f} MB )")
    print(f"      ┣ 送信累計: {total_tx_mb/1024:.3f} GB")
    print(f"      ┗ 受信累計: {total_rx_mb/1024:.3f} GB")
    print("==================================================")
    sys.exit(0)


# --------------------------------------------------
# [2] リアルタイム監視 (Monitor Mode)
# --------------------------------------------------
if os.geteuid() != 0:
    print("エラー: ネットワーク監視にはroot権限が必要です。'sudo python3 monitor.py' で実行してください。")
    sys.exit(1)

# --- ネットワーク回線品質の事前チェック ---
link_speed = "不明"
ts_status = "未テスト"

print("🔌 回線品質をチェック中...")

# 1. 物理リンク速度の取得
try:
    if "wlan" in PHYSICAL_IFACE:
        iw_out = subprocess.check_output(["iw", PHYSICAL_IFACE, "link"], universal_newlines=True, stderr=subprocess.DEVNULL)
        match = re.search(r'tx bitrate:\s+(.+)', iw_out)
        if match: 
            link_speed = match.group(1).strip() + " (Wi-Fi)"
    else:
        eth_out = subprocess.check_output(["ethtool", PHYSICAL_IFACE], universal_newlines=True, stderr=subprocess.DEVNULL)
        match = re.search(r'Speed:\s+(.+)', eth_out)
        if match: 
            link_speed = match.group(1).strip() + " (有線)"
except Exception:
    link_speed = "取得エラー (ethtool/iw未インストール等)"

# 2. Tailscale接続品質 (ピアツーピア確立確認)
if TAILSCALE_TARGET_IP and TAILSCALE_TARGET_IP != "192.168.x.x":
    try:
        ts_out = subprocess.check_output(
            ["tailscale", "ping", "-c", "1", "-timeout", "2s", TAILSCALE_TARGET_IP], 
            universal_newlines=True, stderr=subprocess.STDOUT
        )
        if "direct" in ts_out:
            ts_status = "🟢 Direct (高速ピアツーピア接続)"
        elif "via DERP" in ts_out:
            ts_status = "🔴 DERP経由 (低速中継サーバー迂回)"
        else:
            ts_status = "判定不能"
    except subprocess.CalledProcessError:
        ts_status = "ping失敗 (相手がオフライン)"
    except FileNotFoundError:
        ts_status = "Tailscale未インストール"
else:
    ts_status = "IP未設定 (スキップ)"

print("✅ チェック完了")
time.sleep(1)

# --- bpftrace のセットアップ ---
bpftrace_script = """
kretprobe:tcp_sendmsg /retval > 0/ { printf("%d TX %d\\n", pid, retval); }
kretprobe:tcp_recvmsg /retval > 0/ { printf("%d RX %d\\n", pid, retval); }
kretprobe:udp_sendmsg /retval > 0/ { printf("%d TX %d\\n", pid, retval); }
kretprobe:udp_recvmsg /retval > 0/ { printf("%d RX %d\\n", pid, retval); }
"""

print("カーネルプローブ(bpftrace)をアタッチ中... (数秒お待ちください)")
bpftrace_process = subprocess.Popen(
    ["bpftrace", "-e", bpftrace_script], 
    stdout=subprocess.PIPE, 
    stderr=subprocess.DEVNULL, 
    universal_newlines=True
)

fd = bpftrace_process.stdout.fileno()
fl = fcntl.fcntl(fd, fcntl.F_GETFL)
fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
time.sleep(3) # 起動待ち

# --- ログファイルの準備 ---
time_str = datetime.now().strftime("%m%d_%H_%M_%S")
log_filename = os.path.join(LOG_DIR, f"{time_str}.csv")
file_exists = os.path.isfile(log_filename)

csv_file = open(log_filename, 'a', newline='', encoding='utf-8')
writer = csv.writer(csv_file)
if not file_exists:
    writer.writerow(["Timestamp", "Process_Name", "Status", "Mem_RSS_MB", "Mem_Percent", "CPU_Percent", "RX_Speed_KBps", "TX_Speed_KBps", "Total_RX_MB", "Total_TX_MB"])

total_tx_bytes = 0
total_rx_bytes = 0
buffer = ""

try:
    while True:
        time.sleep(1.0)
        
        # 1. pgrep でプロセスIDを検索
        active_pids = []
        try:
            pgrep_out = subprocess.check_output(["pgrep", "-f", TARGET_PROCESS], universal_newlines=True)
            for p in pgrep_out.strip().split('\n'):
                if p:
                    pid = int(p)
                    if pid != os.getpid():
                        active_pids.append(pid)
        except subprocess.CalledProcessError:
            pass
            
        is_process_running = len(active_pids) > 0
        cpu_percent = 0.0
        mem_rss_mb = 0.0
        mem_percent = 0.0
        current_tx_bytes = 0
        current_rx_bytes = 0

        # 2. CPU / メモリ使用量の取得
        if is_process_running:
            pid_str = ",".join(map(str, active_pids))
            try:
                ps_out = subprocess.check_output(
                    f"ps -p {pid_str} -o %cpu,rss,%mem --no-headers", 
                    shell=True, universal_newlines=True, stderr=subprocess.DEVNULL
                )
                for line in ps_out.strip().split('\n'):
                    parts = line.split()
                    if len(parts) == 3:
                        cpu_percent += float(parts[0])
                        mem_rss_mb += float(parts[1]) / 1024.0
                        mem_percent += float(parts[2])
            except subprocess.CalledProcessError:
                pass

        # 3. bpftrace からの通信量読み取り
        try:
            output = bpftrace_process.stdout.read()
            if output:
                buffer += output
                lines = buffer.split('\n')
                buffer = lines.pop()
                
                for line in lines:
                    parts = line.split()
                    if len(parts) == 3:
                        try:
                            p_pid = int(parts[0])
                            direction = parts[1]
                            b_size = int(parts[2])
                            
                            if p_pid in active_pids:
                                if direction == "TX":
                                    current_tx_bytes += b_size
                                    total_tx_bytes += b_size
                                elif direction == "RX":
                                    current_rx_bytes += b_size
                                    total_rx_bytes += b_size
                        except ValueError:
                            pass
        except Exception:
            pass

        # スピードと累計の計算
        current_tx_kbps = current_tx_bytes / 1024.0
        current_rx_kbps = current_rx_bytes / 1024.0
        total_tx_mb = total_tx_bytes / (1024.0 * 1024.0)
        total_rx_mb = total_rx_bytes / (1024.0 * 1024.0)
        total_mb = total_rx_mb + total_tx_mb
        total_gb = total_mb / 1024.0

        # CSVへの書き込み
        status_log = "Running" if is_process_running else "Waiting"
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        writer.writerow([timestamp, TARGET_PROCESS, status_log, f"{mem_rss_mb:.1f}", f"{mem_percent:.1f}", f"{cpu_percent:.1f}", 
                         f"{current_rx_kbps:.1f}", f"{current_tx_kbps:.1f}", 
                         f"{total_rx_mb:.3f}", f"{total_tx_mb:.3f}"])
        csv_file.flush()

        # 4. 画面の更新
        sys.stdout.write("\033[H\033[J")
        
        gauge_max_gb = 1.0
        gauge_blocks = int((total_gb / gauge_max_gb) * 20) if total_gb < gauge_max_gb else 20
        gauge_str = "|" * gauge_blocks + " " * (20 - gauge_blocks)

        status_text = "🟢 プロセス実行中" if is_process_running else "🔴 プロセスが開始されていません (待機中)"

        print("==================================================")
        print(f"📡 映像転送リソースモニター (bpftrace Backend)")
        print("==================================================")
        print(f"▶ 監視対象    : {TARGET_PROCESS}")
        print(f"▶ 状態        : {status_text}")
        print("--------------------------------------------------")
        print(f"▶ 物理リンク  : {link_speed}")
        print(f"▶ TS接続品質  : {ts_status}")
        print("--------------------------------------------------")
        print(f"▶ メモリ使用  : {mem_rss_mb:.1f} MB ( {mem_percent:.1f} % )")
        print(f"▶ CPU使用率   : {cpu_percent:.1f} %")
        print("--------------------------------------------------")
        print(f"⬆️ 現在の送信 : {current_tx_kbps / 1024.0:.2f} MB/s")
        print(f"⬇️ 現在の受信 : {current_rx_kbps / 1024.0:.2f} MB/s")
        print("--------------------------------------------------")
        print("📱 総データ消費量 (送信＋受信)")
        print(f"   [{gauge_str}]  {total_gb:.3f} GB ( {total_mb:.1f} MB )")
        print("==================================================")
        print(f"※ ログは '{log_filename}' に記録中... (Ctrl+Cで終了)")

except KeyboardInterrupt:
    print("\n監視を終了します。")
finally:
    bpftrace_process.terminate()
    csv_file.close()