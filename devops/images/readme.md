## 安装docker compose插件

先判断：这是 **APT 源重复定义**（`/etc/apt/sources.list` 与 `/etc/apt/sources.list.d/ubuntu.sources` 同时启用）导致的索引混乱，随后你又没拿到包含 `docker-compose-plugin` 的最新索引，所以一直 `Unable to locate package`。不是包“消失”了。

---

# 最优方案

**保留 `ubuntu.sources`（新格式），禁用传统 `sources.list`，刷新索引；随后用官方 Docker 仓库安装 Compose V2 插件。**
这样一次到位，既干净又稳定。

## 操作

```bash
# 1) 禁用旧的 /etc/apt/sources.list（去重）
sudo mv /etc/apt/sources.list /etc/apt/sources.list.disabled.$(date +%F_%H%M%S) || true

# 2) 确保存在“官方 https 源”的 ubuntu.sources（覆盖写入一份标准配置）
sudo tee /etc/apt/sources.list.d/ubuntu.sources >/dev/null <<'EOF'
Types: deb
URIs: https://archive.ubuntu.com/ubuntu
Suites: noble noble-updates noble-backports
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://security.ubuntu.com/ubuntu
Suites: noble-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

# 3) 刷新 APT 索引（先清理再更新）
sudo apt-get clean
sudo apt-get update

# 4) 配置 Docker 官方仓库（用于获取稳定的 compose 插件）
sudo apt-get install -y ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo $VERSION_CODENAME) stable" \
| sudo tee /etc/apt/sources.list.d/docker.list >/dev/null

sudo apt-get update

# 5) 安装 Compose V2 插件（仅 CLI 插件，不动引擎）
sudo apt-get install -y docker-compose-plugin

# 6) 确认版本（出现版本号即成功）
docker compose version

# 7) 清理旧版 v1（避免再次误用 python 版）
sudo apt-get purge -y docker-compose || true
hash -r

# 8) 回到你的项目目录，使用 V2 命令启动
cd ~/code/common/devops/images/redis
docker compose up -d
```

---

## 原理

* **重复源**：`sources.list` 与 `ubuntu.sources` 同时指向相同套件/组件，会触发 “configured multiple times” 警告，且容易造成索引不一致。禁用旧文件只保留 `.sources` 可一次性消除隐患。
* **HTTPS 官方仓库**：使用 `archive.ubuntu.com / security.ubuntu.com` 保证索引新鲜、连通性稳定。
* **Compose V2 插件**：作为 `docker` 的 **Go CLI 插件** 安装到系统（包名 `docker-compose-plugin`），不再依赖 Python/requests，所以不会再遇到 `http+docker` 协议适配的问题。
* **Purge v1**：移除旧的 `docker-compose`（Python 版），避免 PATH 中残留可执行体被误调用。

执行完这些，`docker compose up -d` 应该就能正常跑了。若还报错，把下面两条的完整输出贴一下，我继续顺藤摸瓜：

```bash
docker compose version
apt-cache policy docker-compose-plugin
```






---

## docker走代理：

你的表达没问题。
先判断：这不是 `docker-compose.yaml` 的问题（那个 `version` 警告只是提示可删），而是 **Docker 守护进程没走你本机 7890 代理**，因此拉取 `registry-1.docker.io` 超时；`wget` 能通只是**你的 shell**走了代理，和 **dockerd** 的网络路径无关。

---

# 最优方案

**给 Docker 守护进程配置 HTTP/HTTPS 代理并重启服务**，随后再 `docker compose up -d`。

## 操作

```bash
# 1) 为 dockerd 配置代理（systemd drop-in）
sudo mkdir -p /etc/systemd/system/docker.service.d
sudo tee /etc/systemd/system/docker.service.d/proxy.conf >/dev/null <<'EOF'
[Service]
Environment="HTTP_PROXY=http://127.0.0.1:7890"
Environment="HTTPS_PROXY=http://127.0.0.1:7890"
Environment="NO_PROXY=localhost,127.0.0.1,::1,.local,.internal,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16"
EOF

# 2) 重新加载并重启 Docker
sudo systemctl daemon-reload
sudo systemctl restart docker

# 3) 校验代理已生效（应能看到上面三行 Environment）
systemctl show --property=Environment docker | sed 's/^Environment=//'

# 4) 快速连通性测试（能很快拉下就说明通了）
docker pull hello-world

# 5) （可选）去掉 compose 的旧语法字段，避免警告
sed -i '/^version:/d' docker-compose.yaml

# 6) 回到你的目录启动
docker compose up -d
```

---

## 原理

* `HTTP_PROXY/HTTPS_PROXY` 只对**当前 shell 进程**与其子进程有效；Docker **守护进程**是独立的 systemd 服务，不会继承你的 shell 里的代理环境变量，所以它直连 Docker Hub，导致超时。
* 给 `docker.service` 写入代理环境并重启后，**dockerd 的所有外联**（含拉镜像）都会经过你的 7890 代理；`NO_PROXY` 则让内网/本机流量不走代理。
* `version:` 警告只是 Compose V2 的提示：Compose 规范已不需要顶层 `version` 字段，删除即可，功能不受影响。

> **若你的引擎是 Docker Desktop（可用 `docker context ls` 看当前是否 `desktop-linux`）：**
> 在 *Docker Desktop → Settings → Resources → Proxies* 填入 `http://127.0.0.1:7890`（HTTP/HTTPS），然后 *Apply & Restart*；再执行上面的第 4、6 步即可。


## 将当前用户加入docker命令可访问列表

# 让普通用户也能用 `docker`（免 `sudo`）

## 操作

1. **确保有 `docker` 组**

```bash
getent group docker || sudo groupadd docker
```

2. **把当前用户加入 `docker` 组**

```bash
sudo usermod -aG docker $USER
```

3. **让新组立刻生效**（二选一）

```bash
# 当前 shell 立刻生效
newgrp docker
# 或者：退出并重新登录；WSL2 用 PowerShell 执行：
# wsl --shutdown
```

4. **验证**

```bash
id -nG | tr ' ' '\n' | grep -Fx docker   # 应能看到“docker”
ls -l /var/run/docker.sock               # 期望是 root docker 以及 rw-rw----
docker ps                                # 不报权限错误
# 再跑个最小容器：
docker run --rm hello-world
```

5. **（可选）确保 Docker 服务已启动并开机自启**（常规 Linux，非 WSL2）

```bash
sudo systemctl enable --now docker
```

---

## 原理

* `docker` CLI 通过 **Unix Socket**：`/var/run/docker.sock` 与守护进程通信。该 socket 默认属于 **用户：root**、**用户组：docker**，权限 **660**；把用户加入 `docker` 组即可获得读写权限，从而免 `sudo`。
* `newgrp docker` 只影响当前 shell 会话；重新登录/重启（或 WSL2 的 `wsl --shutdown`）能让所有会话生效。
* 只要能读写 `docker.sock`，`docker compose`（新插件）和旧版 `docker-compose` 一样都能用。
* **安全提醒**：加入 `docker` 组基本等同获得 **root 等级能力**（能挂载主机目录、提权参数等）。单机开发很方便；多用户/高安全环境应谨慎。需要更强隔离时，再考虑 **rootless Docker**（代价是部分功能限制）。
