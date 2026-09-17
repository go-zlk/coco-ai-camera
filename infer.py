#!/usr/bin/env python3
"""
edge-vision 推理脚本：live 实时演示 / bench 基准测试

两种模式：
  live  摄像头实时检测，画面叠加 FPS 角标，可选保存视频（用来做 demo GIF/录屏）
  bench 对视频跑固定轮数，输出 采集/推理/绘制 三段耗时（均值/中位数/p95）与真实 FPS

用法：
  # 实时演示（USB 摄像头，0 号设备；CSI 用 --source csi）
  python infer.py --source 0 --model yolov8n.pt --mode live

  # 用 TensorRT engine 实时跑并保存视频
  python infer.py --source 0 --model yolov8n_fp16.engine --mode live --save docs/demo.mp4

  # 基准测试（出对比表用）
  python infer.py --source test.mp4 --model yolov8n_fp16.engine --mode bench --iters 200

分工说明：
  这里的「推理」是 ultralytics 对单帧的完整调用（含内部 letterbox + NMS + H2D/D2H），
  代表真实闭环体验。更细的 GPU 三段延迟（H2D/compute/D2H）用 scripts/build_engine.sh 里的
  trtexec --avgRuns=200 拿，那是「单帧延迟」一行的权威来源。

依赖：pip install ultralytics opencv-python
"""

import argparse
import statistics
import time
from collections import deque

import cv2
from ultralytics import YOLO

from state_estimator import CatState, CatStateEstimator
from appearance_embedding import extract_embedding
from identity_matcher import GalleryMatcher


def open_capture(source: str) -> cv2.VideoCapture:
    """打开摄像头/视频源。支持 USB(0)、CSI('csi')、视频文件路径。"""
    if source == "csi":
        # Jetson CSI 摄像头走 nvarguscamerasrc，NVMM 零拷贝 → 转 BGR 给 cv2
        gst = (
            "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=1280, height=720, "
            "format=NV12, framerate=30/1 ! nvvidconv ! video/x-raw, format=BGRx ! "
            "videoconvert ! video/x-raw, format=BGR ! appsink"
        )
        return cv2.VideoCapture(gst, cv2.CAP_GSTREAMER)
    if source.isdigit():
        return cv2.VideoCapture(int(source))
    return cv2.VideoCapture(source)


def fmt_ms(x: float) -> str:
    return f"{x * 1000:7.2f} ms"


def run_live(model: YOLO, cap: cv2.VideoCapture, conf: float, imgsz: int,
             save_path: str | None, cat_only: bool = False,
             tracking: bool = False,
             tracker: str = "trackers/bytetrack-cat.yaml",
             gallery: GalleryMatcher | None = None) -> None:
    """实时演示：采集→推理→绘制 三段计时 + FPS 角标，按 q 退出。"""
    fps_win: deque[float] = deque(maxlen=30)  # 30 帧滑窗，FPS 更稳
    writer = None
    cat_state = CatStateEstimator()
    if save_path:
        writer = cv2.VideoWriter(
            save_path, cv2.VideoWriter_fourcc(*"mp4v"), 30.0, (1280, 720)
        )

    print("[live] 按 q 退出")
    while True:
        t0 = time.perf_counter()
        ok, frame = cap.read()
        if not ok:
            break
        t1 = time.perf_counter()

        # 推理：ultralytics 单帧调用，含内部前后处理（诚实标注为「端到端推理」）
        infer_kwargs = {"imgsz": imgsz, "conf": conf, "verbose": False}
        if cat_only:
            # COCO class 15 is cat.  Filtering before plot() prevents large
            # chair/person boxes from hiding the product state in the demo.
            infer_kwargs["classes"] = [15]
        if tracking:
            # ByteTrack keeps a short-lived identity through motion and
            # occasional missed detections (turning/partial occlusion).
            results = model.track(frame, persist=True, tracker=tracker,
                                  **infer_kwargs)[0]
        else:
            results = model(frame, **infer_kwargs)[0]
        t2 = time.perf_counter()

        # COCO class 15 is cat.  Keep only the highest-confidence cat box
        # for the MVP state estimator.
        cat_boxes = []
        if results.boxes is not None:
            for box, cls, score in zip(results.boxes.xyxy.tolist(),
                                       results.boxes.cls.tolist(),
                                       results.boxes.conf.tolist()):
                if int(cls) == 15:
                    cat_boxes.append((score, tuple(box)))
        if cat_boxes:
            score, box = max(cat_boxes)
            state = cat_state.update(box, score, frame.shape[1], frame.shape[0])
            identity_label = "unknown"
            identity_score = 0.0
            if gallery is not None:
                x1, y1, x2, y2 = [max(0, int(v)) for v in box]
                crop = frame[min(y1, frame.shape[0]):min(y2, frame.shape[0]),
                             min(x1, frame.shape[1]):min(x2, frame.shape[1])]
                if crop.size:
                    identity, identity_score = gallery.match(extract_embedding(crop))
                    identity_label = identity or "unknown"
        else:
            state = cat_state.mark_absent()
            identity_label, identity_score = "unknown", 0.0

        annotated = results.plot()
        t3 = time.perf_counter()

        capture_ms = (t1 - t0) * 1000
        infer_ms = (t2 - t1) * 1000
        draw_ms = (t3 - t2) * 1000

        fps_win.append(1.0 / (t3 - t0))
        fps = sum(fps_win) / len(fps_win)
        n_obj = len(results.boxes) if results.boxes is not None else 0

        cv2.putText(annotated, f"FPS {fps:5.1f}", (16, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)
        cv2.putText(annotated, f"cap {capture_ms:.1f}  infer {infer_ms:.1f}  "
                    f"draw {draw_ms:.1f} ms  obj {n_obj}", (16, 76),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 0), 1)
        cv2.putText(annotated, f"cat {state.value}", (16, 112),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 220, 255), 2)
        if cat_boxes:
            cv2.putText(annotated, f"id {identity_label} {identity_score:.2f}", (16, 144),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.65, (255, 210, 0), 2)

        cv2.imshow("edge-vision", annotated)
        if writer is not None:
            writer.write(annotated)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    if writer is not None:
        writer.release()
    cv2.destroyAllWindows()


