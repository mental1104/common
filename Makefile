# Top-level Makefile for building third-party libs, googletest, and cpp code
# Usage:
#   make build            # build everything
#   make build-thirdparty # build libraries under cpp/lib
#   make build-googletest # build googletest under cpp/thirdparty/googletest
#   make build-cpp        # configure & build cpp/ (uses INSTALLROOT libs)
#   make clean            # remove build directories

SHELL := /bin/bash
TOP := $(abspath .)
INSTALLROOT_LIB := $(TOP)/cpp/lib
NUMJOBS := $(shell nproc || echo 4)

# submodules present under cpp/lib (as in .gitmodules provided)
THIRD_SUBS := rapidjson pystring cJSON DataStructure hiredis redis-plus-plus
GTEST_DIR := cpp/thirdparty/googletest
CPP_BUILD := cpp/build

.PHONY: all build git-submodules build-thirdparty build-googletest build-cpp clean $(foreach lib,$(THIRD_SUBS),build-$(lib))
all: build

# Default top-level build
build: git-submodules build-thirdparty build-googletest build-cpp
	@echo "=== All done: built thirdparty libs, googletest, and cpp ==="

# Ensure submodules are initialized (no-op if already initialized)
git-submodules:
	@git submodule update --init --recursive || true

# Build each third-party lib (common pattern)
build-thirdparty: $(foreach lib,$(THIRD_SUBS),build-$(lib))
	@echo "=== thirdparty build complete ==="

# Generic cmake-based build rule generator for most libs
define build_sub
build-$(1):
	@echo "--- Building $(1) ---"; \
	if [ -d "$(INSTALLROOT_LIB)/$(1)" ]; then \
		mkdir -p "$(INSTALLROOT_LIB)/$(1)/build"; \
		cd "$(INSTALLROOT_LIB)/$(1)/build"; \
		if [ -f "$(INSTALLROOT_LIB)/$(1)/CMakeLists.txt" ]; then \
			cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX="$(abspath $(INSTALLROOT_LIB)/$(1)/build)" || true; \
			$(MAKE) -j$(NUMJOBS) || true; \
			$(MAKE) install || true; \
		else \
			cd "$(INSTALLROOT_LIB)/$(1)"; \
			if [ -f configure ]; then ./configure --prefix="$(abspath $(INSTALLROOT_LIB)/$(1)/build)" || true; fi; \
			$(MAKE) -j$(NUMJOBS) || true; \
			mkdir -p "$(abspath $(INSTALLROOT_LIB)/$(1)/build)/lib"; \
			find . -maxdepth 2 -type f \( -name "*.so*" -o -name "*.a" \) -exec cp -u {} "$(abspath $(INSTALLROOT_LIB)/$(1)/build)/lib/" \; || true; \
		fi; \
		echo "Built $(1)."; \
	else \
		echo "Directory $(INSTALLROOT_LIB)/$(1) not found — skipping $(1)"; \
	fi
endef

# Special-case rules
build-hiredis:
	@echo "--- Building hiredis ---"; \
	if [ -d "$(INSTALLROOT_LIB)/hiredis" ]; then \
		mkdir -p "$(INSTALLROOT_LIB)/hiredis/build/lib"; \
		if [ -f "$(INSTALLROOT_LIB)/hiredis/CMakeLists.txt" ]; then \
			cd "$(INSTALLROOT_LIB)/hiredis" && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(abspath $(INSTALLROOT_LIB)/hiredis/build)" || true; \
			$(MAKE) -j$(NUMJOBS) || true; $(MAKE) install || true; \
		else \
			$(MAKE) -C "$(INSTALLROOT_LIB)/hiredis" -j$(NUMJOBS) || true; \
			find "$(INSTALLROOT_LIB)/hiredis" -maxdepth 2 -type f -name "libhiredis*.so*" -exec cp -u {} "$(abspath $(INSTALLROOT_LIB)/hiredis/build/lib)" \; || true; \
		fi; \
		echo "Built hiredis."; \
	else \
		echo "Directory $(INSTALLROOT_LIB)/hiredis not found — skipping"; \
	fi

