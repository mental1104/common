# syntax=docker/dockerfile:1.7
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    TZ=Asia/Shanghai

# 让 /tmp 权限明确且稳定
RUN chmod 1777 /tmp

# -----------------------------
# A) 最低频：系统源 + 系统包（尽量一次性）
#    说明：这里不要做 apt upgrade，会降低可复现性且让缓存价值变差
# -----------------------------
RUN set -eux; \
    cat > /etc/apt/sources.list <<'EOF' \
deb http://mirrors.aliyun.com/ubuntu/ noble main restricted universe multiverse \
deb http://mirrors.aliyun.com/ubuntu/ noble-updates main restricted universe multiverse \
deb http://mirrors.aliyun.com/ubuntu/ noble-backports main restricted universe multiverse \
deb http://mirrors.aliyun.com/ubuntu/ noble-security main restricted universe multiverse \
EOF \
    ; \
    echo "deb https://mirrors.tuna.tsinghua.edu.cn/clickhouse/deb stable main" > /etc/apt/sources.list.d/clickhouse.list

RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt \
    set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      # 构建/调试基础
      build-essential gcc g++ make cmake pkg-config clang clangd clang-format \
      gdb gdb-multiarch strace ltrace \
      # 依赖库
      libpq-dev zlib1g-dev libffi-dev libssl-dev libbz2-dev liblzma-dev \
      libpcap-dev libglib2.0-dev libpixman-1-dev \
      libtinfo6 libncurses-dev libncursesw5 openssl \
      libgtest-dev libbenchmark-dev libboost-all-dev lcov \
      libmpfr-dev libgmp-dev \
      # 网络/运维工具
      curl wget git tree unzip gzip zip jq \
      netcat-openbsd telnet tcpdump iptables iputils-ping \
      openssh-server cron logrotate \
      # DB/中间件客户端
      redis-tools mongodb-clients postgresql-client clickhouse-client \
      # 多语言工具链（你原本就在装）
      python3 python3-pip python3-venv \
      nodejs npm lua5.3 \
      rustc cargo \
      dotnet-sdk-8.0 aspnetcore-runtime-8.0 \
      # 其他
      ffmpeg pandoc xclip xsel vim \
      # qemu 相关（你后续源码编译仍需要）
      qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu bison \
    ; \
    rm -rf /var/lib/apt/lists/*; \
    ln -sf /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6; \
    ldconfig; \
    cd /usr/src/googletest && cmake . && make -j"$(nproc)" && make install

# -----------------------------
# B) 中频：离线资源（INSTALLROOT）先拷贝
#    说明：仅保留当前目录里已有的配置/清单文件
# -----------------------------
COPY devops/INSTALLROOT/amd64 /

# -----------------------------
# C) 低频但耗时：qemu 源码编译（单独一段，且放在“离线资源”之后）
#    说明：这样 INSTALLROOT 不变时，这一层也更稳定
# -----------------------------
RUN set -eux; \
    cd /tmp; \
    wget -q https://download.qemu.org/qemu-4.1.0.tar.xz; \
    tar -xvJf qemu-4.1.0.tar.xz; \
    cd qemu-4.1.0/; \
    ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu "; \
    make -j"$(nproc)"; \
    make install; \
    rm -rf /tmp/qemu*

# -----------------------------
# D) 中频：Go 工具链 + gopls
# -----------------------------
ARG GO_VERSION=1.23.4
RUN set -eux; \
    curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" -o /tmp/go.tar.gz; \
    tar -C /usr/local -xzf /tmp/go.tar.gz; \
    rm -f /tmp/go.tar.gz

ENV GOPROXY=https://goproxy.cn,direct \
    GOPATH=/go \
    GOBIN=/go/bin \
    GO111MODULE=on \
    GOWORK=/opt/mental1104/go.work \
    PATH=/usr/local/go/bin:/go/bin:$PATH

RUN --mount=type=cache,target=/go/pkg/mod \
    --mount=type=cache,target=/root/.cache/go-build \
    set -eux; \
    go install golang.org/x/tools/gopls@latest; \
    ln -sf /usr/local/go/bin/go /usr/bin/go; \
    ln -sf /go/bin/gopls /usr/bin/gopls

# -----------------------------
# E) 低频：Okteto + Syncthing（官网最新）
# -----------------------------
RUN set -eux; \
    mkdir -p /root/.okteto; \
    syncthing_url="$(curl -fsSL https://api.github.com/repos/syncthing/syncthing/releases/latest | jq -r '.assets[] | select(.name | test("^syncthing-linux-amd64-.*\\.tar\\.gz$")) | .browser_download_url' | head -n 1)"; \
    test -n "$syncthing_url"; \
    curl -fsSL "$syncthing_url" -o /tmp/syncthing.tar.gz; \
    syncthing_dir="$(tar -tf /tmp/syncthing.tar.gz | head -n 1 | cut -d/ -f1)"; \
    tar -xzf /tmp/syncthing.tar.gz -C /tmp; \
    install -m 0755 "/tmp/${syncthing_dir}/syncthing" /root/.okteto/syncthing; \
    ln -sf /root/.okteto/syncthing /usr/local/bin/syncthing; \
    rm -rf /tmp/syncthing.tar.gz "/tmp/${syncthing_dir}"; \
    curl -fsSL "https://downloads.okteto.com/cli/okteto-Linux-x86_64" -o /root/.okteto/okteto; \
    chmod +x /root/.okteto/okteto; \
    ln -sf /root/.okteto/okteto /usr/local/bin/okteto

# -----------------------------
# F) 中频：VSCode Server + Extensions（只受 commit/version 与 extensions.txt 影响）
# -----------------------------
ENV VSCODE_COMMIT_VERSION=f1a4fb101478ce6ec82fe9627c43efbf9e98c813

RUN set -eux; \
    mkdir -p /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}; \
    curl -fsSL "https://update.code.visualstudio.com/commit:${VSCODE_COMMIT_VERSION}/server-linux-x64/stable" \
      -o /tmp/vscode-server.tar.gz; \
    tar -xzf /tmp/vscode-server.tar.gz \
      -C /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION} --strip 1; \
    rm -f /tmp/vscode-server.tar.gz; \
    touch /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}/0

COPY devops/INSTALLROOT/root/extensions.txt /root/extensions.txt

RUN set -eux; \
    while read -r extension; do \
        /root/.vscode-server/bin/*/bin/code-server --install-extension "$extension" || true; \
    done < /root/extensions.txt

