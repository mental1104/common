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

