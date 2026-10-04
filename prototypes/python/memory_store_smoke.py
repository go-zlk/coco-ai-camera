#!/usr/bin/env python3
"""Small local verification for the event memory layer."""

from memory_store import MemoryStore


store = MemoryStore("/tmp/home-memory-smoke.db")
store.upsert_entity("coco", "cat", "Coco")
store.record_observation(entity_id="coco", state="active", confidence=.91)
store.record_event(event_type="state_changed", subject_id="coco",
                   place_id="living_room", confidence=.91,
                   metadata={"state": "active"})
assert store.timeline(entity_id="coco", limit=1)[0]["subject_id"] == "coco"
store.close()
print("memory store smoke test: OK")
