#!/usr/bin/env bash
# 初始化 SSH 主机密钥和持久化 VS Code 数据后，再以前台模式启动 sshd。
set -euo pipefail

DEV_HOME="/home/dev"
AUTHORIZED_KEYS="$DEV_HOME/.ssh/authorized_keys"
HOST_KEY_DIR="/etc/ssh/hostkeys"
SEED_ROOT="/opt/common-dev-seed/vscode-server"
DEV_GROUP="$(id -gn dev)"
DEV_UID="$(id -u dev)"

# 不允许在没有有效公钥的情况下启动容器，避免部署成功但远程入口不可用。
if [[ ! -s "$AUTHORIZED_KEYS" ]]; then
  echo "SSH authorized_keys is missing or empty." >&2
  exit 1
fi
if [[ "$(stat -c '%u' "$AUTHORIZED_KEYS")" != "$DEV_UID" ]]; then
  echo "SSH authorized_keys must be owned by the dev user UID." >&2
  exit 1
fi
if [[ "$(stat -c '%a' "$AUTHORIZED_KEYS")" != "600" ]]; then
  echo "SSH authorized_keys must have mode 600." >&2
  exit 1
fi

# bind mount 的持久化目录由 prepare.sh 预先创建；这里只确保容器内权限与开发用户一致。
install -d -o dev -g "$DEV_GROUP" -m 700 \
  "$DEV_HOME/.ssh" \
  "$DEV_HOME/.cache/go-build" \
  "$DEV_HOME/.cache/pip" \
  "$DEV_HOME/go/pkg/mod" \
  "$DEV_HOME/.cargo/registry" \
  "$DEV_HOME/.cargo/git" \
  "$DEV_HOME/.vscode-server/data" \
  "$DEV_HOME/.vscode-server/extensions" \
  /data/cmake-build

# 首次挂载空卷时复制镜像内的默认设置与扩展；已有用户数据不覆盖。
if [[ -d "$SEED_ROOT/data" ]]; then
  runuser -u dev -- cp -an "$SEED_ROOT/data/." "$DEV_HOME/.vscode-server/data/"
fi
if [[ -d "$SEED_ROOT/extensions" ]]; then
  runuser -u dev -- cp -an "$SEED_ROOT/extensions/." "$DEV_HOME/.vscode-server/extensions/"
fi

# 持久化 SSH 主机密钥，容器重建后客户端不会反复收到主机指纹变化警告。
install -d -o root -g root -m 700 "$HOST_KEY_DIR"
if [[ ! -s "$HOST_KEY_DIR/ssh_host_ed25519_key" ]]; then
  ssh-keygen -q -t ed25519 -N "" -f "$HOST_KEY_DIR/ssh_host_ed25519_key"
fi
chmod 600 "$HOST_KEY_DIR/ssh_host_ed25519_key"
chmod 644 "$HOST_KEY_DIR/ssh_host_ed25519_key.pub"

mkdir -p /run/sshd
/usr/sbin/sshd -t
exec "$@"
