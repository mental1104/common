#!/usr/bin/env bash
set -euo pipefail
COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -n "${1:-}" ]]; then export SSH_PRIVATE_KEY="$1"; fi
exec "$COMMON_DIR/dev" build-docker
