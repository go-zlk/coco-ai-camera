"""Lightweight local appearance descriptor for the first cat gallery.

This is a deployable baseline rather than the final ReID model: HSV color
histograms plus a low-resolution grayscale silhouette are cheap on Orin Nano
and keep the gallery pipeline independent from a second neural network.
"""

from __future__ import annotations

import cv2
import numpy as np


def extract_embedding(image: np.ndarray) -> list[float]:
    if image is None or image.size == 0:
        raise ValueError("empty image")
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    hist = cv2.calcHist([hsv], [0, 1], None, [16, 4], [0, 180, 0, 256]).flatten()
    hist = hist / (np.linalg.norm(hist) + 1e-8)
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    shape = cv2.resize(gray, (16, 16), interpolation=cv2.INTER_AREA).flatten().astype(np.float32)
    shape = (shape - shape.mean()) / (shape.std() + 1e-6)
    vector = np.concatenate([hist.astype(np.float32), shape])
    return vector.tolist()


def cosine_similarity(a: list[float], b: list[float]) -> float:
    va, vb = np.asarray(a, dtype=np.float32), np.asarray(b, dtype=np.float32)
    if va.shape != vb.shape or va.size == 0:
        raise ValueError("embedding dimensions must match")
    return float(np.dot(va, vb) / ((np.linalg.norm(va) * np.linalg.norm(vb)) + 1e-8))