build-redis-plus-plus:
	@echo "--- Building redis-plus-plus ---"; \
	if [ -d "$(INSTALLROOT_LIB)/redis-plus-plus" ]; then \
		mkdir -p "$(INSTALLROOT_LIB)/redis-plus-plus/build"; \
		cd "$(INSTALLROOT_LIB)/redis-plus-plus/build" && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX="$(abspath $(INSTALLROOT_LIB)/redis-plus-plus/build)" || true; \
		$(MAKE) -j$(NUMJOBS) || true; $(MAKE) install || true; \
		echo "Built redis-plus-plus."; \
	else \
		echo "Directory $(INSTALLROOT_LIB)/redis-plus-plus not found — skipping"; \
	fi

$(foreach lib,rapidjson pystring cJSON DataStructure,$(eval $(call build_sub,$(lib))))

# Build googletest
.PHONY: build-googletest
build-googletest:
	@echo "--- Building googletest ---"; \
	if [ -d "$(GTEST_DIR)" ]; then \
		mkdir -p "$(GTEST_DIR)/build"; \
		cd "$(GTEST_DIR)/build" && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_GMOCK=OFF -DCMAKE_INSTALL_PREFIX="$(abspath $(GTEST_DIR)/build)" || true; \
		$(MAKE) -j$(NUMJOBS) || true; $(MAKE) install || true; \
		echo "googletest built."; \
	else \
		echo "googletest directory not found: $(GTEST_DIR)"; \
	fi

# Build cpp project
.PHONY: build-cpp
build-cpp:
	@echo "--- Building cpp project ---"
	@mkdir -p $(CPP_BUILD)
	@echo "Collecting thirdparty build paths..."
	@PREFIX_PATHS=$$(for d in $(THIRD_SUBS); do if [ -d "$(INSTALLROOT_LIB)/$$d/build" ]; then printf "%s:" "$(abspath $(INSTALLROOT_LIB))/$$d/build"; fi; done); \
	INCLUDE_PATHS=$$(for d in $(THIRD_SUBS); do if [ -d "$(INSTALLROOT_LIB)/$$d/build/include" ]; then printf "%s:" "$(abspath $(INSTALLROOT_LIB))/$$d/build/include"; fi; done); \
	LIB_RPATH=$$(for d in $(THIRD_SUBS); do if [ -d "$(INSTALLROOT_LIB)/$$d/build/lib" ]; then printf "%s:" "$(abspath $(INSTALLROOT_LIB))/$$d/build/lib"; fi; done); \
	echo "  CMAKE_PREFIX_PATH=[$$PREFIX_PATHS]"; \
	echo "  CMAKE_INCLUDE_PATH=[$$INCLUDE_PATHS]"; \
	echo "  CMAKE_LIBRARY_PATH=[$$LIB_RPATH]"; \
	cd $(CPP_BUILD) && \
		cmake .. \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_PREFIX_PATH="$$PREFIX_PATHS:$(abspath $(INSTALLROOT_LIB))" \
		-DCMAKE_LIBRARY_PATH="$$LIB_RPATH" \
		-DCMAKE_INCLUDE_PATH="$$INCLUDE_PATHS" \
		-Dgtest_DIR="$(abspath $(GTEST_DIR)/build)" \
		-DCMAKE_INSTALL_RPATH="$$LIB_RPATH" || true
	@$(MAKE) -C $(CPP_BUILD) -j$(NUMJOBS) || true
	@echo "cpp build complete (artifacts under $(CPP_BUILD))"

.PHONY: clean
clean:
	@echo "Cleaning build directories..."
	rm -rf $(CPP_BUILD)
	for d in $(THIRD_SUBS); do rm -rf "$(INSTALLROOT_LIB)/$$d/build" || true; done
	rm -rf "$(GTEST_DIR)/build"
	@echo "Clean finished."

