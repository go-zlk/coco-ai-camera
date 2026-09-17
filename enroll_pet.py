#!/usr/bin/env python3
"""Hands-off pet enrollment capture for a fixed Jetson camera.

The user supplies a name once. During the capture window the script detects
cats, quality-filters crops, removes near-duplicate frames, and writes a
small representative gallery for later embedding extraction.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import cv2
import numpy as np
from ultralytics import YOLO

from infer import open_capture
from enrollment_policy import SampleQuality


def crop_signature(crop: np.ndarray) -> np.ndarray:
    gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)
    return cv2.resize(gray, (32, 32), interpolation=cv2.INTER_AREA).astype(np.float32) / 255.0


def capture(name: str, model_path: str, source: str, output: Path,
            duration: float, conf: float, max_samples: int,
            held_seconds: float) -> int:
    model = YOLO(model_path)
    cap = open_capture(source)
    if not cap.isOpened():
        raise SystemExit(f"无法打开视频源: {source}")

    target = output / name
    target.mkdir(parents=True, exist_ok=True)
    signatures: list[np.ndarray] = []
    manifest: list[dict[str, object]] = []
    saved = 0
    started = time.monotonic()
    frame_index = 0
    free_prompted = held_seconds <= 0
    free_seconds = max(0.0, duration - held_seconds)
    if held_seconds > 0:
        print(f"[enroll] 前 {held_seconds:.0f} 秒可抱着 {name} 在镜头前自然转动")
    if free_seconds > 0:
        print(f"[enroll] 随后请放下猫，让它自然活动 {free_seconds:.0f} 秒")

    try:
        while time.monotonic() - started < duration and saved < max_samples:
            elapsed = time.monotonic() - started
            if not free_prompted and elapsed >= held_seconds:
                print("[enroll] 现在请放下猫，继续让它自然活动")
                free_prompted = True
            ok, frame = cap.read()
            if not ok:
                print("[enroll] 摄像头读取失败，停止采集")
                break
            frame_index += 1
            # Avoid running inference on every camera frame; this also gives
            # the cat time to change pose between candidate samples.
            if frame_index % 3:
                continue
            result = model(frame, imgsz=640, conf=conf, classes=[15], verbose=False)[0]
            if result.boxes is None or len(result.boxes) == 0:
                continue
            best = max(zip(result.boxes.xyxy.tolist(), result.boxes.conf.tolist()),
                       key=lambda item: item[1])
            (x1, y1, x2, y2), score = best
            x1, y1 = max(0, int(x1)), max(0, int(y1))
            x2, y2 = min(frame.shape[1], int(x2)), min(frame.shape[0], int(y2))
            if x2 <= x1 or y2 <= y1:
                continue
            crop = frame[y1:y2, x1:x2]
            quality = SampleQuality(
                confidence=float(score),
                area_ratio=((x2 - x1) * (y2 - y1)) / (frame.shape[0] * frame.shape[1]),
                sharpness=float(cv2.Laplacian(crop, cv2.CV_64F).var()),
            )
            if not quality.accepted:
                continue
            signature = crop_signature(crop)
            if any(float(np.mean(np.abs(signature - old))) < 0.035 for old in signatures):
                continue
            path = target / f"sample_{saved + 1:03d}.jpg"
            cv2.imwrite(str(path), crop, [cv2.IMWRITE_JPEG_QUALITY, 92])
            signatures.append(signature)
            elapsed = time.monotonic() - started
            manifest.append({
                "file": path.name,
                "phase": "held" if elapsed < held_seconds else "free",
                "confidence": round(float(score), 4),
                "area_ratio": round(quality.area_ratio, 5),
                "sharpness": round(quality.sharpness, 2),
            })
            saved += 1
            print(f"[enroll] {saved:02d}/{max_samples} 置信度={score:.2f} 清晰度={quality.sharpness:.0f}")
    finally:
        cap.release()

    (target / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    print(f"[enroll] 完成：保存 {saved} 张样本到 {target}")
    if saved < 8:
        print("[enroll] 样本偏少，建议换一个光照或机位再运行一次")
    return saved


def main() -> None:
    parser = argparse.ArgumentParser(description="自动采集宠物身份注册样本")
    parser.add_argument("--name", required=True, help="身份名称，例如 coco 或 kui")
    parser.add_argument("--model", default="yolov8n.pt")
    parser.add_argument("--source", default="csi")
    parser.add_argument("--output", default="data/enrollment")
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--held-seconds", type=float, default=10.0,
                        help="前多少秒允许抱着猫采集补充样本（默认 10）")
    parser.add_argument("--conf", type=float, default=0.20)
    parser.add_argument("--max-samples", type=int, default=24)
    args = parser.parse_args()
    capture(args.name, args.model, args.source, Path(args.output),
            args.duration, args.conf, args.max_samples, args.held_seconds)


if __name__ == "__main__":
    main()
