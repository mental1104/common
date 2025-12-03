SHELL := /bin/bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c

# === 自动加载 .env 到 Make 环境（全局生效） =================
REPO_ROOT := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
ENV_SRC    ?= $(REPO_ROOT)/.env
ENV_STAMP  ?= $(REPO_ROOT)/.env.active
ENV_MK     := $(abspath $(ENV_SRC)).mk
ENV_HAVE   := $(wildcard $(ENV_SRC))
ENV_KEYS   := $(if $(ENV_HAVE),$(shell awk 'BEGIN{FS="="} /^[[:space:]]*(#|$$)/{next} {k=$$1; sub(/^[[:space:]]*export[[:space:]]+/,"",k); sub(/[[:space:]]+/,"",k); print k}' $(ENV_SRC)))

# 仅当 .env 与激活标记同时存在时才导入
ifeq ($(and $(ENV_HAVE),$(wildcard $(ENV_STAMP))),)
  ENV_ACTIVE := 0
else
  ENV_ACTIVE := 1
endif

$(ENV_MK): $(ENV_SRC)
	@set -e
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

ifeq ($(ENV_ACTIVE),1)
include $(ENV_MK)
endif

# 若未激活 env，则清空关键环境变量，避免沿用旧值导致连接外部服务
ifeq ($(ENV_ACTIVE),0)
  $(foreach k,$(ENV_KEYS),$(eval override $(k)=))
endif

.PHONY: env-guard env-print env-expose env-clean
env-guard:
	@missing=""; \
	for k in $(ENV_REQ); do eval 'v=$${'$$k':-}'; [ -n "$$v" ] || missing="$$missing $$k"; done; \
	if [ -n "$$missing" ]; then echo "[err] 缺少必需环境变量:$$missing （来源 $(ENV_SRC)）"; exit 2; fi
env-print:
	@env | grep -E '^(PG|POSTGRES|DATABASE_URL|REDIS|PULSAR)=' | sort || true
env-expose:
	@[ -f "$(ENV_MK)" ] && sed -E 's/^export[[:space:]]+([^=[:space:]]+)[[:space:]]*=[[:space:]]*(.*)$/export \1=\2/' "$(ENV_MK)" || true
env-clean:
	if [ -f "$(ENV_STAMP)" ]; then rm -f "$(ENV_STAMP)"; fi; \
	if [ -f "$(ENV_MK)" ]; then rm -f "$(ENV_MK)"; fi; \
	echo "[ok] 已移除导入文件与激活标记：$(ENV_MK) $(ENV_STAMP)"
# ============================================================

.DEFAULT_GOAL := setup

# ---------- 平台/通用开关 ----------
UID := $(shell id -u)
SUDO := $(if $(filter 0,$(UID)),,sudo)
SUDO_MSG := $(if $(filter 0,$(UID)),,@echo "[info] 当前非root，将使用sudo执行；可能会提示输入管理员密码。")
UNAME_S := $(shell uname -s)
IS_UBUNTU := $(shell sh -lc 'u=$$(uname -s); if [ "$$u" = Linux ] && [ -r /etc/os-release ]; then . /etc/os-release; [ "$$ID" = ubuntu ] && echo 1; fi')
IS_DARWIN := $(if $(filter Darwin,$(UNAME_S)),1,)
JOBS ?= $(shell sh -lc 'command -v nproc >/dev/null 2>&1 && nproc || sysctl -n hw.ncpu 2>/dev/null || echo 4')

VERBOSE ?= 0
TEST_VERBOSE ?= $(VERBOSE)
CTEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-V,)
PYTEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-vv,-q)
GO_TEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-v,)
CARGO_TEST_V := $(if $(filter 1,$(TEST_VERBOSE)),-v,)

BENCH_ARTIFACT_ROOT ?= $(REPO_ROOT)/artifacts/bench
BENCH_GALLERY := $(BENCH_ARTIFACT_ROOT)/index.html

SKIP_DOCKER_ON_DARWIN ?= 1
DOCKER_DISABLED := $(if $(and $(SKIP_DOCKER_ON_DARWIN),$(IS_DARWIN)),1,)

# ---------- Python ----------
PYTHON ?= python3
PIP3 ?= pip3
PY_VENV ?= $(REPO_ROOT)/python/.venv
PY_VENV_PYTHON := $(PY_VENV)/bin/python
PY_VENV_PIP := $(PY_VENV)/bin/pip
PIP_INDEX_URL := https://pypi.tuna.tsinghua.edu.cn/simple
PIP_EXTRA_INDEX_URL :=
PIP_TRUSTED_HOST :=
USE_PIP_MIRROR := 1
PIP_MIRROR_OPTS :=
ifeq ($(USE_PIP_MIRROR),1)
  ifneq ($(strip $(PIP_INDEX_URL)),)
    PIP_MIRROR_OPTS += --index-url $(PIP_INDEX_URL)
  endif
  ifneq ($(strip $(PIP_EXTRA_INDEX_URL)),)
    PIP_MIRROR_OPTS += --extra-index_url $(PIP_EXTRA_INDEX_URL)
  endif
  ifneq ($(strip $(PIP_TRUSTED_HOST)),)
    PIP_MIRROR_OPTS += --trusted-host $(PIP_TRUSTED_HOST)
  endif
endif
PYTEST_BENCH_K ?= bench or benchmark
PY_BENCHMARK_OPTS ?= --benchmark-name=short --benchmark-sort=name
PY_BENCHMARK_CONCURRENCY_OPTS ?= --benchmark-max-time=0.25 --benchmark-min-rounds=3
PY_BENCH_ARTIFACT_DIR := $(BENCH_ARTIFACT_ROOT)/python

# ---------- Go ----------
GO ?= go
GO_DIR := golang
GO_COVER_OUT := $(GO_DIR)/coverage.out
GO_COVER_HTML := $(GO_DIR)/coverage.html
GOPROXY ?=
GOPRIVATE ?=
GOTOOLCHAIN ?= local
GOWORK ?= off
GOBIN ?=

# ---------- C++ ----------
CMAKE ?= cmake
CTEST ?= ctest
CPP_SRC_DIR := cpp
CPP_BUILD_DIR := $(CPP_SRC_DIR)/build
CPP_BUILD_TYPE ?= Debug
CPP_TEST ?=
CPP_TEST_FILES ?=
CPP_TEST_DIRS ?=
CPP_BENCH_FILES ?=
CPP_LOG_LEVEL ?=
CPP_BENCH_ARTIFACT_DIR := $(BENCH_ARTIFACT_ROOT)/cpp
EXPORT_CPP_BUILD_DIR ?= $(REPO_ROOT)/export/cpp/build
PREFIX ?= /usr/local
VET_DIR ?= cpp/test

# ---------- Rust ----------
RUST_DIR := rust/mental1104
RUST_COVER_FAIL_UNDER ?=
MODE ?= all
AUTO_SETUP_RUST_NIGHTLY ?= 0

# =================== 函数/宏（Defines） ===================

define __env_apply_runtime
# 若存在激活标记则加载 .env；否则按 .env 中的键逐个 unset，避免残留环境变量
if [[ -f "$(ENV_STAMP)" && -f "$(ENV_SRC)" ]]; then \
  set -a; . "$(ENV_SRC)"; set +a; \
