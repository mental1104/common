# Makefile — 单文件、无 heredoc；公共逻辑用 define 宏封装，目标直接调用
SHELL := /bin/bash
.SILENT:
.DEFAULT_GOAL := setup

# =================== 基础开关/环境检测 ===================
UID        := $(shell id -u)
SUDO       := $(if $(filter 0,$(UID)),,sudo)
SUDO_MSG   := $(if $(filter 0,$(UID)),,@echo "[info] 当前非root，将使用sudo执行；可能会提示输入管理员密码。")

UNAME_S    := $(shell uname -s)
IS_UBUNTU  := $(shell sh -lc 'u=$$(uname -s); if [ "$$u" = Linux ] && [ -r /etc/os-release ]; then . /etc/os-release; [ "$$ID" = ubuntu ] && echo 1; fi')
BREAK_FLAG := $(if $(IS_UBUNTU),--break-system-packages,)

# CPU 并行度
JOBS ?= $(shell sh -lc 'command -v nproc >/dev/null 2>&1 && nproc || sysctl -n hw.ncpu 2>/dev/null || echo 4')

# =================== 工具/路径/参数 ===================
PYTHON ?= python3
PIP3   ?= pip3
CMAKE  ?= cmake
CTEST  ?= ctest

# C++ 顶层与子模块路径
CPP_SRC_DIR    := cpp
CPP_BUILD_DIR  := $(CPP_SRC_DIR)/build
CPP_BUILD_TYPE ?= Debug
PREFIX         ?= /usr/local

# =================== 公共逻辑宏（函数化） ===================
# ---- 子模块：尝试拉取；如遇锁/脏元数据，自动“deinit+清理+重拉”
define _git_fetch_submodules
	$(SHELL) -lc 'set -e; \
		if [[ -d .git && -f .gitmodules ]]; then \
			echo "[git] 拉取 .gitmodules 中的所有子模块 …"; \
			git submodule sync --recursive >/dev/null 2>&1 || true; \
			if ! git submodule update --init --recursive --depth=1; then \
				echo "[git-fix] 检测到子模块锁/元数据异常，执行一次自动修复…"; \
				git config -f .gitmodules --get-regexp path | awk '\''{print $$2}'\'' > /tmp/submods.list || true; \
				while read p; do \
					[[ -z "$$p" ]] && continue; \
					echo "  - fix: $$p"; \
					git submodule deinit -f -- "$$p" || true; \
					rm -rf ".git/modules/$$p"        || true; \
					rm -rf "$$p"                      || true; \
				done </tmp/submods.list; \
				git submodule sync --recursive; \
				git submodule update --init --recursive --depth=1; \
			fi; \
			echo "[git] 子模块就绪。"; \
		else \
			echo "[git] 未检测到 .git 或 .gitmodules，跳过子模块拉取。"; \
		fi'
endef

# ---- Python: 安装/测试/安装包/清理/覆盖率
define _setup_python
	$(SHELL) -lc "set -e; \
		echo \"[info] 平台: $(UNAME_S)$(if $(IS_UBUNTU), (ubuntu),)\"; \
		if [[ -f python/requirements.txt ]]; then \
			echo \"[pip] 安装依赖到用户目录: python/requirements.txt\"; \
			$(PYTHON) -m pip install --user -r python/requirements.txt $(BREAK_FLAG); \
		else \
			echo \"[info] 未找到 python/requirements.txt，跳过依赖安装。\"; \
		fi; \
		if [[ -f python/generate_init.py ]]; then \
			echo \"[info] 执行 python/generate_init.py …\"; \
			$(PYTHON) python/generate_init.py; \
		else \
			echo \"[info] 未找到 python/generate_init.py，跳过。\"; \
		fi; \
		echo \"[info] 构建本地 wheel（不安装本体）…\"; \
		mkdir -p python/dist; \
		$(PYTHON) -m pip wheel --no-deps -w python/dist python/; \
		echo \"[ok] setup 完成（依赖 --user 安装 + 构建 wheel，不安装本体）。\""
endef

define _test_python
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		if ! command -v pytest >/dev/null 2>&1; then echo "[warn] 未检测到 pytest；请先执行: make setup-python"; exit 1; fi; \
		cd python; \
		$(PYTHON) -m pytest -q -k "not bench and not benchmark"'
endef

# 固定镜像配置（写死在 Makefile 内；不走命令行）
USE_PIP_MIRROR := 1

# 主索引（HTTPS 无需 trusted-host）
PIP_INDEX_URL := https://pypi.tuna.tsinghua.edu.cn/simple

# 备用索引（可选，留空则不加）
PIP_EXTRA_INDEX_URL :=

# 如果你用内网 http 源，写主机名；否则留空
PIP_TRUSTED_HOST :=

