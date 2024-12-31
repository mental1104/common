# 使用ubuntu:24.04镜像
FROM ubuntu:24.04


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
    wget \
    build-essential \
    gdb-multiarch \
    qemu-system-misc \
    gcc-riscv64-linux-gnu \
    binutils-riscv64-linux-gnu \
    libpcap-dev \
    libglib2.0-dev \
    pkg-config \
    libpixman-1-dev \
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
    postgresql-client \
    ffmpeg \
    rustc \
    cargo \
    rust-doc \
    rust-src \
    pip \
    dotnet-sdk-8.0 \
    aspnetcore-runtime-8.0 \
    && \
    ln -s /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6 && ldconfig

## 2 安装很少变动的软件

### 2.1 安装qemu
RUN cd /tmp && wget https://download.qemu.org/qemu-4.1.0.tar.xz && tar xvJf qemu-4.1.0.tar.xz && cd qemu-4.1.0/ && \
    ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu " && \
    make -j$(nproc) && \
    make install && PATH=$PATH:/opt/qemu/bin && \
    rm -rf /tmp/qemu*

COPY INSTALLROOT/amd64 /

### 2.2 安装golang
RUN tar -C /usr/local -xzf /tmp/go1.20.5.linux-amd64.tar.gz
ENV PATH $PATH:/usr/local/go/bin


### 2.4 neovim # 这一步要放早一点
RUN cd /tmp && tar -zxvf nvim-linux64.tar.gz \ 
    && cd nvim-linux64 && cp -rf ./* /usr/local && \
    ln -s  /usr/local/bin/nvim /usr/local/bin/vim


## 3. vscode相关

### 3.1 安装vscode离线ssh连接套件

ENV VSCODE_COMMIT_VERSION=f1a4fb101478ce6ec82fe9627c43efbf9e98c813
RUN mkdir -p /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}
COPY INSTALLROOT/root/.vscode-server /root/.vscode-server
COPY INSTALLROOT/root/extensions.txt /root/extensions.txt
RUN tar -zxvf /root/.vscode-server/vscode-server-linux-x64.tar.gz -C /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION} --strip 1 && touch /root/.vscode-server/bin/${VSCODE_COMMIT_VERSION}/0

### 3.2 下载并安装插件
RUN while read extension; do \
        /root/.vscode-server/bin/*/bin/code-server --install-extension $extension || true; \
    done < /root/extensions.txt

## 4. 安装pip模块
COPY INSTALLROOT/root/requirements.txt /root/requirements.txt
RUN pip3 install  -i https://mirrors.aliyun.com/pypi/simple -r /root/requirements.txt --break-system-packages && rm -f /root/requirements.txt


## 5. 安装自定义代码

### 5.1 安装cpp代码
COPY cpp /tmp/cpp/
RUN cd /tmp/cpp && mkdir -p build && cd build && cmake .. && make -j "$(nproc)" && make install

### 5.2 安装python代码
COPY python /tmp/python/
RUN cd /tmp/python && pip install . --break-system-packages

### 5.3 安装自定义必要脚本
COPY utils /usr/local/bin
RUN chmod -R +x /usr/local/bin

## 6. 快速配置项

### 6.1 杂项
RUN git config --global core.editor "vim"
ENV LANG=C.UTF-8
RUN echo "PS1='\\[\033[32;1m\]\\u@\\[\033[38;5;214;1m\]\\h:\\[\033[01;34m\]\\w\\[\033[00m\] \$ '" >> /root/.bashrc

### 6.2 加载环境变量
### 这里赋一个默认的.env文件，期望宿主机通过挂载环境变量文件的方式将环境变量导入容器，以便ssh使用
RUN touch /root/.env && echo "MENTAL1104_NOENVRION=TRUE" >> /root/.env && echo "export \$(xargs < ~/.env)" >> /root/.bashrc

### 6.3 生成 SSH 主机密钥 && 修改SSH配置文件以允许密码认证
ARG SSH_PRIVATE_KEY
ENV SSH_PRIVATE_KEY=${SSH_PRIVATE_KEY}
RUN echo "root:${SSH_PRIVATE_KEY}" | chpasswd && mkdir -p /run/sshd && echo 'PasswordAuthentication yes' | tee -a /etc/ssh/sshd_config && \
    echo 'PermitRootLogin yes' | tee -a /etc/ssh/sshd_config


EXPOSE 22
CMD ["/usr/sbin/sshd", "-D"]
WORKDIR /root