else \
  if [[ -f "$(ENV_SRC)" ]]; then \
    while IFS= read -r line; do \
      case "$$line" in ''|\#*) continue ;; \
      esac; \
      k="$$line"; k=$${k#export }; k=$${k%%=*}; k=$${k%%[[:space:]]*}; \
      [[ -n "$$k" ]] && unset "$$k"; \
    done < "$(ENV_SRC)"; \
  fi; \
fi
endef

define _setup_python
	$(SHELL) -lc 'set -e; \
		$(call __py_script_prelude)
	set -e
	# 平台信息
	echo "[info] 平台: $(UNAME_S)$(if $(IS_UBUNTU), (ubuntu),)"
	$(call __py_bootstrap_venv)          # 创建或复用 venv 并导出 PATH
	$(call __py_upgrade_build_tools)     # 升级 pip/setuptools/wheel
	$(call __py_install_export_layer)    # 可选安装 export/python 可编辑包
	$(call __py_install_requirements)    # 安装 python/requirements.txt 依赖
	$(call __py_generate_init)           # 可选生成 __init__
	$(call __py_fix_future_annotations)  # 为联合类型注入 __future__ import annotations
	$(call __py_build_wheel)             # 构建 wheel（不安装本体）
	$(call __py_script_finalize)'
endef

define _test_python
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		$(call __py_guard_venv)              # 确认 venv 存在并导出 PATH
		$(call __py_test_env_flags)          # 设定日志级别并清空代理
		$(call __py_test_plugins)            # 配置 pytest 插件并确保 pytest-benchmark 就绪
		$(call __py_test_check_pytest)       # 校验 pytest 可用
		$(call __py_test_export_lib)         # 如果存在则导出 C++ 桥接库及 PYTHONPATH
		PYTEST_ARGS="$(PYTEST_V)"; \
		$(call __py_test_run_pytest)'        # 进入 python 目录按过滤条件运行 pytest
endef

define _install_python
	$(SHELL) -lc 'set -e; \
		$(call __py_install_system)'      # 安装到系统 Python（备份/改写 requirements 中的相对 file:// 再恢复）
endef

define _uninstall_python
	$(SHELL) -lc 'set -e; \
		$(call __py_uninstall_system)'    # 从系统 Python 卸载 mental1104 及 export 层
endef

define _clean_python
	$(SHELL) -lc 'set -e; \
		$(call __py_clean_artifacts)    # 删除构建产物与缓存文件
		$(call __py_clean_caches)       # 清理 __pycache__ / *.pyc / .pytest_cache
		$(call __py_clean_venv)         # 如存在则移除 python/.venv
		echo "[ok] clean 完成。"'
endef

define _coverage_python
	$(SHELL) -lc 'set -e; \
		$(call __py_guard_venv)         # 确认 venv 存在并导出 PATH
		$(call __py_test_env_flags)     # 同测试运行的环境变量设置
		$(call __py_test_plugins)       # 预装并声明所需 pytest 插件
		$(call __py_test_check_pytest)  # 校验 pytest 可用
		$(call __py_test_export_lib)    # 导出 C++ 桥接库及 PYTHONPATH
		$(call __py_coverage_run)'      # 使用 coverage 包裹 pytest
endef

define _fmt_python
	$(SHELL) -lc 'set -e; \
		$(call __py_guard_venv)          # 确认 venv 存在并导出 PATH
		$(call __py_fmt_ensure_tool)     # 如缺失则安装 autopep8
		$(call __py_fmt_run)'            # 进入 python 目录执行格式化
endef

define _bench_python
	$(SHELL) -lc 'set -e; \
		$(call __py_guard_venv)             # 确认 venv 存在并导出 PATH
		$(call __py_bench_env)              # 环境开关（日志、代理、PYTHONPATH）
		$(call __py_bench_plugins)          # 仅加载/安装 pytest-benchmark 插件（禁用自动插件）
		$(call __py_bench_check_pytest)     # 校验 pytest/pytest-benchmark 可用
		$(call __py_bench_export_lib)       # 导出 C++ 桥接库及 PYTHONPATH
		$(call __py_bench_select_and_run)'  # 选择基准文件并执行/出图
endef

define _vet_python
	$(SHELL) -lc 'set -e; \
		$(call __py_guard_venv)          # 确认 venv 存在并导出 PATH
		$(call __py_vet_ruff)'           # 使用 ruff 进行静态检查'
endef

define _guard_python
	$(SHELL) -lc 'set -e; \
		$(call __py_guard_venv)          # 确认 venv 存在并导出 PATH
		$(call __py_vet_ruff)            # 静态检查（ruff）
		$(call __py_guard_pytest)'       # 运行一次 pytest（过滤 bench）
endef

# =================== 直达入口（Python） ===================
.PHONY: setup-python build-python test-python install-python uninstall-python clean-python coverage-python fmt-python bench-python
setup-python:   ; $(call _setup_python)
build-python:   ; $(call __py_guard_venv) ; $(call __py_build_wheel)   # 依赖已就绪的 venv，单独构建 wheel
test-python:    | build-export-cpp ; $(call _test_python)              # 需导出 C++ 桥接库，pytest 内部自检 venv
install-python: ; $(call _install_python)                             # 系统安装，不强制前置 setup
uninstall-python: ; $(call _uninstall_python)                         # 系统卸载 mental1104 包
clean-python:   ; $(call _clean_python)
coverage-python:; $(call _coverage_python)
fmt-python:     ; $(call _fmt_python)
bench-python:   | build-export-cpp
	$(call __py_guard_venv)
	$(call _bench_python)
	@$(MAKE) --no-print-directory bench-report

define _configure_cpp
	$(SHELL) -lc 'set -e; \
		$(call __cpp_configure_env)   # 处理 pybind11 路径、创建 build 目录
		$(call __cpp_configure_run)'  # 调用 cmake 配置
endef

define _build_cpp
	$(SHELL) -lc 'set -e; \
		$(call __cpp_build_run)'      # 构建顶层 C++ 工程
endef

define _test_cpp
	$(SHELL) -lc 'set -e; \
		cache="$(CPP_BUILD_DIR)/CMakeCache.txt"; \
		actual=""; [[ -f "$$cache" ]] && actual=$$(sed -n "s/^CMAKE_BUILD_TYPE:STRING=//p" "$$cache"); \
		if [[ "$$actual" != "Debug" ]]; then echo "[error] $(CPP_BUILD_DIR) 不是 Debug 构建(实际: $$actual)。请先执行: make build-cpp-debug"; exit 1; fi; \
		$(call __cpp_test_env)        # 清空代理、设置日志级别
		$(call __cpp_test_patterns)   # 解析 ctest/gtest 过滤条件
		$(call __cpp_test_run)'       # 运行 ctest（排除 bench）
endef

define _coverage_cpp
	$(SHELL) -lc 'set -e; \
		cache="$(CPP_BUILD_DIR)/CMakeCache.txt"; \
		actual=""; [[ -f "$$cache" ]] && actual=$$(sed -n "s/^CMAKE_BUILD_TYPE:STRING=//p" "$$cache"); \
		if [[ "$$actual" != "Debug" ]]; then echo "[error] $(CPP_BUILD_DIR) 不是 Debug 构建(实际: $$actual)。请先执行: make build-cpp-debug"; exit 1; fi; \
		$(call __cpp_coverage_run)'  # 运行 ctest + gcovr/lcov 汇总覆盖率
endef

define _install_cpp
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		$(call __cpp_install_run)'    # 安装到前缀
endef

define _uninstall_cpp
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		$(call __cpp_uninstall_run)'  # 依据 manifest 卸载
endef

define _clean_cpp
	$(SHELL) -lc 'set -e; \
		$(call __cpp_clean_build)'    # 删除顶层 C++ build 目录
endef

define _fmt_cpp
	$(SHELL) -lc 'set -e; \
		$(call __cpp_fmt_check_tool)  # 确认 clang-format 存在
		$(call __cpp_fmt_run)'        # 运行 clang-format（排除 lib/thirdparty）
endef

define _bench_cpp
	$(SHELL) -lc 'set -e; \
		cache="$(CPP_BUILD_DIR)/CMakeCache.txt"; \
		actual=""; [[ -f "$$cache" ]] && actual=$$(sed -n "s/^CMAKE_BUILD_TYPE:STRING=//p" "$$cache"); \
		if [[ "$$actual" != "Release" ]]; then echo "[error] $(CPP_BUILD_DIR) 不是 Release 构建(实际: $$actual)。请先执行: make build-cpp-release"; exit 1; fi; \
		$(call __cpp_bench_env)        # 日志级别/构建目录检查
		$(call __cpp_bench_collect)    # 收集 bench_* 可执行文件并过滤
		$(call __cpp_bench_run)'       # 运行基准并生成图表
endef

define _bench_report
	$(SHELL) -lc 'set -e; \
		$(PYTHON) python/tools/assemble_bench_gallery.py \
			--root "$(BENCH_ARTIFACT_ROOT)" \
			--output "$(BENCH_GALLERY)"; \
		echo "[bench] 图库：$(BENCH_GALLERY)"'
endef

# =================== 直达入口（C++） ===================
.PHONY: git-submodules setup-cpp build-cpp build-cpp-release build-cpp-debug build-cpp-core test-cpp install-cpp uninstall-cpp clean-cpp coverage-cpp fmt-cpp bench-cpp
git-submodules:        ; $(call _git_fetch_submodules)

setup-cpp:
	$(MAKE) git-submodules
	$(call _build_cpp_submodules)
	$(call _configure_cpp)

build-cpp-core: | setup-cpp ; $(call _build_cpp)

build-cpp-release:
	$(MAKE) --no-print-directory build-cpp-core CPP_BUILD_TYPE=Release

build-cpp-debug:
	$(MAKE) --no-print-directory build-cpp-core CPP_BUILD_TYPE=Debug

build-cpp: build-cpp-release

test-cpp:       ; $(call _test_cpp)
install-cpp:    ; $(call _install_cpp)
uninstall-cpp:  ; $(call _uninstall_cpp)

clean-cpp:
	$(call _clean_cpp_submodules)
	$(call _clean_cpp)

coverage-cpp:   | test-cpp  ; $(call _coverage_cpp)
fmt-cpp:        ; $(call _fmt_cpp)
bench-cpp:
	$(call _bench_cpp)
	@$(MAKE) --no-print-directory bench-report

# Export layer (C++ JSON -> Python)
.PHONY: build-export-cpp clean-export-cpp
build-export-cpp:
	$(SHELL) -lc 'set -e; \
		$(call __export_cpp_use_venv)     # 如有 venv 则导出 PATH 以查找 pybind11 cmake 配置
		$(call __export_cpp_pybind_dir)   # 发现 pybind11_DIR 供 CMake 使用
		$(call __export_cpp_configure)    # 运行 cmake 配置 export/cpp
		$(call __export_cpp_build)'       # 构建 export/cpp

clean-export-cpp:
	$(SHELL) -lc 'rm -rf $(EXPORT_CPP_BUILD_DIR); echo "[ok] cleaned export/cpp build"'

# =================== Go 宏 ===================
define _setup_go
	$(SHELL) -lc 'set -e; \
		$(call __go_guard_tool)           # 检查 go 命令存在 \
		$(call __go_guard_mod)            # 确认 go.mod 存在 \
		$(call __go_mod_tidy_download)'   # tidy + download
endef

define _build_go
	$(SHELL) -lc 'set -e; \
		$(call __go_build_pkgs)'          # go build ./...
endef

define _build_go_bins
	$(SHELL) -lc 'set -e; \
		$(call __go_build_bins)'          # 构建 package main 可执行文件到 bin/
endef

define _test_go
	$(SHELL) -lc 'set -e; \
		$(call __go_env_clear_proxy)      # 清空代理提示
		$(call __go_test_run)'            # go test（支持 FILE/FILTER）
endef

define _coverage_go
	$(SHELL) -lc 'set -e; \
		$(call __go_env_clear_proxy)      # 清空代理提示
		$(call __go_coverage_run)'        # go test 覆盖率并生成报告
endef

define _fmt_go
	$(SHELL) -lc 'set -e; \
		$(call __go_fmt_run)'            # go fmt ./...
endef

define _bench_go
	$(SHELL) -lc 'set -e; \
		$(call __go_bench_run)'          # go test -bench...
endef

define _install_go
	$(SHELL) -lc 'set -e; \
		$(call __go_install_run)'        # go install ./...
endef

define _uninstall_go
	$(SHELL) -lc 'set -e; \
		$(call __go_uninstall_run)'      # 删除 GOBIN/GOPATH/bin 中的已安装可执行文件
endef

define _clean_go
	$(SHELL) -lc 'set -e; \
		$(call __go_clean_run)'          # 清理覆盖率、产物与缓存
endef

# =================== 直达入口（Go） ===================
.PHONY: setup-go build-go test-go coverage-go install-go uninstall-go clean-go fmt-go bench-go
setup-go:    ; $(call _setup_go)
build-go:    | setup-go ; $(call _build_go)
test-go:     ; $(call _test_go)
coverage-go: | test-go  ; $(call _coverage_go)
install-go:  ; $(call _install_go)
uninstall-go:; $(call _uninstall_go)
clean-go:    ; $(call _clean_go)
fmt-go:      ; $(call _fmt_go)
bench-go:
	$(call _bench_go)
	@$(MAKE) --no-print-directory bench-report

define _setup_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_guard_cargo) ; \
		$(call __rust_setup_toolchain) ; \
		$(call __rust_fetch_deps)'           # cargo fetch 拉取依赖
