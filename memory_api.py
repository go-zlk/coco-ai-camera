#!/usr/bin/env python3
"""Tiny read-only HTTP API for the local Home Memory database."""

from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from memory_store import MemoryStore


class Handler(BaseHTTPRequestHandler):
    store: MemoryStore

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        if parsed.path == "/api/timeline":
            entity = query.get("entity_id", [None])[0]
            body = {"events": self.store.timeline(entity_id=entity,
                                                    limit=int(query.get("limit", [100])[0]))}
        elif parsed.path == "/api/entities":
            rows = self.store.db.execute(
                "SELECT entity_id, entity_type, display_name, enabled FROM entities ORDER BY display_name"
            ).fetchall()
            body = {"entities": [dict(row) for row in rows]}
        elif parsed.path == "/api/status":
            row = self.store.db.execute(
                "SELECT * FROM observations ORDER BY observed_at DESC LIMIT 1"
            ).fetchone()
            body = {"status": dict(row) if row else None}
        else:
            self.send_error(404)
            return
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
    Handler.store = MemoryStore(args.db)
    print(f"[api] listening on http://{args.host}:{args.port}")
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()
