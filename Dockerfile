# 使用ubuntu:24.04镜像
FROM ubuntu:24.04


ENV DEBIAN_FRONTEND=noninteractive

RUN chmod 1777 /tmp

## 1. 安装基础库
RUN echo "deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb https://mirrors.tuna.tsinghua.edu.cn/clickhouse/deb stable main" | tee /etc/apt/sources.list.d/clickhouse.list && \
    apt-get update && apt-get upgrade -y && apt-get install -y \
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
    clang-format \
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
    zip \
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
    libgtest-dev \
    libbenchmark-dev \
    libboost-all-dev \
    lcov \
    clickhouse-client \
    cron \
    logrotate \
    pandoc \
    jq \
    xclip \
    xsel \
    clangd \
    && \
    ln -s /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6 && ldconfig && \
    cd /usr/src/googletest && cmake . && make -j$(nproc) && make install
## 2 安装很少变动的软件

### 2.1 安装qemu
RUN cd /tmp && wget https://download.qemu.org/qemu-4.1.0.tar.xz && tar xvJf qemu-4.1.0.tar.xz && cd qemu-4.1.0/ && \
    ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu " && \
    make -j$(nproc) && \
    make install && PATH=$PATH:/opt/qemu/bin && \
    rm -rf /tmp/qemu*

COPY INSTALLROOT/amd64 /

### 2.2 安装golang
RUN tar -C /usr/local -xzf /tmp/go1.23.4.linux-amd64.tar.gz
ENV PATH $PATH:/usr/local/go/bin
ENV GOPROXY=https://goproxy.cn,direct

ENV GOPATH="/go"
ENV GOBIN="/go/bin"
ENV PATH="${GOBIN}:${PATH}"
ENV GO111MODULE="on"

# 安装 gopls
RUN go install golang.org/x/tools/gopls@latest

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

# 安装 cJSON
COPY INSTALLROOT/lib/ /tmp/lib
RUN cd /tmp/lib && cd cJSON && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    make -j$(nproc)&& \
    make install && \
    cd /tmp/lib && rm -rf cJSON*

# 安装pystring
RUN cd /tmp/lib && cd pystring && \
mkdir build && cd build && cmake .. && make -j$(nproc) && make install && \
cd /tmp/lib && rm -rf pystring*

# 安装rapidjson
RUN cd /tmp/lib && cd rapidjson && \
    mkdir build && cd build && cmake -DRAPIDJSON_BUILD_CXX11=OFF -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF .. && \
    make -j$(nproc) && make install && \
    cd /tmp/lib && rm -rf rapidjson*

# 安装DataStruncture
RUN cd /tmp/lib && cd DataStructure && \
    mkdir build && cd build && cmake .. && \
    make -j$(nproc) && make install && \
    cd /tmp/lib && rm -rf DataStructure*

# 安装lazyvim
COPY INSTALLROOT/root/.config /root/.config
COPY INSTALLROOT/root/.local /root/.local

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
RUN touch /root/.env && echo "MENTAL1104_NOENVRION=TRUE" >> /root/.env && echo "export \$(grep -v '^#' ~/.env | xargs)" >> /root/.bashrc

### 6.3 别名命令定义
RUN echo "alias vi='vim'" >> /root/.bashrc && \
    echo "alias view='vim -R'" >> /root/.bashrc && \
    echo "alias python='python3'" >> /root/.bashrc && \
    echo 'alias redis="redis-cli -h ${REDIS_HOST} -p ${REDIS_PORT}"' >> /root/.bashrc && \
    echo 'alias clickhouse="clickhouse-client -h ${CLICKHOUSE_HOST} --port ${CLICKHOUSE_PORT} -d ${CLICKHOUSE_DATABASE} -u ${CLICKHOUSE_USER}"' >> /root/.bashrc && \
    echo 'alias mongodb="mongo --host ${MONGO_HOST} --port ${MONGO_PORT}"' >> /root/.bashrc

### 6.4 生成 SSH 主机密钥 && 修改SSH配置文件以允许密码认证
ARG SSH_PRIVATE_KEY
ENV SSH_PRIVATE_KEY=${SSH_PRIVATE_KEY}
RUN echo "root:${SSH_PRIVATE_KEY}" | chpasswd && mkdir -p /run/sshd && echo 'PasswordAuthentication yes' | tee -a /etc/ssh/sshd_config && \
    echo 'PermitRootLogin yes' | tee -a /etc/ssh/sshd_config

### 6.5 调整时区
RUN ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
### 6.6 映射neovim的clangd为系统clangd
RUN mkdir -p /root/.local/share/nvim/mason/packages/clangd/clangd_19.1.2/bin && ln -s /usr/bin/clangd /root/.local/share/nvim/mason/packages/clangd/clangd_19.1.2/bin/clangd
### 6.7 送入clang-format
COPY INSTALLROOT/root/.clang-format /usr/lib/llvm-18/bin
COPY INSTALLROOT/root/.clang-format /root

### 6.8 建立go相关的软链接
RUN ln -s /usr/local/go/bin/go /usr/bin/go && ln -s /go/bin/gopls /usr/bin/gopls
RUN ldconfig

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D"]
WORKDIR /root