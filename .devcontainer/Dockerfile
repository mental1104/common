# syntax=docker/dockerfile:1
ARG UBUNTU_VERSION=24.04
FROM ubuntu:${UBUNTU_VERSION}

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH
ARG DEV_UID=1000
ARG DEV_GID=1000
ARG GO_VERSION=1.23.4
ARG RUSTUP_DIST_SERVER=https://rsproxy.cn
ARG RUSTUP_UPDATE_ROOT=https://rsproxy.cn/rustup

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TZ=Asia/Shanghai \
    GOPROXY=https://goproxy.cn,direct \
    RUSTUP_DIST_SERVER=${RUSTUP_DIST_SERVER} \
    RUSTUP_UPDATE_ROOT=${RUSTUP_UPDATE_ROOT} \
    RUSTUP_HOME=/usr/local/rustup \
    GOPATH=/home/developer/go \
    GOBIN=/home/developer/go/bin \
    CARGO_HOME=/usr/local/cargo \
    GOWORK=/opt/mental1104/go.work \
    PATH=/home/developer/go/bin:/usr/local/go/bin:/usr/local/cargo/bin:/usr/local/bin:$PATH

# 安装 common 多语言开发、调试和常用数据库客户端依赖。
RUN set -eux; \
    chmod 1777 /tmp; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      ca-certificates curl wget gnupg software-properties-common \
      build-essential gcc g++ make cmake ninja-build pkg-config \
      clang clangd clang-format gdb gdb-multiarch strace ltrace \
      libpq-dev zlib1g-dev libffi-dev libssl-dev libbz2-dev liblzma-dev \
      libpcap-dev libglib2.0-dev libpixman-1-dev libtinfo6 libncurses-dev \
      libgtest-dev libbenchmark-dev libboost-all-dev lcov \
      libmpfr-dev libgmp-dev libtirpc-dev \
      git git-lfs tree unzip gzip zip jq netcat-openbsd telnet tcpdump \
      iptables iputils-ping openssh-client openssh-server redis-tools postgresql-client \
      python3 python3-pip python3-venv python3-dev nodejs npm lua5.3 \
      exuberant-ctags ffmpeg pandoc vim \
      qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu bison \
      default-jdk maven; \
    rm -rf /var/lib/apt/lists/*

# .NET SDK 使用微软官方 Ubuntu 软件源，支持当前构建目标架构。
RUN set -eux; \
    curl -fsSL https://packages.microsoft.com/config/ubuntu/24.04/packages-microsoft-prod.deb \
      -o /tmp/packages-microsoft-prod.deb; \
    dpkg -i /tmp/packages-microsoft-prod.deb; \
    rm -f /tmp/packages-microsoft-prod.deb; \
    apt-get update; \
    apt-get install -y --no-install-recommends dotnet-sdk-8.0; \
    rm -rf /var/lib/apt/lists/*

# 按目标架构安装固定版本 Go，避免把 AMD64 二进制带入 ARM64 镜像。
RUN set -eux; \
    case "${TARGETARCH}" in \
      amd64) go_arch=amd64 ;; \
      arm64) go_arch=arm64 ;; \
      *) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
    esac; \
    curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-${go_arch}.tar.gz" -o /tmp/go.tar.gz; \
    tar -C /usr/local -xzf /tmp/go.tar.gz; \
    rm -f /tmp/go.tar.gz

# 安装 Rust stable 工具链，并让工具链本身对开发用户只读共享。
RUN set -eux; \
    mkdir -p "${RUSTUP_HOME}" /usr/local/cargo; \
    curl -fsSL https://sh.rustup.rs -o /tmp/rustup-init.sh; \
    sh /tmp/rustup-init.sh -y --no-modify-path --profile minimal --default-toolchain stable; \
    rm -f /tmp/rustup-init.sh; \
    chmod -R a+rX "${RUSTUP_HOME}" /usr/local/cargo

# 使用可配置 UID/GID 的普通用户，保证 bind mount 中新建文件归属正确。
RUN set -eux; \
    if ! getent group "${DEV_GID}" >/dev/null; then groupadd --gid "${DEV_GID}" developer; fi; \
    useradd --uid "${DEV_UID}" --gid "${DEV_GID}" --create-home --shell /bin/bash developer; \
    passwd -d developer; \
    mkdir -p /home/developer/.cargo/registry /home/developer/.cargo/git \
      /home/developer/.cache /home/developer/go/bin /home/developer/go/pkg/mod; \
    chown -R developer:"$(id -gn developer)" /home/developer; \
    printf '%s\n' \
      '[patch.crates-io]' \
      'mental1104 = { path = "/opt/mental1104/rust/mental1104" }' \
      > /home/developer/.cargo/config.toml; \
    chown developer:"$(id -gn developer)" /home/developer/.cargo/config.toml; \
    printf '%s\n' \
      "alias vi='vim'" \
      "alias python='python3'" \
      'export MENTAL1104_NOENVRION=TRUE' \
      >> /home/developer/.bashrc; \
    mkdir -p /home/developer/.ssh /var/lib/sshd-hostkeys /etc/ssh/sshd_config.d; \
    touch /home/developer/.ssh/authorized_keys; \
    chown -R developer:"$(id -gn developer)" /home/developer/.ssh; \
    chmod 700 /home/developer/.ssh; \
    chmod 600 /home/developer/.ssh/authorized_keys

COPY sshd_config /etc/ssh/sshd_config
COPY sshd-entrypoint.sh /usr/local/sbin/sshd-entrypoint
RUN chmod 0755 /usr/local/sbin/sshd-entrypoint

ENV CARGO_HOME=/home/developer/.cargo
WORKDIR /workspace/workflow
# sshd 需要 root 启动；SSH 登录后仍以 developer 用户运行开发工具。
USER root
CMD ["/usr/local/sbin/sshd-entrypoint"]
