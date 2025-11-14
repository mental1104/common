# mental1104 — 使用说明

Python + C++ 混合工程，个人用的公共封装能力，为日常重复工作提效。

单文件 `Makefile` 驱动；支持子模块自动修复、并行构建、覆盖率与安装。


## 快速开始（操作）

> 默认目标：`make setup`。以下命令均在仓库根目录执行。

### 1) 一次性准备
```bash
# 初始化 Python 依赖 + 生成代码 + 构建本地 wheel（不安装本体）
# 同时：构建 C++ 子模块并完成顶层 cmake 配置
make setup
````

### 2) 构建

```bash
# Python（已随 setup 构建过，无需重复）
# C++ 并行编译
make build
```

### 3) 测试

```bash
# Python（pytest）
# C++（ctest）
make test
```

### 4) 安装（可选）

```bash
# 安装 Python 包与 C++ 产物
# 说明：Python 侧会执行 “pip3 install python/ --upgrade”，可能提示输入 sudo 密码
make install
```

### 5) 清理

```bash
# 清理仓库内构建/缓存产物（无需 sudo）
make clean
```

### 6) 覆盖率（可选）

```bash
# Python 覆盖率（coverage）
# C++ 覆盖率（gcovr/lcov，如已安装）
make coverage
```

---

## 常用目标（命令速查）

### 聚合

* `make setup` ＝ `setup-python` + `setup-cpp`
* `make build` ＝ `build-python` + `build-cpp`
* `make test` ＝ `test-python` + `test-cpp`
* `make install` ＝ `install-python` + `install-cpp`
* `make clean` ＝ `clean-python` + `clean-cpp`
* `make coverage` ＝ `coverage-python` + `coverage-cpp`
* `make help` 打印帮助

### Python

* `make setup-python`：按 `python/requirements.txt` 将依赖安装到用户目录（`--user`）；执行 `python/generate_init.py`；构建 wheel 至 `python/dist/`（**不安装本体**）
* `make test-python`：运行 `pytest python`
* `make install-python`：`pip3 install python/ --upgrade`（可能使用 `sudo`）
* `make clean-python`：清理 `python/build`、`python/dist`、`*.egg-info`、`__pycache__`、`.pytest_cache`、覆盖率产物
* `make coverage-python`：`coverage run -m pytest && coverage report`

### C++

* `make git-submodules`：拉取并自动修复子模块
* `make setup-cpp`：逐个构建 `cpp/lib/*` 至自身 `build/`，再配置顶层
* `make build-cpp`：并行编译顶层工程
* `make test-cpp`：`ctest --output-on-failure`
* `make install-cpp`：安装到 `PREFIX`（默认 `/usr/local`，可能使用 `sudo`）
* `make clean-cpp`：删除 `cpp/build` 与 `cpp/lib/*/build`
* `make coverage-cpp`：执行测试并输出覆盖率（如已安装 `gcovr/lcov`）

### 性能基准与可视化

* `make bench`：逐语言执行性能基准（Python/Go/C++/Rust）。Python 端按文件逐个执行 `python/test_benchmark/**/*.py`，C++ 端运行 `cpp/build/bin/bench_*`，并收集 `pytest-benchmark`/`google-benchmark` 的 JSON 结果到 `artifacts/bench/<lang>/`。
* 基准结束后自动调用 `mental1104.plot.BenchmarkPlotter` 生成只关注 `real_time_ms` 的“纵轴 = 各用例”图表（默认纵向堆叠），位于 `artifacts/bench/<lang>/plots/`。
* 同步生成 `artifacts/bench/index.html` 图库，可直接在浏览器中一站式查看全部图表。如需局域网分享，可运行 `python -m http.server -d artifacts/bench 8080`。
* 若只需要部分语言，可运行 `make bench-python` 或 `make bench-cpp`；图表仍会被纳入 gallery。
* 更细粒度的对比可用 `python/tools/render_bench_plots.py`：例如 `python/tools/render_bench_plots.py --input artifacts/bench/cpp/bench_bench_json_gbench.json --test-type google-benchmark --chart comparison --metric real_time_ms --group-field arg --variant-field variant --filter stat=mean` 可直接比较同一数据规模下 cJSON 与 RapidJSON 的 real_time_ms。

---

## 可配置参数（按需在命令行覆盖）

| 变量               | 默认                                | 说明                                     |
| ---------------- | --------------------------------- | -------------------------------------- |
| `JOBS`           | 自动探测                              | C++ 并行度                                |
| `CPP_BUILD_TYPE` | `Debug`                           | C++ 构建类型（`Release`/`RelWithDebInfo` 等） |
| `PREFIX`         | `/usr/local`                      | C++ 安装前缀                               |
| `PYTHON`         | `python3`                         | Python 解释器                             |
| `PIP3`           | `pip3`                            | pip 命令                                 |
| `CMAKE`          | `cmake`                           | cmake 命令                               |
| `CTEST`          | `ctest`                           | ctest 命令                               |
| `SUDO`           | 非 root 时为 `sudo`                  | 安装目标默认会使用；可覆盖为空禁用                      |
| `BREAK_FLAG`     | Ubuntu: `--break-system-packages` | 适配 PEP 668                             |

**示例：Release 构建并安装到用户目录前缀（避免 sudo）**

```bash
make build-cpp CPP_BUILD_TYPE=Release JOBS=8
make install-cpp PREFIX="$HOME/.local" SUDO=
```

---

## Docker（可选）

仓库提供 `Dockerfile` 与 `docker-compose.yaml`，用于封装构建/运行环境。常见用法：

```bash
# 直接构建镜像
docker build -t mental1104:dev .

