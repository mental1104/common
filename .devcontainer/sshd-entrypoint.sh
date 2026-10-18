#!/bin/sh
set -eu

# 首次启动时生成并持久化容器 SSH 主机密钥，避免重建容器后身份变化。
host_key=/var/lib/sshd-hostkeys/ssh_host_ed25519_key
mkdir -p /run/sshd /var/lib/sshd-hostkeys
chown root:root /var/lib/sshd-hostkeys
chmod 700 /var/lib/sshd-hostkeys
if [ ! -s "$host_key" ]; then
    ssh-keygen -q -t ed25519 -N "" -f "$host_key"
fi
chown root:root "$host_key" "$host_key.pub"
chmod 600 "$host_key"
chmod 644 "$host_key.pub"

if [ ! -s /home/developer/.ssh/authorized_keys ]; then
    echo "Missing SSH public keys: /home/developer/.ssh/authorized_keys" >&2
    exit 1
fi

/usr/sbin/sshd -t
exec /usr/sbin/sshd -D -e
