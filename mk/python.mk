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