PIP_MIRROR_OPTS :=
ifeq ($(USE_PIP_MIRROR),1)
  ifneq ($(strip $(PIP_INDEX_URL)),)
    PIP_MIRROR_OPTS += --index-url $(PIP_INDEX_URL)
  endif
  ifneq ($(strip $(PIP_EXTRA_INDEX_URL)),)
    PIP_MIRROR_OPTS += --extra-index-url $(PIP_EXTRA_INDEX_URL)
  endif
  ifneq ($(strip $(PIP_TRUSTED_HOST)),)
    PIP_MIRROR_OPTS += --trusted-host $(PIP_TRUSTED_HOST)
  endif
endif

define _install_python
	$(SHELL) -lc 'set -e; \
		echo "[info] $(PIP3) install python/ --upgrade $(BREAK_FLAG) $(PIP_MIRROR_OPTS)"; \
		$(SUDO) $(PIP3) install python/ --upgrade $(BREAK_FLAG) $(PIP_MIRROR_OPTS); \
		echo "[ok] 安装完成。"'
endef


define _clean_python
	$(SHELL) -lc 'set -e; \
		echo "[info] 清理 Python 缓存与构建产物…"; \
		rm -rf python/build python/dist python/*.egg-info .pytest_cache .mypy_cache python/.coverage htmlcov python/.ruff_cache python/.pytest_cache python/.benchmarks python/memray.bin; \
		find python -type d -name "__pycache__" -exec rm -rf {} +; \
		find python -type f -name "*.py[co]" -delete; \
		find python -type d -name ".pytest_cache" -exec rm -rf {} +; \
		echo "[ok] clean 完成。"' 
endef

define _coverage_python
	$(SHELL) -lc 'set -e; \
		echo "[info] 运行python单元测试覆盖率"; \
		cd python && $(PYTHON) -m coverage run --source=. -m pytest && coverage report;'
endef

define _fmt_python
	$(SHELL) -lc 'set -e; \
		if ! $(PYTHON) -c "import autopep8" >/dev/null 2>&1; then $(PYTHON) -m pip install --user autopep8 $(BREAK_FLAG); fi; \
		cd python; \
		$(PYTHON) -m autopep8 --in-place --recursive --max-line-length=120 --ignore=E402,E226,E24,W50,W690 .; \
		echo "[ok] python autopep8 fmt 完成。"'
endef

define _bench_python
	$(SHELL) -lc 'set -e; \
		if ! command -v pytest >/dev/null 2>&1; then echo "[error] 未检测到 pytest；请先执行: make setup-python"; exit 1; fi; \
		cd python; \
		if $(PYTHON) -m pytest -q --help 2>/dev/null | grep -qi benchmark; then \
			echo "[pytest-benchmark] 基准用例"; \
			$(PYTHON) -m pytest -q -k "bench or benchmark" --benchmark-only --benchmark-autosave; \
		else \
			echo "[warn] 未检测到 pytest-benchmark，回退到名称筛选"; \
			$(PYTHON) -m pytest -q -k "bench or benchmark"; \
		fi'
endef

# ---- C++ 顶层：配置/编译/测试/覆盖率/安装/清理
define _configure_cpp
	$(SHELL) -lc 'set -e; \
		mkdir -p "$(CPP_BUILD_DIR)"; \
		$(CMAKE) -S "$(CPP_SRC_DIR)" -B "$(CPP_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CPP_BUILD_TYPE)"; \
		echo "[ok] 顶层 cmake 配置完成（$(CPP_BUILD_TYPE)）"'
endef

define _build_cpp
	$(SHELL) -lc 'set -e; \
		if $(CMAKE) --build "$(CPP_BUILD_DIR)" --parallel $(JOBS); then :; \
		else $(CMAKE) --build "$(CPP_BUILD_DIR)" -- -j $(JOBS); fi; \
		echo "[ok] 顶层 C++ 构建完成（-j $(JOBS)）"'
endef

define _test_cpp
	$(SHELL) -lc 'set -e; \
		cd "$(CPP_BUILD_DIR)"; \
		$(CTEST) --output-on-failure -LE bench -j $(JOBS)'
endef

define _coverage_cpp
	$(SHELL) -lc 'set -e; \
		cd cpp/build; \
		ctest --output-on-failure || true; \
		if command -v gcovr >/dev/null 2>&1; then \
			echo "[info] 使用 gcovr 汇总覆盖率（已排除 lib/ 与 thirdparty/）"; \
			gcovr -r .. --object-directory . \
			      --exclude "(^|.*/)(test|external|gtest|lib|thirdparty|overlay)/" \
			      --exclude "/usr/include/.*" \
			      --txt --print-summary; \
		else \
			echo "[info] 未检测到 gcovr，回退到 lcov"; \
			if ! command -v lcov >/dev/null 2>&1; then \
				echo "[error] 未安装 lcov；请安装 gcovr 或 lcov 任一工具"; exit 1; \
			fi; \
			lcov --directory . --capture --output-file coverage.info \
			     --ignore-errors mismatch,negative,inconsistent \
			     --no-external --rc geninfo_unexecuted_blocks=1; \
			lcov --remove coverage.info \
			     "*/test/*" "*/external/*" "*/gtest/*" "*/lib/*" "*/thirdparty/*" "/usr/*" "/overlay/*" \
			     -o coverage.filtered.info || true; \
			lcov --list coverage.filtered.info || lcov --list coverage.info; \
			echo "[ok] 生成：cpp/build/coverage.info（过滤版：coverage.filtered.info）"; \
		fi'
