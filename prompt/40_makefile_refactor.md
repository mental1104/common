# Makefile（终版）设计原则与使用说明

## 设计原则
- **单一入口、私有实现**：公开目标保持简洁（setup/build/test/install/clean/coverage/fmt/bench/vet/guard 等），具体流程下沉到私有宏 `__xxx`，便于复用和阅读。
- **语言分块**：Python / C++ / Go / Rust 的宏与入口按语言集中放置，入口紧邻其一层配方（`_setup_xxx` 等），避免跨文件查找。
- **可重入/可跳过**：各语言的守护宏会检查 venv、build 目录或工具存在，缺失时提示而非盲目操作；覆盖率/bench 会过滤性能用例或避免重复运行。
- **最小副作用**：Docker 仅通过 `setup-docker` / `clean-docker` 触发；`.env` 只有在 `.env.active` 存在时才自动导入，`clean-docker`/`env-clean` 会移除激活标记与 `.env.mk`。
- **系统安装与卸载对称**：install-* 负责系统安装（或 `cargo/go install`），新增的 uninstall-* 负责从系统/安装目录移除对应产物。

## 核心入口
- **聚合**：`make setup|build|test|install|clean|coverage|fmt|bench|guard|vet|uninstall`
- **语言入口**：`setup-xxx/build-xxx/test-xxx/install-xxx/uninstall-xxx/clean-xxx/coverage-xxx/fmt-xxx/bench-xxx`（xxx ∈ python/go/cpp/rust）
- **Docker**：`setup-docker`（生成 `.env.mk` 并按 images/ 下 compose up）、`clean-docker`（down + 清理 `.env.active`/`.env.mk`）。macOS 可通过 `SKIP_DOCKER_ON_DARWIN=1` 跳过。
- **Export Layer**：`build-export-cpp` 构建 C++→Python 的桥接层，Python 测试/bench 默认依赖它。

## 环境变量与 .env
- `.env` + `.env.active` 同时存在时自动导入（生成 `.env.mk`），否则不导入。
- `env-clean`/`clean-docker` 会删除 `.env.active` 与 `.env.mk`。再次使用环境需运行 `make setup-docker` 或手动 `touch .env.active && make .env.mk`。
- 想查看当前导入变量：`make env-print`。生成示例：`make env-example`。

## 安装/卸载
- **Python**：`install-python` 使用 `pip`（必要时加 `--break-system-packages`）安装 wheel 或源码；`uninstall-python` 依次卸载 `mental1104` 与 export layer。
- **Go**：`install-go` 使用 `go install ./...`；`uninstall-go` 根据包内 `main` 名称从 GOBIN/GOPATH/bin 删除对应可执行文件。
- **C++**：`install-cpp`/`uninstall-cpp` 依赖 CMake 的 install manifest。
- **Rust**：`install-rust` 针对 bin 则 `cargo install --path .`，否则构建 release；`uninstall-rust` 通过 `cargo uninstall mental1104`（忽略未安装）。

## 运行提示
- **覆盖率**：`coverage-cpp` 只在需要时重跑 ctest（`RUN_CTEST_FOR_COVERAGE=1`），gcovr 优先，缺失则回退 lcov；Python 覆盖率会禁用/过滤性能用例。
- **Bench**：各 bench 目标运行后追加 `bench-report` 汇总。
- **Guard**：Go/Rust/C++ 提供 race/asan/tsan/miri 等模式；Python guard 仅做快速 pytest+ruff。
- **过滤**：多数 test/bench 支持 `FILE`/`FILTER` 环境变量（例如 `make test-go FILE=redis FILTER=Cache`）。

## 注意事项
- 不要直接编辑私有宏的调用顺序，公开入口保持“薄”层调用 `_xxx`，逻辑集中在 `__xxx`。
- `.env.active` 是开启自动导入的开关；若想完全隔离外部服务，执行 `make clean-docker` 后运行测试，相关环境依赖用例会因缺参被 skip。
- CMake/gcovr/lcov/go/cargo 等外部工具需自行安装；缺失时宏会给出提示或尽量降级。 
