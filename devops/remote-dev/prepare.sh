#!/usr/bin/env bash
# 准备 NAS SSD 上的开发容器持久化目录，并检查 SSH 公钥认证前置条件。
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"
ENV_EXAMPLE="$SCRIPT_DIR/.env.example"

# 首次运行时只生成本地配置模板，避免直接使用未核对的默认路径。
if [[ ! -f "$ENV_FILE" ]]; then
  cp "$ENV_EXAMPLE" "$ENV_FILE"
  if [[ "$EUID" -eq 0 && -n "${SUDO_UID:-}" ]]; then
    chown "$SUDO_UID:${SUDO_GID:-$SUDO_UID}" "$ENV_FILE"
  fi
  echo "Created $ENV_FILE. Review the paths and DEV_UID/DEV_GID, then run this script again."
  exit 1
fi

if [[ "$EUID" -ne 0 ]]; then
  echo "Run this script with sudo so it can prepare Docker-owned directories." >&2
  exit 1
fi

set -a
# .env 是管理员在本机维护的部署配置，不应提交到仓库。
# shellcheck disable=SC1090
source "$ENV_FILE"
set +a

: "${WORKFLOW_ROOT:=/volume2/workflow}"
: "${COMMON_REPO_ROOT:=/volume2/workflow/common}"
: "${DEV_DATA_ROOT:=/volume2/docker/common-dev}"
: "${DEV_UID:=1026}"
: "${DEV_GID:=100}"

OWNER_UID="${SUDO_UID:-0}"
OWNER_GID="${SUDO_GID:-0}"
if [[ "$OWNER_UID" == "0" || "$OWNER_GID" == "0" ]]; then
  echo "Run this script through sudo from the NAS account that owns the workflow files." >&2
  exit 1
fi

if [[ "$DEV_UID" != "$OWNER_UID" || "$DEV_GID" != "$OWNER_GID" ]]; then
  echo "DEV_UID/DEV_GID must match the invoking NAS account: $OWNER_UID:$OWNER_GID." >&2
  echo "Update $ENV_FILE and rebuild the image before continuing." >&2
  exit 1
fi

if [[ ! -d "$WORKFLOW_ROOT" ]]; then
  echo "WORKFLOW_ROOT does not exist: $WORKFLOW_ROOT" >&2
  exit 1
fi
if [[ ! -d "$COMMON_REPO_ROOT" || ! -f "$COMMON_REPO_ROOT/Dockerfile" ]]; then
  echo "COMMON_REPO_ROOT must point to the common repository: $COMMON_REPO_ROOT" >&2
  exit 1
fi

# 将可写数据目录归属给开发账号，避免容器内创建 root 所有的缓存文件。
install -d -o "$OWNER_UID" -g "$OWNER_GID" -m 700 \
  "$DEV_DATA_ROOT" \
  "$DEV_DATA_ROOT/config" \
  "$DEV_DATA_ROOT/cache/go-mod" \
  "$DEV_DATA_ROOT/cache/go-build" \
  "$DEV_DATA_ROOT/cache/pip" \
  "$DEV_DATA_ROOT/cache/cargo-registry" \
  "$DEV_DATA_ROOT/cache/cargo-git" \
  "$DEV_DATA_ROOT/vscode-server/data" \
  "$DEV_DATA_ROOT/vscode-server/extensions" \
  "$DEV_DATA_ROOT/build" \
  "$DEV_DATA_ROOT/image-transfer"

install -d -o "$OWNER_UID" -g "$OWNER_GID" -m 700 "$DEV_DATA_ROOT/ssh"
install -d -o 0 -g 0 -m 700 "$DEV_DATA_ROOT/ssh/hostkeys"

AUTHORIZED_KEYS="$DEV_DATA_ROOT/ssh/authorized_keys"
if [[ ! -e "$AUTHORIZED_KEYS" ]]; then
  install -o "$OWNER_UID" -g "$OWNER_GID" -m 600 /dev/null "$AUTHORIZED_KEYS"
else
  chown "$OWNER_UID:$OWNER_GID" "$AUTHORIZED_KEYS"
  chmod 600 "$AUTHORIZED_KEYS"
fi

DEV_ENV_FILE="$DEV_DATA_ROOT/config/dev.env"
if [[ ! -e "$DEV_ENV_FILE" ]]; then
  install -o "$OWNER_UID" -g "$OWNER_GID" -m 600 /dev/null "$DEV_ENV_FILE"
else
  chown "$OWNER_UID:$OWNER_GID" "$DEV_ENV_FILE"
  chmod 600 "$DEV_ENV_FILE"
fi

if [[ ! -s "$AUTHORIZED_KEYS" ]]; then
  echo "SSH public keys are missing. Append the client .pub key to:" >&2
  echo "$AUTHORIZED_KEYS" >&2
  echo "Then run this script again." >&2
  exit 1
fi

echo "Remote development directories are ready."
echo "Workflow: $WORKFLOW_ROOT"
echo "Common repository: $COMMON_REPO_ROOT"
echo "Persistent data: $DEV_DATA_ROOT"
