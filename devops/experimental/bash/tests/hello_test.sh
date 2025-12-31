#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$PROJECT_ROOT/src/hello.sh"

output=$(extract_world "$HELLO")
if [[ "$output" != "world" ]]; then
  echo "[fail] expected world, got: $output" >&2
  exit 1
fi

if extract_world "missing" >/dev/null 2>&1; then
  echo "[fail] expected missing world to fail" >&2
  exit 1
fi

echo "[ok] bash tests passed"
