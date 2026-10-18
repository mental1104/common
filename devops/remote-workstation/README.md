# NAS 远程开发镜像

本目录说明如何将仓库根目录的 Dockerfile 用作 NAS 上的 VS Code Remote-SSH 开发镜像。工具链与 common 库在镜像构建时安装，运行时仓库代码来自 NAS 上的持久化工作区。

## 安全约定

- SSH 不使用构建参数或镜像环境变量传递密码。
- 禁用密码认证与 root SSH 登录，只允许 `dev` 用户使用公钥登录。
- SSH 主机密钥在首次启动时生成，并持久化到 NAS SSD。
- 工作区和用户主目录通过 bind mount 持久化，重建容器不会删除仓库或 VS Code Server 状态。
- 不挂载 Docker Socket、宿主机 SSH 私钥或 Kubernetes 凭据。

## 构建参数

`DEV_UID` 和 `DEV_GID` 应与 NAS 上拥有 workflow 工作区的用户 UID/GID 一致。NAS 上可用 `id mental1104` 查询。Compose 部署样例由 `homelab-services/services/dev-workstation` 管理。

## 远程连接

在客户端 VS Code 安装 Microsoft 的 Remote - SSH 扩展，然后连接：

```ssh
ssh dev@<NAS-LAN-IP> -p 31104
```

首次连接时核对并接受 SSH 主机指纹。请勿将 31104 端口直接映射到公网；远程访问应通过已有的可信私有网络或 VPN。