# 或使用 compose（如定义了服务）
docker compose up -d
```

> `INSTALLROOT/` 下的目录与 `install.sh`/`run.sh` 可用于镜像内组件/扩展安装，请结合自身环境按需使用。

---

## 常见问题（FAQ）

### 1. `make clean` 报 `Permission denied`

历史上若执行过带 `sudo` 的安装（尤其是 `make install-python`），可能在仓库内生成 root 拥有的 `python/build` 或 `python/*.egg-info`。一次性回收所有权后再清理：

```bash
sudo chown -R "$USER":"$USER" .
make clean
```

### 2. Ubuntu 上 pip 报 “externally managed environment”

Makefile 已在 Ubuntu 环境自动带上 `--break-system-packages`。若不希望写入系统 Python，请使用虚拟环境或用户级安装（`python3 -m venv` / `pip --user`）。

### 3. `pytest` 不存在

执行：

```bash
make setup-python
```

该目标会按 `python/requirements.txt` 安装依赖。

### 4. 子模块拉取失败或元数据损坏

执行：

```bash
make git-submodules
```

Makefile 内置了自动修复流程（deinit + 清理 + 重拉）。

---

## 工作原理（简述）

* **Python**
  `setup-python`：仅安装依赖至用户目录（`--user`）→ 执行 `generate_init.py` → 构建 wheel 到 `python/dist/`；不安装本体。
  `install-python`：将 `python/` 作为项目源安装（可能触发 `sudo`）。
  `clean-python`：删除构建与缓存目录，保持仓库整洁。

* **C++**
  `setup-cpp`：对子模块分别在 `cpp/lib/*/build` 内独立构建，然后进行顶层 `cmake` 配置。
  `build-cpp`/`test-cpp`：并行构建与 `ctest` 测试；`coverage-cpp` 在具备工具时输出覆盖率。
  `install-cpp`：按 `PREFIX` 安装，默认 `/usr/local`。

---

## 开发建议

* Python 依赖维护于 `python/requirements.txt`，`pyproject.toml` 负责项目信息与打包配置。
* C++ 头文件位于 `cpp/include/mental1104/`，单元测试位于 `cpp/test/`，第三方依赖在 `cpp/lib/` 与 `cpp/thirdparty/`。
* 提交前请确保：`make test` 通过；必要时运行 `make coverage` 检查覆盖率；最终执行 `make clean` 保持仓库干净。

---

```