def run_bench(model: YOLO, cap: cv2.VideoCapture, conf: float, imgsz: int,
              iters: int) -> None:
    """基准测试：固定帧数，输出三段耗时统计与真实 FPS。"""
    t_capture: list[float] = []
    t_infer: list[float] = []
    t_draw: list[float] = []

    print(f"[bench] 预热 10 帧...")
    for _ in range(10):
        ok, frame = cap.read()
        if not ok:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ok, frame = cap.read()
        model(frame, imgsz=imgsz, conf=conf, verbose=False)

    print(f"[bench] 正式测 {iters} 帧...")
    for _ in range(iters):
        t0 = time.perf_counter()
        ok, frame = cap.read()
        if not ok:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)  # 视频循环播放
            ok, frame = cap.read()
        t1 = time.perf_counter()
        if not ok:
            break
        results = model(frame, imgsz=imgsz, conf=conf, verbose=False)[0]
        t2 = time.perf_counter()
        annotated = results.plot()
        t3 = time.perf_counter()

        t_capture.append(t1 - t0)
        t_infer.append(t2 - t1)
        t_draw.append(t3 - t2)

    n = len(t_infer)
    total = sum(t_capture) + sum(t_infer) + sum(t_draw)

    def stat(name: str, arr: list[float]) -> None:
        mean = statistics.mean(arr)
        median = statistics.median(arr)
        p95 = sorted(arr)[int(n * 0.95) - 1]
        print(f"  {name:10s}  均值 {fmt_ms(mean)}  中位 {fmt_ms(median)}  "
              f"p95 {fmt_ms(p95)}")

    print("\n=== 三段耗时（秒）===")
    stat("采集", t_capture)
    stat("推理(端到端)", t_infer)
    stat("绘制", t_draw)
    print(f"\n  闭环吞吐  真实 FPS = {n / total:6.2f}")
    print(f"  推理吞吐  纯推理 FPS = {n / sum(t_infer):6.2f}（不含采集/绘制）")


def main() -> None:
    ap = argparse.ArgumentParser(description="Jetson 端侧视觉推理")
    ap.add_argument("--source", default="0", help="0=USB摄像头 / csi / 视频路径")
    ap.add_argument("--model", default="yolov8n.pt",
                    help=".pt / .onnx / .engine 路径")
    ap.add_argument("--mode", default="live", choices=["live", "bench"])
    ap.add_argument("--imgsz", type=int, default=640, help="推理输入尺寸")
    ap.add_argument("--conf", type=float, default=0.25, help="置信度阈值")
    ap.add_argument("--iters", type=int, default=200, help="bench 帧数")
    ap.add_argument("--save", default=None, help="live 模式保存视频路径")
    ap.add_argument("--device", default="0", help="推理设备，Jetson 上为 0")
    ap.add_argument("--cat-only", action="store_true",
                    help="仅推理/显示 COCO cat 类别，减少无关框干扰")
    ap.add_argument("--track", action="store_true",
                    help="启用 ByteTrack，保持短时遮挡/漏检时的目标轨迹")
    ap.add_argument("--tracker", default="trackers/bytetrack-cat.yaml",
                    help="跟踪器 YAML 配置路径")
    ap.add_argument("--gallery", default=None,
                    help="身份 gallery JSON（由 build_gallery.py 生成）")
    ap.add_argument("--identity-threshold", type=float, default=0.78)
    args = ap.parse_args()

    model = YOLO(args.model)
    cap = open_capture(args.source)
    if not cap.isOpened():
        raise SystemExit(f"无法打开视频源: {args.source}")

    gallery = (GalleryMatcher(args.gallery, args.identity_threshold)
               if args.gallery else None)
    if args.mode == "live":
        run_live(model, cap, args.conf, args.imgsz, args.save,
                 args.cat_only, args.track, args.tracker, gallery)
    else:
        run_bench(model, cap, args.conf, args.imgsz, args.iters)

    cap.release()


if __name__ == "__main__":
    main()