.PHONY: test
test: build
	@echo "Running ctest (if available)..."
	@if [ -d "$(CPP_BUILD)" ]; then cd $(CPP_BUILD) && ctest --output-on-failure || true; else echo "No build dir at $(CPP_BUILD)"; fi

# Run all tests: first build, then run tests in each submodule under cpp/lib,
# finally run top-level cpp/build tests.
.PHONY: run-tests
run-tests: build
	@echo "=== Running all tests (submodules + cpp) ==="
	@FAILS=0; \
	# 先跑子仓库测试
	for d in $(THIRD_SUBS); do \
		BUILD_DIR="$(INSTALLROOT_LIB)/$$d/build"; \
		if [ -d "$$BUILD_DIR" ]; then \
			echo ">>> Running tests in $$d ..."; \
			cd "$$BUILD_DIR"; \
			if command -v ctest >/dev/null 2>&1 && [ -f CTestTestfile.cmake ]; then \
				ctest --output-on-failure --parallel $(NUMJOBS) || FAILS=$$((FAILS+1)); \
			else \
				TESTS=$$(find . -type f -executable \( -name 'test*' -o -name '*test' -o -name '*_test' \) -print); \
				for t in $$TESTS; do \
					echo "---- Running $$d:$$t ----"; \
					"./$$t" || { echo "FAIL: $$d:$$t"; FAILS=$$((FAILS+1)); }; \
				done; \
			fi; \
		fi; \
	done; \
	# 再跑主cpp/build的测试
	if [ -d "$(CPP_BUILD)" ]; then \
		echo ">>> Running tests in cpp/build ..."; \
		cd "$(CPP_BUILD)"; \
		if command -v ctest >/dev/null 2>&1 && [ -f CTestTestfile.cmake ]; then \
			ctest --output-on-failure --parallel $(NUMJOBS) || FAILS=$$((FAILS+1)); \
		else \
			TESTS=$$(find . -type f -executable \( -name 'test*' -o -name '*test' -o -name '*_test' \) -print); \
			for t in $$TESTS; do \
				echo "---- Running cpp:$$t ----"; \
				"./$$t" || { echo "FAIL: cpp:$$t"; FAILS=$$((FAILS+1)); }; \
			done; \
		fi; \
	fi; \
	# 汇总结果
	if [ $$FAILS -ne 0 ]; then \
		echo "=== $$FAILS test group(s) failed ==="; exit 1; \
	else \
		echo "=== All tests passed ==="; \
	fi

# ---------- safer install targets ----------
PYTHON ?= python3
PIP := $(PYTHON) -m pip
VENV_DIR ?= $(HOME)/.local/venvs/mental1104

.PHONY: install install-system uninstall

