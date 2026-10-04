#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
formatter="${CLANG_FORMAT:-clang-format}"
if [[ "$($formatter --version)" != *"version 18."* ]]; then
  echo "Use clang-format 18 (CLANG_FORMAT can specify its executable)." >&2
  exit 1
fi
options=(-i)
case "${1:---check}" in
  --check) options=(--dry-run --Werror) ;;
  --fix) ;;
  *) echo "Usage: $0 [--check|--fix]" >&2; exit 1 ;;
esac
find apps include src tests -type f \( -name '*.cc' -o -name '*.h' \) -print0 |
  xargs -0 "$formatter" "${options[@]}"