endef

define _build_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_build_run)'            # cargo build --release
endef

define _test_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_env_clear_proxy) ; \
		$(call __rust_test_run)'             # cargo test（支持 FILE/FILTER）
endef

define _bench_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_bench_run)'            # cargo bench（支持 FILE/FILTER）
endef

define _fmt_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_fmt_run)'              # cargo fmt --all
endef

define _clippy_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_clippy_run)'           # cargo clippy --all-targets --all-features
endef

define _example_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_example_run)'          # cargo run --example contains
endef

define _clean_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_clean_run)'            # 清理构建、覆盖率与 perf 产物
endef

define _install_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_guard_cargo) ; \
		$(call __rust_install_run)'          # cargo install 或构建库 crate
endef

define _uninstall_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_uninstall_run)'        # cargo uninstall mental1104（忽略未安装）
endef

define _coverage_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_coverage_run)'         # cargo llvm-cov 生成 HTML/LCOV 并打印汇总
endef

define _vet_rust
	$(SHELL) -lc 'set -e; \
		$(call __rust_vet_run)'              # 运行 clippy（忽略警告退出码）
endef

define _vet_go
	$(SHELL) -lc 'set -e; \
		$(call __go_vet_run)'           # go vet ./...
endef

define _vet_cpp
	$(SHELL) -lc 'set -e; \
		$(call __cpp_vet_run)'         # 运行 clang-tidy 检查
endef

define _guard_cpp
	$(SHELL) -lc 'set -e -o pipefail; \
		$(call __cpp_guard_run)'      # 按 MODE 运行 sanitizer / miri / massif 等诊断
endef

define _guard_go
	$(SHELL) -lc 'set -e -o pipefail; \
		$(call __go_guard_race)'        # go test -race -count=1 ./...
endef

define _guard_rust
	$(SHELL) -lc 'set -euo pipefail; \
		$(call __rust_guard_run)'             # 运行 mem/race/miri 诊断
endef

# =================== 直达入口（Rust） ===================
.PHONY: setup-rust build-rust test-rust bench-rust fmt-rust clippy-rust example-rust clean-rust install-rust uninstall-rust coverage-rust
setup-rust:   ; $(call _setup_rust)
build-rust:   | setup-rust ; $(call _build_rust)
test-rust:    ; $(call _test_rust)
bench-rust:
	$(call _bench_rust)
	@$(MAKE) --no-print-directory bench-report
fmt-rust:     ; $(call _fmt_rust)
clippy-rust:  ; $(call _clippy_rust)
example-rust: ; $(call _example_rust)
clean-rust:   ; $(call _clean_rust)
install-rust: ; $(call _install_rust)
uninstall-rust:; $(call _uninstall_rust)
coverage-rust:; $(call _coverage_rust)

.PHONY: _docker-up-all-if-needed _docker-down-all-if-needed
_docker-up-all-if-needed:
	@if [ -n "$(DOCKER_DISABLED)" ]; then \
		echo "[skip] macOS 检测到，跳过 setup-docker"; \
	else \
		$(MAKE) --no-print-directory setup-docker; \
	fi

_docker-down-all-if-needed:
	@if [ -n "$(DOCKER_DISABLED)" ]; then \
		echo "[skip] macOS 检测到，跳过 clean-docker"; \
	else \
		$(MAKE) --no-print-directory clean-docker; \
	fi

# =================== 入口/目标（Targets） ===================

.PHONY: vet vet-rust vet-go vet-python vet-cpp
vet:        vet-python vet-cpp vet-go vet-rust
vet-rust:   ; $(call _vet_rust)
vet-go:     ; $(call _vet_go)
vet-python: ; $(call _vet_python)
vet-cpp:    ; $(call _vet_cpp)

.PHONY: guard-rust guard-rust-mem guard-rust-race guard-rust-miri
guard-rust:        ; $(call _guard_rust)
guard-rust-mem:    ; $(MAKE) --no-print-directory guard-rust MODE=mem
guard-rust-race:   ; $(MAKE) --no-print-directory guard-rust MODE=race
guard-rust-miri:   ; $(MAKE) --no-print-directory guard-rust MODE=miri

.PHONY: guard guard-cpp guard-go guard-rust guard-python
guard-cpp:    ; $(call _guard_cpp)
guard-go:     ; $(call _guard_go)
guard-python: ; $(call _guard_python)
guard:        guard-cpp guard-go guard-rust guard-python

# ============ env 模板生成 ============
ENV_EXAMPLE  ?= $(REPO_ROOT)/.env.example

.PHONY: env-example
env-example:
	@if [ -f $(ENV_SRC) ]; then \
		awk '\
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
		' $(ENV_SRC) > $(ENV_EXAMPLE); \
		echo "[ok] 生成 $(ENV_EXAMPLE)"; \
	else \
		echo "[warn] $(ENV_SRC) 不存在，跳过 env-example"; \
	fi

# =================== Docker（仅扫描 images/） ===================
COMPOSE_BIN        ?= docker compose
COMPOSE_FILE_NAME  ?= docker-compose.yaml
COMPOSE_DIRS := $(shell find $(REPO_ROOT)/images -type f -name $(COMPOSE_FILE_NAME) -exec dirname {} \; | sort -u)
ENV_FILE_OPT := $(shell [ -f "$(ENV_SRC)" ] && printf -- '--env-file %s' "$(ENV_SRC)")

.PHONY: setup-docker clean-docker
setup-docker: $(ENV_MK)
	@touch "$(ENV_STAMP)"
	$(call __docker_up_all)

clean-docker:
	$(call __docker_down_all)
	@rm -f "$(ENV_STAMP)" "$(ENV_MK)" || true

# =================== 聚合入口（Python + Go + C++ + Rust + Docker） ===================

.PHONY: setup build test install clean coverage help fmt bench
setup:
	$(MAKE) env-example
	-$(MAKE) --no-print-directory _docker-up-all-if-needed
	$(MAKE) setup-python
	$(MAKE) setup-go
	$(MAKE) setup-cpp
	$(MAKE) setup-rust

build:    build-python build-go build-cpp build-rust
test:     test-python test-go test-cpp test-rust
install:  install-python install-go install-cpp install-rust

clean:
	-$(MAKE) --no-print-directory _docker-down-all-if-needed
	$(MAKE) clean-python
	$(MAKE) clean-go
	$(MAKE) clean-cpp
	$(MAKE) clean-rust
	$(MAKE) env-clean

coverage: coverage-python coverage-go coverage-cpp coverage-rust
fmt:      fmt-python fmt-go fmt-cpp fmt-rust
bench:    bench-python bench-go bench-cpp bench-rust
	@$(MAKE) --no-print-directory bench-report

.PHONY: uninstall
uninstall: uninstall-python uninstall-go uninstall-cpp uninstall-rust

.PHONY: test-v test-cpp-v test-python-v test-go-v test-rust-v
test-v:        ; $(MAKE) --no-print-directory test        VERBOSE=1
test-cpp-v:    ; $(MAKE) --no-print-directory test-cpp    VERBOSE=1
test-python-v: ; $(MAKE) --no-print-directory test-python VERBOSE=1
test-go-v:     ; $(MAKE) --no-print-directory test-go     VERBOSE=1
test-rust-v:   ; $(MAKE) --no-print-directory test-rust   VERBOSE=1

.PHONY: bench-v bench-cpp-v bench-python-v bench-go-v bench-rust-v
bench-v:        ; $(MAKE) --no-print-directory bench        VERBOSE=1
bench-cpp-v:    ; $(MAKE) --no-print-directory bench-cpp    VERBOSE=1 SEQ=1
bench-python-v: ; $(MAKE) --no-print-directory bench-python VERBOSE=1
bench-go-v:     ; $(MAKE) --no-print-directory bench-go     VERBOSE=1
bench-rust-v:   ; $(MAKE) --no-print-directory bench-rust   VERBOSE=1

.NOTPARALLEL: bench bench-python bench-go bench-cpp bench-rust bench-report

help:
	@echo "用法：make <target> [VERBOSE=1] [JOBS=N] [MODE=mem|race|miri|all]"
	@echo ""
	@echo "—— 聚合 ——"
	@echo "  setup            生成/导入 .env -> 尝试 setup-docker(images/) -> 各语言 setup（docker 出错不阻塞）"
	@echo "  build            编译全部子项目"
	@echo "  test             运行全部单测"
	@echo "  install          安装全部产物（可能使用sudo）"
	@echo "  clean            尝试 clean-docker(images/) -> 清理构建 -> 移除 env 导入文件（出错不阻塞）"
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
	@echo "—— Python / Go / C++ / Rust ——（略，保持与原文一致）"
	@echo ""
	@echo "提示：仅扫描 images/ 下的 docker-compose.yaml；docker 缺失或单服务失败均不阻塞。"
