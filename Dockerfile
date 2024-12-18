# 使用龙蜥镜像
FROM ubuntu:24.04

ARG HTTP_PROXY
ARG HTTPS_PROXY

ENV HTTP_PROXY=$HTTP_PROXY
ENV HTTPS_PROXY=$HTTPS_PROXY

ENV DEBIAN_FRONTEND=noninteractive

# 更新软件源并使用阿里云镜像
RUN echo "deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    apt-get update && apt-get upgrade -y


RUN chmod 1777 /tmp

## 1. 安装基础库
RUN apt-get install -y \
    gcc \
    g++ \
    make \
    libpq-dev \
    zlib1g-dev \
    libffi-dev \
    libssl-dev \
    libbz2-dev \
    liblzma-dev \
    pkg-config \
    wget && \
    ln -s /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6 && ldconfig


COPY INSTALLROOT/amd64 /
## 2. 编程语言相关操作

### 2.1 python 相关操作

#### 2.1.1 编译python

RUN mkdir -p /usr/local/python3.12 && cd /tmp && tar xzf Python-3.12.3.tgz && \
    cd ./Python-3.12.3 && ./configure --prefix=/usr/local/python3.12 --enable-optimizations --with-lto --with-computed-gotos && \
    make -j "$(nproc)" && make altinstall && rm /tmp/Python-3.12.3.tgz && \
    /usr/local/python3.12/bin/python3.12 -m pip install --upgrade pip setuptools wheel

ENV PATH $PATH:/usr/local/python3.12/bin
RUN ln -s /usr/local/python3.12/bin/python3.12        /usr/local/python3.12/bin/python3 && \
    ln -s /usr/local/python3.12/bin/python3.12        /usr/local/python3.12/bin/python && \
    ln -s /usr/local/python3.12/bin/pydoc3.12         /usr/local/python3.12/bin/pydoc && \
    ln -s /usr/local/python3.12/bin/idle3.12          /usr/local/python3.12/bin/idle && \
    ln -s /usr/local/python3.12/bin/python3.12-config      /usr/local/python3.12/bin/python-config

RUN apt-get install -y \
    clang \
    cmake \
    gdb \
    strace \
    ltrace \
    curl \
    nodejs \
    lua5.3 \
    tree \
    npm \
    git \
    libtinfo6 \
    libncurses-dev \
    libncursesw5 \
    openssl \
    netcat \
    telnet \
    tcpdump \
    iptables \
    openssh-server \
    bison \
    cmake \
    unzip \
    gzip \
    iputils-ping \
    redis-tools \
    mongodb-clients \
    postgresql-client

### 2.2 安装golang
RUN tar -C /usr/local -xzf /tmp/go1.20.5.linux-amd64.tar.gz
ENV PATH $PATH:/usr/local/go/bin

## 3 安装基础系统工具

### 3.1 系统相关

### 3.1.1 neovim # 这一步要放早一点
RUN cd /tmp && tar -zxvf nvim-linux64.tar.gz \ 
    && cd nvim-linux64 && cp -rf ./* /usr/local && \
    ln -s  /usr/local/bin/nvim /usr/local/bin/vim

#### 3.2.2 开放ssh端口

# 生成 SSH 主机密钥
ARG SSH_PRIVATE_KEY
ENV SSH_PRIVATE_KEY=${SSH_PRIVATE_KEY}
# 设置root用户密码（此处将密码设为"password"，实际使用中请使用安全的密码）
RUN echo "root:${SSH_PRIVATE_KEY}" | chpasswd

# 修改SSH配置文件以允许密码认证
RUN mkdir -p /run/sshd && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config

ENV LANG=C.UTF-8

# 暴露 SSH 默认端口
EXPOSE 22


### 2.1.2 pip库安装


RUN echo "PS1='\\[\033[32;1m\]\\u@\\[\033[38;5;214;1m\]\\h:\\[\033[01;34m\]\\w\\[\033[00m\] \$ '" >> /root/.bashrc


# 安装vscode离线ssh连接套件
ENV VSCODE_COMMIT_VERSION=f1a4fb101478ce6ec82fe9627c43efbf9e98c813
RUN mkdir -p /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}
COPY INSTALLROOT/root/ /root/
RUN tar -zxvf /root/.vscode-server/vscode-server-linux-x64.tar.gz -C /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION} --strip 1 && touch /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}/0


RUN pip3 install -i https://pypi.tuna.tsinghua.edu.cn/simple -r /root/requirements.txt && rm -f /root/requirements.txt

## 其他环境变量
RUN ln -s /usr/local/python3.12/bin/python /usr/bin/python && \
    ln -s /usr/local/python3.12/bin/pip /usr/bin/pip && \
    ln -s /usr/local/go/bin/go /usr/bin/go

# 下载并安装插件
RUN while read extension; do \
        /root/.vscode-server/bin/*/bin/code-server --install-extension $extension || true; \
    done < /root/extensions.txt

ENV HTTP_PROXY=
ENV HTTPS_PROXY=

RUN apt-get install -y build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu libpcap-dev libglib2.0-dev pkg-config libpixman-1-dev

RUN cd /tmp && wget https://download.qemu.org/qemu-4.1.0.tar.xz && tar xvJf qemu-4.1.0.tar.xz && cd qemu-4.1.0/ && \
    ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu " && \
    make -j$(nproc) && \
    make install && PATH=$PATH:/opt/qemu/bin && \
    rm -rf /tmp/qemu*

RUN apt-get install -y ffmpeg

COPY cpp /tmp/cpp/
COPY python /tmp/python/

RUN cd /tmp/cpp && mkdir -p build && cd build && cmake .. && make -j "$(nproc)" && make install && \
    cd /tmp/python && pip install .

COPY utils /root/
RUN git config --global core.editor "vim"

CMD ["/usr/sbin/sshd", "-D"]
WORKDIR /root