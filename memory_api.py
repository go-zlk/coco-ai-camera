#!/usr/bin/env python3
"""Tiny read-only HTTP API for the local Home Memory database."""

from __future__ import annotations

import argparse
import json
import sqlite3
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from memory_store import MemoryStore


class Handler(BaseHTTPRequestHandler):
    db_path: Path

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        if parsed.path not in {"/api/timeline", "/api/entities", "/api/status"}:
            self.send_error(404)
            return
        try:
            limit = min(500, max(1, int(query.get("limit", [100])[0])))
        except ValueError:
            self.send_error(400, "limit must be an integer")
            return
        # ThreadingHTTPServer runs handlers on separate threads.  A fresh
        # read-only connection keeps SQLite ownership local to each request.
        with sqlite3.connect(f"{self.db_path.resolve().as_uri()}?mode=ro", uri=True) as db:
            db.row_factory = sqlite3.Row
            if parsed.path == "/api/timeline":
                entity = query.get("entity_id", [None])[0]
                if entity:
                    rows = db.execute(
                        """SELECT * FROM events WHERE actor_id=? OR subject_id=?
                           ORDER BY start_at DESC LIMIT ?""", (entity, entity, limit)
                    ).fetchall()
                else:
                    rows = db.execute(
                        "SELECT * FROM events ORDER BY start_at DESC LIMIT ?", (limit,)
                    ).fetchall()
                body = {"events": [dict(row) for row in rows]}
            elif parsed.path == "/api/entities":
                rows = db.execute(
                    "SELECT entity_id, entity_type, display_name, enabled FROM entities ORDER BY display_name"
                ).fetchall()
                body = {"entities": [dict(row) for row in rows]}
            else:
                row = db.execute(
                    "SELECT * FROM observations ORDER BY observed_at DESC LIMIT 1"
                ).fetchone()
                body = {"status": dict(row) if row else None}
        payload = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *_args: object) -> None:
        return


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Home Memory 本地查询 API")
    parser.add_argument("--db", default="data/home_memory.db")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    MemoryStore(args.db).close()  # Create schema if this is the first launch.
    Handler.db_path = Path(args.db)
    print(f"[api] listening on http://{args.host}:{args.port}")
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()