endef


define _install_cpp
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		echo "[info] 安装到前缀: $(PREFIX)"; \
		$(SUDO) $(CMAKE) --install "$(CPP_BUILD_DIR)" --prefix "$(PREFIX)"; \
		echo "[ok] 安装完成。"' 
endef

define _clean_cpp
	$(SHELL) -lc 'set -e; \
		echo "[info] 清理顶层 C++ 构建目录: $(CPP_BUILD_DIR)"; \
		rm -rf "$(CPP_BUILD_DIR)"; \
		echo "[ok] clean 完成。"' 
endef

define _fmt_cpp
	$(SHELL) -lc 'set -e; \
		if ! command -v clang-format >/dev/null 2>&1; then echo "[error] 未找到 clang-format"; exit 1; fi; \
		find cpp \
		  \( -path "cpp/lib" -o -path "cpp/lib/*" -o -path "cpp/thirdparty" -o -path "cpp/thirdparty/*" \) -prune -o \
		  -type f -regex ".*\.\(h\|hh\|hpp\|hxx\|c\|cc\|cpp\|cxx\)" -print0 | xargs -0 -n 50 clang-format -i; \
		echo "[ok] cpp fmt 完成（已排除 cpp/lib 与 cpp/thirdparty）"'
endef

define _bench_cpp
	$(SHELL) -lc 'set -e; \
		if [[ ! -d "$(CPP_BUILD_DIR)" ]]; then echo "[info] 未发现 $(CPP_BUILD_DIR)，先执行: make build-cpp"; exit 1; fi; \
		cd "$(CPP_BUILD_DIR)"; \
		if $(CTEST) -N -L bench >/dev/null 2>&1; then $(CTEST) --output-on-failure -L bench -j $(JOBS); else $(CTEST) --output-on-failure -j $(JOBS); fi'
endef

# ---- C++ 子模块：在 cpp/lib/* 各自目录下独立构建与清理
# 要求：每个子模块目录包含自己的 CMakeLists.txt
define _build_cpp_submodules
	$(SHELL) -lc 'set -e; \
		if [[ ! -f .gitmodules ]]; then \
			echo "[cpp-submods] 未检测到 .gitmodules，跳过子模块构建。"; exit 0; \
		fi; \
		paths=$$(git config -f .gitmodules --get-regexp '^submodule\..*\.path' 2>/dev/null \
			| while read -r _ p; do echo $$p; done \
			| grep -E "^$(CPP_SRC_DIR)/lib/" || true); \
		echo "[cpp-submods] 待构建列表:"; echo "$$paths" | sed "s/^/  - /"; \
		declare -a fails=(); declare -a built=(); \
		for p in $$paths; do \
			if [[ ! -d "$$p" ]]; then echo "  - 跳过 $$p（目录不存在）"; continue; fi; \
			if [[ ! -f "$$p/CMakeLists.txt" ]]; then echo "  - 跳过 $$p（无 CMakeLists.txt）"; continue; fi; \
			name=$$(basename "$$p"); \
			b="$$p/build"; mkdir -p "$$b"; \
			extra_opts=""; \
			case "$$name" in \
				rapidjson) extra_opts="-DRAPIDJSON_BUILD_TESTS=OFF -DRAPIDJSON_BUILD_EXAMPLES=OFF -DRAPIDJSON_BUILD_DOC=OFF";; \
				*)         extra_opts="-DBUILD_TESTING=OFF";; \
			esac; \
			echo "  - 构建 $$p -> $$b  (opts: $$extra_opts)"; \
			( \
				set -e; \
				$(CMAKE) -S "$$p" -B "$$b" -DCMAKE_BUILD_TYPE="$(CPP_BUILD_TYPE)" $$extra_opts; \
				$(CMAKE) --build "$$b" --parallel $(JOBS) || $(CMAKE) --build "$$b" -- -j $(JOBS); \
			) && { built+=("$$p"); echo "    -> OK: $$p"; } \
			  || { echo "    -> FAIL: $$p（已记录，继续后续模块）"; fails+=("$$p"); }; \
		done; \
		echo "[cpp-submods] 汇总：$${#built[@]} 成功，$${#fails[@]} 失败。"; \
		if (( $${#fails[@]} > 0 )); then echo "  失败清单："; for f in "$${fails[@]}"; do echo "    - $$f"; done; fi; \
		exit 0;'
