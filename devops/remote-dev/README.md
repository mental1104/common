# NAS 远程开发工作站

本目录提供 common 多语言开发镜像的本地构建、NAS 部署和 SSH 连接流程。镜像沿用仓库现有的 C/C++、Python、Go、Rust、.NET 等工具链，工作区和开发缓存则保存在 NAS SSD。

## 部署结构

- **/volume2/workflow**：挂载到容器 **/workspace/workflow**，容器可访问其中的全部仓库。
- **COMMON_REPO_ROOT**：单独挂载到 **/opt/mental1104**，与镜像中的 ./dev、Go workspace 和 Rust patch 路径保持一致，避免编辑 NAS 源码却构建镜像内旧副本。
- **/volume2/docker/common-dev**：保存 SSH 授权公钥、SSH 主机密钥、VS Code 数据与扩展、语言缓存、CMake 构建目录和临时镜像包。
- 容器 SSH 只绑定 NAS 的 **127.0.0.1**，客户端通过 SSH ProxyJump 连接；容器内只允许 **dev** 用户使用公钥登录。
- 默认不挂载 Docker Socket、宿主机私钥或 Kubernetes 凭据。容器可以通过 NAS 已发布的服务端口访问开发依赖，但不会默认获得管理宿主机 Docker 的权限。

## 1. 在 NAS 准备配置

先确认 common 仓库位于 COMMON_REPO_ROOT，并确认 /volume2/workflow 是实际工作区。首次执行会复制配置模板：

~~~bash
cd /volume2/workflow/common
sudo bash devops/remote-dev/prepare.sh
~~~

编辑 devops/remote-dev/.env，核对以下值：

- WORKFLOW_ROOT：整个工作区根目录。
- COMMON_REPO_ROOT：当前 common 仓库的实际路径。
- DEV_UID 和 DEV_GID：必须与拥有这些仓库的 NAS 账号一致。用命令 id <NAS账号> 查看数字 UID/GID；常见默认值是 1026:100，但请核对。
- DEV_DATA_ROOT：应位于 SSD，默认 /volume2/docker/common-dev。

再次运行准备脚本：

~~~bash
sudo bash devops/remote-dev/prepare.sh
~~~

如果提示缺少 SSH 公钥，脚本已经创建授权文件。先在客户端生成专用密钥（已有专用密钥可复用）：

~~~bash
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519_common_dev -C common-dev
~~~

将客户端公钥追加到 NAS 上的文件 /volume2/docker/common-dev/ssh/authorized_keys，然后再次运行准备脚本。只复制 .pub 公钥，**不要上传私钥**。如果你调整了 DEV_DATA_ROOT，请同步替换上述路径。

## 2. 在本机构建镜像

推荐使用 x86 台式机的 Linux/WSL2 环境构建，避免让 DS224+ 长时间承担完整工具链编译。先在本机的 common 仓库复制配置模板，并将 DEV_UID、DEV_GID 设置为 NAS 上的值：

~~~bash
cp devops/remote-dev/.env.example devops/remote-dev/.env
bash devops/remote-dev/build-image.sh
~~~

脚本会构建 linux/amd64 的 mental1104_dev:latest 镜像。镜像包含开发工具链和 VS Code Server，不包含 SSH 私钥、NAS 授权公钥或数据库凭据。

## 3. 传输并在 NAS 加载镜像

先确保本机 SSH 配置中已经有连接 NAS 的别名，例如 nas。然后在本机执行：

~~~bash
bash devops/remote-dev/transfer-image.sh nas
~~~

把 nas 替换为你自己的 SSH 别名。传输脚本会将压缩镜像写入 NAS 的 image-transfer 目录。随后在 NAS 执行：

~~~bash
sudo docker load -i /volume2/docker/common-dev/image-transfer/mental1104_dev.tar.gz
rm /volume2/docker/common-dev/image-transfer/mental1104_dev.tar.gz
cd /volume2/workflow/common
sudo docker compose --env-file devops/remote-dev/.env -f docker-compose.yaml up -d
~~~

如果 NAS 上的 common 路径或 DEV_DATA_ROOT 不同，请使用 .env 中对应的实际路径。Compose 不会自动从公共镜像仓库拉取同名镜像。

## 4. 配置 VS Code Remote-SSH

在客户端 ~/.ssh/config 中添加一个开发主机条目。nas 必须替换成你已有的 NAS SSH Host 别名：

~~~sshconfig
Host common-dev
  HostName 127.0.0.1
  Port 31104
  User dev
  ProxyJump nas
  IdentityFile ~/.ssh/id_ed25519_common_dev
  IdentitiesOnly yes
~~~

如果 NAS SSH 端口不是默认的 22，在 nas 对应的 Host 条目里配置正确端口。然后先验证：

~~~bash
ssh common-dev
~~~

连接成功后，在 VS Code 中安装或启用 Remote - SSH 扩展，执行 Remote-SSH: Connect to Host... 并选择 common-dev。打开 /workspace/workflow 即可浏览所有仓库；打开 /opt/mental1104 则是 common 仓库的同一份源码挂载。

## 5. 日常更新与运维

更新镜像时，在本机重新运行 build-image.sh 和 transfer-image.sh，在 NAS 重新执行 docker load，再运行：

~~~bash
sudo docker compose --env-file devops/remote-dev/.env -f docker-compose.yaml up -d --force-recreate
~~~

常用检查：

~~~bash
sudo docker logs --tail 100 mental1104_dev
sudo docker ps --filter name=mental1104_dev
sudo docker compose --env-file devops/remote-dev/.env -f docker-compose.yaml down
~~~

down 只移除容器，不会删除 /volume2/docker/common-dev 中的持久化数据；不要对这个服务执行会删除卷或手工清理 SSD 数据目录的操作，除非你明确要重置开发环境。

开发容器有 3 GiB 内存和 2 CPU 的默认上限，以免影响 NAS 上其他服务。需要调整时修改 COMMON_DEV_MEM_LIMIT 或 COMMON_DEV_CPUS。

当前默认不挂载 /var/run/docker.sock。若后续确实需要从容器操作 NAS 的 Docker 守护进程，应单独评估授权边界；该 Socket 实际上赋予容器近乎完整的宿主机控制权，不应作为普通开发功能默认开启。
