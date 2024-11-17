# 使用龙蜥镜像
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# 更新软件源并使用阿里云镜像
RUN echo "deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    apt-get update && apt-get upgrade -y


RUN chmod 1777 /tmp

COPY INSTALLROOT/amd64 /

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
    lua5.4 \
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
    openssh-server && \
    ln -s /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6 && ldconfig

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
    
RUN apt-get install -y bison cmake unzip

#### 3.2.2 开放ssh端口

# 生成 SSH 主机密钥
ARG SSH_PRIVATE_KEY
RUN mkdir /var/run/sshd && ssh-keygen -A && echo 'root:$(SSH_PRIVATE_KEY)' | chpasswd && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
ENV LANG=C.UTF-8

# 暴露 SSH 默认端口
EXPOSE 22

## 4 安装中间件连接工具

### 2.1.2 pip库安装
RUN pip3 install -i https://pypi.tuna.tsinghua.edu.cn/simple -r /tmp/requirements.txt && rm -f /tmp/requirements.txt

RUN mkdir -p /root/.vscode-server/
COPY INSTALLROOT/root/.vscode-server/ /root/.vscode-server/
# 安装数据库连接工具


# 安装 Redis CLI
RUN apt-get install -y redis-tools

# 安装 MongoDB 客户端
RUN apt-get install -y mongodb-clients

# 安装 PostgreSQL 客户端 (psql)
RUN apt-get install -y postgresql-client


RUN echo "PS1='\\[\033[32;1m\]\\u@\\[\033[38;5;214;1m\]\\h:\\[\033[01;34m\]\\w\\[\033[00m\] \$ '" >> /root/.bashrc


CMD ["/usr/sbin/sshd", "-D"]
WORKDIR /root