endef



define _clean_cpp_submodules
	$(SHELL) -lc 'set -e; \
		if [[ ! -f .gitmodules ]]; then exit 0; fi; \
		echo "[clean-submods] 清理 $(CPP_SRC_DIR)/lib/* 子模块的 build/ …"; \
		paths=$$(git config -f .gitmodules --get-regexp path | awk '\''{print $$2}'\'' | grep -E "^$(CPP_SRC_DIR)/lib/" || true); \
		for p in $$paths; do \
			b="$$p/build"; \
			if [[ -d "$$b" ]]; then echo "  - rm -rf $$b"; rm -rf "$$b"; fi; \
		done; \
		echo "[clean-submods] 完成。"'
endef

# =================== Go 工具与参数 ===================
GO            ?= go
GO_DIR        := golang
GO_COVER_OUT  := $(GO_DIR)/coverage.out
GO_COVER_HTML := $(GO_DIR)/coverage.html

# 可覆盖：例如 make coverage-go GOPROXY=https://goproxy.io,direct
GOPROXY     ?=
GOPRIVATE   ?=
GOTOOLCHAIN ?= local
GOWORK      ?= off                # 强制忽略 go.work

# ---- Go: setup/build/test/coverage ----
define _setup_go
	$(SHELL) -lc 'set -e; \
		echo "[go] 目录: $(GO_DIR)"; \
		if ! command -v $(GO) >/dev/null 2>&1; then echo "[error] 未找到 go 命令"; exit 1; fi; \
		if [[ ! -f "$(GO_DIR)/go.mod" ]]; then \
			echo "[warn] $(GO_DIR)/go.mod 不存在，请先在 $(GO_DIR) 执行: go mod init <module>"; \
			exit 1; \
		fi; \
		cd "$(GO_DIR)"; \
		echo "[go] env: GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY=$(GOPROXY) GOPRIVATE=$(GOPRIVATE)"; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) mod tidy; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) mod download; \
		echo "[ok] go setup 完成。"'
endef

define _build_go
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; \
		echo "[go] build ./... (库包仅做编译检查，不产生仓库内产物)"; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) build ./...; \
		echo "[ok] go build 完成。"'
endef

# 发现 package main 并在 golang/bin 下产出可执行文件
define _build_go_bins
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; mkdir -p bin; \
		echo "[go] 扫描 package main …"; \
		$(GO) list -f "{{if eq .Name \"main\"}}{{.ImportPath}}|{{.Dir}}{{end}}" ./... \
		  | sed "/^$$/d" \
		  | while IFS="|" read -r pkg dir; do \
				name=$$(basename "$$dir"); \
				echo "  - build $$pkg -> bin/$$name"; \
				GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
				$(GO) build -o "bin/$$name" "$$pkg"; \
			done; \
		echo "[ok] go 可执行产物已生成到 $(GO_DIR)/bin/";'
endef

define _test_go
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; \
		echo "[go] test -count=1 -v ./..."; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) test -count=1 -v ./...; \
		echo "[ok] go test 通过。"'
endef

define _coverage_go
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; \
		echo "[go] 计算 coverpkg（跨包覆盖）…"; \
		PKGS=$$($(GO) list ./... | paste -sd, -); \
		PKGS=$$(echo "$$PKGS" | sed "s/,\$$//"); \
		echo "[go] test -covermode=atomic -coverpkg=$$PKGS -coverprofile=coverage.out ./..."; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
		$(GO) test -count=1 -covermode=atomic -coverpkg="$$PKGS" -coverprofile=coverage.out ./...; \
		echo "[go] 覆盖率汇总："; \
		$(GO) tool cover -func=coverage.out; \
		echo "[go] 生成 HTML 报告：$(GO_COVER_HTML)"; \
		$(GO) tool cover -html=coverage.out -o coverage.html; \
		echo "[ok] 覆盖率生成完成：$(GO_COVER_OUT) / $(GO_COVER_HTML)";'
endef

define _fmt_go
	$(SHELL) -lc 'set -e; cd "$(GO_DIR)"; go fmt ./... >/dev/null; echo "[ok] go fmt 完成。"'
endef

define _bench_go
	$(SHELL) -lc 'set -e; cd "$(GO_DIR)"; GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) test -bench=. -benchmem ./...'
endef

# 可选：自定义可执行安装路径
GOBIN ?=

define _install_go
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; \
		echo "[go] install ./... （仅对 package main 生效）"; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
		GOBIN="$(GOBIN)" $(GO) install ./...; \
		if [[ -n "$(GOBIN)" ]]; then \
			echo "[ok] 可执行文件安装到: $(GOBIN)"; \
		else \
			echo "[ok] 可执行文件已安装到默认 GOBIN（见: go env GOBIN 或 GOPATH/bin）"; \
		fi'
