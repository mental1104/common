#!/usr/bin/env bash
# 在本机 x86 Linux/WSL2 上构建适用于 Synology x86_64 NAS 的开发镜像。
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"
if [[ ! -f "$ENV_FILE" ]]; then
  ENV_FILE="$SCRIPT_DIR/.env.example"
fi

set -a
# 本地 .env 只用于读取目标 NAS 的 UID/GID；不需要在本机存在 NAS 挂载目录。
# shellcheck disable=SC1090
source "$ENV_FILE"
set +a
: "${DEV_UID:=1026}"
: "${DEV_GID:=100}"

IMAGE="mental1104_dev:latest"
docker buildx build \
  --platform linux/amd64 \
  --load \
  --tag "$IMAGE" \
  --build-arg "DEV_UID=$DEV_UID" \
  --build-arg "DEV_GID=$DEV_GID" \
  --file "$REPO_ROOT/Dockerfile" \
  "$REPO_ROOT"

echo "Built $IMAGE for linux/amd64."
