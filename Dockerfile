ARG BASE_IMAGE=ubuntu:24.04
FROM ${BASE_IMAGE}

ARG HTTP_PROXY
ARG HTTPS_PROXY
ARG NO_PROXY
ARG ALL_PROXY
ARG http_proxy
ARG https_proxy
ARG no_proxy
ARG all_proxy
ARG NUGET_SOURCE=https://api.nuget.org/v3/index.json
ARG WEBBENCH_VERSION=1.5

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
    printf '%s\n' \
      'deb http://mirrors.aliyun.com/ubuntu/ noble main restricted universe multiverse' \
      'deb http://mirrors.aliyun.com/ubuntu/ noble-updates main restricted universe multiverse' \
      'deb http://mirrors.aliyun.com/ubuntu/ noble-backports main restricted universe multiverse' \
      'deb http://mirrors.aliyun.com/ubuntu/ noble-security main restricted universe multiverse' \
      > /etc/apt/sources.list

RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt \
    set -eux; \
    APT_ENV="env -u HTTP_PROXY -u HTTPS_PROXY -u NO_PROXY -u ALL_PROXY -u http_proxy -u https_proxy -u no_proxy -u all_proxy"; \
    $APT_ENV apt-get update; \
    $APT_ENV apt-get install -y --no-install-recommends ca-certificates gnupg curl; \
    update-ca-certificates; \
    clickhouse_key_tmp="$(mktemp)"; \
    if curl -fsSL https://packages.clickhouse.com/CLICKHOUSE-KEY.GPG -o "$clickhouse_key_tmp" \
      || curl -fsSL https://mirrors.tuna.tsinghua.edu.cn/clickhouse/deb/CLICKHOUSE-KEY.GPG -o "$clickhouse_key_tmp"; then \
        gpg --dearmor -o /usr/share/keyrings/clickhouse.gpg "$clickhouse_key_tmp"; \
        echo "deb [signed-by=/usr/share/keyrings/clickhouse.gpg] https://mirrors.tuna.tsinghua.edu.cn/clickhouse/deb stable main" \
          > /etc/apt/sources.list.d/clickhouse.list; \
        $APT_ENV apt-get update; \
        install_clickhouse=1; \
    else \
        echo "[warn] clickhouse key fetch failed; skipping clickhouse repo"; \
        install_clickhouse=0; \
    fi; \
    rm -f "$clickhouse_key_tmp"; \
    $APT_ENV apt-get install -y --no-install-recommends \
      # 构建/调试基础
      build-essential gcc g++ make cmake pkg-config clang clangd clang-format \
      gdb gdb-multiarch strace ltrace \
      # 依赖库
      libpq-dev zlib1g-dev libffi-dev libssl-dev libbz2-dev liblzma-dev \
      libpcap-dev libglib2.0-dev libpixman-1-dev \
      libtinfo6 libncurses-dev libncursesw6 openssl \
      libgtest-dev libbenchmark-dev libboost-all-dev lcov \
      libmpfr-dev libgmp-dev \
      # 网络/运维工具
      curl wget git tree unzip gzip zip jq \
      netcat-openbsd telnet tcpdump iptables iputils-ping \
      openssh-server cron logrotate \
      # DB/中间件客户端
      redis-tools postgresql-client \
      # 多语言工具链（你原本就在装）
      python3 python3-pip python3-venv \
      nodejs npm lua5.3 \
      dotnet-sdk-8.0 aspnetcore-runtime-8.0 \
      # 其他
      exuberant-ctags ffmpeg pandoc xclip xsel vim \
      # qemu 相关（你后续源码编译仍需要）
      qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu bison \
    ; \
    if [ "$install_clickhouse" = "1" ]; then \
        $APT_ENV apt-get install -y --no-install-recommends clickhouse-client; \
    fi; \
    mongo_pkg=""; \
    for cand in mongodb-clients mongodb-mongosh; do \
        if apt-cache show "$cand" >/dev/null 2>&1; then mongo_pkg="$cand"; break; fi; \
    done; \
    if [ -n "$mongo_pkg" ]; then apt-get install -y --no-install-recommends "$mongo_pkg"; fi; \
    if command -v mongosh >/dev/null 2>&1 && ! command -v mongo >/dev/null 2>&1; then \
        ln -sf "$(command -v mongosh)" /usr/local/bin/mongo; \
    fi; \
    rm -rf /var/lib/apt/lists/*; \
    ln -sf /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6; \
    ldconfig; \
    cd /usr/src/googletest && cmake . && make -j"$(nproc)" && make install

# -----------------------------
# A2) 低频：webbench（版本可参数化）
# -----------------------------
RUN set -eux; \
    tmp_dir="$(mktemp -d)"; \
    cd "$tmp_dir"; \
    curl -fsSL "http://home.tiscali.cz/~cz210552/distfiles/webbench-${WEBBENCH_VERSION}.tar.gz" -o webbench.tar.gz; \
    tar -xzf webbench.tar.gz; \
    cd "webbench-${WEBBENCH_VERSION}"; \
    make; \
    make install; \
    cd /; \
    rm -rf "$tmp_dir"

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

ARG RUSTUP_DIST_SERVER=https://rsproxy.cn
ARG RUSTUP_UPDATE_ROOT=https://rsproxy.cn/rustup
ENV GOPROXY=https://goproxy.cn,direct \
    GOPATH=/go \
    GOBIN=/go/bin \
    GO111MODULE=on \
    GOWORK=/opt/mental1104/go.work \
    RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    RUSTUP_DIST_SERVER=${RUSTUP_DIST_SERVER} \
    RUSTUP_UPDATE_ROOT=${RUSTUP_UPDATE_ROOT} \
    PATH=/usr/local/cargo/bin:/usr/local/go/bin:/go/bin:$PATH

RUN --mount=type=cache,target=/go/pkg/mod \
    --mount=type=cache,target=/root/.cache/go-build \
    set -eux; \
    go install golang.org/x/tools/gopls@latest; \
    ln -sf /usr/local/go/bin/go /usr/bin/go; \
    ln -sf /go/bin/gopls /usr/bin/gopls

# -----------------------------
# D2) 中频：Rust 工具链（rustup + cargo）
# -----------------------------
RUN set -eux; \
    mkdir -p "$RUSTUP_HOME" "$CARGO_HOME"; \
    curl -fsSL https://sh.rustup.rs -o /tmp/rustup-init.sh; \
    sh /tmp/rustup-init.sh -y --no-modify-path --profile minimal --default-toolchain stable; \
    rm -f /tmp/rustup-init.sh; \
    rustup --version; \
    cargo --version; \
    rustc --version

# -----------------------------
# E) 低频：Okteto + Syncthing（官网最新）
# -----------------------------
RUN set -eux; \
    mkdir -p /root/.okteto; \
    CURL_RETRY="--retry 5 --retry-delay 2 --retry-connrefused"; \
    syncthing_tag="$(curl -fsSL $CURL_RETRY -o /dev/null -w '%{url_effective}' https://github.com/syncthing/syncthing/releases/latest | awk -F/ '{print $NF}')"; \
    if [ -z "$syncthing_tag" ]; then \
        syncthing_tag="$(curl -fsSL $CURL_RETRY https://github.com/syncthing/syncthing/releases/latest | grep -Eo 'syncthing-linux-amd64-v[0-9]+\\.[0-9]+\\.[0-9]+\\.tar\\.gz' | head -n 1 | sed 's/^syncthing-linux-amd64-//; s/\\.tar\\.gz$//')"; \
    fi; \
    test -n "$syncthing_tag"; \
    syncthing_url="https://github.com/syncthing/syncthing/releases/download/${syncthing_tag}/syncthing-linux-amd64-${syncthing_tag}.tar.gz"; \
    curl -fsSL $CURL_RETRY "$syncthing_url" -o /tmp/syncthing.tar.gz; \
    syncthing_dir="$(tar -tf /tmp/syncthing.tar.gz | head -n 1 | cut -d/ -f1)"; \
    tar -xzf /tmp/syncthing.tar.gz -C /tmp; \
    install -m 0755 "/tmp/${syncthing_dir}/syncthing" /root/.okteto/syncthing; \
    ln -sf /root/.okteto/syncthing /usr/local/bin/syncthing; \
    rm -rf /tmp/syncthing.tar.gz "/tmp/${syncthing_dir}"; \
    okteto_tmp="$(mktemp)"; \
    okteto_ok=0; \
    for url in \
      "https://downloads.okteto.com/cli/okteto-Linux-x86_64" \
      "https://github.com/okteto/okteto/releases/latest/download/okteto-Linux-x86_64" \
      "https://github.com/okteto/okteto/releases/latest/download/okteto-linux-amd64"; do \
        if curl -fsSL $CURL_RETRY "$url" -o "$okteto_tmp"; then \
            okteto_ok=1; \
            break; \
        fi; \
    done; \
    if [ "$okteto_ok" != "1" ]; then \
        echo "[error] failed to download okteto CLI"; \
        exit 1; \
    fi; \
    install -m 0755 "$okteto_tmp" /root/.okteto/okteto; \
    rm -f "$okteto_tmp"; \
    ln -sf /root/.okteto/okteto /usr/local/bin/okteto

# -----------------------------
# F) 中频：VSCode Server（只受 commit/version 影响）
# 参考：https://www.cnblogs.com/michaelcjl/p/18262833
# -----------------------------
# VSCode Server 版本信息（离线预装，修改 VSCODE_COMMIT 会触发此层重建）
# - VSCODE_COMMIT: 585eba7c0c34fd6b30faac7c62a42050bfbc0086（VSCode 1.108.1, `code --version` 第 2 行）
ARG VSCODE_COMMIT=585eba7c0c34fd6b30faac7c62a42050bfbc0086
ARG TARGETARCH=amd64

RUN set -eux; \
    if ! command -v curl >/dev/null 2>&1 || ! command -v tar >/dev/null 2>&1; then \
        APT_ENV="env -u HTTP_PROXY -u HTTPS_PROXY -u NO_PROXY -u ALL_PROXY -u http_proxy -u https_proxy -u no_proxy -u all_proxy"; \
        $APT_ENV apt-get update; \
        $APT_ENV apt-get install -y --no-install-recommends curl tar ca-certificates; \
        rm -rf /var/lib/apt/lists/*; \
    fi; \
    mkdir -p /root/.vscode-server/cli/servers /root/.vscode-server/data /root/.vscode-server/extensions; \
    rm -rf "/root/.vscode-server/cli/servers/Stable-${VSCODE_COMMIT}"; \
    rm -f "/root/.vscode-server/code-${VSCODE_COMMIT}"; \
    case "${TARGETARCH}" in \
      amd64|x86_64) \
        server_url="https://vscode.download.prss.microsoft.com/dbazure/download/stable/${VSCODE_COMMIT}/vscode-server-linux-x64.tar.gz"; \
        cli_url="https://vscode.download.prss.microsoft.com/dbazure/download/stable/${VSCODE_COMMIT}/vscode_cli_alpine_x64_cli.tar.gz"; \
        ;; \
      arm64) \
        server_url="https://vscode.download.prss.microsoft.com/dbazure/download/stable/${VSCODE_COMMIT}/vscode-server-linux-arm64.tar.gz"; \
        cli_url="https://vscode.download.prss.microsoft.com/dbazure/download/stable/${VSCODE_COMMIT}/vscode_cli_alpine_arm64_cli.tar.gz"; \
        ;; \
      *) \
        echo "[error] unsupported TARGETARCH: ${TARGETARCH}"; \
        exit 1; \
        ;; \
    esac; \
    tmp_dir="$(mktemp -d /tmp/vscode.XXXXXX)"; \
    curl -fsSL "$server_url" -o "$tmp_dir/vscode-server.tar.gz"; \
    curl -fsSL "$cli_url" -o "$tmp_dir/vscode-cli.tar.gz"; \
    tar -xzf "$tmp_dir/vscode-server.tar.gz" -C "$tmp_dir"; \
    server_dir="$(find "$tmp_dir" -maxdepth 1 -type d -name 'vscode-server-linux-*' | head -n 1)"; \
    test -n "$server_dir"; \
    mkdir -p "/root/.vscode-server/cli/servers/Stable-${VSCODE_COMMIT}"; \
    mv "$server_dir" "/root/.vscode-server/cli/servers/Stable-${VSCODE_COMMIT}/server"; \
    tar -xzf "$tmp_dir/vscode-cli.tar.gz" -C "$tmp_dir"; \
    cli_bin="$(find "$tmp_dir" -maxdepth 2 -type f -name code | head -n 1)"; \
    test -n "$cli_bin"; \
    install -m 0755 "$cli_bin" "/root/.vscode-server/code-${VSCODE_COMMIT}"; \
    rm -rf "$tmp_dir"

# -----------------------------
# G) 高频：VSCode Extensions（耗时，尽量早于代码层）
# -----------------------------
COPY devops/INSTALLROOT/root/extensions.list /root/vscode-extensions.list

RUN set -eux; \
    server_dir="/root/.vscode-server/cli/servers/Stable-${VSCODE_COMMIT}/server"; \
    install_cli="${server_dir}/bin/code-server"; \
    if [ ! -x "$install_cli" ]; then \
        install_cli="$(find "$server_dir" -maxdepth 3 -type f \( -name code-server -o -name code \) | head -n 1)"; \
    fi; \
    if [ -z "$install_cli" ] || [ ! -x "$install_cli" ]; then \
        echo "[error] VSCode CLI not found for extension install"; \
        exit 1; \
    fi; \
    if [ -s /root/vscode-extensions.list ]; then \
        failed=0; \
        failed_ext=""; \
        while read -r extension; do \
            case "$extension" in ""|\#*) continue ;; esac; \
            if ! "$install_cli" \
                --user-data-dir /root/.vscode-server/data \
                --extensions-dir /root/.vscode-server/extensions \
                --install-extension "$extension"; then \
                echo "[error] failed to install extension: $extension" >&2; \
                failed=1; \
                if [ -z "$failed_ext" ]; then \
                    failed_ext="$extension"; \
                else \
                    failed_ext="${failed_ext},${extension}"; \
                fi; \
            fi; \
        done < /root/vscode-extensions.list; \
        if [ "$failed" -ne 0 ]; then \
            echo "[error] extension install failures: ${failed_ext}" >&2; \
            exit 1; \
        fi; \
    fi; \
    rm -f /root/vscode-extensions.list

# -----------------------------
# H) 中频：pip 依赖（只受 requirements.txt 影响，单独层）
# -----------------------------
COPY python/requirements.txt /root/requirements.txt

RUN --mount=type=cache,target=/root/.cache/pip \
    set -eux; \
    sed '/^mental1104_export_layer /d' /root/requirements.txt > /root/requirements.docker.txt; \
    pip3 install -i https://mirrors.aliyun.com/pypi/simple -r /root/requirements.docker.txt --break-system-packages; \
    rm -f /root/requirements.txt /root/requirements.docker.txt

# -----------------------------
# I) 低频：配置文件（htop/clang-format/时区）
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
# J) 高频：你的自定义代码（使用 ./dev install）
# -----------------------------
COPY . /opt/mental1104/
RUN set -eux; \
    cd /opt/mental1104; \
    mkdir -p /root/.config/NuGet; \
    printf '%s\n' \
      '<?xml version="1.0" encoding="utf-8"?>' \
      '<configuration>' \
      '  <packageSources>' \
      '    <add key="local" value="/usr/local/share/nuget" />' \
      "    <add key=\"nuget\" value=\"${NUGET_SOURCE}\" />" \
      '  </packageSources>' \
      '</configuration>' \
      > /root/.config/NuGet/NuGet.Config; \
    DOTNET_CLEAN_ALLOW_FAIL=1 ./dev clean-all; \
    ./dev setup-dotnet; \
    ./dev build dotnet; \
    ./dev install dotnet; \
    mkdir -p /usr/local/share/nuget; \
    dotnet pack dotnet/src/Mental1104/Mental1104.csproj --configuration Release --output /usr/local/share/nuget

RUN set -eux; \
    cd /opt/mental1104; \
    ./dev build rust; \
    ./dev install rust; \
    mkdir -p /root/.cargo; \
    printf '%s\n' \
      '[patch.crates-io]' \
      'mental1104 = { path = "/opt/mental1104/rust/mental1104" }' \
      > /root/.cargo/config.toml

RUN set -eux; \
    cd /opt/mental1104; \
    ./dev build go; \
    ./dev install go; \
    mkdir -p /go/src/github.com/mental1104/common; \
    ln -sf /opt/mental1104/golang /go/src/github.com/mental1104/common/golang

RUN set -eux; \
    cd /opt/mental1104; \
    BUILD_SUBMODULES=all SKIP_SUBMODULES=cpp/lib/boost ./dev build cpp; \
    ./dev install cpp

RUN --mount=type=cache,target=/root/.cache/pip \
    set -eux; \
    cd /opt/mental1104; \
    ./dev install python

# -----------------------------
# K) 高频：VSCode settings（只受 extensions.json 影响）
# -----------------------------
COPY devops/INSTALLROOT/root/extensions.json /root/vscode-extensions.json
COPY devops/INSTALLROOT/root/vscode_extensions.py /root/vscode-extensions.py

RUN set -eux; \
    python3 /root/vscode-extensions.py; \
    rm -f /root/vscode-extensions.py

# -----------------------------
# L) 高频/变量层：SSH 密码注入与 sshd 配置（必须放到最后，避免打爆缓存）
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