endef

define _clean_go
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; \
		echo "[go] 清理覆盖率与产物 …"; \
		rm -f coverage.out coverage.html; \
		rm -rf bin; \
		echo "[go] 执行 go clean（移除测试缓存与临时对象） …"; \
		GOWORK=$(GOWORK) $(GO) clean -testcache; \
		GOWORK=$(GOWORK) $(GO) clean ./...; \
		echo "[ok] go clean 完成。"'
endef

# ----- Rust 通用目标 -----
RUST_DIR := rust/mental1104

define _setup_rust
	$(SHELL) -lc 'set -e; \
		if ! command -v cargo >/dev/null 2>&1; then echo "[error] 未找到 cargo"; exit 1; fi; \
		cd "$(RUST_DIR)"; \
		if [[ -f rust-toolchain.toml ]]; then rustup toolchain install stable || true; rustup override set stable || true; fi; \
		cargo fetch; \
		echo "[ok] rust setup 完成。"'
endef

define _build_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo build --release; echo "[ok] rust build 完成。"'
endef

define _test_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo test --all-features'
endef

define _bench_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo bench'
endef

define _fmt_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo fmt --all'
endef

define _clippy_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo clippy --all-targets --all-features -- -D warnings'
endef

define _example_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo run --example contains'
endef

define _clean_rust
	$(SHELL) -lc 'set -e; \
		cd "$(RUST_DIR)"; \
		cargo clean; \
		cd - >/dev/null; \
		rm -rf "$(RUST_DIR)/coverage" \
		       "$(RUST_DIR)/flamegraph.svg" \
		       "$(RUST_DIR)"/perf.data* || true; \
		find "$(RUST_DIR)" -type f -name "*.profraw" -delete || true; \
		find "$(RUST_DIR)" -type f -name "*.profdata" -delete || true; \
		echo "[ok] rust clean 完成。"'
endef

define _install_rust
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		cd "$(RUST_DIR)"; \
		cargo build --release; \
		cargo doc --no-deps --release; \
		libd="$(PREFIX)/lib/mental1104"; docd="$(PREFIX)/share/doc/mental1104"; \
		$(SUDO) mkdir -p "$$libd" "$$docd"; \
		find target/release/deps -maxdepth 1 -type f \( -name "libmental1104*.rlib" -o -name "libmental1104*.rmeta" \) -exec $(SUDO) cp -f {} "$$libd"/ \; || true; \
		$(SUDO) rm -rf "$$docd/mental1104"; \
		$(SUDO) cp -r target/doc/mental1104 "$$docd"/; \
		echo "[ok] rust 安装完成：库到 $$libd，文档到 $$docd/mental1104"'
endef

# 可选：最低行覆盖率阈值（不带%）。不设则不触发失败
RUST_COVER_FAIL_UNDER ?=

define _coverage_rust
	$(SHELL) -lc 'set -e; \
		cd "$(RUST_DIR)"; \
		rustup component add llvm-tools-preview >/dev/null 2>&1 || true; \
		if ! command -v cargo-llvm-cov >/dev/null 2>&1; then cargo install cargo-llvm-cov; fi; \
		mkdir -p coverage/html coverage; \
		IGNORE="(^|/)(tests?|benches?|examples)/"; \
		echo "[info] 生成 HTML 与 LCOV 报告（仅统计源码，忽略 tests/benches/examples）…"; \
		cargo llvm-cov --all-features --ignore-filename-regex "$$IGNORE" \
			--html --output-dir coverage/html; \
		cargo llvm-cov --all-features --ignore-filename-regex "$$IGNORE" \
			--lcov --output-path coverage/lcov.info; \
		echo "[ok] HTML: coverage/html/index.html"; \
		echo "[ok] LCOV: coverage/lcov.info"; \
		FAIL_ARG=""; \
		if [[ -n "$${RUST_COVER_FAIL_UNDER:-}" ]]; then FAIL_ARG="--fail-under-lines $$RUST_COVER_FAIL_UNDER"; fi; \
		echo "[info] 覆盖率汇总（最后打印）"; \
		cargo llvm-cov --all-features --ignore-filename-regex "$$IGNORE" \
			--summary-only $$FAIL_ARG; \
	'
endef


define _vet_rust
	$(SHELL) -lc 'set -e; \
		cd rust/mental1104; \
		echo "[info] 运行 cargo clippy..."; \
		cargo clippy --all-targets --all-features || true; \
		echo "[ok] vet-rust 检查完成（忽略警告）。"'
endef


define _vet_go
	$(SHELL) -lc 'set -e; \
		cd golang; \
		echo "[info] 运行 go vet 静态分析..."; \
		out=$$(GOWORK=$(GOWORK) $(GO) vet ./... 2>&1 || true); \
		if [ -z "$$out" ]; then \
			echo "[ok] go vet 未发现问题。"; \
		else \
			echo "$$out"; \
			exit 1; \
		fi'
