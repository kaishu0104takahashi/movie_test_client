#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess
import datetime
import socket

def get_traffic_gb(interface="eth0"):
    try:
        result = subprocess.check_output(["vnstat", "-i", interface, "-m"], text=True)
        current_month = datetime.datetime.now().strftime("%Y-%m")
        for line in result.split("\n"):
            if current_month in line:
                parts = line.split("|")
                if len(parts) >= 3:
                    total_str = parts[2].strip()
                    value_str, unit = total_str.split()
                    value = float(value_str)
                    if unit in ["KiB", "KB"]: return value / (1024 * 1024)
                    elif unit in ["MiB", "MB"]: return value / 1024
                    elif unit in ["GiB", "GB"]: return value
                    elif unit in ["TiB", "TB"]: return value * 1024
        return 0.0 
    except Exception:
        return -1.0

def main():
    HOST = '0.0.0.0'
    PORT = 50060
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(1)
        while True:
            conn, addr = s.accept()
            with conn:
                val = get_traffic_gb("eth0")
                conn.sendall(str(val).encode('utf-8'))

if __name__ == "__main__":
    main()