.PHONY: bench-report
bench-report: ; $(call _bench_report)

# =================== 私有函数（仅内部调用） ===================
define __py_bootstrap_venv
# 创建或重建虚拟环境
if [[ -e "$(PY_VENV_PYTHON)" && ! -x "$(PY_VENV_PYTHON)" ]]; then rm -rf "$(PY_VENV)"; fi
if [[ ! -x "$(PY_VENV_PYTHON)" ]]; then
  echo "[venv] 创建 $(PY_VENV)"
  $(PYTHON) -m venv "$(PY_VENV)"
else
  echo "[venv] 已存在: $(PY_VENV)"
fi
chmod u+x "$(PY_VENV)"/bin/python* "$(PY_VENV)"/bin/pip* 2>/dev/null || true
export VIRTUAL_ENV="$(PY_VENV)"; export PATH="$(PY_VENV)/bin:$$PATH"
endef

define __py_upgrade_build_tools
# 升级 pip/setuptools/wheel
echo "[venv] 升级 pip/setuptools/wheel"
"$(PY_VENV_PIP)" install --no-build-isolation --upgrade pip setuptools wheel
endef

define __py_install_export_layer
# 安装本地 export/python (editable，可选)
if [[ -d export/python ]]; then
  echo "[pip] 安装 mental1104_export_layer (editable)"
  cd export/python; "$(PY_VENV_PIP)" install --no-build-isolation -e .
  cd "$(REPO_ROOT)"
else
  echo "[warn] 未找到 export/python，跳过 mental1104_export_layer 安装"
fi
endef

define __py_install_requirements
# 安装 python/requirements.txt 依赖
if [[ -f python/requirements.txt ]]; then
  echo "[pip] 安装依赖到 venv: python/requirements.txt (no build isolation)"
  EXPORT_ABS_DIR="$(REPO_ROOT)/export/python"
  if [[ ! -d "$$EXPORT_ABS_DIR" ]]; then echo "[err] 缺少 export/python 目录：$$EXPORT_ABS_DIR"; exit 1; fi
  REQ_FILE="$(REPO_ROOT)/python/requirements.txt"
  REQ_BAK="$$REQ_FILE.bak.setup"
  restore_req(){ [[ -f "$$REQ_BAK" ]] && mv "$$REQ_BAK" "$$REQ_FILE"; }
  trap restore_req EXIT
  if grep -q "file://../export/python" "$$REQ_FILE"; then
    cp "$$REQ_FILE" "$$REQ_BAK"
    python3 - "$$REQ_FILE" "$$EXPORT_ABS_DIR" <<-'PY'
from pathlib import Path; import sys
req=Path(sys.argv[1]); target=Path(sys.argv[2]).resolve()
req.write_text(req.read_text().replace("file://../export/python", f"file://{target}"))
PY
  fi
  cd python
  "$(PY_VENV_PIP)" install --no-build-isolation -r requirements.txt
  restore_req
  trap - EXIT
else
  echo "[info] 未找到 python/requirements.txt，跳过依赖安装。"
fi
cd "$(REPO_ROOT)"
endef

define __py_generate_init
# 生成 __init__（可选）
if [[ -f python/generate_init.py ]]; then
  echo "[info] 执行 python/generate_init.py …"
  "$(PY_VENV_PYTHON)" python/generate_init.py
else
  echo "[info] 未找到 python/generate_init.py，跳过。"
fi
endef

define __py_fix_future_annotations
# 兼容性修复：为使用 | 联合类型注解的源码注入 __future__ import annotations
echo "[compat] 扫描并修复使用 | 联合类型注解的源码（兼容 Py3.9）…"
files=$$(grep -R -l -E ":\s*[^#]*\|[^=]*" python --exclude-dir=.venv || true)
inserted=0
for f in $$files; do
  [ -f "$$f" ] || continue
  if grep -q "^from __future__ import annotations$$" "$$f"; then continue; fi
  tmp="$$f.tmp.$$RANDOM"
  awk "BEGIN{in_doc=0;doc_done=0;ins=0} NR==1 && substr(\$$0,1,2)==\"#!\" {ins=NR+1} NR<=2 && match(\$$0,/(coding[:=])/){if(NR>=ins) ins=NR+1} doc_done==0{line=\$$0;gsub(/^[[:space:]]+/ ,\"\", line); if(in_doc==0){ if(line ~ /^([rRuUbBfF]{0,2})?\"\"\"/){ in_doc=1; if(gsub(/\"\"\"/ ,\"&\")>=2){ in_doc=0; doc_done=1; if(NR>=ins) ins=NR } } else if (line!=\"\" && substr(line,1,1)!=\"#\"){ doc_done=1 } } else { if(index(\$$0, \"\\\"\\\"\\\"\")>0){ in_doc=0; doc_done=1; if(NR>=ins) ins=NR } }} { lines[NR]=\$$0 } END{ if(ins==0) ins=0; for(i=1;i<=NR;i++){ print lines[i]; if(i==ins) print \"from __future__ import annotations\" } if(NR==0) print \"from __future__ import annotations\" }" "$$f" > "$$tmp" && mv "$$tmp" "$$f" && inserted=$$((inserted+1))
done
cnt=$$(printf "%s\n" $$files | sed "/^$$/d" | wc -l | tr -d " ")
echo "[compat] files_found=$$cnt, inserted=$$inserted"
endef

define __py_build_wheel
# 构建 wheel（不安装本体）
echo "[info] 构建本地 wheel（不安装本体）…"
mkdir -p python/dist
REQ_FILE="$(REPO_ROOT)/python/requirements.txt"; \
REQ_BAK="$$REQ_FILE.bak.build"; \
EXPORT_ABS_DIR="$(REPO_ROOT)/export/python"; \
restore_req(){ [[ -f "$$REQ_BAK" ]] && mv "$$REQ_BAK" "$$REQ_FILE"; }; \
trap restore_req EXIT; \
if [[ -f "$$REQ_FILE" && -d "$$EXPORT_ABS_DIR" ]]; then \
  if grep -q "file://../export/python" "$$REQ_FILE"; then \
    cp "$$REQ_FILE" "$$REQ_BAK"; \
    python3 - "$$REQ_FILE" "$$EXPORT_ABS_DIR" <<-'PY'
from pathlib import Path; import sys
req=Path(sys.argv[1]); target=Path(sys.argv[2]).resolve()
req.write_text(req.read_text().replace("file://../export/python", f"file://{target}"))
PY
  fi; \
fi; \
"$(PY_VENV_PYTHON)" -m pip wheel --no-deps -w python/dist python/; \
restore_req; trap - EXIT
echo "[ok] setup 完成（venv 安装依赖 + 构建 wheel，不安装本体）。"
echo "[hint] 如需使用此 venv，请执行: source $(PY_VENV)/bin/activate"
endef

define __py_guard_venv
if [[ ! -x "$(PY_VENV_PYTHON)" || ! -x "$(PY_VENV_PIP)" ]]; then echo "[error] 未找到项目 venv: $(PY_VENV)，请先执行 make setup-python"; exit 1; fi; \
export VIRTUAL_ENV="$(PY_VENV)"; export PATH="$(PY_VENV)/bin:$$PATH";
endef

define __py_test_env_flags
export EXPORT_LAYER_LOG_LEVEL=INFO; \
unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)";
endef

define __py_test_plugins
export PYTEST_DISABLE_PLUGIN_AUTOLOAD=0; \
export PYTEST_PLUGINS=pytest_benchmark.plugin,pytest_asyncio.plugin,pytest_mock; \
if ! "$(PY_VENV_PIP)" show pytest-benchmark >/dev/null 2>&1; then \
  echo "[info] 安装 pytest-benchmark 到 venv …"; \
  "$(PY_VENV_PIP)" install pytest-benchmark; \
fi;
endef

define __py_test_check_pytest
if ! "$(PY_VENV_PYTHON)" -m pytest --version >/dev/null 2>&1; then echo "[warn] 未检测到 pytest；请先执行: make setup-python"; exit 1; fi;
endef

define __py_test_export_lib
EXP_LIB=""; \
for ext in so dylib dll; do \
  cand="$(EXPORT_CPP_BUILD_DIR)/libexport_json.$$ext"; \
  if [[ -f "$$cand" ]]; then EXP_LIB="$$cand"; break; fi; \
done; \
if [[ -n "$$EXP_LIB" ]]; then export EXPORT_LAYER_CTYPE_LIB="$$EXP_LIB"; fi; \
export PYTHONPATH="$(EXPORT_CPP_BUILD_DIR):$$PYTHONPATH";
endef

define __py_test_run_pytest
run_cmd="$(PY_VENV_PYTHON) -m pytest"; \
if [[ -n "$${PY_RUNNER:-}" ]]; then run_cmd="$$PY_RUNNER"; fi; \
cd python; \
kexpr="$${KEXPR_DEFAULT:-not bench and not benchmark}"; \
[[ -n "$${FILTER:-}" ]] && kexpr="($${FILTER}) and $$kexpr"; \
[[ -n "$${FILE:-}" ]] && kexpr="($${FILE}) and $$kexpr"; \
py_args="$${PYTEST_ARGS:-}"; \
eval "$$run_cmd $$py_args -k \"$$kexpr\""
endef

define __py_script_prelude
script_file=$$(mktemp); \
trap "rm -f \"$$script_file\"" EXIT; \
cat <<-'"'"'__SETUP_PYTHON__'"'"' >"$$script_file"
endef

define __py_script_finalize
__SETUP_PYTHON__
bash "$$script_file"
endef

