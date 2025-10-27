# Makefile — sudo-aware, 简化且可扩展（Ubuntu 才加 --break-system-packages）
SHELL := /bin/bash
.SILENT:

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

.DEFAULT_GOAL := setup
.PHONY: setup test install help

setup: ## 默认：安装依赖并执行自定义步骤
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
	py_install(){ \
		echo "[info] 平台: $(UNAME_S)$${IS_UBUNTU:+ (ubuntu)}"; \
		echo "[info] pip install python/ --upgrade $(BREAK_FLAG)"; \
		$(SUDO) $(PIP3) install python/ --upgrade $(BREAK_FLAG); \
	}; \
	py_run_init(){ \
		if [[ -f python/generate_init.py ]]; then \
			echo "[info] 执行 python/generate_init.py …"; \
			$(SUDO) $(PYTHON) python/generate_init.py; \
		else \
			echo "[info] 未找到 python/generate_init.py，跳过。"; \
		fi; \
	}; \
	py_install; \
	py_run_init; \
	echo "[ok] setup 完成。"'

test:  ## 运行测试；若失败提示先 make
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
	if ! command -v pytest >/dev/null 2>&1; then echo "[warn] 未检测到 pytest；请先执行: make"; exit 1; fi; \
	pytest python || { echo "[hint] 测试失败。请先执行: make"; exit 1; }'

install: ## 安装本地包（非editable）
	$(SUDO_MSG)
	$(SHELL) -lc 'set -e; \
		echo "[info] pip install python/ --upgrade $(BREAK_FLAG)"; \
		$(SUDO) $(PIP3) install python/ --upgrade $(BREAK_FLAG); \
		echo "[ok] 安装完成。"'

help:   ## 帮助
	echo "可用目标："
	echo "  make / setup   安装依赖并执行generate_init.py（python）"
	echo "  test           运行pytest（目录：python）"
	echo "  install        pip3安装本地包（python/）"
