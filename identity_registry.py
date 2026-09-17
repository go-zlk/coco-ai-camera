"""Local identity gallery primitives shared by cat and person adapters.

The extractor is intentionally separate: cat appearance and human face
embeddings must use different models, while matching and enrollment policy
remain the same.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from math import sqrt


@dataclass
class IdentityProfile:
    identity_id: str
    display_name: str
    entity_type: str  # "cat" or "person"
    prototypes: list[list[float]] = field(default_factory=list)
    enabled: bool = True
    model_version: str = "unknown"


@dataclass(frozen=True)
class IdentityMatch:
    identity_id: str | None
    display_name: str | None
    status: str  # matched / unknown / disabled / no_gallery
    similarity: float


def cosine_similarity(a: list[float], b: list[float]) -> float:
    if len(a) != len(b) or not a:
        raise ValueError("embedding dimensions must match and be non-empty")
    dot = sum(x * y for x, y in zip(a, b))
    norm_a = sqrt(sum(x * x for x in a))
    norm_b = sqrt(sum(y * y for y in b))
    if norm_a == 0 or norm_b == 0:
        return 0.0
    return dot / (norm_a * norm_b)


class IdentityRegistry:
    def __init__(self, threshold: float = 0.65) -> None:
        self.threshold = threshold
        self.profiles: dict[str, IdentityProfile] = {}

    def enroll(self, profile: IdentityProfile) -> None:
        if not profile.prototypes:
            raise ValueError("an identity needs at least one prototype")
        self.profiles[profile.identity_id] = profile

    def match(self, embedding: list[float], entity_type: str) -> IdentityMatch:
        candidates = [p for p in self.profiles.values()
                      if p.enabled and p.entity_type == entity_type]
        if not candidates:
            return IdentityMatch(None, None, "no_gallery", 0.0)
        best = max(((cosine_similarity(embedding, proto), profile)
                    for profile in candidates for proto in profile.prototypes),
                   key=lambda item: item[0])
        similarity, profile = best
        if similarity < self.threshold:
            return IdentityMatch(None, None, "unknown", similarity)
        return IdentityMatch(profile.identity_id, profile.display_name,
                             "matched", similarity)