define __py_install_system
wheel=$$(ls python/dist/*.whl 2>/dev/null | tail -n1); \
SYS_BREAK_FLAG="$(if $(IS_UBUNTU),--break-system-packages,)"; \
echo "[info] $(PIP3) install --upgrade pip setuptools wheel $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG"; \
$(SUDO) $(PIP3) install --upgrade pip setuptools wheel $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG; \
EXPORT_ABS_DIR="$(REPO_ROOT)/export/python"; \
if [[ -n "$$wheel" ]]; then \
  if [[ -d "$$EXPORT_ABS_DIR" ]]; then \
    echo "[info] $(PIP3) install $$EXPORT_ABS_DIR --no-build-isolation --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG"; \
    PIP_NO_BUILD_ISOLATION=1 PIP_NO_DEPS=1 $(SUDO) $(PIP3) install "$$EXPORT_ABS_DIR" --no-build-isolation --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG; \
  fi; \
  echo "[info] $(PIP3) install $$wheel --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG"; \
  PIP_NO_BUILD_ISOLATION=1 PIP_NO_DEPS=1 $(SUDO) $(PIP3) install "$$wheel" --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG; \
else \
  echo "[info] $(PIP3) install python/ --upgrade $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG (no wheel found)"; \
  REQ_FILE="$(REPO_ROOT)/python/requirements.txt"; \
  REQ_BAK="$$REQ_FILE.bak.install"; \
  restore_req(){ [[ -f "$$REQ_BAK" ]] && mv "$$REQ_BAK" "$$REQ_FILE"; }; \
  trap restore_req EXIT; \
  if [[ -f "$$REQ_FILE" && -d "$$EXPORT_ABS_DIR" ]]; then \
    if grep -q "file://../export/python" "$$REQ_FILE"; then \
      cp "$$REQ_FILE" "$$REQ_BAK"; \
      python3 - "$$REQ_FILE" "$$EXPORT_ABS_DIR" <<-'PY'
from pathlib import Path; import sys
req=Path(sys.argv[1]); target=Path(sys.argv[2]).resolve()
req.write_text(req.read_text().replace("file://../export/python", f"file://{target}"))
PY
    fi; \
  fi; \
  if [[ -d "$$EXPORT_ABS_DIR" ]]; then \
    echo "[info] $(PIP3) install $$EXPORT_ABS_DIR --no-build-isolation --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG"; \
    PIP_NO_BUILD_ISOLATION=1 PIP_NO_DEPS=1 $(SUDO) $(PIP3) install "$$EXPORT_ABS_DIR" --no-build-isolation --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG; \
  fi; \
  PIP_NO_BUILD_ISOLATION=1 PIP_NO_DEPS=1 $(SUDO) $(PIP3) install python/ --upgrade --no-build-isolation --no-deps $(PIP_MIRROR_OPTS) $$SYS_BREAK_FLAG; \
  restore_req; trap - EXIT; \
fi; \
echo "[ok] 安装完成（已安装到系统 Python）。"
endef

define __py_uninstall_system
SYS_BREAK_FLAG="$(if $(IS_UBUNTU),--break-system-packages,)"; \
for pkg in mental1104_export_layer mental1104-export-layer mental1104; do \
  if $(PIP3) show "$$pkg" >/dev/null 2>&1; then \
    echo "[info] $(PIP3) uninstall -y $$pkg $$SYS_BREAK_FLAG"; \
    $(SUDO) $(PIP3) uninstall -y "$$pkg" $$SYS_BREAK_FLAG || true; \
  fi; \
done; \
echo "[ok] 系统 Python 已尝试卸载 mental1104 相关包"
endef

define __py_clean_artifacts
echo "[info] 清理 Python 缓存与构建产物…"; \
rm -rf python/build python/dist python/*.egg-info .pytest_cache .mypy_cache python/.coverage htmlcov python/.ruff_cache python/.pytest_cache python/.benchmarks python/memray.bin;
endef

define __py_clean_caches
find python -type d -name "__pycache__" -exec rm -rf {} +; \
find python -type f -name "*.py[co]" -delete; \
find python -type d -name ".pytest_cache" -exec rm -rf {} +;
endef

define __py_clean_venv
if [[ -d "python/.venv" ]]; then \
  echo "[info] 移除 Python venv: python/.venv"; \
  rm -rf python/.venv; \
fi
endef

define __py_coverage_run
echo "[info] 运行python单元测试覆盖率"; \
cd python; \
kexpr="not bench and not benchmark"; \
[[ -n "$${FILTER:-}" ]] && kexpr="($${FILTER}) and $$kexpr"; \
[[ -n "$${FILE:-}" ]] && kexpr="($${FILE}) and $$kexpr"; \
$(PY_VENV_PYTHON) -m coverage run --source=. -m pytest -c /dev/null -k "$$kexpr"; \
$(PY_VENV_PYTHON) -m coverage report
endef

define __py_fmt_ensure_tool
if ! "$(PY_VENV_PYTHON)" -c "import autopep8" >/dev/null 2>&1; then \
  "$(PY_VENV_PIP)" install autopep8; \
fi
endef

define __py_fmt_run
cd python; \
"$(PY_VENV_PYTHON)" -m autopep8 --in-place --recursive --max-line-length=120 --ignore=E402,E226,E24,W50,W690 .; \
echo "[ok] python autopep8 fmt 完成。"
endef

define __py_bench_env
export EXPORT_LAYER_LOG_LEVEL=INFO; \
unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"; \
export PYTHON="$(PY_VENV_PYTHON)";
endef

define __py_bench_check_pytest
if ! command -v pytest >/dev/null 2>&1; then echo "[error] 未检测到 pytest；请先执行: make setup-python"; exit 1; fi;
endef

define __py_bench_export_lib
EXP_LIB=""; \
for ext in so dylib dll; do \
  cand="$(EXPORT_CPP_BUILD_DIR)/libexport_json.$$ext"; \
  if [[ -f "$$cand" ]]; then EXP_LIB="$$cand"; break; fi; \
done; \
if [[ -n "$$EXP_LIB" ]]; then export EXPORT_LAYER_CTYPE_LIB="$$EXP_LIB"; fi; \
export PYTHONPATH="$(EXPORT_CPP_BUILD_DIR):$(REPO_ROOT)/export/python/src:$$PYTHONPATH";
endef

define __py_bench_select_and_run
cd python; \
PY_BIN="$(PY_VENV_PYTHON)"; \
if ! "$$PY_BIN" -m pytest -q --help 2>/dev/null | grep -qi benchmark; then \
  echo "[warn] 未检测到 pytest-benchmark 插件，回退到名称筛选"; \
  kexpr="$(PYTEST_BENCH_K)"; \
  [[ -n "$${FILTER:-}" ]] && kexpr="($${FILTER}) and ($$kexpr)"; \
  [[ -n "$${FILE:-}" ]] && kexpr="($${FILE}) and ($$kexpr)"; \
  "$$PY_BIN" -m pytest $(PYTEST_V) -k "$$kexpr"; \
  exit 0; \
fi; \
bench_files=$$(find test_benchmark -type f -name "test_*.py" | sort); \
if [ -n "$${FILE:-}" ]; then \
  bench_files=$$(printf "%s\n" $$bench_files | grep -E "$${FILE}" || true); \
elif [ -n "$${FILTER:-}" ]; then \
  bench_files=$$(printf "%s\n" $$bench_files | grep -Ei "$${FILTER}" || true); \
fi; \
if [ -z "$$bench_files" ]; then \
  echo "[warn] 未找到 test_benchmark/* 基准文件，回退到名称筛选"; \
  kexpr="$(PYTEST_BENCH_K)"; \
  [[ -n "$${FILTER:-}" ]] && kexpr="($${FILTER}) and ($$kexpr)"; \
  [[ -n "$${FILE:-}" ]] && kexpr="($${FILE}) and ($$kexpr)"; \
  "$$PY_BIN" -m pytest $(PYTEST_V) -k "$$kexpr" --benchmark-only; \
  exit 0; \
fi; \
rm -rf "$(PY_BENCH_ARTIFACT_DIR)"; \
mkdir -p "$(PY_BENCH_ARTIFACT_DIR)/plots"; \
for file in $$bench_files; do \
  slug=$$(echo "$$file" | sed "s@/@__@g" | sed "s/\\.py$$//"); \
  json="$(PY_BENCH_ARTIFACT_DIR)/$${slug}.json"; \
  title="Python $$file"; \
  case "$$file" in \
    test_benchmark/test_concurrency/*) extra_opts="$(PY_BENCHMARK_CONCURRENCY_OPTS)";; \
    *) extra_opts="";; \
  esac; \
  echo "[bench-python] $$file -> $$json"; \
  kexpr=""; \
  [[ -n "$${FILTER:-}" ]] && kexpr="($${FILTER})"; \
  [[ -n "$${FILE:-}" ]] && kexpr="$${kexpr:+($$kexpr) and }($${FILE})"; \
  kargs=(); \
  [[ -n "$$kexpr" ]] && kargs+=(-k "$$kexpr"); \
  "$$PY_BIN" -m pytest $(PYTEST_V) "$$file" --benchmark-only --benchmark-json "$$json" $(PY_BENCHMARK_OPTS) $$extra_opts "$${kargs[@]}"; \
"$$PY_BIN" tools/render_bench_plots.py \
    --input "$$json" \
    --test-type pytest-benchmark \
    --chart case-matrix \
    --output "$(PY_BENCH_ARTIFACT_DIR)/plots/$${slug}.png" \
    --title "$$title"; \
done; \
echo "[bench-python] 图表输出目录：$(PY_BENCH_ARTIFACT_DIR)/plots"
endef

define __py_guard_pytest
$(call __py_test_env_flags)      # 清空代理、设定日志
$(call __py_test_plugins)        # 加载/安装 pytest 需要的插件
$(call __py_test_check_pytest)   # 校验 pytest 可用
$(call __py_test_export_lib)     # 导出 C++ 桥接库及 PYTHONPATH
PY_RUNNER="$(PY_VENV_PYTHON) -m pytest"; \
PYTEST_ARGS=""; \
KEXPR_DEFAULT="not bench and not benchmark"; \
$(call __py_test_run_pytest)
endef

define __py_vet_ruff
echo "[info] 运行 ruff 静态检查 (F,B,UP,PERF)"; \
if ! "$(PY_VENV_PYTHON)" -c "import ruff" >/dev/null 2>&1; then \
  echo "[ruff] 未找到 ruff，尝试安装到 venv"; \
  "$(PY_VENV_PIP)" install ruff; \
fi; \
cd python; \
"$(PY_VENV_PYTHON)" -m ruff check --select F,B,UP,PERF mental1104; \
echo "[ok] vet-python 完成。"
endef

define __cpp_configure_env
mkdir -p "$(CPP_BUILD_DIR)"; \
pybind_dir=""; \
if [[ -x "$(PY_VENV_PYTHON)" ]]; then \
  pybind_dir="$$( "$(PY_VENV_PYTHON)" -m pybind11 --cmakedir 2>/dev/null || true)"; \
fi; \
extra=""; \
[[ -n "$$pybind_dir" ]] && extra="-Dpybind11_DIR=$$pybind_dir";
endef

define __cpp_configure_run
$(CMAKE) -S "$(CPP_SRC_DIR)" -B "$(CPP_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CPP_BUILD_TYPE)" -DPYBIND11_FINDPYTHON=ON $$extra; \
echo "[ok] 顶层 cmake 配置完成（$(CPP_BUILD_TYPE)）"
endef

define __cpp_build_run
if $(CMAKE) --build "$(CPP_BUILD_DIR)" --parallel $(JOBS); then :; \
else $(CMAKE) --build "$(CPP_BUILD_DIR)" -- -j $(JOBS); fi; \
echo "[ok] 顶层 C++ 构建完成（-j $(JOBS)）"
endef

define __cpp_test_env
unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
echo "[proxy] disabled for pytest (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"; \
if [[ -n "$(CPP_LOG_LEVEL)" ]]; then \
  export MENTAL1104_LOG_LEVEL="$(CPP_LOG_LEVEL)"; \
  echo "[log] MENTAL1104_LOG_LEVEL=$(CPP_LOG_LEVEL)"; \
fi; \
cd "$(CPP_BUILD_DIR)"
endef

define __cpp_test_patterns
ctest_pat="$${FILE:-}"; \
gtest_filter="$${FILTER:-}"; \
if [[ -z "$$ctest_pat" && -n "$(CPP_TEST_FILES)$(CPP_TEST_DIRS)" ]]; then \
  files=""; \
  [[ -n "$(CPP_TEST_FILES)" ]] && files+=" $(CPP_TEST_FILES)"; \
  if [[ -n "$(CPP_TEST_DIRS)" ]]; then \
    while IFS= read -r f; do files+=" $$f"; done < <(find $(CPP_TEST_DIRS) -type f -name "*.cpp" | sort); \
  fi; \
  names=$$(for f in $$files; do b=$$(basename "$$f"); echo $${b%.cpp}; done | sort -u | paste -sd"|" -); \
  [[ -n "$$names" ]] && ctest_pat="^($$names)$$"; \
elif [[ -z "$$ctest_pat" && -n "$(CPP_TEST)" ]]; then \
  files=""; unknown=""; \
  for tok in $(CPP_TEST); do \
    if [[ -f "$$tok" ]]; then files+=" $$tok"; continue; fi; \
    if [[ -d "$$tok" ]]; then \
      while IFS= read -r f; do files+=" $$f"; done < <(find "$$tok" -type f -name "*.cpp" | sort); \
      continue; \
    fi; \
    unknown="$$tok"; \
  done; \
  if [[ -n "$$files" ]]; then \
    names=$$(for f in $$files; do b=$$(basename "$$f"); echo $${b%.cpp}; done | sort -u | paste -sd"|" -); \
    [[ -n "$$names" ]] && ctest_pat="^($$names)$$"; \
  elif [[ -z "$$ctest_pat" && -n "$$unknown" ]]; then \
    ctest_pat="$$unknown"; \
  fi; \
fi; \
args=(--output-on-failure -LE bench -j $(JOBS)); \
[[ -n "$(CTEST_V)" ]] && args+=($(CTEST_V)); \
[[ -n "$$ctest_pat" ]] && args+=(-R "$$ctest_pat")
endef

define __cpp_test_run
if [[ -n "$$gtest_filter" ]]; then \
  echo "[ctest] GTEST_FILTER=$$gtest_filter $(CTEST) $${args[*]}"; \
  GTEST_FILTER="$$gtest_filter" $(CTEST) "$${args[@]}"; \
else \
  echo "[ctest] $(CTEST) $${args[*]}"; \
  $(CTEST) "$${args[@]}"; \
fi
endef

define __cpp_install_run
echo "[info] 安装到前缀: $(PREFIX)"; \
$(SUDO) $(CMAKE) --install "$(CPP_BUILD_DIR)" --prefix "$(PREFIX)"; \
echo "[ok] 安装完成。"
endef

define __cpp_fmt_check_tool
if ! command -v clang-format >/dev/null 2>&1; then echo "[error] 未找到 clang-format"; exit 1; fi
endef

define __cpp_fmt_run
find cpp \
  \( -path "cpp/lib" -o -path "cpp/lib/*" -o -path "cpp/thirdparty" -o -path "cpp/thirdparty/*" \) -prune -o \
  -type f -regex ".*\.\(h\|hh\|hpp\|hxx\|c\|cc\|cpp\|cxx\)" -print0 | xargs -0 -n 50 clang-format -i; \
echo "[ok] cpp fmt 完成（已排除 cpp/lib 与 cpp/thirdparty）"
endef

define __cpp_bench_env
if [[ -n "$(CPP_LOG_LEVEL)" ]]; then \
  export MENTAL1104_LOG_LEVEL="$(CPP_LOG_LEVEL)"; \
  echo "[log] MENTAL1104_LOG_LEVEL=$(CPP_LOG_LEVEL)"; \
fi; \
if [[ ! -d "$(CPP_BUILD_DIR)" ]]; then echo "[info] 未发现 $(CPP_BUILD_DIR)，请先执行: make build-cpp"; exit 1; fi; \
shopt -s nullglob; \
binaries=($(CPP_BUILD_DIR)/bin/bench_*); \
shopt -u nullglob; \
file_pat="$${FILE:-}"; \
bench_filter="$${FILTER:-}";
endef

define __cpp_bench_collect
if [[ -n "$(CPP_BENCH_FILES)" ]]; then \
  declare -A want=(); \
  for f in $(CPP_BENCH_FILES); do b=$$(basename "$$f"); b=$${b%.cpp}; want["$$b"]=1; done; \
  filtered=(); \
  for exe in "$${binaries[@]}"; do \
    base=$$(basename "$$exe"); \
    [[ -n "$${want[$$base]}" ]] && filtered+=("$${exe}"); \
  done; \
  binaries=("$${filtered[@]}"); \
fi; \
if [[ -n "$$file_pat" ]]; then \
  filtered=(); \
  for exe in "$${binaries[@]}"; do \
    base=$$(basename "$$exe"); \
    if [[ "$$base" =~ $$file_pat ]]; then filtered+=("$${exe}"); fi; \
  done; \
  binaries=("$${filtered[@]}"); \
fi
endef

define __cpp_bench_run
if [[ $${#binaries[@]} -eq 0 ]]; then \
  echo "[warn] 未找到 bench_* 可执行文件，回退到 ctest"; \
  cd "$(CPP_BUILD_DIR)"; \
  args=(--output-on-failure -L bench -j $(JOBS)); \
  [[ -n "$(CTEST_V)" ]] && args+=($(CTEST_V)); \
  [[ -n "$$file_pat" ]] && args+=(-R "$$file_pat"); \
  echo "[ctest] $(CTEST) $${args[*]}"; \
  $(CTEST) "$${args[@]}"; \
  exit 0; \
fi; \
rm -rf "$(CPP_BENCH_ARTIFACT_DIR)"; \
mkdir -p "$(CPP_BENCH_ARTIFACT_DIR)/plots"; \
for exe in "$${binaries[@]}"; do \
  name=$$(basename "$$exe"); \
  json="$(CPP_BENCH_ARTIFACT_DIR)/$${name}.json"; \
  echo "[bench-cpp] $$name -> $$json"; \
  args=( \
    --benchmark_out="$$json" \
    --benchmark_out_format=json \
    --benchmark_min_time=0.2 \
    --benchmark_repetitions=5 \
    --benchmark_display_aggregates_only=true \
    --benchmark_time_unit=ms \
  ); \
  [[ -n "$$bench_filter" ]] && args+=(--benchmark_filter="$$bench_filter"); \
  if ! "$$exe" "$${args[@]}"; then \
    if [[ -n "$$bench_filter" ]]; then \
      echo "[bench-cpp][skip] $$name 无匹配项 (FILTER=$$bench_filter)"; \
      rm -f "$$json"; \
      continue; \
    else \
      echo "[bench-cpp][fail] $$name 运行失败"; \
      exit 1; \
    fi; \
  fi; \
  if [[ ! -s "$$json" ]]; then \
    echo "[bench-cpp][skip] $$name 未生成有效输出"; \
    continue; \
  fi; \
  $(PYTHON) python/tools/render_bench_plots.py \
    --input "$$json" \
    --test-type google-benchmark \
    --chart case-matrix \
    --output "$(CPP_BENCH_ARTIFACT_DIR)/plots/$${name}.png" \
    --title "C++ $$name"; \
done; \
echo "[bench-cpp] 图表输出目录：$(CPP_BENCH_ARTIFACT_DIR)/plots"
endef

define __cpp_coverage_run
cd cpp/build; \
if [[ -n "$${RUN_CTEST_FOR_COVERAGE:-}" ]]; then \
  # 预清理旧的 gcda，随后重跑 ctest 生成新数据
  find . -name "*.gcda" -delete 2>/dev/null || true; \
  echo "[info] RUN_CTEST_FOR_COVERAGE=1 -> 仅运行一次 ctest（排除 bench）"; \
  ctest --output-on-failure -LE bench || true; \
else \
  echo "[info] 已依赖 test-cpp，跳过重复 ctest；如需重跑可设置 RUN_CTEST_FOR_COVERAGE=1"; \
fi; \
if command -v gcovr >/dev/null 2>&1; then \
  echo "[info] 使用 gcovr 汇总覆盖率（已排除 lib/ 与 thirdparty/）"; \
  if ! gcovr -r .. --object-directory . \
        --exclude "(^|.*/)(test|bench|external|gtest|lib|thirdparty|overlay)/" \
        --exclude "/usr/include/.*" \
        --exclude-directories ".*/build-(asan|tsan|ubsan|msan).*" \
        --gcov-ignore-parse-errors \
        --txt --print-summary; then \
    echo "[warn] gcovr 生成覆盖率失败（可能未开启 --coverage 编译）"; \
    echo "[hint] 请尝试: make clean-cpp && make build-cpp-debug COVERAGE=ON"; \
    exit 1; \
  fi; \
else \
  echo "[info] 未检测到 gcovr，回退到 lcov"; \
  if ! command -v lcov >/dev/null 2>&1; then \
    echo "[error] 未安装 lcov；请安装 gcovr 或 lcov 任一工具"; exit 1; \
  fi; \
  BASE=".."; \
  lcov --directory . --capture --output-file coverage.info --base-directory "$$BASE" \
       --ignore-errors mismatch,negative,inconsistent,empty,unused \
       --no-external --rc geninfo_unexecuted_blocks=1; \
  lcov --remove coverage.info --base-directory "$$BASE" \
       "*/test/*" "*/bench/*" "*/external/*" "*/gtest/*" "*/lib/*" "*/thirdparty/*" "/usr/*" "/overlay/*" \
       -o coverage.filtered.info \
       --ignore-errors empty,unused || true; \
  if ! lcov --list coverage.filtered.info --base-directory "$$BASE" --ignore-errors empty,unused; then \
    echo "[warn] lcov 未产生有效数据（可能未启用 --coverage 或过滤过严）"; \
    echo "[hint] 请尝试: make clean-cpp && make build-cpp-debug COVERAGE=ON"; \
    exit 1; \
  fi; \
  echo "[ok] 生成：cpp/build/coverage.info（过滤版：coverage.filtered.info）"; \
fi
endef

define __cpp_uninstall_run
manifest="$(CPP_BUILD_DIR)/install_manifest.txt"; \
if [ ! -f "$$manifest" ]; then \
  echo "[error] 未找到 $$manifest，请先执行 make install-cpp"; exit 1; \
fi; \
echo "[info] 从 $$manifest 卸载已安装文件"; \
while IFS= read -r f; do \
  [ -z "$$f" ] && continue; \
  if [ -e "$$f" ]; then \
    echo "[rm] $$f"; \
    $(SUDO) rm -f "$$f"; \
  else \
    echo "[warn] 跳过缺失文件: $$f"; \
  fi; \
done < "$$manifest"; \
echo "[ok] 卸载完成。"
endef

define __cpp_clean_build
echo "[info] 清理顶层 C++ 构建目录: $(CPP_BUILD_DIR)"; \
rm -rf "$(CPP_BUILD_DIR)"; \
echo "[ok] clean 完成。"
endef

define __cpp_vet_run
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
    --header-filter="^.*/cpp/include/mental1104/.*"; \
echo "[ok] vet-cpp 完成。"
endef

