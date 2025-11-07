# Makefile — 单文件、无 heredoc；公共逻辑用 define 宏封装，目标直接调用

# =================== 变量（Variables） ===================

SHELL := /bin/bash
.ONESHELL:
.SILENT:

# === 自动加载 .env 到 Make 环境（全局生效） =================
REPO_ROOT := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
ENV_SRC   ?= $(REPO_ROOT)/.env
ENV_MK    := $(abspath $(ENV_SRC)).mk
ENV_REQ   ?= PGUSER PGPASSWORD PGHOST PGPORT PGDATABASE   # 需要可改

$(ENV_MK):
	@set -e
	if [ -f "$(ENV_SRC)" ]; then
		awk '\
		  /^[[:space:]]*#/ || /^[[:space:]]*$$/ { next } \
		  { line=$$0; sub(/^[[:space:]]*export[[:space:]]+/, "", line); \
		    i=index(line,"="); if(i==0) next; \
		    key=substr(line,1,i-1); val=substr(line,i+1); \
		    sub(/^[[:space:]]+|[[:space:]]+$$/,"",key); \
		    sub(/^[[:space:]]+/,"",val); sub(/[[:space:]]+#.*/, "", val); \
		    if(val ~ /^".*"$$/){sub(/^"/,"",val); sub(/"$$/,"",val)} \
		    else if(val ~ /^'\''.*'\''$$/){sub(/^'\''/,"",val); sub(/'\''$$/,"",val)} \
		    print "export " key " = " val; }' \
		  "$(ENV_SRC)" > "$(ENV_MK)"
		echo "[ok] .env -> $(ENV_MK)"
	else
		: > "$(ENV_MK)"; echo "[warn] 未找到 $(ENV_SRC)，以空环境继续"
	fi

-include $(ENV_MK)
export

.PHONY: env-guard env-print env-expose
env-guard:
	@missing=""; \
	for k in $(ENV_REQ); do eval 'v=$${'$$k':-}'; [ -n "$$v" ] || missing="$$missing $$k"; done; \
	if [ -n "$$missing" ]; then echo "[err] 缺少必需环境变量:$$missing （来源 $(ENV_SRC)）"; exit 2; fi
env-print:
	@env | grep -E '^(PG|POSTGRES|DATABASE_URL|REDIS|PULSAR)=' | sort || true
env-expose:   # 可让当前交互式 shell 生效： eval "$$(make env-expose)"
	@sed -E 's/^export[[:space:]]+([^=[:space:]]+)[[:space:]]*=[[:space:]]*(.*)$/export \1=\2/' "$(ENV_MK)"
# ============================================================


# 变量: .DEFAULT_GOAL —— 默认入口目标
.DEFAULT_GOAL := setup

# ---------- 基础开关/环境检测 ----------
# 变量: UID —— 当前用户 ID
UID := $(shell id -u)

# 变量: SUDO —— 非 root 时使用 sudo
SUDO := $(if $(filter 0,$(UID)),,sudo)

# 变量: SUDO_MSG —— 提示将使用 sudo
SUDO_MSG := $(if $(filter 0,$(UID)),,@echo "[info] 当前非root，将使用sudo执行；可能会提示输入管理员密码。")

# 变量: UNAME_S —— 系统内核名（Linux/Darwin 等）
UNAME_S := $(shell uname -s)

# 变量: IS_UBUNTU —— 是否为 Ubuntu（1 表示是）
IS_UBUNTU := $(shell sh -lc 'u=$$(uname -s); if [ "$$u" = Linux ] && [ -r /etc/os-release ]; then . /etc/os-release; [ "$$ID" = ubuntu ] && echo 1; fi')

# 变量: BREAK_FLAG —— Ubuntu 上 pip 的 --break-system-packages 兼容标记
BREAK_FLAG := $(if $(IS_UBUNTU),--break-system-packages,)

# 变量: JOBS —— CPU 并行度
JOBS ?= $(shell sh -lc 'command -v nproc >/dev/null 2>&1 && nproc || sysctl -n hw.ncpu 2>/dev/null || echo 4')

# ---------- 工具/路径/参数 ----------
# 变量: PYTHON —— Python 解释器
PYTHON ?= python3

# 变量: PIP3 —— pip 命令
PIP3 ?= pip3

# 变量: CMAKE —— CMake 命令
CMAKE ?= cmake

# 变量: CTEST —— CTest 命令
CTEST ?= ctest

# 变量: CPP_SRC_DIR —— C++ 源代码顶层目录
CPP_SRC_DIR := cpp

# 变量: CPP_BUILD_DIR —— C++ 顶层构建目录
CPP_BUILD_DIR := $(CPP_SRC_DIR)/build

# 变量: CPP_BUILD_TYPE —— CMake 构建类型
CPP_BUILD_TYPE ?= Debug

# 变量: PREFIX —— 安装前缀路径
PREFIX ?= /usr/local

# ---------- 通用测试 verbose 开关 ----------
# 变量: VERBOSE —— 全局冗余开关（0/1）
VERBOSE ?= 0

# 变量: TEST_VERBOSE —— 测试冗余开关（继承 VERBOSE）
TEST_VERBOSE ?= $(VERBOSE)

# 变量: CTEST_V —— CTest 的 -V 开关
CTEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-V,)

# 变量: PYTEST_V —— pytest 冗余开关
PYTEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-vv,-q)

