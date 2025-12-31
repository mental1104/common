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