define __cpp_guard_run
MODE="${MODE:-mem}"; \
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
fi; echo "[ok] guard-cpp[mem] 通过"
endef

define __docker_up_all
@if ! command -v docker >/dev/null 2>&1; then echo "[warn] 未检测到 docker，跳过 setup-docker"; exit 0; fi
@[ -n "$(COMPOSE_DIRS)" ] || { echo "[warn] images/ 未找到 $(COMPOSE_FILE_NAME)"; exit 0; }
@for d in $(COMPOSE_DIRS); do \
	echo ">> UP $$d"; \
	$(COMPOSE_BIN) \
	  --project-directory "$(REPO_ROOT)" \
	  $(ENV_FILE_OPT) \
	  -f "$$d/$(COMPOSE_FILE_NAME)" up -d \
	|| { echo "[warn] $$d 启动失败（已忽略）"; continue; }; \
done
@echo "[ok] setup-docker 完成（出错已忽略）"
endef

define __docker_down_all
@$(MAKE) --no-print-directory env-clean
@if ! command -v docker >/dev/null 2>&1; then echo "[warn] 未检测到 docker，跳过 clean-docker"; exit 0; fi
@[ -n "$(COMPOSE_DIRS)" ] || { echo "[warn] images/ 未找到 $(COMPOSE_FILE_NAME)"; exit 0; }
@for d in $(COMPOSE_DIRS); do \
	echo ">> DOWN $$d"; \
	$(COMPOSE_BIN) \
		--project-directory "$(REPO_ROOT)" \
		$(ENV_FILE_OPT) \
		-f "$$d/$(COMPOSE_FILE_NAME)" down --remove-orphans \
		|| { echo "[warn] $$d 关闭失败（已忽略）"; continue; }; \
