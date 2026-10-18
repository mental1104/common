#!/usr/bin/env bash
set -euo pipefail

# 初始化持久化 SSH 主机密钥，避免容器重建后身份变化，也避免多个环境共用镜像中的密钥。
SSH_HOST_KEY_DIR="/var/lib/ssh-host-keys"
install -d -m 0700 "$SSH_HOST_KEY_DIR"

if [[ ! -s "$SSH_HOST_KEY_DIR/ssh_host_ed25519_key" ]]; then
  rm -f "$SSH_HOST_KEY_DIR"/ssh_host_*
  ssh-keygen -q -t ed25519 -N "" -f "$SSH_HOST_KEY_DIR/ssh_host_ed25519_key"
  chmod 0600 "$SSH_HOST_KEY_DIR/ssh_host_ed25519_key"
  chmod 0644 "$SSH_HOST_KEY_DIR/ssh_host_ed25519_key.pub"
fi

# 启动前验证 SSH 配置，并让 sshd 直接接收容器停止信号。
sshd -t
exec /usr/sbin/sshd -D -e
