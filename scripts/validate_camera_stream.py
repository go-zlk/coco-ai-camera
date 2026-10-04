#!/usr/bin/env python3
"""Bounded RTSP decode and person/cat inference check; saves no media."""
import argparse
import json
import time
from collections import Counter
from pathlib import Path

import cv2
from ultralytics import YOLO


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='rtsp://127.0.0.1:8554/xiaomi_living_room')
    parser.add_argument('--seconds', type=float, default=30)
    parser.add_argument('--model', default=str(Path(__file__).resolve().parents[1] / 'yolov8n.pt'))
    parser.add_argument('--device', default='cpu')
    parser.add_argument('--target', choices=['person', 'cat', 'both'], default='both',
                        help='要统计的目标类别；person 适合用人验证视频链路')
    parser.add_argument('--conf', type=float, default=0.25)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error('--seconds must be positive')
    model = YOLO(args.model)
    cap = cv2.VideoCapture(args.source, cv2.CAP_FFMPEG, [
        cv2.CAP_PROP_OPEN_TIMEOUT_MSEC, 15000,
        cv2.CAP_PROP_READ_TIMEOUT_MSEC, 10000,
    ])
    if not cap.isOpened():
        raise SystemExit('视频源打开失败；先检查 go2rtc 是否正在运行、摄像头是否在线。')
    class_ids = {'person': [0], 'cat': [15], 'both': [0, 15]}[args.target]
    started = time.monotonic()
    frames = checks = 0
    detections = Counter()
    last_check = float('-inf')
    failures = 0
    shape = None
    try:
        while time.monotonic() - started < args.seconds:
            ok, frame = cap.read()
            if not ok:
                failures += 1
                break
            frames += 1
            shape = [frame.shape[1], frame.shape[0]]
            now = time.monotonic()
            if now - last_check >= 1:
                last_check = now
                result = model(frame, classes=class_ids, device=args.device,
                               imgsz=640, conf=args.conf, verbose=False)[0]
                checks += 1
                # Count frames with each class, not unique animals or people.
                for cls in set(result.boxes.cls.int().tolist()):
                    detections[result.names[cls]] += 1
    finally:
        cap.release()
    elapsed = time.monotonic() - started
    print(json.dumps({
        'duration_seconds': round(elapsed, 2),
        'decoded_frames': frames,
        'decoded_fps': round(frames / elapsed, 2),
        'resolution': shape,
        'inference_checks': checks,
        'target': args.target,
        'confidence_threshold': args.conf,
        'checks_with_detection': dict(detections),
        'read_failures': failures,
        'media_saved': False,
    }, ensure_ascii=False, indent=2))
    if failures or not checks:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