install: build
	@echo ">>> Installing headers/libs and python package for current user (PREFIX=$(PREFIX))"
	@# 1) 安装头文件到单一的目标目录（避免双层 mental1104）
	@if [ -d "cpp/include/mental1104" ]; then \
	  DEST_INC="$(PREFIX)/include/mental1104"; \
	  mkdir -p "$$(dirname $$DEST_INC)"; \
	  cp -aR cpp/include/mental1104 "$$DEST_INC"; \
	else \
	  DEST_INC="$(PREFIX)/include/mental1104"; \
	  mkdir -p "$$DEST_INC"; \
	  cp -aR cpp/include/. "$$DEST_INC"; \
	fi; \
	echo "  headers copied -> $$DEST_INC"

	@# 2) 拷贝库文件（只拷贝存在的 .so/.so.* 到 PREFIX/lib）
	@mkdir -p "$(PREFIX)/lib"; \
	if [ -d "$(CPP_BUILD)" ]; then \
	  find "$(CPP_BUILD)" -type f \( -name "*.so" -o -name "*.so.*" \) -exec cp -u {} "$(PREFIX)/lib/" \; -print 2>/dev/null || true; \
	fi; \
	for d in $(THIRD_SUBS); do \
	  LIBDIR="$(INSTALLROOT_LIB)/$$d/build/lib"; \
	  if [ -d "$$LIBDIR" ]; then \
	    find "$$LIBDIR" -type f \( -name "*.so" -o -name "*.so.*" \) -exec cp -u {} "$(PREFIX)/lib/" \; -print 2>/dev/null || true; \
	  fi; \
	done; \
	echo "  libs copied -> $(PREFIX)/lib (if any)"

	@# 3) 只对我们实际写入的目标目录修改权限（绝不对 /lib 或系统根做递归）
	@echo "  fixing permissions on installed files (only on targets we wrote)"; \
	if [ -n "$$DEST_INC" ] && [ -d "$$DEST_INC" ]; then chmod -R a+rX "$$DEST_INC" || true; fi; \
	find "$(PREFIX)/lib" -maxdepth 1 -type f \( -name "*.so" -o -name "*.so.*" \) -exec chmod a+rX {} \; || true

	@# 4) 尝试刷新链接缓存（仅提示，可能需要 sudo；ldconfig 的警告通常是非致命）
	@if command -v ldconfig >/dev/null 2>&1; then \
	  echo "  running ldconfig to refresh linker cache (may need sudo)"; \
	  ldconfig || true; \
	else \
	  echo "  ldconfig not found; skip."; \
	fi

	@# 5) Python 包安装（稳健、优先 --user，失败时回退到 per-user venv）
	@echo "  - installing python package (user install preferred)"; \
	$(PYTHON) -m pip install --upgrade --force-reinstall ./python --break-system-package; \
	$(PYTHON) python/generate_init.py;

# install-system: 仅在你明确想把文件放到系统级目录时用 sudo 调用（只把最必要步骤 sudo 掉）
install-system:
	@echo ">>> System install (will use sudo for actions that require root)"
	@sudo mkdir -p $(PREFIX)/include $(PREFIX)/lib
	@# 复制头：若 cpp/include/mental1104 存在，则直接复制该目录到 /usr/local/include/
	@if [ -d "cpp/include/mental1104" ]; then \
	  sudo cp -aR cpp/include/mental1104 $(PREFIX)/include/; \
	else \
	  sudo mkdir -p $(PREFIX)/include/mental1104; \
	  sudo cp -aR cpp/include/. $(PREFIX)/include/mental1104/; \
	fi
	@# 复制库
	@for f in $(shell find $(CPP_BUILD) -type f \( -name "*.so" -o -name "*.so.*" \) 2>/dev/null); do sudo cp -u $$f $(PREFIX)/lib/; done
	@for d in $(THIRD_SUBS); do \
	  LIBDIR="$(INSTALLROOT_LIB)/$$d/build/lib"; \
	  if [ -d "$$LIBDIR" ]; then \
	    for f in $$(find "$$LIBDIR" -type f \( -name "*.so" -o -name "*.so.*" \) 2>/dev/null); do sudo cp -u $$f $(PREFIX)/lib/; done; \
	  fi; \
	done
	@echo "  running sudo ldconfig"; sudo ldconfig || true
	@echo ">>> system install finished."

uninstall:
	@echo ">>> Uninstalling (user-level best-effort)"
	@rm -rf "$(PREFIX)/include/mental1104" || true
	@for f in $(shell find "$(CPP_BUILD)" -type f \( -name "*.so" -o -name "*.so.*" \) 2>/dev/null -printf "%f\n"); do rm -f "$(PREFIX)/lib/$$f" || true; done
	@for d in $(THIRD_SUBS); do \
	  if [ -d "$(INSTALLROOT_LIB)/$$d/build/lib" ]; then \
	    for f in $$(find "$(INSTALLROOT_LIB)/$$d/build/lib" -type f \( -name "*.so" -o -name "*.so.*" \) 2>/dev/null -printf "%f\n"); do \
	      rm -f "$(PREFIX)/lib/$$f" || true; \
	    done; \
	  fi; \
	done
	@echo "  uninstalling python package from user site (best-effort)"
	@$(PYTHON) -m pip uninstall -y mental1104 || true
	@echo ">>> uninstall finished."
