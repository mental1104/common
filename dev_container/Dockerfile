# 使用龙蜥镜像
FROM openanolis/anolisos:8.4-x86_64

COPY INSTALLROOT/amd64 /

## 1. 安装基础库
RUN yum install -y \
    gcc \
    glibc \
    gcc-c++ \
    make \
    wget \
    postgresql-devel \
    zlib-devel \
    libffi-devel \
    openssl-devel

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
RUN yum install -y git 

### 3.1.2 htop
RUN yum install -y libncurses* ncurses-devel
RUN wget -P /tmp https://hisham.hm/htop/releases/2.2.0/htop-2.2.0.tar.gz && cd /tmp && tar -zxvf htop-2.2.0.tar.gz && cd htop-2.2.0/ && ./configure && make && make install

RUN yum install -y bison
RUN wget -4c -P /tmp https://ftp.gnu.org/gnu/glibc/glibc-2.29.tar.gz && cd /tmp && \
    tar -zxvf glibc-2.29.tar.gz && \
    cd glibc-2.29 && \
    mkdir build_dir && \
    cd build_dir && \
    ../configure --prefix=/opt/glibc && \
    make && \
    make install

### 3.1.3 neovim # 这一步要放早一点
COPY INSTALLROOT/newer/* /tmp
RUN cd /tmp && tar -zxvf nvim-linux64.tar.gz \ 
    && cd nvim-linux64 && cp -rf ./* /usr/local && \
    ln -s  /usr/local/bin/nvim /usr/local/bin/vim
    
RUN yum install -y bison


# RUN yum install -y cmake
RUN yum install -y unzip
# RUN wget -O /tmp/neovim-master.zip https://github.com/neovim/neovim/archive/refs/heads/master.zip && cd /tmp && unzip neovim-master.zip

# ENV HTTPS_PROXY 192.168.31.239:7890

# RUN cd /tmp/neovim-master && make install

### 3.2 安装基础网络工具
RUN yum install -y openssl nc telnet tcpdump iptables openssh-server

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
