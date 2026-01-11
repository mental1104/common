# mental1104 — 使用说明

## Walkthrough

[![walkthrough](https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fmental1104.github.io%2Fcommon%2Fprogress.json&query=%24.pct&suffix=%25&label=walkthrough)](https://mental1104.github.io/common/) 

![C++ Coverage](https://mental1104.github.io/common/coverage/cpp.svg)

Python + C++ 混合工程，使用统一的 `dev` 命令驱动（macOS/Linux：`./dev`，Windows：`dev`）。子命令按语言拆分，后续扩展只需新增文件。

## 本地依赖（最小子集）

参考 CI workflow，在一台干净主机上跑 `dev` 系列命令前需要先准备的可执行文件/工具：

- 通用：`git`、Python 3.12（含 `pip` 和 `venv`），可执行 `bash`（Windows 使用 PowerShell 调用 `python -m devtool.cli ...` 也可直接运行 `dev`），如需启动依赖服务请安装 Docker/Docker Desktop。
- Ubuntu（apt）：
  - 构建/测试 C++：`sudo apt-get install -y build-essential cmake ninja-build clang-format clang-tidy gcovr lcov libmpfr-dev libgmp-dev`
  - 其他语言：Go 1.22（官方 tar.gz 或包管理器）、Rust（`rustup` 安装 stable 并添加 `rustfmt`/`clippy`）。
- macOS（Homebrew）：
  - `brew install cmake llvm ninja gcovr lcov mpfr gmp`，并将 `$(brew --prefix llvm)/bin` 置入 PATH（匹配 CI 的 `GCOV="llvm-cov gcov"`）。
  - Go 1.22：`brew install go`；Rust：`rustup toolchain install stable --profile minimal && rustup component add rustfmt clippy`。
- Windows：
  - Python 3.12（PATH 中的 `python`），Git。
  - Visual Studio 2022 Build Tools：安装 “使用 C++ 的桌面开发” 工作负载（包含 MSVC、CMake、Ninja 可选）以满足 `CMAKE_GENERATOR="Visual Studio 17 2022"`。
  - Go 1.22（官方 MSI）；Rust：`rustup` stable 并添加 `rustfmt`/`clippy`。
  - 如需启动依赖服务，安装 Docker Desktop。

## 顶层入口（TL;DR）

```bash
# 一次性准备：Python venv + 依赖 + wheel，Go/Rust/C++ 配置
./dev setup

# 构建 / 测试 / 安装 / 清理 / 覆盖率
./dev build all
./dev test all
./dev install all
./dev clean all
./dev coverage all

# 其他高频
./dev fmt all
./dev bench all          # 含基准图表与 gallery
./dev vet all
./dev guard all          # C++ ASAN/TSAN，Go race，Rust sanitizer/miri，Python ruff+pytest
./dev setup-docker       # 启动 devops/images/ 下的 compose，自动读取 .env
./dev clean-docker       # 关闭 devops/images/ 下的 compose
```

参数透传：在子命令后加 `--`，其后的内容直接交给底层工具，例如 `./dev test python -- -k "expr"` 或 `./dev test cpp -- --gtest_filter=Foo.*`。

## 常用子命令速查

### 聚合

- `./dev setup`：生成 `.env.example`（若存在 `.env`）+ 各语言依赖准备。
- `./dev build all`：Python wheel、Go build、C++（含子模块配置）、Rust release build。
- `./dev test all`：pytest / go test / ctest / cargo test。
- `./dev install all` / `./dev uninstall all`
- `./dev clean all`：含 `.env.active` 标记清理。
- `./dev coverage all`：pytest+coverage / go cover / ctest+gcovr（表格概要，自动回退 gcov） / cargo llvm-cov。
- `./dev fmt all`
- `./dev bench all`：完成后自动生成 `artifacts/bench/index.html`。
- `./dev vet all`
- `./dev guard all`

### Python

- `./dev setup-python`：创建/复用 `python/.venv`，安装依赖，修正 `__future__ annotations`，构建 wheel。
- `./dev build python`：在 venv 下构建 wheel。
- `./dev test python [-- --pytest-opts]`：自动构建 export/cpp，清空代理并运行 pytest。
- `./dev coverage python` / `./dev fmt python` / `./dev bench python` / `./dev clean python`
- `./dev install python` / `./dev uninstall python`
- `./dev vet python` / `./dev guard python`

### Go

- `./dev setup-go`（tidy + download）
- `./dev build go`
- `./dev test go` / `./dev coverage go` / `./dev fmt go` / `./dev bench go`
- `./dev install go` / `./dev uninstall go` / `./dev clean go` / `./dev vet go` / `./dev guard go`

### C++

- `./dev git-submodules`：自动修复/拉取子模块。
- `./dev setup-cpp`：构建 `cpp/lib/*/build` 并执行顶层 cmake 配置。
- `./dev build cpp [--config Debug|Release] [--build-verbose]`
- `./dev test cpp [--filter <gtest>] [--file <ctest>]`
- `./dev coverage cpp`（默认使用 gcovr，缺失时回退 lcov）
- `./dev fmt cpp` / `./dev bench cpp` / `./dev install cpp` / `./dev uninstall cpp` / `./dev clean cpp`
- `./dev build-export-cpp` / `./dev clean-export-cpp`：仅构建/清理 pybind 导出层
- `./dev guard cpp --mode mem|race|heap|all`
- 子模块体积控制：`SKIP_SUBMODULES="cpp/lib/boost"` 可跳过，`BUILD_SUBMODULES=all` 覆盖全部；默认只构建 cJSON/hiredis/redis-plus-plus。

### Rust

- `./dev setup-rust`
- `./dev build rust` / `./dev test rust` / `./dev bench rust`
- `./dev fmt rust` / `./dev clippy-rust` / `./dev example-rust`
- `./dev install rust` / `./dev uninstall rust` / `./dev clean rust`
- `./dev coverage rust`
- `./dev guard rust --mode mem|race|miri|all`

### 诊断/别名

- `./dev test -v ...` / `./dev bench -v ...`：等价于 `VERBOSE=1`。
- 覆盖率：C++ 使用 `gcovr --merge-mode-functions=separate` 输出概要表格；缺失时回退逐目录 `gcov -b -c`。

## 可配置参数

以下环境变量会被 `dev` 读取，均有默认值：

| 变量               | 默认                                | 说明                                     |
| ---------------- | --------------------------------- | -------------------------------------- |
| `JOBS`           | 自动探测                              | 并行度（cmake/ctest 等）                    |
| `CPP_BUILD_TYPE` | `Debug`                           | C++ 构建类型                              |
| `PREFIX`         | `/usr/local`                      | C++ 安装前缀                               |
| `PYTHON`         | `python3`                         | Python 解释器                             |
| `PIP3`           | `pip3`                            | pip 命令                                 |
| `CMAKE` / `CTEST`| `cmake` / `ctest`                 | C++ 工具链                               |
| `GCOV`           | `gcov`                            | C++ 覆盖率工具（传给 gcovr/gcov）              |
| `BUILD_VERBOSE`  | `0`                               | CMake 构建是否输出完整编译命令（1 开启）          |
| `SUDO`           | 非 root 时为 `sudo`                  | 安装目标默认使用；可设为空禁用                       |
| `BREAK_FLAG`     | Ubuntu: `--break-system-packages` | pip 系统安装时的兼容参数                         |
| `COMPOSE_BIN`    | `docker compose`                  | docker compose 可执行（可填 docker-compose）      |
| `COMPOSE_FILE_NAME` | `docker-compose.yaml`          | compose 文件名                             |
| `SKIP_SUBMODULES`/`BUILD_SUBMODULES` | 空 / 默认核心组件 | 控制 C++ 子模块拉取和构建                        |

示例：Release 构建并安装到用户前缀（避免 sudo）

```bash
CPP_BUILD_TYPE=Release JOBS=8 ./dev build cpp
PREFIX="$HOME/.local" SUDO= ./dev install cpp
```

## Docker（可选）

仓库提供 `Dockerfile` 与 devops/images/ 目录下的 compose：

```bash
./dev setup-docker      # 自动检测 docker-compose / docker compose；跳过缺失
./dev clean-docker      # down --remove-orphans
# 若需手动：
docker build -t mental1104:dev .
```

`devops/INSTALLROOT/` + `install.sh`/`run.sh` 可在镜像内按需安装组件。

说明：`setup-docker` 自动加载根目录 `.env`，按服务目录独立 project-name 启动；已有同名容器且在运行时会跳过，避免误停。

## FAQ

1) `./dev clean` 报 Permission denied  
   若历史上使用过 `sudo` 安装，仓库内可能残留 root 拥有的文件。一次性回收后再清理：
   ```bash
   sudo chown -R "$USER":"$USER" .
   ./dev clean all
   ```

2) Ubuntu pip 提示 “externally managed environment”  
   已自动添加 `--break-system-packages`。若不希望写入系统 Python，请使用虚拟环境或 `PIP3=... --user`。

