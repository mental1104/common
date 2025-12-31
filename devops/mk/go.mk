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

