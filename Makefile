# Makefile for project root
# Default behaviour:
#   make install
# will build C++ code in ./cpp (uses cmake) and install shared libraries and headers
# and will install the Python package under ./python using pip.
#
# Customization:
#   PREFIX=/opt/common    install prefix (default)
#   PY_SYS=yes            install python package into system site-packages (uses sudo)
# Examples:
#   make install
#   sudo make install PREFIX=/usr/local PY_SYS=yes

PREFIX ?= /opt/common
CPPDIR ?= cpp
PYDIR ?= python
BUILD_DIR ?= $(CPPDIR)/build
PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
CMAKE ?= cmake
MAKECMD ?= $(MAKE)

.PHONY: all build cpp_build cpp_install python_install install clean help
all: build

help:
	@echo "Usage: make [target] [VARIABLE=value]"
	@echo
	@echo "Targets:"
	@echo "  build           - build both C++ and (no-op) python build step"
	@echo "  install         - build and install C++ libs+headers and Python package"
	@echo "  cpp_build       - run cmake && make in $(CPPDIR)"
	@echo "  cpp_install     - attempts 'make install' then falls back to manual copy"
	@echo "  python_install  - install python package (uses pip)." 
	@echo "  clean           - clean generated build artifacts"
	@echo
	@echo "Variables you can set: PREFIX (default: $(PREFIX)), PY_SYS=yes to pip install system-wide"
	@echo "Examples: sudo make install PREFIX=/usr/local PY_SYS=yes"

build: cpp_build
	@echo "(python package will be installed during 'make install')"

# Build C++ using cmake (in-tree build directory)
cpp_build:
	@echo "==> Preparing build dir: $(BUILD_DIR)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) ..
	@$(MAKECMD) -C $(BUILD_DIR)
	@echo "==> C++ build done"

# Install C++: try standard 'make install' (requires CMakeLists install rules).
# If that fails, fallback to copying produced .so files and headers.
cpp_install: cpp_build
	@echo "==> Installing C++ artifacts to $(PREFIX)"
	@cd $(BUILD_DIR) && $(MAKECMD) install 2>/dev/null || ( \
		echo "No 'install' target found in build. Falling back to manual installation..."; \
		mkdir -p $(PREFIX)/lib; mkdir -p $(PREFIX)/include; \
		find $(BUILD_DIR) -type f -name '*.so' -exec cp -v {} $(PREFIX)/lib/ \; ; \
		if [ -d $(CPPDIR)/include ]; then cp -av $(CPPDIR)/include/* $(PREFIX)/include/; fi; \
		echo "Manual copy done: libs -> $(PREFIX)/lib , headers -> $(PREFIX)/include"; \
	)
	@echo "==> C++ install complete"


python_install:
	@echo "==> Checking python package directory: $(PYDIR)"
	@if [ ! -d "$(PYDIR)" ]; then echo "Error: Python directory '$(PYDIR)' not found"; exit 1; fi
	@if [ ! -f "$(PYDIR)/pyproject.toml" -a ! -f "$(PYDIR)/setup.py" ]; then \
		echo "Warning: no pyproject.toml or setup.py found in $(PYDIR). pip may fail to install."; \
	fi

	@echo "==> Installing Python package into system site-packages (may require sudo)"
	@sudo $(PYTHON) -m pip3 install --upgrade ./$(PYDIR) || ( \
		echo ""; \
		echo "ERROR: pip3 install failed. Possible network/mirror issue."; \
		echo ""; \
		echo "Try configuring a pip mirror source:"; \
		echo "  * Temporary (single command):"; \
		echo "      pip3 install -i https://mirror.company/simple ./$(PYDIR)"; \
		echo "  * Permanent (per-user config):"; \
		echo "      pip3 config set global.index-url https://mirror.company/simple"; \
		echo "  * You may also need:"; \
		echo "      pip3 config set global.trusted-host mirror.company"; \
		echo ""; \
		echo "For air-gapped installs, build wheels on a machine with Internet:"; \
		echo "  pip3 wheel -w wheelhouse ./$(PYDIR)"; \
		echo "  pip3 install --no-index --find-links=wheelhouse <package>"; \
		echo ""; \
		exit 1; \
	)

	@echo "==> Python install complete"


install: cpp_install python_install
	@echo
	@echo "Installation finished. Installed under: $(PREFIX)"
	@echo "Notes:"
	@echo "  * If you installed to a system prefix (e.g. /usr/local) you may need to run: sudo ldconfig"
	@echo "  * If you used a non-standard prefix (like /opt/common):"
	@echo "      - add '$(PREFIX)/lib' to /etc/ld.so.conf.d/ or set LD_LIBRARY_PATH to include it"
	@echo "      - for Python, pip --prefix installs to $(PREFIX)/lib/pythonX.Y/site-packages; add that to PYTHONPATH or use 'pip install' system-wide."

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -rf $(PYDIR)/build $(PYDIR)/dist $(PYDIR)/*.egg-info
	@find $(CPPDIR) -name '*.o' -delete || true
	@echo "Clean done"

# ------------------ Testing targets ------------------
.PHONY: test test_cpp test_py
# Run all tests (C++ + Python)
test: test_cpp test_py

# Run C++ tests: ensure build then run ctest (requires CMake's enable_testing)
test_cpp: cpp_build
	@echo "==> Running C++ tests (ctest) in $(BUILD_DIR)..."
	@if [ ! -d "$(BUILD_DIR)" ]; then echo "Error: build directory $(BUILD_DIR) not found"; exit 1; fi
	@cd $(BUILD_DIR) && ctest --output-on-failure || (echo "ERROR: C++ tests failed"; exit 1)
	@echo "==> C++ tests finished"

# Run Python tests using pytest. By default runs tests under $(PYDIR)/test.
# You can pass additional pytest args: make test_py PYTEST_ARGS='-k something -q'
PYTEST_ARGS ?=
test_py:
	@echo "==> Running Python tests (pytest) in $(PYDIR)/test..."
	@if [ ! -d "$(PYDIR)/test" ]; then echo "No Python tests found at $(PYDIR)/test - skipping"; exit 0; fi
	@command -v $(PYTHON) >/dev/null 2>&1 || (echo "Error: $(PYTHON) not found"; exit 1)
	@PYTHONPATH=$(PYDIR) $(PYTHON) -m pytest $(PYTEST_ARGS) $(PYDIR)/test || (echo "ERROR: Python tests failed"; exit 1)
	@echo "==> Python tests finished"

# ------------------ end testing ------------------


# Simple uninstall hints (best-effort). Uninstalling must often be done manually.
.PHONY: uninstall
uninstall:
	@echo "Uninstall is not fully automated. Hints:"
	@echo "  * If you used 'make install' with a CMake install target, use your build system's 'make uninstall' if available (not always provided)."
	@echo "  * To remove C++ artefacts manually: rm -f $(PREFIX)/lib/*.so; rm -rf $(PREFIX)/include/*"
	@echo "  * To uninstall python package from system: sudo python3 -m pip uninstall <package-name>"
	@echo "  * To remove python installed under prefix: delete $(PREFIX)/lib/python*/site-packages/<package>"
	@exit 0