# -----------------------------
# G) 中频：pip 依赖（只受 requirements.txt 影响，单独层）
# -----------------------------
COPY devops/INSTALLROOT/root/requirements.txt /root/requirements.txt

RUN --mount=type=cache,target=/root/.cache/pip \
    set -eux; \
    pip3 install -i https://mirrors.aliyun.com/pypi/simple -r /root/requirements.txt --break-system-packages; \
    rm -f /root/requirements.txt

# -----------------------------
# H) 低频：配置文件（htop/clang-format/时区）
# -----------------------------
COPY devops/INSTALLROOT/root/.config /root/.config

# clang-format 放置（低频）
COPY devops/INSTALLROOT/root/.clang-format /usr/lib/llvm-18/bin
COPY devops/INSTALLROOT/root/.clang-format /root

# 时区/缓存（低频）
RUN set -eux; \
    ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime; \
    ldconfig

# -----------------------------
# I) 高频：你的自定义代码（使用 ./dev install）
# -----------------------------
COPY . /opt/mental1104/
RUN --mount=type=cache,target=/root/.cache/pip \
    set -eux; \
    cd /opt/mental1104; \
    BUILD_SUBMODULES=all ./dev build cpp; \
    ./dev install cpp; \
    ./dev build go; \
    ./dev install go; \
    ./dev build rust; \
    ./dev install rust; \
    ./dev build dotnet; \
    ./dev install dotnet; \
    ./dev install python; \
    mkdir -p /go/src/github.com/mental1104/common; \
    ln -sf /opt/mental1104/golang /go/src/github.com/mental1104/common/golang; \
    mkdir -p /root/.cargo; \
    printf '%s\n' \
      '[patch.crates-io]' \
      'mental1104 = { path = "/opt/mental1104/rust/mental1104" }' \
      > /root/.cargo/config.toml; \
    mkdir -p /usr/local/share/nuget; \
    dotnet pack dotnet/src/Mental1104/Mental1104.csproj --configuration Release --output /usr/local/share/nuget; \
    mkdir -p /root/.config/NuGet; \
    printf '%s\n' \
      '<?xml version="1.0" encoding="utf-8"?>' \
      '<configuration>' \
      '  <packageSources>' \
      '    <add key="local" value="/usr/local/share/nuget" />' \
      '    <add key="nuget.org" value="https://api.nuget.org/v3/index.json" />' \
      '  </packageSources>' \
      '</configuration>' \
      > /root/.config/NuGet/NuGet.Config

# -----------------------------
# J) 高频/变量层：SSH 密码注入与 sshd 配置（必须放到最后，避免打爆缓存）
# -----------------------------
ARG SSH_PRIVATE_KEY
ENV SSH_PRIVATE_KEY=${SSH_PRIVATE_KEY}

RUN set -eux; \
    git config --global core.editor "vim"; \
    echo "PS1='\\[\033[32;1m\]\\u@\\[\033[38;5;214;1m\]\\h:\\[\033[01;34m\]\\w\\[\033[00m\] \$ '" >> /root/.bashrc; \
    touch /root/.env; \
    echo "MENTAL1104_NOENVRION=TRUE" >> /root/.env; \
    echo "export \$(grep -v '^#' ~/.env | xargs)" >> /root/.bashrc; \
    echo "alias vi='vim'" >> /root/.bashrc; \
    echo "alias view='vim -R'" >> /root/.bashrc; \
    echo "alias python='python3'" >> /root/.bashrc; \
    echo 'alias redis="redis-cli -h ${REDIS_HOST} -p ${REDIS_PORT}"' >> /root/.bashrc; \
    echo 'alias clickhouse="clickhouse-client -h ${CLICKHOUSE_HOST} --port ${CLICKHOUSE_PORT} -d ${CLICKHOUSE_DATABASE} -u ${CLICKHOUSE_USER}"' >> /root/.bashrc; \
    echo 'alias mongodb="mongo --host ${MONGO_HOST} --port ${MONGO_PORT}"' >> /root/.bashrc; \
    echo "root:${SSH_PRIVATE_KEY}" | chpasswd; \
    mkdir -p /run/sshd; \
    echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config; \
    echo 'PermitRootLogin yes' >> /etc/ssh/sshd_config

EXPOSE 22
WORKDIR /root
CMD ["/usr/sbin/sshd", "-D"]