done
@echo "[ok] clean-docker 完成（出错已忽略）"
endef

define __go_guard_tool
echo "[go] 目录: $(GO_DIR)"; \
if ! command -v $(GO) >/dev/null 2>&1; then echo "[error] 未找到 go 命令"; exit 1; fi
endef

define __go_guard_mod
if [[ ! -f "$(GO_DIR)/go.mod" ]]; then \
  echo "[warn] $(GO_DIR)/go.mod 不存在，请先在 $(GO_DIR) 执行: go mod init <module>"; \
  exit 1; \
fi
endef

define __go_mod_tidy_download
cd "$(GO_DIR)"; \
echo "[go] env: GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY=$(GOPROXY) GOPRIVATE=$(GOPRIVATE)"; \
GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) mod tidy; \
GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) mod download; \
echo "[ok] go setup 完成。"
endef

define __go_build_pkgs
cd "$(GO_DIR)"; \
echo "[go] build ./... (库包仅做编译检查，不产生仓库内产物)"; \
GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" $(GO) build ./...; \
echo "[ok] go build 完成。"
endef

define __go_build_bins
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
echo "[ok] go 可执行产物已生成到 $(GO_DIR)/bin/"
endef

define __go_env_clear_proxy
unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
echo "[proxy] disabled for go (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"
endef

