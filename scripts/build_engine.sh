#!/usr/bin/env bash
# 导出 ONNX → trtexec 构建 TensorRT engine（FP16/INT8），并输出精细 GPU 三段延迟
# 用法：./scripts/build_engine.sh yolov8n
set -euo pipefail

MODEL="${1:-yolov8n}"
IMGSZ="${2:-640}"

echo "==> 1/4 导出 ONNX (${MODEL}.pt → ${MODEL}.onnx)"
yolo export model="${MODEL}.pt" format=onnx imgsz="${IMGSZ}"

echo "==> 2/4 构建 FP16 engine"
trtexec \
  --onnx="${MODEL}.onnx" \
  --saveEngine="${MODEL}_fp16.engine" \
  --fp16 \
  --minShapes=images:1x3x${IMGSZ}x${IMGSZ} \
  --optShapes=images:1x3x${IMGSZ}x${IMGSZ} \
  --maxShapes=images:1x3x${IMGSZ}x${IMGSZ}

echo "==> 3/4 构建 INT8 engine（首次需校准集，见下方注释）"
# INT8 需要代表性校准数据，否则精度掉得厉害。最小可用做法：
#   先准备 100~500 张真实场景图，用 --int8 --calib=<calib.txt> 指定。
# 简版先跳过 INT8，跑通 FP16 出第一张对比表，INT8 放第二周。
trtexec \
  --onnx="${MODEL}.onnx" \
  --saveEngine="${MODEL}_int8.engine" \
  --int8 \
  --minShapes=images:1x3x${IMGSZ}x${IMGSZ} \
  --optShapes=images:1x3x${IMGSZ}x${IMGSZ} \
  --maxShapes=images:1x3x${IMGSZ}x${IMGSZ} \
  || echo "  [!] INT8 构建失败（缺校准数据正常，先忽略）"

echo "==> 4/4 FP16 精细延迟（H2D / GPU compute / D2H，这是「单帧延迟」的权威来源）"
trtexec --loadEngine="${MODEL}_fp16.engine" --fp16 --avgRuns=200 \
  --warmUp=50 --duration=10 2>&1 | grep -E "H2D|GPU Compute|D2H|Throughput|Latency|Total Host Walltime" || true

echo ""
echo "完成。engine 已生成，记录上面的 H2D/GPU compute/D2H 三项，填入 README 对比表。"
