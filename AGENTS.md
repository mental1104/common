# AGENTS

This file captures recent work plus C++ portability guidance across compilers, languages, and platforms.

## Recent Work (context)
- C++11/14 compatibility fixes: JSON now uses `mental1104::string_view` and a variant wrapper (std::variant in C++17+, boost::variant2 fallback). Avoided C++17-only constructs in headers.
- Redis++ ABI alignment: `M1104_REDISPP_CXX_STANDARD` is injected from CMake (`cpp/cmake/deps.cmake`) and used in `redis_lock.h` to select the right redis++ namespace/ABI helpers.
- C++11 fix in bloom filter: function templates use trailing return types instead of C++14 auto-deduction.
- CI now runs coverage steps instead of separate test steps in GitHub Actions (tests are included in coverage runs). `test-redispp` remains a standalone test step.
- Boost sparse checkout includes `mp11` and `variant2` to support the C++11/14 JSON fallback.

## C++ Portability Guidance

### Cross-standard (C++11/14/17/20/23)
- Use feature macros from `cpp/include/mental1104/meta/compiler_support.h`:
  `M1104_HAS_CXX11/14/17/20/23`, `M1104_HAS_INCLUDE`, `M1104_HAS_STRING_VIEW`.
- Prefer `mental1104::string_view` in public APIs; it aliases to `std::string_view` on C++17+ and to `std::string` on C++11/14.
- Avoid C++17-only features (e.g., `std::variant`, `std::optional`, inline variables, `if constexpr`) unless guarded with the macros above.
- For template return types in C++11, use trailing return types with `decltype(...)`.

### Cross-compiler (GCC/Clang/MSVC)
- MSVC: rely on `M1104_CPLUSPLUS` (uses `_MSVC_LANG`) instead of raw `__cplusplus`.
- GCC/Clang: keep warnings clean under `-Wall -Wextra` to avoid CI failures.
- Avoid compiler-specific extensions unless behind a compile-time check.

### Cross-platform (Linux/macOS/Windows)
- macOS: Homebrew LLVM clang should use libc++ (include and lib paths), avoid mixing libc++/libstdc++.
- Windows: dynamic CRT is enforced in CMake (`CMAKE_MSVC_RUNTIME_LIBRARY`); avoid POSIX-only APIs or guard with `#ifdef _WIN32`.
- Linux: build under both GCC and Clang; avoid UB and rely on feature macros instead of compiler detection.

### Cross-language / ABI
- C++ is consumed by Python via `export/cpp` (C API + pybind). Keep `extern "C"` APIs stable and POD-only.
- Do not throw exceptions across C ABI boundaries; return errors via result structs/strings.
- Avoid exposing STL types in C APIs; keep ownership/allocator rules explicit.

### Warnings-as-Errors Culture
- Several third-party components and CI configurations treat warnings as errors (`-Werror` or `/WX`), so new warnings can fail builds.
- Code should be warning-clean across GCC/Clang/MSVC (unused params, narrowing, sign conversions, missing initializers, etc.).
