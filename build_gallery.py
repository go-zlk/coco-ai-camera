#!/usr/bin/env python3
"""Build a local identity gallery from enroll_pet.py output."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2

from appearance_embedding import extract_embedding


def build(root: Path, output: Path) -> int:
    profiles: list[dict[str, object]] = []
    for identity_dir in sorted(p for p in root.iterdir() if p.is_dir()):
        samples = []
        for path in sorted(identity_dir.glob("sample_*.jpg")):
            image = cv2.imread(str(path))
            if image is not None:
                samples.append({"file": path.name, "embedding": extract_embedding(image)})
        if samples:
            profiles.append({"identity": identity_dir.name, "samples": samples})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"version": 1, "profiles": profiles},
                                 ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"[gallery] 写入 {len(profiles)} 个身份到 {output}")
    return len(profiles)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="构建宠物身份 gallery")
    parser.add_argument("--root", default="data/enrollment")
    parser.add_argument("--output", default="data/gallery.json")
    args = parser.parse_args()
    build(Path(args.root), Path(args.output))
