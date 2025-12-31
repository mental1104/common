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

build-cpp: build-cpp-debug

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

