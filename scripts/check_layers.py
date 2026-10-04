#!/usr/bin/env python3
"""Reject project includes that cross the documented dependency boundaries."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {
    "domain": {"domain"},
    "media": {"domain", "media"},
    "capture": {"domain", "media", "capture"},
    "perception": {"domain", "media", "perception"},
    "storage": {"domain", "storage"},
    "transport": {"domain", "transport"},
    "application": set(),
    "apps": {"application"},
}
ALLOWED["application"] = set(ALLOWED) - {"apps"}
errors = []
for folder in ("include/coco", "src", "apps"):
    for path in sorted((ROOT / folder).rglob("*")):
        if path.name.startswith("._") or path.suffix not in (".h", ".cc"):
            continue
        layer = "apps" if folder == "apps" else path.relative_to(ROOT / folder).parts[0]
        for number, line in enumerate(path.read_text().splitlines(), 1):
            project_include = re.match(r'\s*#include\s*["<]coco/([^/]+)/', line)
            if project_include and project_include[1] not in ALLOWED[layer]:
                errors.append(f"{path.relative_to(ROOT)}:{number}: {layer} cannot include {project_include[1]}")
            if layer == "domain" and re.match(r'\s*#include\s*["<](opencv|NvInfer|cuda|sqlite|sys/|arpa/)', line):
                errors.append(f"{path.relative_to(ROOT)}:{number}: domain must use only standard C++")
if errors:
    raise SystemExit("\n".join(errors))
print("Layer dependency checks passed")