endef


define _vet_python
	$(SHELL) -lc 'set -e; cd python; \
		if ! $(PYTHON) -c "import importlib,sys; sys.exit(0 if importlib.util.find_spec(\"ruff\") else 1)"; then \
			echo \"[error] 未找到 ruff；请先执行: $(PYTHON) -m pip install --user ruff\"; exit 1; \
		fi; \
		$(PYTHON) -m ruff check --select F,B,UP,PERF mental1104'
endef

# 你想检查的目录，默认仅测单元测试目录，避免把 bench/ 拉进来
VET_DIR ?= cpp/test
CPP_BUILD_DIR ?= cpp/build

define _vet_cpp
	$(SHELL) -lc 'set -e; \
		if ! command -v clang-tidy >/dev/null 2>&1; then \
			echo "[error] 未找到 clang-tidy（如：sudo apt install clang-tidy）"; exit 1; \
		fi; \
		if [[ ! -f "$(CPP_BUILD_DIR)/compile_commands.json" ]]; then \
			echo "[hint] 先生成编译数据库：cmake -S cpp -B $(CPP_BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build $(CPP_BUILD_DIR)"; \
			exit 1; \
		fi; \
		echo "[info] clang-tidy 目录: $(VET_DIR)"; \
		find "$(VET_DIR)" -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \) -print0 \
		| xargs -0 -r clang-tidy -p "$(CPP_BUILD_DIR)" --quiet \
			--checks="-*,bugprone-*,performance-*,clang-analyzer-*" \
			--warnings-as-errors="*" \
			-header-filter="^.*/cpp/include/mental1104/.*"; \
		echo "[ok] vet-cpp 完成。"; \
	'
endef

.PHONY: vet vet-rust vet-go vet-python vet-cpp
vet:        vet-python vet-cpp vet-go vet-rust
vet-rust:   ; $(call _vet_rust)
vet-go:     ; $(call _vet_go)
vet-python: ; $(call _vet_python)
vet-cpp:    ; $(call _vet_cpp)


define _guard_cpp
	$(SHELL) -lc 'set -e -o pipefail; MODE="${MODE:-mem}"; \
		if [ "$$MODE" = "all" ]; then $(MAKE) guard-cpp MODE=mem; $(MAKE) guard-cpp MODE=race; exit 0; fi; \
		if [ "$$MODE" = "heap" ]; then \
			if ! command -v valgrind >/dev/null 2>&1; then echo "[error] 未安装 valgrind"; exit 1; fi; \
			cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; cmake --build cpp/build -j $(JOBS); \
			echo "[info] 运行 massif 生成内存占用画像"; \
			find cpp/build/bin -maxdepth 1 -type f -name "test_*" -executable | while read -r t; do \
				out="`basename $$t`.massif"; echo "[info] massif $$t -> $$out"; valgrind --tool=massif --time-unit=ms --stacks=yes --massif-out-file="$$out" "$$t" || true; \
			done; echo "[ok] guard-cpp[heap] 已生成 massif 报告"; exit 0; \
		fi; \
		if [ "$$MODE" = "race" ]; then \
			B="cpp/build-tsan"; CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=thread"; \
			TS="TSAN_OPTIONS=halt_on_error=1"; cmake -S cpp -B "$$B" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$$CFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$$CFLAGS" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; \
			cmake --build "$$B" -j $(JOBS); cd "$$B"; log="guard_tsan.log"; \
			if ! env $$TS ctest --output-on-failure -j $(JOBS) -LE bench | tee "$$log"; then \
				if grep -qiE "ThreadSanitizer|data race" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$B/$$log"; else echo "[fail][test] 非并发导致失败, 日志 $$B/$$log"; fi; exit 1; \
			fi; if grep -qiE "ThreadSanitizer|data race" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$B/$$log"; exit 1; fi; \
			echo "[ok] guard-cpp[race] 通过"; exit 0; \
		fi; \
		B="cpp/build-asan"; CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined"; \
		AA="ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:check_initialization_order=1:detect_stack_use_after_return=1:halt_on_error=1"; \
		UA="UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"; \
		cmake -S cpp -B "$$B" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$$CFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$$CFLAGS" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; \
		cmake --build "$$B" -j $(JOBS); cd "$$B"; log="guard_asan.log"; \
		if ! env $$AA $$UA ctest --output-on-failure -j $(JOBS) -LE bench | tee "$$log"; then \
			if grep -qiE "AddressSanitizer|heap-use-after-free|use-after-free|stack-use-after-return|buffer-overflow|leak" "$$log"; then echo "[fail][memory] 内存读写或泄漏问题, 日志 $$B/$$log"; \
			elif grep -qiE "UndefinedBehaviorSanitizer|runtime error" "$$log"; then echo "[fail][ub] 未定义行为问题, 日志 $$B/$$log"; \
			else echo "[fail][test] 非内存导致失败, 日志 $$B/$$log"; fi; exit 1; \
		fi; echo "[ok] guard-cpp[mem] 通过"; \
	'
