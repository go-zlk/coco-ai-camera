#!/usr/bin/env python3
"""Print within-identity similarity to choose a sensible threshold."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2

from appearance_embedding import extract_embedding, cosine_similarity


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="data/enrollment")
    args = ap.parse_args()
    root = Path(args.root)
    for identity in sorted(p for p in root.iterdir() if p.is_dir()):
        vectors = [extract_embedding(im) for p in sorted(identity.glob("sample_*.jpg"))
                   if (im := cv2.imread(str(p))) is not None]
        if len(vectors) < 2:
            print(f"{identity.name}: only {len(vectors)} sample(s)")
            continue
        scores = [cosine_similarity(a, b) for i, a in enumerate(vectors)
                  for b in vectors[i + 1:]]
        print(f"{identity.name}: n={len(vectors)} min={min(scores):.3f} "
              f"median={sorted(scores)[len(scores)//2]:.3f} max={max(scores):.3f}")


if __name__ == "__main__":
    main()