# 变量: GO_TEST_V —— go test 冗余开关
GO_TEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-v,)

# 变量: CARGO_TEST_V —— cargo test 冗余开关
CARGO_TEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-v,)

# ---------- PIP 镜像配置 ----------
# 变量: USE_PIP_MIRROR —— 是否启用镜像（1=启用）
USE_PIP_MIRROR := 1

# 变量: PIP_INDEX_URL —— 主索引（HTTPS）
PIP_INDEX_URL := https://pypi.tuna.tsinghua.edu.cn/simple

# 变量: PIP_EXTRA_INDEX_URL —— 备用索引（可空）
PIP_EXTRA_INDEX_URL :=

# 变量: PIP_TRUSTED_HOST —— 可信主机名（用于 http 源；可空）
PIP_TRUSTED_HOST :=

# 变量: PIP_MIRROR_OPTS —— 根据上述配置拼接出的 pip 选项
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

# ---------- Go 工具与参数 ----------
# 变量: GO —— go 命令
GO ?= go

# 变量: GO_DIR —— Go 项目根目录
GO_DIR := golang

# 变量: GO_COVER_OUT —— Go 覆盖率输出文件
GO_COVER_OUT := $(GO_DIR)/coverage.out

# 变量: GO_COVER_HTML —— Go 覆盖率 HTML 报告
GO_COVER_HTML := $(GO_DIR)/coverage.html

# 变量: GOPROXY —— Go 代理（可空）
GOPROXY ?=

# 变量: GOPRIVATE —— 私有模块前缀（可空）
GOPRIVATE ?=

# 变量: GOTOOLCHAIN —— Go 工具链策略
GOTOOLCHAIN ?= local

# 变量: GOWORK —— 是否启用 go.work（off=忽略）
GOWORK ?= off

# 变量: GOBIN —— go install 目标目录（可空，默认 GOPATH/bin）
GOBIN ?=

# ---------- Rust 基础变量 ----------
# 变量: RUST_DIR —— 根目录下 Rust 工程相对路径
RUST_DIR := rust/mental1104

# 变量: RUST_COVER_FAIL_UNDER —— Rust 覆盖率最小行覆盖阈值（可空）
RUST_COVER_FAIL_UNDER ?=

# ---------- 额外 Vet/Guard 配置中的变量（保持原语义与重复行） ----------
# 变量: VET_DIR —— clang-tidy 检查目录（默认仅测单测目录）
VET_DIR ?= cpp/test

# 变量: CPP_BUILD_DIR（二次兜底）—— clang-tidy 需要的编译数据库目录
CPP_BUILD_DIR ?= cpp/build

# 变量: RUST_DIR（重复兜底）—— 再次声明为可覆盖
RUST_DIR ?= rust/mental1104

# 变量: MODE —— guard 的默认模式（mem/race/miri/all）
MODE ?= all

# 变量: AUTO_SETUP_RUST_NIGHTLY —— 是否自动装 nightly（0/1）
AUTO_SETUP_RUST_NIGHTLY ?= 0

# 变量: RUST_DIR（重复兜底）
RUST_DIR ?= rust/mental1104

# 变量: MODE（重复声明，保持原文件习惯）
MODE ?= all

# 变量: AUTO_SETUP_RUST_NIGHTLY（重复声明，保持原文件习惯）
AUTO_SETUP_RUST_NIGHTLY ?= 0

# 变量: RUST_DIR（再次重复兜底）
RUST_DIR ?= rust/mental1104

# 变量: MODE（再次重复，保持原文件习惯）
MODE ?= all


# =================== 函数/宏（Defines） ===================

# 函数: _git_fetch_submodules —— 拉取并自动修复可能损坏的 git 子模块
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

# 函数: _setup_python —— 安装 Python 依赖并构建本地 wheel
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

# 函数: _test_python —— 运行 Python 测试（排除 bench/benchmark）
define _test_python
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
		echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"; \
		if ! command -v pytest >/dev/null 2>&1; then echo "[warn] 未检测到 pytest；请先执行: make setup-python"; exit 1; fi; \
		cd python; \
		$(PYTHON) -m pytest $(PYTEST_V) -k "not bench and not benchmark"'
endef

# 函数: _install_python —— 安装本地 python/ 包（尊重镜像与 break flag）
define _install_python
	$(SHELL) -lc 'set -e; \
		echo "[info] $(PIP3) install python/ --upgrade $(BREAK_FLAG) $(PIP_MIRROR_OPTS)"; \
		$(SUDO) $(PIP3) install python/ --upgrade $(BREAK_FLAG) $(PIP_MIRROR_OPTS); \
		echo "[ok] 安装完成。"'
endef

# 函数: _clean_python —— 清理 Python 构建/缓存/临时文件
define _clean_python
	$(SHELL) -lc 'set -e; \
		echo "[info] 清理 Python 缓存与构建产物…"; \
		rm -rf python/build python/dist python/*.egg-info .pytest_cache .mypy_cache python/.coverage htmlcov python/.ruff_cache python/.pytest_cache python/.benchmarks python/memray.bin; \
		find python -type d -name "__pycache__" -exec rm -rf {} +; \
		find python -type f -name "*.py[co]" -delete; \
		find python -type d -name ".pytest_cache" -exec rm -rf {} +; \
		echo "[ok] clean 完成。"' 
endef

