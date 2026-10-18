#!/usr/bin/env bash
# 将本机已构建的镜像压缩后传到 NAS SSD，不依赖额外镜像仓库。
set -euo pipefail

if [[ "$#" -ne 1 ]]; then
  echo "Usage: $0 <nas-ssh-alias>" >&2
  exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"
if [[ ! -f "$ENV_FILE" ]]; then
  ENV_FILE="$SCRIPT_DIR/.env.example"
fi

set -a
# 远端路径必须与 NAS 上 prepare.sh 使用的 DEV_DATA_ROOT 一致。
# shellcheck disable=SC1090
source "$ENV_FILE"
set +a
: "${DEV_DATA_ROOT:=/volume2/docker/common-dev}"

IMAGE="mental1104_dev:latest"
REMOTE_DIR="$DEV_DATA_ROOT/image-transfer"
REMOTE_FILE="$REMOTE_DIR/mental1104_dev.tar.gz"

docker image inspect "$IMAGE" >/dev/null
echo "Streaming $IMAGE to $1:$REMOTE_FILE ..."
docker save "$IMAGE" | gzip -1 | ssh "$1" "mkdir -p '$REMOTE_DIR' && cat > '$REMOTE_FILE'"
echo "Image transfer complete. Load it on the NAS with: sudo docker load -i $REMOTE_FILE"
