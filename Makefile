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
		if ! command -v pytest >/dev/null 2>&1; then \
			echo "[warn] 未检测到 pytest；请先执行: make setup-python"; exit 1; \
		fi; \
		pytest python || { echo "[hint] 测试失败。请先执行: make setup-python"; exit 1; }'
endef

define _install_python
	$(SHELL) -lc 'set -e; \
		echo "[info] $(PIP3) install python/ --upgrade $(BREAK_FLAG)"; \
		$(SUDO) $(PIP3) install python/ --upgrade $(BREAK_FLAG); \
		echo "[ok] 安装完成。"' 
endef

define _clean_python
	$(SHELL) -lc 'set -e; \
		echo "[info] 清理 Python 缓存与构建产物…"; \
		rm -rf python/build python/dist python/*.egg-info .pytest_cache .mypy_cache .coverage htmlcov; \
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
		$(CTEST) --output-on-failure -j $(JOBS)'
endef

define _coverage_cpp
	$(SHELL) -lc 'set -e; \
		cd cpp/build; \
		ctest --output-on-failure || true; \
		cmake --build . --target coverage -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 8); \
		if command -v gcovr >/dev/null 2>&1; then \
			echo "[info] 使用 gcovr 打印终端表格："; \
			gcovr -r .. --object-directory . \
			      --exclude "(^|.*/)(test|external|gtest)/" \
			      --txt --print-summary || true; \
		else \
			echo "[info] 未安装 gcovr，已用 lcov --list 在上一步输出覆盖率"; \
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

# =================== 直达入口（Python） ===================
.PHONY: setup-python build-python test-python install-python clean-python coverage-python
setup-python:   ; $(call _setup_python)
build-python:   | setup-python
test-python:    ; $(call _test_python)
install-python: ; $(call _install_python)
clean-python:   ; $(call _clean_python)
coverage-python:; $(call _coverage_python)

# =================== 直达入口（C++） ===================
.PHONY: git-submodules setup-cpp build-cpp test-cpp install-cpp clean-cpp coverage-cpp
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

# =================== 聚合入口（Python + C++） ===================
.PHONY: setup build test install clean coverage help
setup:    setup-python setup-cpp
build:    build-python build-cpp
test:     test-python  test-cpp
install:  install-python install-cpp
clean:    clean-python  clean-cpp
coverage: coverage-python coverage-cpp

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