# 函数: _coverage_python —— 生成 Python 覆盖率报告
define _coverage_python
	$(SHELL) -lc 'set -e; \
		echo "[info] 运行python单元测试覆盖率"; \
		cd python && $(PYTHON) -m coverage run --source=. -m pytest && coverage report;'
endef

# 函数: _fmt_python —— 使用 autopep8 格式化 Python 代码
define _fmt_python
	$(SHELL) -lc 'set -e; \
		if ! $(PYTHON) -c "import autopep8" >/dev/null 2>&1; then $(PYTHON) -m pip install --user autopep8 $(BREAK_FLAG); fi; \
		cd python; \
		$(PYTHON) -m autopep8 --in-place --recursive --max-line-length=120 --ignore=E402,E226,E24,W50,W690 .; \
		echo "[ok] python autopep8 fmt 完成。"'
endef

# 函数: _bench_python —— 运行 Python 基准（pytest-benchmark 优先）
define _bench_python
	$(SHELL) -lc 'set -e; \
		if ! command -v pytest >/dev/null 2>&1; then echo "[error] 未检测到 pytest；请先执行: make setup-python"; exit 1; fi; \
		cd python; \
		if $(PYTHON) -m pytest -q --help 2>/dev/null | grep -qi benchmark; then \
			echo "[pytest-benchmark] 基准用例"; \
			$(PYTHON) -m pytest $(PYTEST_V) -k "bench or benchmark" --benchmark-only --benchmark-autosave; \
		else \
			echo "[warn] 未检测到 pytest-benchmark，回退到名称筛选"; \
			$(PYTHON) -m pytest $(PYTEST_V) -k "bench or benchmark"; \
		fi'
endef

# 函数: _configure_cpp —— 顶层 C++ CMake 配置
define _configure_cpp
	$(SHELL) -lc 'set -e; \
		mkdir -p "$(CPP_BUILD_DIR)"; \
		$(CMAKE) -S "$(CPP_SRC_DIR)" -B "$(CPP_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CPP_BUILD_TYPE)"; \
		echo "[ok] 顶层 cmake 配置完成（$(CPP_BUILD_TYPE)）"'
endef

# 函数: _build_cpp —— 顶层 C++ 并行构建
define _build_cpp
	$(SHELL) -lc 'set -e; \
		if $(CMAKE) --build "$(CPP_BUILD_DIR)" --parallel $(JOBS); then :; \
		else $(CMAKE) --build "$(CPP_BUILD_DIR)" -- -j $(JOBS); fi; \
		echo "[ok] 顶层 C++ 构建完成（-j $(JOBS)）"'
endef

# 函数: _test_cpp —— 顶层 C++ 测试（排除 bench）
define _test_cpp
	$(SHELL) -lc 'set -e; \
		unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
		echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"; \
		cd "$(CPP_BUILD_DIR)"; \
		$(CTEST) --output-on-failure -LE bench -j $(JOBS) $(CTEST_V)'
endef

# 函数: _coverage_cpp —— C++ 覆盖率（优先 gcovr，回退 lcov）
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

# 函数: _install_cpp —— 安装 C++ 产物到 PREFIX
define _install_cpp
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		echo "[info] 安装到前缀: $(PREFIX)"; \
		$(SUDO) $(CMAKE) --install "$(CPP_BUILD_DIR)" --prefix "$(PREFIX)"; \
		echo "[ok] 安装完成。"' 
endef

# 函数: _clean_cpp —— 清理顶层 C++ 构建目录
define _clean_cpp
	$(SHELL) -lc 'set -e; \
		echo "[info] 清理顶层 C++ 构建目录: $(CPP_BUILD_DIR)"; \
		rm -rf "$(CPP_BUILD_DIR)"; \
		echo "[ok] clean 完成。"' 
endef

# 函数: _fmt_cpp —— 使用 clang-format 格式化 C++（排除 lib/ 与 thirdparty/）
define _fmt_cpp
	$(SHELL) -lc 'set -e; \
		if ! command -v clang-format >/dev/null 2>&1; then echo "[error] 未找到 clang-format"; exit 1; fi; \
		find cpp \
		  \( -path "cpp/lib" -o -path "cpp/lib/*" -o -path "cpp/thirdparty" -o -path "cpp/thirdparty/*" \) -prune -o \
		  -type f -regex ".*\.\(h\|hh\|hpp\|hxx\|c\|cc\|cpp\|cxx\)" -print0 | xargs -0 -n 50 clang-format -i; \
		echo "[ok] cpp fmt 完成（已排除 cpp/lib 与 cpp/thirdparty）"'
endef

# 函数: _bench_cpp —— 运行 C++ 基准（优先 label=bench）
define _bench_cpp
	$(SHELL) -lc 'set -e; \
		if [[ ! -d "$(CPP_BUILD_DIR)" ]]; then echo "[info] 未发现 $(CPP_BUILD_DIR)，先执行: make build-cpp"; exit 1; fi; \
		cd "$(CPP_BUILD_DIR)"; \
		if $(CTEST) -N -L bench >/dev/null 2>&1; then $(CTEST) --output-on-failure -L bench -j $(JOBS) $(CTEST_V); else $(CTEST) --output-on-failure -j $(JOBS) $(CTEST_V); fi'
endef

# 函数: _build_cpp_submodules —— 在 cpp/lib/* 各模块内独立构建
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

