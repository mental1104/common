# Makefile — 单文件、无 heredoc；公共逻辑用 define 宏封装，目标直接调用
SHELL := /bin/bash
.SILENT:
.DEFAULT_GOAL := setup

# --- sudo 开关 ---
UID      := $(shell id -u)
SUDO     := $(if $(filter 0,$(UID)),,sudo)
SUDO_MSG := $(if $(filter 0,$(UID)),,@echo "[info] 当前非root，将使用sudo执行；可能会提示输入管理员密码。")

# --- 平台检测 ---
UNAME_S   := $(shell uname -s)
# IS_UBUNTU 为 1 表示当前是 Linux 且 /etc/os-release 的 ID=ubuntu
IS_UBUNTU := $(shell sh -lc 'u=$$(uname -s); if [ "$$u" = Linux ] && [ -r /etc/os-release ]; then . /etc/os-release; [ "$$ID" = ubuntu ] && echo 1; fi')
BREAK_FLAG := $(if $(IS_UBUNTU),--break-system-packages,)

# --- 工具 ---
PYTHON ?= python3
PIP3   ?= pip3

.PHONY: setup setup-python build-python \
        test test-python \
        install install-python \
        clean clean-python \
        coverage coverage-python \
        help

# ================= 公共逻辑宏（函数化） =================
# 说明：
# - 每个宏都是可复用的“函数”，在目标内直接展开调用
# - 使用 $(SHELL) -lc '...'，内部命令用 \ 续行，避免 heredoc/转义地狱

define _setup_python
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		echo "[info] 平台: $(UNAME_S)$(if $(IS_UBUNTU), (ubuntu),)"; \
		echo "[info] $(PIP3) install python/ --upgrade $(BREAK_FLAG)"; \
		$(SUDO) $(PIP3) install python/ --upgrade $(BREAK_FLAG); \
		if [[ -f python/generate_init.py ]]; then \
			echo "[info] 执行 python/generate_init.py …"; \
			$(SUDO) $(PYTHON) python/generate_init.py; \
		else \
			echo "[info] 未找到 python/generate_init.py，跳过。"; \
		fi; \
		echo "[ok] setup 完成。"' 
endef

define _test_python
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		if ! command -v pytest >/dev/null 2>&1; then \
			echo "[warn] 未检测到 pytest；请先执行: make"; exit 1; \
		fi; \
		pytest python || { echo "[hint] 测试失败。请先执行: make"; exit 1; }'
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

# ================ 直达入口（只跑 python，不走聚合） ================
setup-python:
	$(call _setup_python)

build-python: setup-python

test-python:
	$(call _test_python)

install-python:
	$(call _install_python)

clean-python:
	$(call _clean_python)

coverage-python:
	$(call _coverage_python)

# ================ 聚合入口（主逻辑，后续可并 go/cpp） ================
# 现在仅聚合 python；未来加语言时，把依赖加上即可（如：setup: setup-python setup-go setup-cpp）
setup: setup-python
test:  test-python
install: install-python
clean: clean-python
coverage: coverage-python

help:
	@echo "可用目标："
	@echo "  setup / setup-python           安装依赖并执行 generate_init.py（python）"
	@echo "  test  / test-python            运行 pytest（目录：python）"
	@echo "  install / install-python       安装本地包（python/）"
	@echo "  clean / clean-python           清理缓存与构建产物"
	@echo "  coverage / coverage-python     覆盖率报告"
	@echo "变量：PYTHON=$(PYTHON)  PIP3=$(PIP3)"
