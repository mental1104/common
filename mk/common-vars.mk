
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