# 函数: _clean_cpp_submodules —— 清理 cpp/lib/* 下的 build/ 目录
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

# 函数: _setup_go —— Go 环境准备（tidy/download）
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

# 函数: _build_go —— Go 构建（库包仅做编译检查）
define _build_go
	$(SHELL) -lc 'set -e; \
		cd "$(GO_DIR)"; \
		echo "[go] build ./... (库包仅做编译检查，不产生仓库内产物)"; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) build ./...; \
		echo "[ok] go build 完成。"'
endef

# 函数: _build_go_bins —— 探测 package main 并产出二进制到 golang/bin
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

# 函数: _test_go —— 运行 Go 测试
define _test_go
	$(SHELL) -lc 'set -e; \
		unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
		echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"; \
		cd "$(GO_DIR)"; \
		echo "[go] test -count=1 $(GO_TEST_V) ./..."; \
		GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
		$(GO) test -count=1 $(GO_TEST_V) ./...; \
		echo "[ok] go test 通过。"'
endef

# 函数: _coverage_go —— 计算 Go 覆盖率（跨包）
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

# 函数: _fmt_go —— go fmt
define _fmt_go
	$(SHELL) -lc 'set -e; cd "$(GO_DIR)"; go fmt ./... >/dev/null; echo "[ok] go fmt 完成。"'
endef

# 函数: _bench_go —— 运行 Go 基准
define _bench_go
	$(SHELL) -lc 'set -e; cd "$(GO_DIR)"; GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
	$(GO) test $(GO_TEST_V) -bench=. -benchmem ./...'
endef

# 函数: _install_go —— go install（仅对 package main 生效）
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

# 函数: _clean_go —— 清理 Go 产物与测试缓存
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

# 函数: _setup_rust —— Rust 环境准备
define _setup_rust
	$(SHELL) -lc 'set -e; \
		if ! command -v cargo >/dev/null 2>&1; then echo "[error] 未找到 cargo"; exit 1; fi; \
		cd "$(RUST_DIR)"; \
		if [[ -f rust-toolchain.toml ]]; then rustup toolchain install stable || true; rustup override set stable || true; fi; \
		cargo fetch; \
		echo "[ok] rust setup 完成。"'
endef

# 函数: _build_rust —— Rust 构建 release
define _build_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo build --release; echo "[ok] rust build 完成。"'
endef

# 函数: _test_rust —— Rust 测试
define _test_rust
	$(SHELL) -lc 'set -e; unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
		echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"; \
		cd "$(RUST_DIR)"; cargo test --all-features $(CARGO_TEST_V)'
endef

# 函数: _bench_rust —— Rust 基准
define _bench_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo bench $(CARGO_TEST_V)'
endef

# 函数: _fmt_rust —— cargo fmt
define _fmt_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo fmt --all'
endef

# 函数: _clippy_rust —— cargo clippy（将警告视为错误）
define _clippy_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo clippy --all-targets --all-features -- -D warnings'
endef

# 函数: _example_rust —— 运行示例 contains
define _example_rust
	$(SHELL) -lc 'set -e; cd "$(RUST_DIR)"; cargo run --example contains'
endef

# 函数: _clean_rust —— 清理 Rust 构建与分析产物
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

# 函数: _install_rust —— 安装 Rust 库与文档到 PREFIX
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

# 函数: _coverage_rust —— 生成 Rust 覆盖率（HTML/LCOV + 汇总）
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

# 函数: _vet_rust —— 运行 cargo clippy（忽略警告退出码）
define _vet_rust
	$(SHELL) -lc 'set -e; \
		cd rust/mental1104; \
		echo "[info] 运行 cargo clippy..."; \
		cargo clippy --all-targets --all-features || true; \
		echo "[ok] vet-rust 检查完成（忽略警告）。"'
endef

# 函数: _vet_go —— 运行 go vet（有输出则失败）
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

# 函数: _vet_python —— 运行 ruff（F/B/UP/PERF 规则）
define _vet_python
	$(SHELL) -lc 'set -e; cd python; \
		if ! $(PYTHON) -c "import importlib,sys; sys.exit(0 if importlib.util.find_spec(\"ruff\") else 1)"; then \
			echo \"[error] 未找到 ruff；请先执行: $(PYTHON) -m pip install --user ruff\"; exit 1; \
		fi; \
		$(PYTHON) -m ruff check --select F,B,UP,PERF mental1104'
endef

# 函数: _vet_cpp —— 使用 clang-tidy 对指定目录进行检查
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

# 函数: _guard_cpp —— C++ 组合防护（ASAN/UBSAN/TSAN/Massif）
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

# 函数: _guard_go —— Go 并发竞态与测试防护
define _guard_go
	$(SHELL) -lc 'set -e -o pipefail; cd "$(GO_DIR)"; log=$$(mktemp -t guard_go.XXXX).log; \
		echo "[go] go test -race -count=1 ./..."; \
		if ! GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) $(GO) test -race -count=1 ./... | tee "$$log"; then \
			if grep -q "DATA RACE" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$log"; else echo "[fail][test] 测试失败, 日志 $$log"; fi; exit 1; \
		fi; if grep -q "DATA RACE" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$log"; exit 1; fi; \
		echo "[ok] guard-go 通过"; \
	'
endef

# 函数: _guard_rust —— Rust 组合防护（ASan/Tsan/Miri，可 MODE=mem|race|miri|all）
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