endef

define _guard_go
	$(SHELL) -lc 'set -e -o pipefail; cd "$(GO_DIR)"; log=$$(mktemp -t guard_go.XXXX).log; \
		echo "[go] go test -race -count=1 ./..."; \
		if ! GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) $(GO) test -race -count=1 ./... | tee "$$log"; then \
			if grep -q "DATA RACE" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$log"; else echo "[fail][test] 测试失败, 日志 $$log"; fi; exit 1; \
		fi; if grep -q "DATA RACE" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$log"; exit 1; fi; \
		echo "[ok] guard-go 通过"; \
	'
endef

RUST_DIR ?= rust/mental1104
MODE ?= all

# 可选：设为 1 时自动安装缺失的 nightly / 组件；默认仅提示
AUTO_SETUP_RUST_NIGHTLY ?= 0

# 根目录下的 Rust 工程相对路径
RUST_DIR ?= rust/mental1104

# 设个默认模式，用户也可临时覆盖：make guard-rust MODE=mem
MODE ?= all
# AUTO_SETUP_RUST_NIGHTLY=1 时才自动安装 nightly/组件；默认只提示
AUTO_SETUP_RUST_NIGHTLY ?= 0

# 根目录相对路径
RUST_DIR ?= rust/mental1104
# 默认跑内存 + 并发
MODE ?= all

define _guard_rust
	$(SHELL) -lc 'set -euo pipefail; \
		cd "$(RUST_DIR)"; \
		MODE_VAL="$${MODE:-all}"; \
		case "$$MODE_VAL" in mem|race|miri|all) ;; *) echo "[error] MODE 仅支持 mem|race|miri|all"; exit 2;; esac; \
		echo "[info] rust guard MODE=$$MODE_VAL"; \
		if [ "$$MODE_VAL" = "all" ]; then MODES="mem race"; else MODES="$$MODE_VAL"; fi; \
		NEED_NIGHTLY=0; for m in $$MODES; do case "$$m" in mem|race|miri) NEED_NIGHTLY=1;; esac; done; \
		if [ $$NEED_NIGHTLY -eq 1 ]; then \
			if ! rustup toolchain list | grep -q "^nightly"; then \
				echo "[error] 未检测到 nightly 工具链。请先运行: rustup toolchain install nightly"; exit 3; \
			fi; \
		fi; \
		for m in $$MODES; do \
			if [ "$$m" = "mem" ]; then \
				echo "[mem] AddressSanitizer 开始"; \
				export RUSTFLAGS="-Zsanitizer=address"; \
				export RUSTDOCFLAGS="-Zsanitizer=address"; \
				export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:malloc_context_size=20"; \
				cargo +nightly test --tests -Zbuild-std || { echo "[fail][memory] 内存问题"; exit 1; }; \
				echo "[ok] mem 通过"; \
			elif [ "$$m" = "race" ]; then \
				echo "[race] ThreadSanitizer 开始"; \
				export RUSTFLAGS="-Zsanitizer=thread"; \
				export RUSTDOCFLAGS="-Zsanitizer=thread"; \
				export TSAN_OPTIONS="halt_on_error=1:report_signal_unsafe=0"; \
				cargo +nightly test --tests -Zbuild-std || { echo "[fail][race] 并发竞态问题"; exit 1; }; \
				echo "[ok] race 通过"; \
			else \
				echo "[miri] 开始"; \
				if ! cargo +nightly miri --version >/dev/null 2>&1; then \
					echo "[error] 未安装 miri。请执行: rustup +nightly component add miri"; exit 4; \
				fi; \
				cargo +nightly miri test || { echo "[fail][miri] 未定义行为"; exit 1; }; \
				echo "[ok] miri 通过"; \
			fi; \
		done'
endef

.PHONY: guard-rust guard-rust-mem guard-rust-race guard-rust-miri
guard-rust:
	$(call _guard_rust)

guard-rust-mem:
	$(MAKE) --no-print-directory guard-rust MODE=mem

guard-rust-race:
	$(MAKE) --no-print-directory guard-rust MODE=race)

guard-rust-miri:
	$(MAKE) --no-print-directory guard-rust MODE=miri


