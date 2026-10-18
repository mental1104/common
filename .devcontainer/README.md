# NAS VS Code 开发环境

本配置在 NAS 上构建独立的多语言开发镜像，通过 VS Code Remote-SSH 连接 NAS，再使用 Dev Containers 进入容器。容器不运行 sshd、不挂载 Docker Socket，也不需要把 SSH 凭据写入镜像。

## 首次部署

1. 将 `.devcontainer/.env.example` 复制为 `.devcontainer/.env`，按实际路径修改配置。
2. 在 NAS SSH 中执行 `stat -c '%u:%g' /volume2/workflow/common`，将输出的 UID/GID 填入 `.env`。
3. 创建缓存目录并将其所有权交给上述 UID/GID：

   ```sh
   set -a
   . /volume2/workflow/common/.devcontainer/.env
   set +a
   mkdir -p "$DEVCONTAINER_DATA_ROOT/vscode-server" \\
     "$DEVCONTAINER_DATA_ROOT/go-bin" "$DEVCONTAINER_DATA_ROOT/go-mod" \\
     "$DEVCONTAINER_DATA_ROOT/cargo-registry" "$DEVCONTAINER_DATA_ROOT/cargo-git" \\
     "$DEVCONTAINER_DATA_ROOT/cache"
   sudo chown -R "$DEV_UID:$DEV_GID" "$DEVCONTAINER_DATA_ROOT"
   ```

4. 在 NAS SSH 中构建并启动容器：

   ```sh
   cd /volume2/workflow/common/.devcontainer
   docker compose up -d --build
   docker compose ps
   ```

5. 在本地 VS Code 安装 **Remote - SSH** 和 **Dev Containers** 扩展，先通过 Remote-SSH 连接 NAS。
6. 在远程窗口打开 `/volume2/workflow/common`，执行命令面板中的 **Dev Containers: Reopen in Container**。

## 日常使用

- VS Code 工作区根目录为容器中的 `/workspace/workflow`，可访问其中所有仓库。
- `common` 同时映射到 `/opt/mental1104`，用于兼容当前 Go workspace 与 Cargo patch 配置。
- 源码直接保存在 NAS 的 workflow 目录；扩展和语言依赖缓存保存在 `DEVCONTAINER_DATA_ROOT`。
- 修改镜像依赖或工具链后，在 NAS 上执行 `docker compose up -d --build`，然后在 VS Code 中重新连接容器。
- 日常修改仓库代码不需要重建镜像。

## 安全边界与限制

- 不要把 NAS 的 Docker Socket、SSH 私钥、Kubernetes 配置或含凭据的环境文件挂载进开发容器。
- 该容器只访问 workflow 文件和自身缓存；连接 NAS 上其他数据库或服务时，使用已开放的 NAS 服务地址和端口。
- 镜像为多语言通用基线。项目专用依赖仍以各仓库的构建脚本和 README 为准。
