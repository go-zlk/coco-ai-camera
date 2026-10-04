"""Runtime matching against the local appearance gallery."""

from __future__ import annotations

import json
from pathlib import Path

from appearance_embedding import cosine_similarity


class GalleryMatcher:
    def __init__(self, gallery_path: str | Path, threshold: float = 0.55) -> None:
        data = json.loads(Path(gallery_path).read_text(encoding="utf-8"))
        self.profiles = data.get("profiles", [])
        self.threshold = threshold

    @property
    def identity_count(self) -> int:
        return len(self.profiles)

    def match(self, embedding: list[float]) -> tuple[str | None, float]:
        best_name, best_score = None, -1.0
        for profile in self.profiles:
            for sample in profile.get("samples", []):
                score = cosine_similarity(embedding, sample["embedding"])
                # Held samples are useful, but their arm/clothing context is
                # less representative than free-motion samples.
                weight = float(sample.get("weight", 1.0))
                effective_score = score - (1.0 - weight) * 0.05
                if effective_score > best_score:
                    best_name, best_score = profile["identity"], effective_score
        if best_score < self.threshold:
            return None, best_score
        return best_name, best_score
