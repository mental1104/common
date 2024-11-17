# 使用龙蜥镜像
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# 更新软件源并使用阿里云镜像
RUN echo "deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    apt-get update && apt-get upgrade -y

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
    libssl-dev

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

### 2.1.2 pip库安装
RUN pip3 install -i https://pypi.tuna.tsinghua.edu.cn/simple -r /tmp/requirements.txt && rm -f /tmp/requirements.txt

### 2.2 安装golang
RUN tar -C /usr/local -xzf /tmp/go1.20.5.linux-amd64.tar.gz
ENV PATH $PATH:/usr/local/go/bin


## 3 安装基础系统工具

### 3.1 系统相关

### 3.1.1 git
RUN apt-get install -y git

### 3.1.2 htop
# RUN apt-get install -y libtinfo6=6.2-0ubuntu2.1 libncurses-dev=6.2-0ubuntu2.1

RUN chmod 1777 /tmp
RUN apt-get install -y libtinfo6 libncurses-dev libncursesw5 && ln -s /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libtinfow.so.6 && ldconfig


### 3.1.3 neovim # 这一步要放早一点
RUN cd /tmp && tar -zxvf nvim-linux64.tar.gz \ 
    && cd nvim-linux64 && cp -rf ./* /usr/local && \
    ln -s  /usr/local/bin/nvim /usr/local/bin/vim
    
RUN apt-get install -y bison cmake unzip

# RUN wget -O /tmp/neovim-master.zip https://github.com/neovim/neovim/archive/refs/heads/master.zip && cd /tmp && unzip neovim-master.zip

# ENV HTTPS_PROXY 192.168.31.239:7890

# RUN cd /tmp/neovim-master && make install

### 3.2 安装基础网络工具
RUN apt-get install -y openssl netcat telnet tcpdump iptables openssh-server

#### 3.2.2 开放ssh端口
RUN mkdir /var/run/sshd
# 生成 SSH 主机密钥
RUN ssh-keygen -A
RUN echo 'root:password' | chpasswd
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
ENV LANG=C.UTF-8
# 暴露 SSH 默认端口
EXPOSE 22

## 4 安装中间件连接工具



# 安装数据库连接工具


CMD ["/usr/sbin/sshd", "-D"]