3) `pytest` 不存在  
   运行 `./dev setup-python`，会创建 `python/.venv` 并安装依赖。

4) 子模块拉取失败或元数据损坏  
   运行 `./dev git-submodules`，内置自动修复（deinit + 清理 + 重拉）。

5) 覆盖率报错 “function … on multiple lines” 或找不到 notes  
   已在 `coverage-cpp` 中默认使用 `--merge-mode-functions=separate`；若 gcovr 缺失则回退 gcov，必要时先 `./dev build cpp --config Debug` 再跑覆盖率。

## 工作原理（简述）

- **dev CLI**：Python `argparse` 分发；子命令各在 `devops/devtool/commands/*.py` 中注册。
- **命令执行**：统一经过 `devtool.context.sh`，日志格式 `[dev] (cwd)$ cmd`。
- **语言侧逻辑**：从旧自动化脚本迁移的 shell 片段直接嵌入各子命令，确保行为与历史一致（venv/bootstrap、pybind11 导出、ctest/gcovr/gcov、cargo llvm-cov 等）。
- **环境变量**：`base_env` 自动加载 `.env`，测试/覆盖率会自动剥离 HTTP(S)_PROXY/ALL_PROXY，避免代理影响内部容器。

## 开发建议

- Python 依赖维护于 `python/requirements.txt`，`pyproject.toml` 描述打包信息。
- C++ 头文件位于 `cpp/include/mental1104/`，单测在 `cpp/test/`，第三方依赖于 `cpp/lib/`、`cpp/thirdparty/`。
- 提交前请至少执行：`./dev test all`；必要时 `./dev coverage all`；结束前可 `./dev clean all` 保持仓库整洁。
