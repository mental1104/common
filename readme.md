本仓库包含以下两部分内容

1. 公共可复用代码
2. 常见开发工具+编辑器 开发镜像

## 公共可复用代码

### C++

运行测试需要安装gtest

ubuntu:

```sh
sudo apt install -y libgtest-dev && cd /usr/src/googletest && cmake . && make -j$(nproc) && make install
cd cpp && mkdir build && cd build && cmake .. && make -j8
```

运行测试

```sh
ctest -r
```

安装包

```sh
sudo make install
```

测试代码覆盖率报告生成
```
make coverage
```

### Python

## 开发镜像


## TODO 

- [ ] python requirements.txt逻辑执行应在vscode插件之后

