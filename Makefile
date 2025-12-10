.DEFAULT_GOAL := setup

.PHONY: setup build test install uninstall clean coverage fmt bench vet guard
setup:        ; ./dev setup
build:        ; ./dev build all
test:         ; ./dev test all
install:      ; ./dev install all
uninstall:    ; ./dev uninstall all
clean:        ; ./dev clean all
coverage:     ; ./dev coverage all
fmt:          ; ./dev fmt all
bench:        ; ./dev bench all
vet:          ; ./dev vet all
guard:        ; ./dev guard all

.PHONY: setup-python build-python test-python install-python uninstall-python clean-python coverage-python fmt-python bench-python
setup-python:    ; ./dev setup-python
build-python:    ; ./dev build python
test-python:     ; ./dev test python
install-python:  ; ./dev install python
uninstall-python:; ./dev uninstall python
clean-python:    ; ./dev clean python
coverage-python: ; ./dev coverage python
fmt-python:      ; ./dev fmt python
bench-python:    ; ./dev bench python

.PHONY: setup-go build-go test-go coverage-go install-go uninstall-go clean-go fmt-go bench-go
setup-go:    ; ./dev setup-go
build-go:    ; ./dev build go
test-go:     ; ./dev test go
coverage-go: ; ./dev coverage go
install-go:  ; ./dev install go
uninstall-go:; ./dev uninstall go
clean-go:    ; ./dev clean go
fmt-go:      ; ./dev fmt go
bench-go:    ; ./dev bench go

.PHONY: git-submodules setup-cpp build-cpp build-cpp-release build-cpp-debug build-cpp-core test-cpp install-cpp uninstall-cpp clean-cpp coverage-cpp fmt-cpp bench-cpp build-export-cpp clean-export-cpp
git-submodules:    ; ./dev git-submodules
setup-cpp:         ; ./dev setup-cpp
build-cpp:         ; ./dev build cpp
build-cpp-release: ; ./dev build cpp --config Release
build-cpp-debug:   ; ./dev build cpp --config Debug
build-cpp-core:    ; ./dev build cpp
test-cpp:          ; ./dev test cpp
install-cpp:       ; ./dev install cpp
uninstall-cpp:     ; ./dev uninstall cpp
clean-cpp:         ; ./dev clean cpp
coverage-cpp:      ; ./dev coverage cpp
fmt-cpp:           ; ./dev fmt cpp
bench-cpp:         ; ./dev bench cpp
build-export-cpp:  ; ./dev build-export-cpp
clean-export-cpp:  ; ./dev clean-export-cpp

.PHONY: setup-rust build-rust test-rust bench-rust fmt-rust clippy-rust example-rust clean-rust install-rust uninstall-rust coverage-rust
setup-rust:    ; ./dev setup-rust
build-rust:    ; ./dev build rust
test-rust:     ; ./dev test rust
bench-rust:    ; ./dev bench rust
fmt-rust:      ; ./dev fmt rust
clippy-rust:   ; ./dev clippy-rust
example-rust:  ; ./dev example-rust
clean-rust:    ; ./dev clean rust
install-rust:  ; ./dev install rust
uninstall-rust:; ./dev uninstall rust
coverage-rust: ; ./dev coverage rust

.PHONY: test-v test-cpp-v test-python-v test-go-v test-rust-v bench-v bench-cpp-v bench-python-v bench-go-v bench-rust-v
test-v:        ; ./dev test all -v
test-cpp-v:    ; ./dev test cpp -v
test-python-v: ; ./dev test python -v
test-go-v:     ; ./dev test go -v
test-rust-v:   ; ./dev test rust -v
bench-v:       ; ./dev bench all -v
bench-cpp-v:   ; ./dev bench cpp -v
bench-python-v:; ./dev bench python -v
bench-go-v:    ; ./dev bench go -v
bench-rust-v:  ; ./dev bench rust -v

.PHONY: setup-docker clean-docker
setup-docker: ; ./dev setup-docker
clean-docker: ; ./dev clean-docker

.PHONY: vet-python vet-go vet-cpp vet-rust
vet-python: ; ./dev vet python
vet-go:     ; ./dev vet go
vet-cpp:    ; ./dev vet cpp
vet-rust:   ; ./dev vet rust

.PHONY: guard-python guard-go guard-cpp guard-rust guard-rust-mem guard-rust-race guard-rust-miri
guard-python:   ; ./dev guard python
guard-go:       ; ./dev guard go
guard-cpp:      ; ./dev guard cpp
guard-rust:     ; ./dev guard rust
guard-rust-mem: ; ./dev guard rust --mode mem
guard-rust-race:; ./dev guard rust --mode race
guard-rust-miri:; ./dev guard rust --mode miri

.PHONY: bench-report
bench-report: ; ./dev bench-report
