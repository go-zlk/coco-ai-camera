"""Small, deterministic cat state estimator for the first MVP.

It deliberately converts detector observations into coarse product states;
it is not a behavior classifier.  Motion is estimated from the detected cat
bounding-box centre over a short window.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import Enum
from time import monotonic


class CatState(str, Enum):
    ACTIVE = "active"
    RESTING = "resting"
    OUT_OF_VIEW = "out_of_view"
    OFFLINE = "offline"


@dataclass(frozen=True)
class CatObservation:
    timestamp: float
    center_x: float
    center_y: float
    confidence: float


class CatStateEstimator:
    def __init__(self, *, motion_threshold: float = 0.025,
                 absent_timeout: float = 2.0, window_size: int = 12) -> None:
        self.motion_threshold = motion_threshold
        self.absent_timeout = absent_timeout
        self._observations: deque[CatObservation] = deque(maxlen=window_size)
        self._last_seen: float | None = None
        self.state = CatState.OUT_OF_VIEW

    def update(self, bbox: tuple[float, float, float, float],
               confidence: float, frame_width: int, frame_height: int,
               now: float | None = None) -> CatState:
        now = monotonic() if now is None else now
        x1, y1, x2, y2 = bbox
        obs = CatObservation(now, ((x1 + x2) / 2) / frame_width,
                             ((y1 + y2) / 2) / frame_height, confidence)
        self._observations.append(obs)
        self._last_seen = now
        self.state = self._classify_motion()
        return self.state

    def mark_absent(self, now: float | None = None) -> CatState:
        now = monotonic() if now is None else now
        if self._last_seen is None or now - self._last_seen >= self.absent_timeout:
            self.state = CatState.OUT_OF_VIEW
        return self.state

    def mark_offline(self) -> CatState:
        self.state = CatState.OFFLINE
        return self.state

    def _classify_motion(self) -> CatState:
        if len(self._observations) < 2:
            return CatState.RESTING
        first, last = self._observations[0], self._observations[-1]
        displacement = ((last.center_x - first.center_x) ** 2 +
                        (last.center_y - first.center_y) ** 2) ** 0.5
        return (CatState.ACTIVE if displacement >= self.motion_threshold
                else CatState.RESTING)
