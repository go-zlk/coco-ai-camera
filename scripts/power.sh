#!/usr/bin/env bash
# 功率模式切换 + 功耗/内存采样
# 用法：
#   ./scripts/power.sh 0     # 切到 MAXN（最高性能）
#   ./scripts/power.sh 1     # 切到中档
#   ./scripts/power.sh 2     # 切到低功耗
#   ./scripts/power.sh watch # 每秒采样功耗/温度/内存（边跑 infer.py 边开另一个终端）
set -euo pipefail

MODE="${1:-0}"

case "$MODE" in
  watch)
    echo "每秒采样（Ctrl-C 停止）。推理全程跑着 infer.py，看这里记峰值/均值。"
    tegrastats --interval 1000
    ;;
  *)
    echo "==> 切功率模式 ${MODE} + jetson_clocks 锁频"
    sudo nvpmodel -m "$MODE"
    sudo jetson_clocks
    nvpmodel -q
    echo "==> 当前功率模式已切换。可用 jtop 或 'tegrastats' 看实时功耗。"
    ;;
esac