# 函数: _guard_python —— Python 内存画像/泄漏/回退 pytest
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


# =================== 入口/目标（Targets） ===================

.PHONY: vet vet-rust vet-go vet-python vet-cpp
vet:        vet-python vet-cpp vet-go vet-rust
vet-rust:   ; $(call _vet_rust)
vet-go:     ; $(call _vet_go)
vet-python: ; $(call _vet_python)
vet-cpp:    ; $(call _vet_cpp)

.PHONY: guard-rust guard-rust-mem guard-rust-race guard-rust-miri
guard-rust:
	$(call _guard_rust)

guard-rust-mem:
	$(MAKE) --no-print-directory guard-rust MODE=mem

guard-rust-race:
	$(MAKE) --no-print-directory guard-rust MODE=race

guard-rust-miri:
	$(MAKE) --no-print-directory guard-rust MODE=miri

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

# ============ env 模板生成 ============
ENV_SRC      ?= .env
ENV_EXAMPLE  ?= .env.example

.PHONY: env-example
env-example:
	@[ -f $(ENV_SRC) ] || { echo "[err] $(ENV_SRC) 不存在"; exit 1; }
	@awk '\
		/^[[:space:]]*#/ { print; next } \
		/^[[:space:]]*$$/ { print; next } \
		{ \
			line = $$0; \
			sub(/^[[:space:]]*export[[:space:]]+/, "", line); \
			if (index(line, "=") == 0) { print "#" $$0; next } \
			key = line; \
			sub(/=.*/, "", key); \
			sub(/^[[:space:]]+|[[:space:]]+$$/, "", key); \
			print key "="; \
		} \
	' $(ENV_SRC) > $(ENV_EXAMPLE)
	@echo "[ok] 生成 $(ENV_EXAMPLE)"


# =================== 聚合入口（Python + C++ + Go + Rust） ===================
.PHONY: setup build test install clean coverage help fmt bench
setup:    env-example setup-python setup-go setup-cpp setup-rust
build:    build-python build-go build-cpp build-rust
test:     test-python test-go test-cpp test-rust
install:  install-python install-go install-cpp install-rust
clean:    clean-python clean-go clean-cpp clean-rust
coverage: coverage-python coverage-go coverage-cpp coverage-rust
fmt:      fmt-go fmt-cpp fmt-rust
bench:    bench-python bench-go bench-cpp bench-rust

.PHONY: test-v test-cpp-v test-python-v test-go-v test-rust-v
test-v:        ; $(MAKE) --no-print-directory test        VERBOSE=1
test-cpp-v:    ; $(MAKE) --no-print-directory test-cpp    VERBOSE=1
test-python-v: ; $(MAKE) --no-print-directory test-python VERBOSE=1
test-go-v:     ; $(MAKE) --no-print-directory test-go     VERBOSE=1
test-rust-v:   ; $(MAKE) --no-print-directory test-rust   VERBOSE=1

.PHONY: bench-v bench-cpp-v bench-python-v bench-go-v bench-rust-v
bench-v:        ; $(MAKE) --no-print-directory bench        VERBOSE=1
bench-cpp-v:    ; $(MAKE) --no-print-directory bench-cpp    VERBOSE=1
bench-python-v: ; $(MAKE) --no-print-directory bench-python VERBOSE=1
bench-go-v:     ; $(MAKE) --no-print-directory bench-go     VERBOSE=1
bench-rust-v:   ; $(MAKE) --no-print-directory bench-rust   VERBOSE=1

# --- docker相关

# Makefile — 管理树状目录下的 docker-compose.yaml（所有目标以 docker- 前缀）
.ONESHELL:
.SILENT:
MAKEFLAGS += --no-builtin-rules

# 可调参数
COMPOSE_BIN        ?= docker compose
COMPOSE_FILE_NAME  ?= docker-compose.yaml

# 自动发现所有含 compose 文件的目录
COMPOSE_DIRS := $(shell find . -type f -name $(COMPOSE_FILE_NAME) -exec dirname {} \; | sort -u)

.PHONY: docker-help docker-list docker-up-all docker-down-all docker-up docker-down docker-ps docker-logs docker-pull docker-net

docker-help: ## 显示可用命令
	@echo "Targets:"
	@awk 'BEGIN{FS":.*## "}/^docker-[a-zA-Z0-9_.-]+:.*## /{printf "  %-18s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
	@echo "示例："
	@echo "  make docker-up-all"
	@echo "  make docker-down-all"
	@echo "  make docker-up   NAME=redis"
	@echo "  make docker-down DIR=./services/redis"
	@echo "  make docker-logs NAME=redis ARGS=\"-f --tail=100\""
	@echo "  make docker-net  NET=shared_network"


# ==== 固定根路径与 .env ====
REPO_ROOT := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
ROOT_ENV  := $(REPO_ROOT)/.env

COMPOSE_BIN        ?= docker compose
COMPOSE_FILE_NAME  ?= docker-compose.yaml

# 仅扫描 images/ 下的 compose（也可改成 $(REPO_ROOT) 整树扫描）
COMPOSE_DIRS := $(shell find $(REPO_ROOT)/images -type f -name $(COMPOSE_FILE_NAME) -exec dirname {} \; | sort -u)

.PHONY: docker-list docker-up-all docker-down-all docker-up docker-down docker-config

