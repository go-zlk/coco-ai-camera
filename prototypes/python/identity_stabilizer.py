"""Temporal hysteresis for identity labels on a tracked target."""

from __future__ import annotations

from collections import defaultdict, deque


class IdentityStabilizer:
    def __init__(self, votes: int = 5, margin: float = 0.06,
                 history_size: int = 8) -> None:
        self.votes = votes
        self.margin = margin
        self.history: dict[int, deque[tuple[str | None, float]]] = defaultdict(
            lambda: deque(maxlen=history_size)
        )
        self.current: dict[int, str | None] = {}

    def update(self, track_id: int, candidate: str | None,
               score: float) -> str | None:
        history = self.history[track_id]
        history.append((candidate, score))
        active = self.current.get(track_id)
        if active is None:
            if candidate is not None:
                history_candidates = [name for name, _ in history if name is not None]
                if history_candidates.count(candidate) >= self.votes:
                    self.current[track_id] = candidate
            return self.current.get(track_id)

        if candidate == active:
            return active
        recent = [(name, value) for name, value in history if name is not None]
        support = [value for name, value in recent if name == candidate]
        active_scores = [value for name, value in recent if name == active]
        if (candidate is not None and len(support) >= self.votes and
                (sum(support) / len(support)) >=
                (sum(active_scores) / len(active_scores) if active_scores else 0.0) + self.margin):
            self.current[track_id] = candidate
        return self.current.get(track_id)

    def forget(self, track_id: int) -> None:
        self.history.pop(track_id, None)
        self.current.pop(track_id, None)