define _guard_python
	@set -euo pipefail; \
	cd python; \
	if python3 -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec(\"memray\") else 1)"; then \
		echo "[py] memray 运行 pytest 以生成内存分配画像"; \
		python3 -m memray run -o memray.bin -m pytest -q \
		|| { echo "[fail][memory] pytest 失败或内存画像生成失败 (python/memray.bin)"; exit 1; }; \
		python3 -m memray summary memray.bin || true; \
		echo "[ok] guard-python[memray] 完成"; \
	elif python3 -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec(\"pytest_leaks\") else 1)"; then \
		echo "[py] 使用 pytest-leaks 检测引用泄漏"; \
		pytest --leaks -q \
		|| { echo "[fail][memory] 可能存在引用泄漏"; exit 1; }; \
		echo "[ok] guard-python[leaks] 通过"; \
	else \
		echo "[hint] 未安装 memray 或 pytest-leaks，回退为纯 pytest"; \
		python3 -m pytest -q \
		|| { echo "[fail][test] pytest 失败"; exit 1; }; \
		echo "[ok] guard-python 通过"; \
	fi
endef


.PHONY: guard guard-cpp guard-go guard-rust guard-python
guard-cpp:    ; $(call _guard_cpp)
guard-go:     ; $(call _guard_go)
guard-python: ; $(call _guard_python)
guard:        guard-cpp guard-go guard-rust guard-python




.PHONY: setup-rust build-rust test-rust bench-rust fmt-rust clippy-rust example-rust clean-rust
setup-rust:   ; $(call _setup_rust)
build-rust:   | setup-rust ; $(call _build_rust)
test-rust:    ; $(call _test_rust)
bench-rust:   ; $(call _bench_rust)
fmt-rust:     ; $(call _fmt_rust)
clippy-rust:  ; $(call _clippy_rust)
example-rust: ; $(call _example_rust)
clean-rust:   ; $(call _clean_rust)
install-rust: ; $(call _install_rust)
coverage-rust:; $(call _coverage_rust)

# =================== 直达入口（Python） ===================
.PHONY: setup-python build-python test-python install-python clean-python coverage-python fmt-python bench-python
setup-python:   ; $(call _setup_python)
build-python:   | setup-python
test-python:    ; $(call _test_python)
install-python: ; $(call _install_python)
clean-python:   ; $(call _clean_python)
coverage-python:; $(call _coverage_python)
fmt-python:     ; $(call _fmt_python)
bench-python:   ; $(call _bench_python)

# =================== 直达入口（Go） ===================
.PHONY: setup-go build-go test-go coverage-go install-go clean-go fmt-go bench-go
setup-go:    ; $(call _setup_go)
build-go:    | setup-go ; $(call _build_go)
test-go:     ; $(call _test_go)
coverage-go: | test-go  ; $(call _coverage_go) 
install-go: ; $(call _install_go)
clean-go:   ; $(call _clean_go)
fmt-go:   ; $(call _fmt_go)
bench-go: ; $(call _bench_go)


# =================== 直达入口（C++） ===================
.PHONY: git-submodules setup-cpp build-cpp test-cpp install-cpp clean-cpp coverage-cpp fmt-cpp bench-cpp
git-submodules:        ; $(call _git_fetch_submodules)

# 关键改动：setup-cpp 会**逐个进入 cpp/lib/* 构建到各自 build/**，随后顶层做一次 cmake 配置
setup-cpp:
	$(MAKE) git-submodules
	$(call _build_cpp_submodules)
	$(call _configure_cpp)

# 顶层构建依赖 setup-cpp，避免找不到子模块产物
build-cpp:      | setup-cpp ; $(call _build_cpp)
test-cpp:       ; $(call _test_cpp)
install-cpp:    ; $(call _install_cpp)

clean-cpp:
	$(call _clean_cpp_submodules)
	$(call _clean_cpp)

coverage-cpp:   | test-cpp  ; $(call _coverage_cpp)
fmt-cpp:  ; $(call _fmt_cpp)
bench-cpp:; $(call _bench_cpp)


# =================== 聚合入口（Python + C++） ===================
.PHONY: setup build test install clean coverage help fmt bench
setup:    setup-python setup-go setup-cpp setup-rust
build:    build-python build-go build-cpp build-rust
test:     test-python test-go test-cpp test-rust
install:  install-python install-go install-cpp install-rust
clean:    clean-python clean-go clean-cpp clean-rust
coverage: coverage-python coverage-go coverage-cpp coverage-rust
fmt:      fmt-go fmt-cpp fmt-rust

help:
	@echo "可用目标："
	@echo "  —— Python ——"
	@echo "    setup-python           安装依赖并执行 generate_init.py（python/）"
	@echo "    test-python            运行 pytest（目录：python）"
	@echo "    install-python         安装本地包（python/）"
	@echo "    clean-python           清理缓存与构建产物"
	@echo "    coverage-python        覆盖率报告"
	@echo "  —— C++ ——"
	@echo "    git-submodules         拉取并自动修复子模块"
	@echo "    setup-cpp              **逐个构建 cpp/lib/* 到各自 build/**，并配置顶层***