define __go_test_run
cd "$(GO_DIR)"; \
run_args=(); pkg_args=(./...); \
[[ -n "$${FILTER:-}" ]] && run_args+=(-run "$${FILTER}"); \
if [[ -n "$${FILE:-}" ]]; then \
	mapfile -t dirs < <(find . -name "*_test.go" | grep -E "$${FILE}" | xargs -r -n1 dirname | sort -u); \
	if [[ $${#dirs[@]} -gt 0 ]]; then \
		pkg_args=(); \
		for d in "$${dirs[@]}"; do pkg_args+=("$${d#./}"); done; \
	else \
		echo "[warn] FILE=$${FILE} 未匹配到 *_test.go，回退全量包"; \
	fi; \
fi; \
echo "[go] test -count=1 $(GO_TEST_V) $${run_args[*]:-} $${pkg_args[*]}"; \
GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
$(GO) test -count=1 $(GO_TEST_V) "$${run_args[@]}" "$${pkg_args[@]}"; \
echo "[ok] go test 通过。"
endef

define __go_coverage_run
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
echo "[ok] 覆盖率生成完成：$(GO_COVER_OUT) / $(GO_COVER_HTML)"
endef

define __go_fmt_run
cd "$(GO_DIR)"; go fmt ./... >/dev/null; echo "[ok] go fmt 完成。"
endef

define __go_bench_run
cd "$(GO_DIR)"; \
bench_pat="$${FILTER:-.}"; pkg_args=(./...); \
if [[ -n "$${FILE:-}" ]]; then \
	mapfile -t dirs < <(find . -name "*_test.go" | grep -E "$${FILE}" | xargs -r -n1 dirname | sort -u); \
	if [[ $${#dirs[@]} -gt 0 ]]; then \
		pkg_args=(); \
		for d in "$${dirs[@]}"; do pkg_args+=("$${d#./}"); done; \
	else \
		echo "[warn] FILE=$${FILE} 未匹配到 *_test.go，回退全量包"; \
	fi; \
fi; \
echo "[go] test $(GO_TEST_V) -bench=\"$$bench_pat\" -benchmem $${pkg_args[*]}"; \
GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
$(GO) test $(GO_TEST_V) -bench="$$bench_pat" -benchmem "$${pkg_args[@]}"
endef

define __go_install_run
cd "$(GO_DIR)"; \
echo "[go] install ./... （仅对 package main 生效）"; \
GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) GOPROXY="$(GOPROXY)" GOPRIVATE="$(GOPRIVATE)" \
GOBIN="$(GOBIN)" $(GO) install ./...; \
if [[ -n "$(GOBIN)" ]]; then \
	echo "[ok] 可执行文件安装到: $(GOBIN)"; \
else \
	echo "[ok] 可执行文件已安装到默认 GOBIN（见: go env GOBIN 或 GOPATH/bin）"; \
fi
endef

define __go_clean_run
cd "$(GO_DIR)"; \
echo "[go] 清理覆盖率与产物 …"; \
rm -f coverage.out coverage.html; \
rm -rf bin; \
echo "[go] 执行 go clean（移除测试缓存与临时对象） …"; \
GOWORK=$(GOWORK) $(GO) clean -testcache; \
GOWORK=$(GOWORK) $(GO) clean ./...; \
echo "[ok] go clean 完成。"
endef

define __go_vet_run
cd golang; \
echo "[info] 运行 go vet 静态分析..."; \
out=$$(GOWORK=$(GOWORK) $(GO) vet ./... 2>&1 || true); \
if [ -z "$$out" ]; then \
  echo "[ok] go vet 未发现问题。"; \
else \
  echo "$$out"; \
  exit 1; \
fi
endef

define __go_guard_race
cd "$(GO_DIR)"; log=$$(mktemp -t guard_go.XXXX).log; \
echo "[go] go test -race -count=1 ./..."; \
if ! GOWORK=$(GOWORK) GOTOOLCHAIN=$(GOTOOLCHAIN) $(GO) test -race -count=1 ./... | tee "$$log"; then \
  if grep -q "DATA RACE" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$log"; else echo "[fail][test] 测试失败, 日志 $$log"; fi; exit 1; \
fi; if grep -q "DATA RACE" "$$log"; then echo "[fail][concurrency] 并发竞态问题, 日志 $$log"; exit 1; fi; \
echo "[ok] guard-go 通过"
endef

define __go_uninstall_run
cd "$(GO_DIR)"; \
bin_dir="$(GOBIN)"; \
if [ -z "$$bin_dir" ]; then bin_dir="$$( $(GO) env GOBIN )"; fi; \
if [ -z "$$bin_dir" ]; then bin_dir="$$( $(GO) env GOPATH )/bin"; fi; \
echo "[go] uninstall from $$bin_dir"; \
if [ ! -d "$$bin_dir" ]; then echo "[warn] 未找到 bin 目录，跳过卸载"; exit 0; fi; \
mapfile -t mains < <($(GO) list -f "{{if eq .Name \"main\"}}{{.Dir}}{{end}}" ./... | sed "/^$$/d"); \
if [ $${#mains[@]} -eq 0 ]; then echo "[info] 未找到 package main，跳过卸载"; exit 0; fi; \
for d in "$${mains[@]}"; do \
  name=$$(basename "$$d"); \
  target="$$bin_dir/$$name"; \
  if [ -f "$$target" ]; then \
    echo "[go] rm $$target"; \
    rm -f "$$target"; \
  fi; \
done; \
echo "[ok] go uninstall 完成（如上已删除对应可执行文件）"
endef

define __rust_guard_cargo
if ! command -v cargo >/dev/null 2>&1; then echo "[error] 未找到 cargo"; exit 1; fi
endef

define __rust_setup_toolchain
cd "$(RUST_DIR)"; \
if [[ -f rust-toolchain.toml ]]; then rustup toolchain install stable || true; rustup override set stable || true; fi
endef

define __rust_fetch_deps
cd "$(RUST_DIR)"; \
cargo fetch; \
echo "[ok] rust setup 完成。"
endef

define __rust_build_run
cd "$(RUST_DIR)"; \
cargo build --release; \
echo "[ok] rust build 完成。"
endef

define __rust_env_clear_proxy
unset HTTP_PROXY HTTPS_PROXY NO_PROXY ALL_PROXY http_proxy https_proxy no_proxy all_proxy; \
echo "[proxy] disabled for cargo test (HTTP_PROXY/HTTPS_PROXY/NO_PROXY/ALL_PROXY 皆已清空)"
endef

define __rust_test_run
cd "$(RUST_DIR)"; \
file_pat="$${FILE:-}"; fn_pat="$${FILTER:-}"; \
if [[ -n "$$file_pat" ]]; then \
  mapfile -t bins < <(find tests -maxdepth 1 -type f -name "*.rs" 2>/dev/null | grep -E "$$file_pat" | sed "s#.*/##" | sed "s/\\.rs$$//"); \
  if [[ $${#bins[@]} -gt 0 ]]; then \
    for b in "$${bins[@]}"; do \
      args=(test --all-features $(CARGO_TEST_V) --test "$$b"); \
      [[ -n "$$fn_pat" ]] && args+=("$$fn_pat"); \
      cargo "$${args[@]}"; \
    done; \
    exit 0; \
  fi; \
  echo "[warn] FILE=$$file_pat 未匹配到 tests/*.rs，回退全量"; \
fi; \
args=(test --all-features $(CARGO_TEST_V)); \
[[ -n "$$fn_pat" ]] && args+=("$$fn_pat"); \
cargo "$${args[@]}"
endef

define __rust_bench_run
cd "$(RUST_DIR)"; \
file_pat="$${FILE:-}"; fn_pat="$${FILTER:-}"; \
if [[ -n "$$file_pat" ]]; then \
  mapfile -t bins < <(find benches -maxdepth 1 -type f -name "*.rs" 2>/dev/null | grep -E "$$file_pat" | sed "s#.*/##" | sed "s/\\.rs$$//"); \
  if [[ $${#bins[@]} -gt 0 ]]; then \
    for b in "$${bins[@]}"; do \
      args=(bench $(CARGO_TEST_V) --bench "$$b"); \
      [[ -n "$$fn_pat" ]] && args+=("$$fn_pat"); \
      cargo "$${args[@]}"; \
    done; \
    exit 0; \
  fi; \
  echo "[warn] FILE=$$file_pat 未匹配到 benches/*.rs，回退全量"; \
fi; \
args=(bench $(CARGO_TEST_V)); \
[[ -n "$$fn_pat" ]] && args+=("$$fn_pat"); \
cargo "$${args[@]}"
endef

define __rust_fmt_run
cd "$(RUST_DIR)"; \
cargo fmt --all
endef

define __rust_clippy_run
cd "$(RUST_DIR)"; \
cargo clippy --all-targets --all-features -- -D warnings
endef

define __rust_example_run
cd "$(RUST_DIR)"; \
cargo run --example contains
endef

define __rust_clean_run
cd "$(RUST_DIR)"; \
cargo clean; \
rm -rf coverage flamegraph.svg perf.data* || true; \
find . -type f -name "*.profraw" -delete || true; \
find . -type f -name "*.profdata" -delete || true; \
echo "[ok] rust clean 完成。"
endef

define __rust_install_run
cd "$(RUST_DIR)"; \
if grep -q "\[\[bin\]\]" Cargo.toml || [ -f src/main.rs ]; then \
  echo "[rust] cargo install --path . --locked --force"; \
  cargo install --path . --locked --force; \
  echo "[ok] rust install 完成（可执行文件路径见 cargo install 输出）。"; \
else \
  echo "[warn] 当前 crate 未声明二进制入口，执行 cargo build --release 代替 install"; \
  cargo build --release; \
  echo "[ok] rust 库 crate 构建完成（无可执行文件可安装）。"; \
fi
endef

define __rust_uninstall_run
cd "$(RUST_DIR)"; \
if command -v cargo >/dev/null 2>&1; then \
  echo "[rust] cargo uninstall mental1104 （如未安装则忽略）"; \
  cargo uninstall mental1104 >/dev/null 2>&1 || true; \
  echo "[ok] rust uninstall 完成（若已安装则已移除 mental1104）"; \
else \
  echo "[warn] 未找到 cargo，跳过 rust uninstall"; \
fi
endef

define __rust_coverage_run
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
  --summary-only $$FAIL_ARG
endef

define __rust_vet_run
cd "$(RUST_DIR)"; \
echo "[info] 运行 cargo clippy..."; \
cargo clippy --all-targets --all-features || true; \
echo "[ok] vet-rust 检查完成（忽略警告）。"
endef

define __rust_guard_run
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
done
endef

define __export_cpp_use_venv
if [[ -x "$(PY_VENV_PYTHON)" ]]; then \
  export VIRTUAL_ENV="$(PY_VENV)"; \
  export PATH="$(PY_VENV)/bin:$$PATH"; \
fi
endef

define __export_cpp_pybind_dir
pybind_dir="$$( $(PY_VENV_PYTHON) -m pybind11 --cmakedir 2>/dev/null || true)"; \
extra=""; \
[[ -n "$$pybind_dir" ]] && extra="-Dpybind11_DIR=$$pybind_dir";
endef

define __export_cpp_configure
cd export/cpp; \
$(CMAKE) -S . -B $(EXPORT_CPP_BUILD_DIR) -DEXPORT_BUILD_PYBIND11=ON -DPYBIND11_FINDPYTHON=ON -DCMAKE_BUILD_TYPE=$(CPP_BUILD_TYPE) $$extra;
endef

define __export_cpp_build
$(CMAKE) --build $(EXPORT_CPP_BUILD_DIR); \
echo "[ok] export/cpp built (pybind11 optional)";
endef
define __py_bench_plugins
export PYTEST_DISABLE_PLUGIN_AUTOLOAD=1; \
export PYTEST_PLUGINS=pytest_benchmark.plugin; \
if ! "$(PY_VENV_PIP)" show pytest-benchmark >/dev/null 2>&1; then \
  echo "[info] 安装 pytest-benchmark 到 venv …"; \
  "$(PY_VENV_PIP)" install pytest-benchmark; \
fi;
endef



define _git_fetch_submodules
	$(SHELL) -lc 'set -e; \
		cleanup_path(){ \
			local target="$$1"; \
			[ -z "$$target" ] && return 0; \
			if [[ -e "$$target" ]]; then \
				if command -v chflags >/dev/null 2>&1; then \
					chflags -R nouchg "$$target" >/dev/null 2>&1 || true; \
				fi; \
				chmod -R u+w "$$target" >/dev/null 2>&1 || true; \
				rm -rf "$$target" || true; \
			fi; \
		}; \
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
					cleanup_path ".git/modules/$$p"; \
					cleanup_path "$$p"; \
				done </tmp/submods.list; \
				git submodule sync --recursive; \
				if ! git submodule update --init --recursive --depth=1; then \
					echo "[git] 子模块仍无法自动恢复，请手动执行: git submodule update --init --recursive"; \
					exit 1; \
				fi; \
			fi; \
			echo "[git] 子模块就绪。"; \
		else \
			echo "[git] 未检测到 .git 或 .gitmodules，跳过子模块拉取。"; \
		fi'
endef

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
