"""Quality gates for automatic identity enrollment samples."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class SampleQuality:
    confidence: float
    area_ratio: float
    sharpness: float
    occlusion: float = 0.0

    @property
    def accepted(self) -> bool:
        return (self.confidence >= 0.65 and self.area_ratio >= 0.04
                and self.sharpness >= 40.0 and self.occlusion <= 0.45)


def diverse_enough(candidate: list[float], existing: list[list[float]],
                   min_distance: float = 0.08) -> bool:
    """Keep a candidate only when it adds meaningful appearance diversity.

    Vectors are expected to be normalized embeddings.  Euclidean distance is
    used here so the policy remains independent of a particular extractor.
    """
    if not existing:
        return True
    return all(sum((a - b) ** 2 for a, b in zip(candidate, sample)) ** 0.5
               >= min_distance for sample in existing)
