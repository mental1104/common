# NAS VS Code 开发环境

本配置提供可直接通过 VS Code Remote-SSH 连接的多语言开发容器。客户端直接连接 NAS 上发布的容器 SSH 端口，不需要先登录 NAS 再执行 Reopen in Container。

## 首次部署

### 1. 准备客户端 SSH 密钥

在你运行 VS Code 的电脑上执行。已有专用密钥时可跳过生成步骤：

```sh
ssh-keygen -t ed25519 -f ~/.ssh/common_dev_ed25519 -C vscode-common-dev
cat ~/.ssh/common_dev_ed25519.pub
```

将输出的整行公钥复制下来。**只复制 `.pub` 公钥；私钥 `common_dev_ed25519` 必须留在客户端，不能上传 NAS。**

### 2. 配置 NAS

在 NAS SSH 中执行：

```sh
cd /volume2/workflow/common
cp .devcontainer/.env.example .devcontainer/.env
```

确认 `.devcontainer/.env` 中的 `WORKFLOW_ROOT`、`DEVCONTAINER_DATA_ROOT`、`DEV_UID`、`DEV_GID` 与 NAS 实际情况一致。当前 NAS 已确认仓库所有者是 UID 1026、GID 100。

创建持久化目录与公钥文件：

```sh
set -a
. .devcontainer/.env
set +a

sudo mkdir -p \
  "$DEVCONTAINER_DATA_ROOT/vscode-server" \
  "$DEVCONTAINER_DATA_ROOT/go-bin" \
  "$DEVCONTAINER_DATA_ROOT/go-mod" \
  "$DEVCONTAINER_DATA_ROOT/cargo-registry" \
  "$DEVCONTAINER_DATA_ROOT/cargo-git" \
  "$DEVCONTAINER_DATA_ROOT/cache" \
  "$DEVCONTAINER_DATA_ROOT/ssh/hostkeys"
sudo touch "$DEVCONTAINER_DATA_ROOT/ssh/authorized_keys"
sudo chown -R "$DEV_UID:$DEV_GID" "$DEVCONTAINER_DATA_ROOT"
sudo chmod 700 "$DEVCONTAINER_DATA_ROOT/ssh"
sudo chmod 700 "$DEVCONTAINER_DATA_ROOT/ssh/hostkeys"
sudo chmod 600 "$DEVCONTAINER_DATA_ROOT/ssh/authorized_keys"
```

把刚才复制的客户端公钥追加到 `$DEVCONTAINER_DATA_ROOT/ssh/authorized_keys`，每把公钥独占一行。可使用 `vi` 编辑该文件，然后再次执行：

```sh
sudo chown "$DEV_UID:$DEV_GID" "$DEVCONTAINER_DATA_ROOT/ssh/authorized_keys"
sudo chmod 600 "$DEVCONTAINER_DATA_ROOT/ssh/authorized_keys"
```

### 3. 构建并启动

```sh
cd /volume2/workflow/common
git pull --ff-only
cd .devcontainer
docker compose config
docker compose up -d --build
docker compose ps
```

默认将 NAS 的 TCP 2222 端口映射到容器 SSH 22 端口。请在群晖防火墙中限制 2222 端口只允许可信 LAN/VPN 网段访问；**不要将该端口通过路由器转发到公网**。如果端口已被占用，可修改 `DEVCONTAINER_SSH_PORT`。

## VS Code 直接连接

在 VS Code 客户端的 SSH 配置文件（通常为 `~/.ssh/config`）中添加以下配置，把地址替换为 NAS 的局域网 IP 或可信 VPN 地址：

```sshconfig
Host common-dev
    HostName 192.168.1.100
    User developer
    Port 2222
    IdentityFile ~/.ssh/common_dev_ed25519
    IdentitiesOnly yes
```

先在客户端终端验证连接：

```sh
ssh common-dev
```

然后在 VS Code 安装 **Remote - SSH** 扩展，通过命令面板执行 `Remote-SSH: Connect to Host...`，选择 `common-dev`。连接成功后选择 **File → Open Folder**，打开：

```text
/workspace/workflow
```

这是直接 SSH 进入开发容器。只需要 Remote - SSH，不需要再通过 NAS 的 Remote-SSH 窗口运行 Dev Containers。

## 日常使用与安全

- 源码直接保存在 NAS 的 `/volume2/workflow`，容器中统一显示为 `/workspace/workflow`。
- `common` 额外挂载到 `/opt/mental1104`，以兼容现有 Go workspace 和 Cargo patch 配置。
- SSH 只允许 `developer` 用户使用公钥认证；禁止密码登录和 root 登录。
- 客户端私钥不会进入 NAS 或容器；容器只读挂载 `authorized_keys`。
- SSH 主机密钥保存在 `DEVCONTAINER_DATA_ROOT/ssh/hostkeys`，重建容器后身份保持稳定。
- 不要把 NAS 的 Docker Socket、NAS 私钥、Kubernetes 配置或其他含凭据的环境文件挂载进开发容器。
- 修改工具链后重新执行 `docker compose up -d --build`；日常改代码不需要重建镜像。
- 项目专用依赖仍以各仓库的构建脚本和 README 为准。
