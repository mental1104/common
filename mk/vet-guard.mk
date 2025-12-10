# =================== 入口/目标（Targets） ===================

.PHONY: vet vet-rust vet-go vet-python vet-cpp
vet:        vet-python vet-cpp vet-go vet-rust
vet-rust:   ; $(call _vet_rust)
vet-go:     ; $(call _vet_go)
vet-python: ; $(call _vet_python)
vet-cpp:    ; $(call _vet_cpp)

.PHONY: guard-rust guard-rust-mem guard-rust-race guard-rust-miri
guard-rust:        ; $(call _guard_rust)
guard-rust-mem:    ; $(MAKE) --no-print-directory guard-rust MODE=mem
guard-rust-race:   ; $(MAKE) --no-print-directory guard-rust MODE=race
guard-rust-miri:   ; $(MAKE) --no-print-directory guard-rust MODE=miri

.PHONY: guard guard-cpp guard-go guard-rust guard-python
guard-cpp:    ; $(call _guard_cpp)
guard-go:     ; $(call _guard_go)
guard-python: ; $(call _guard_python)
guard:        guard-cpp guard-go guard-rust guard-python