docker-list: ## 列出所有 compose 目录
	@[ -n "$(COMPOSE_DIRS)" ] || { echo "[warn] 未找到 $(COMPOSE_FILE_NAME)"; exit 0; }
	@printf "%s\n" $(COMPOSE_DIRS)

docker-up-all:
	@[ -n "$(COMPOSE_DIRS)" ] || { echo "[warn] 未找到 $(COMPOSE_FILE_NAME)"; exit 0; }
	@for d in $(COMPOSE_DIRS); do
		echo ">> UP $$d"
		$(COMPOSE_BIN) \
		  --project-directory "$(REPO_ROOT)" \
		  --env-file "$(ROOT_ENV)" \
		  -f "$$d/$(COMPOSE_FILE_NAME)" up -d
	done
	@echo "[ok] 全部已启动"

docker-down-all:
	@[ -n "$(COMPOSE_DIRS)" ] || { echo "[warn] 未找到 $(COMPOSE_FILE_NAME)"; exit 0; }
	@for d in $(COMPOSE_DIRS); do
		echo ">> DOWN $$d"
		$(COMPOSE_BIN) \
		  --project-directory "$(REPO_ROOT)" \
		  --env-file "$(ROOT_ENV)" \
		  -f "$$d/$(COMPOSE_FILE_NAME)" down
	done
	@echo "[ok] 全部已关闭"

docker-up:
	@set -Eeuo pipefail
	# 1) 选择目录
	if [[ -n "$(DIR)" ]]; then sel="$(DIR)"; \
	elif [[ -n "$(NAME)" ]]; then \
		sel=$$(for d in $(COMPOSE_DIRS); do [[ "$$(basename "$$d")" == "$(NAME)" ]] && echo "$$d"; done); \
	else
		echo "[err] 需要指定 DIR=... 或 NAME=..."; exit 2; \
	fi
	[[ -n "$$sel" ]] || { echo "[err] 未匹配到服务目录"; exit 2; }

	# 2) 逐目录 up
	for d in $$sel; do
		f="$$d/$(COMPOSE_FILE_NAME)"
		if [[ ! -f "$$f" ]]; then
			echo "[skip] $$f 不存在"; continue
		fi
		echo ">> UP $$d"
		$(COMPOSE_BIN) \
		  --project-directory "$(REPO_ROOT)" \
		  --env-file "$(ROOT_ENV)" \
		  -f "$$f" up -d
	done

docker-down:
	@set -Eeuo pipefail
	# 1) 选择目录
	if [[ -n "$(DIR)" ]]; then sel="$(DIR)"; \
	elif [[ -n "$(NAME)" ]]; then \
		sel=$$(for d in $(COMPOSE_DIRS); do [[ "$$(basename "$$d")" == "$(NAME)" ]] && echo "$$d"; done); \
	else
		echo "[err] 需要指定 DIR=... 或 NAME=..."; exit 2; \
	fi
	[[ -n "$$sel" ]] || { echo "[err] 未匹配到服务目录"; exit 2; }

	# 2) 逐目录 down
	for d in $$sel; do
		f="$$d/$(COMPOSE_FILE_NAME)"
		if [[ ! -f "$$f" ]]; then
			echo "[skip] $$f 不存在"; continue
		fi
		echo ">> DOWN $$d"
		$(COMPOSE_BIN) \
		  --project-directory "$(REPO_ROOT)" \
		  --env-file "$(ROOT_ENV)" \
		  -f "$$f" down --remove-orphans
	done


# 渲染检查（看变量展开是否来自根 .env）：
# 用法：make docker-config DIR=images/redis
docker-config:
	@[ -n "$(DIR)" ] || { echo "[err] 需指定 DIR=..."; exit 2; }
	@$(COMPOSE_BIN) \
	  --project-directory "$(REPO_ROOT)" \
	  --env-file "$(ROOT_ENV)" \
	  -f "$(DIR)/$(COMPOSE_FILE_NAME)" config

docker-ps: ## 汇总查看状态（逐目录 docker compose ps）
	@set -e
	for d in $(COMPOSE_DIRS); do
		echo "== $$d"; (cd "$$d" && $(COMPOSE_BIN) ps); echo
	done

docker-logs: ## 查看日志：NAME=目录名 或 DIR=路径（ARGS 透传给 logs）
	@set -e
	if [ -n "$(DIR)" ]; then sel="$(DIR)"; \
	elif [ -n "$(NAME)" ]; then \
		sel=$$(for d in $(COMPOSE_DIRS); do [ "$$(basename "$$d")" = "$(NAME)" ] && echo "$$d"; done); \
	else sel="$(COMPOSE_DIRS)"; fi
	for d in $$sel; do
		echo "== $$d"; (cd "$$d" && $(COMPOSE_BIN) logs $(ARGS)); echo
	done

docker-pull: ## 全量拉取镜像
	@set -e
	for d in $(COMPOSE_DIRS); do
		echo ">> PULL $$d"
		(cd "$$d" && $(COMPOSE_BIN) pull)
	done
	@echo "[ok] 全部镜像已拉取"

docker-net: ## 创建 Docker 网络：NET=名称（默认 shared_network）
	@set -e
	NET_NAME="$(NET)"; [ -n "$$NET_NAME" ] || NET_NAME="shared_network"
	if docker network inspect "$$NET_NAME" >/dev/null 2>&1; then
		echo "[ok] 网络已存在：$$NET_NAME"
	else
		docker network create "$$NET_NAME"
		echo "[ok] 已创建网络：$$NET_NAME"
	fi

