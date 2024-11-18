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
    wget \
    libpq-dev \
    zlib1g-dev \
    libffi-dev \
    libssl-dev \
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
    gzip && \
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
RUN mkdir /var/run/sshd && ssh-keygen -A && echo 'root:${SSH_PRIVATE_KEY}' | chpasswd && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed -i 's/#PubkeyAuthentication yes/PubkeyAuthentication yes/' /etc/ssh/sshd_config && \
    echo "PasswordAuthentication no" >> /etc/ssh/sshd_config
ENV LANG=C.UTF-8

# 暴露 SSH 默认端口
EXPOSE 22

## 4 安装中间件连接工具

# 安装数据库连接工具


# 安装 Redis CLI
RUN apt-get install -y redis-tools

# 安装 MongoDB 客户端
RUN apt-get install -y mongodb-clients

# 安装 PostgreSQL 客户端 (psql)
RUN apt-get install -y postgresql-client

### 2.1.2 pip库安装


RUN echo "PS1='\\[\033[32;1m\]\\u@\\[\033[38;5;214;1m\]\\h:\\[\033[01;34m\]\\w\\[\033[00m\] \$ '" >> /root/.bashrc


# 安装vscode离线ssh连接套件
ENV VSCODE_COMMIT_VERSION=f1a4fb101478ce6ec82fe9627c43efbf9e98c813
RUN mkdir -p /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}
COPY INSTALLROOT/root/ /root/
RUN tar -zxvf /root/.vscode-server/vscode-server-linux-x64.tar.gz -C /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION} --strip1 && touch /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}/0


RUN pip3 install -i https://pypi.tuna.tsinghua.edu.cn/simple -r /root/requirements.txt && rm -f /root/requirements.txt

## 其他环境变量
RUN ln -s /usr/local/python3.12/bin/python /usr/bin/python && \
    ln -s /usr/local/python3.12/bin/pip /usr/bin/pip && \
    ln -s /usr/local/go/bin/go /usr/bin/go

ENV HTTP_PROXY=
ENV HTTPS_PROXY=

CMD ["/usr/sbin/sshd", "-D"]
WORKDIR /root