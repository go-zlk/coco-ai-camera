"""Local-first event memory store for AI Home Memory.

The store keeps facts separate from later summaries or generated replay.  It
is intentionally small and SQLite-based so the Jetson can operate offline.
"""

from __future__ import annotations

import json
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA = """
PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS entities (
  entity_id TEXT PRIMARY KEY, entity_type TEXT NOT NULL,
  display_name TEXT NOT NULL, enabled INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS observations (
  observation_id INTEGER PRIMARY KEY AUTOINCREMENT,
  entity_id TEXT, observed_at TEXT NOT NULL, state TEXT NOT NULL,
  place_id TEXT, confidence REAL NOT NULL, track_id INTEGER,
  model_version TEXT, metadata_json TEXT NOT NULL DEFAULT '{}'
);
CREATE INDEX IF NOT EXISTS idx_observations_time ON observations(observed_at);
CREATE INDEX IF NOT EXISTS idx_observations_entity ON observations(entity_id, observed_at);
CREATE TABLE IF NOT EXISTS events (
  event_id INTEGER PRIMARY KEY AUTOINCREMENT,
  event_type TEXT NOT NULL, actor_id TEXT, subject_id TEXT,
  start_at TEXT NOT NULL, end_at TEXT, place_id TEXT,
  confidence REAL NOT NULL, evidence_id TEXT, source TEXT NOT NULL,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);
CREATE INDEX IF NOT EXISTS idx_events_time ON events(start_at);
"""


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


class MemoryStore:
    def __init__(self, path: str | Path = "data/home_memory.db") -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(self.path)
        self.db.row_factory = sqlite3.Row
        self.db.executescript(SCHEMA)
        self.db.commit()

    def close(self) -> None:
        self.db.close()

    def upsert_entity(self, entity_id: str, entity_type: str,
                      display_name: str) -> None:
        self.db.execute(
            """INSERT INTO entities(entity_id, entity_type, display_name, created_at)
               VALUES (?, ?, ?, ?)
               ON CONFLICT(entity_id) DO UPDATE SET display_name=excluded.display_name,
               entity_type=excluded.entity_type, enabled=1""",
            (entity_id, entity_type, display_name, now_iso()),
        )
        self.db.commit()

    def record_observation(self, *, entity_id: str | None, state: str,
                           confidence: float, place_id: str | None = None,
                           track_id: int | None = None,
                           model_version: str | None = None,
                           observed_at: str | None = None,
                           metadata: dict[str, Any] | None = None) -> int:
        cur = self.db.execute(
            """INSERT INTO observations(entity_id, observed_at, state, place_id,
               confidence, track_id, model_version, metadata_json)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
            (entity_id, observed_at or now_iso(), state, place_id, confidence,
             track_id, model_version, json.dumps(metadata or {}, ensure_ascii=False)),
        )
        self.db.commit()
        return int(cur.lastrowid)

    def record_event(self, *, event_type: str, actor_id: str | None = None,
                     subject_id: str | None = None, place_id: str | None = None,
                     confidence: float = 1.0, evidence_id: str | None = None,
                     source: str = "vision", start_at: str | None = None,
                     end_at: str | None = None,
                     metadata: dict[str, Any] | None = None) -> int:
        cur = self.db.execute(
            """INSERT INTO events(event_type, actor_id, subject_id, start_at,
               end_at, place_id, confidence, evidence_id, source, metadata_json)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (event_type, actor_id, subject_id, start_at or now_iso(), end_at,
             place_id, confidence, evidence_id, source,
             json.dumps(metadata or {}, ensure_ascii=False)),
        )
        self.db.commit()
        return int(cur.lastrowid)

    def timeline(self, *, entity_id: str | None = None,
                 limit: int = 100) -> list[dict[str, Any]]:
        if entity_id:
            rows = self.db.execute(
                """SELECT * FROM events WHERE actor_id=? OR subject_id=?
                   ORDER BY start_at DESC LIMIT ?""", (entity_id, entity_id, limit)
            ).fetchall()
        else:
            rows = self.db.execute(
                "SELECT * FROM events ORDER BY start_at DESC LIMIT ?", (limit,)
            ).fetchall()
        return [dict(row) for row in rows]

    def close_entity(self, entity_id: str) -> None:
        self.db.execute("UPDATE entities SET enabled=0 WHERE entity_id=?", (entity_id,))
        self.db.commit()