# Makefile — 通过变量设置 Docker registry mirrors，并自动备份原配置

# ---- 可调参数 ---------------------------------------------------------------
# 用逗号或空格分隔多个镜像站
MIRRORS ?= https://mirror.gcr.io
# dockerd 的配置路径（必要时可改）
DOCKER_DAEMON_JSON ?= /etc/docker/daemon.json

# 提权
SUDO := $(if $(filter 0,$(shell id -u)),,sudo)

.PHONY: docker-mirror-help docker-mirror-apply docker-mirror-show

docker-mirror-help: ## 显示帮助
	@echo "用法："
	@echo "  make docker-mirror-apply MIRRORS='https://mirror.gcr.io, https://hub-mirror.c.163.com'"
	@echo "  make docker-mirror-show"
	@echo
	@echo "变量："
	@echo "  MIRRORS             逗号或空格分隔的镜像站列表（默认：$(MIRRORS))"
	@echo "  DOCKER_DAEMON_JSON  dockerd 配置路径（默认：$(DOCKER_DAEMON_JSON))"

docker-mirror-show: ## 查看当前 daemon.json 与 docker info 中的镜像站
	@if [ -f "$(DOCKER_DAEMON_JSON)" ]; then
		echo "== $(DOCKER_DAEMON_JSON) =="; cat "$(DOCKER_DAEMON_JSON)"; echo
	else
		echo "[info] 未找到 $(DOCKER_DAEMON_JSON)"
	fi
	echo "== docker info (Registry Mirrors) ==";
	docker info 2>/dev/null | sed -n '/Registry Mirrors/,$$p'

docker-mirror-apply: ## 备份原配置 -> 写入新镜像站 -> 重启 dockerd
	@set -euo pipefail

	# 1) 友好提示（Docker Desktop 检测）
	if docker info 2>/dev/null | grep -q "Docker Desktop"; then
		echo "[warn] 检测到 Docker Desktop；镜像站通常应在 Desktop 偏好设置中配置。"
		echo "[warn] 本命令仅在独立 dockerd 生效（例如原生 Linux/WSL 单独安装的 docker-ce）。"
	fi

	# 2) 处理 MIRRORS 为 JSON 数组字符串：["m1","m2",...]
	mirrors_list="$(MIRRORS)"
	mirrors_list="$${mirrors_list//,/ }"
	json_elems=""
	for x in $$mirrors_list; do
		[ -n "$$x" ] || continue
		json_elems="$$json_elems\"$$x\","
	done
	mirrors_json="[$${json_elems%,}]"
	if [ "$$mirrors_json" = "[]" ]; then
		echo "[err] MIRRORS 为空，放弃修改"; exit 2
	fi
	echo "[info] 将设置 registry-mirrors = $$mirrors_json"

	# 3) 准备目录并备份（按日期命名）
	$(SUDO) mkdir -p "$(dir $(DOCKER_DAEMON_JSON))"
	if [ -f "$(DOCKER_DAEMON_JSON)" ]; then
		ts="$$(date +%F_%H%M%S)"
		$(SUDO) cp -a "$(DOCKER_DAEMON_JSON)" "$(DOCKER_DAEMON_JSON).$$ts.bak"
		echo "[ok] 已备份为 $(DOCKER_DAEMON_JSON).$$ts.bak"
	else
		echo "[info] 原配置不存在，无需备份"
	fi

	# 4) 生成新配置（优先用 jq 合并原配置；无 jq 则写最小配置）
	tmp="$$(mktemp)"
	if command -v jq >/dev/null 2>&1; then
		orig="$$( [ -f "$(DOCKER_DAEMON_JSON)" ] && cat "$(DOCKER_DAEMON_JSON)" || echo '{}' )"
		printf "%s" "$$orig" \
		| jq --argjson mirrors "$$mirrors_json" \
		   '.["registry-mirrors"] = $mirrors
		    | .features = (.features // {})
		    | .features.buildkit = true' > "$$tmp"
		echo "[ok] 已使用 jq 合并写入（保留原有其余字段）"
	else
		cat > "$$tmp" <<-JSON
		{
		  "registry-mirrors": $$mirrors_json,
		  "features": { "buildkit": true }
		}
		JSON
		echo "[ok] 未检测到 jq，已写入最小可用配置（其它字段以备份为准）"
	fi

	# 5) 下发新文件并设置权限
	$(SUDO) install -m 0644 -o root -g root "$$tmp" "$(DOCKER_DAEMON_JSON)"
	rm -f "$$tmp"
	echo "[ok] 已更新 $(DOCKER_DAEMON_JSON)"

	# 6) 重启 dockerd（多方案兜底）
	if command -v systemctl >/dev/null 2>&1; then
		$(SUDO) systemctl restart docker && echo "[ok] systemd 已重启 docker"
	elif command -v service >/dev/null 2>&1; then
		$(SUDO) service docker restart && echo "[ok] SysV 已重启 docker"
	else
		echo "[warn] 未找到 systemctl/service，可能是 WSL 或非 systemd 环境，请手动重启 dockerd（或执行 wsl --shutdown）"
	fi

	# 7) 打印生效确认
	echo "== 生效确认（docker info）=="
	docker info 2>/dev/null | sed -n '/Registry Mirrors/,$$p'


help:
	@echo "用法：make <target> [VERBOSE=1] [JOBS=N] [MODE=mem|race|miri|all]"
	@echo ""
	@echo "—— 聚合 ——"
	@echo "  setup            安装/配置 Python、Go、C++、Rust"
	@echo "  build            编译全部子项目"
	@echo "  test             运行全部单测"
	@echo "  install          安装全部产物（可能使用sudo）"
	@echo "  clean            清理全部构建/缓存"
	@echo "  coverage         汇总覆盖率（Python/Go/C++/Rust）"
	@echo "  fmt              代码格式化（Go/C++/Rust；Python用 fmt-python）"
	@echo "  bench            运行基准（Python/Go/C++/Rust）"
	@echo ""
	@echo "—— 诊断 ——"
	@echo "  vet              静态检查（python ruff / cpp clang-tidy / go vet / rust clippy）"
	@echo "  guard            内存/并发防护组合（C++/Go/Rust/Python）"
	@echo "  test-v           等价 test（VERBOSE=1）"
	@echo "  bench-v          等价 bench（VERBOSE=1）"
	@echo ""
	@echo "—— Python ——"
	@echo "  setup-python     安装依赖并构建 wheel（不安装本体）"
	@echo "  build-python     （占位；随 setup-python 一起使用）"
	@echo "  test-python      pytest（排除 bench/benchmark）"
	@echo "  install-python   pip 安装 python/（支持镜像与 BREAK_FLAG）"
	@echo "  clean-python     清理构建/缓存/临时文件"
	@echo "  coverage-python  coverage run + report"
	@echo "  fmt-python       autopep8 格式化"
	@echo "  bench-python     pytest-benchmark 或名称筛选"
	@echo "  guard-python     memray / pytest-leaks；无则回退 pytest"
	@echo "  test-python-v    test-python + 详细输出"
	@echo "  bench-python-v   bench-python + 详细输出"
	@echo ""
	@echo "—— Go ——"
	@echo "  setup-go         mod tidy / download（尊重 GOWORK/GOTOOLCHAIN 等）"
	@echo "  build-go         编译 ./...（库仅编译检查）"
	@echo "  test-go          go test"
	@echo "  coverage-go      覆盖率：coverage.out / coverage.html"
	@echo "  install-go       go install ./...（仅 main 包）"
	@echo "  clean-go         清理 bin/ 覆盖率与测试缓存"
	@echo "  fmt-go           go fmt ./..."
	@echo "  bench-go         go test -bench"
	@echo "  guard-go         go test -race"
	@echo "  test-go-v        test-go + 详细输出"
	@echo "  bench-go-v       bench-go + 详细输出"
	@echo ""
	@echo "—— C++ ——"
	@echo "  git-submodules   拉取并自动修复子模块"
	@echo "  setup-cpp        先构建 cpp/lib/* 子模块，再配置顶层"
	@echo "  build-cpp        顶层并行构建"
	@echo "  test-cpp         CTest（排除 bench）"
	@echo "  coverage-cpp     覆盖率：gcovr 优先，回退 lcov"
	@echo "  install-cpp      安装到 PREFIX"
	@echo "  clean-cpp        清理顶层与子模块 build/"
	@echo "  fmt-cpp          clang-format（排除 lib/ 与 thirdparty/）"
	@echo "  bench-cpp        运行 label=bench 的基准"
	@echo "  vet-cpp          clang-tidy（需 compile_commands.json）"
	@echo "  guard-cpp        ASAN/UBSAN/TSAN/Massif"
	@echo "  test-cpp-v       test-cpp + 详细输出"
	@echo "  bench-cpp-v      bench-cpp + 详细输出"
	@echo ""
	@echo "—— Rust ——"
	@echo "  setup-rust       cargo fetch（如有 rust-toolchain.toml 则设置 stable）"
	@echo "  build-rust       cargo build --release"
	@echo "  test-rust        cargo test"
	@echo "  coverage-rust    cargo-llvm-cov（HTML/LCOV + 汇总，可设 RUST_COVER_FAIL_UNDER）"
	@echo "  install-rust     安装库与文档到 PREFIX"
	@echo "  clean-rust       清理 target/ 与分析产物"
	@echo "  fmt-rust         cargo fmt"
	@echo "  clippy-rust      cargo clippy -D warnings"
	@echo "  bench-rust       cargo bench"
	@echo "  example-rust     cargo run --example contains"
	@echo "  guard-rust       ASan/Tsan/Miri（MODE=mem|race|miri|all）"
	@echo "  test-rust-v      test-rust + 详细输出"
	@echo "  bench-rust-v     bench-rust + 详细输出"
	@echo ""
	@echo "—— 变量提示 ——"
	@echo "  常用变量: VERBOSE=1  JOBS=N  PREFIX=/usr/local  CPP_BUILD_TYPE=Release"
	@echo "           GOWORK=off  GOTOOLCHAIN=local  GOPROXY=<url>  GOBIN=<dir>"
	@echo "           RUST_COVER_FAIL_UNDER=<N>  MODE=mem|race|miri|all"
	@echo ""
	@echo "示例："
	@echo "  make build-cpp CPP_BUILD_TYPE=Release JOBS=8"
	@echo "  make coverage-go GOWORK=off GOPROXY=https://goproxy.cn,direct"
	@echo "  make guard-rust MODE=race"
	@echo "  make test-v"
