Export layer: parse_json (C++ → Python)
======================================

This note captures the end-to-end steps we used to get the RapidJSON-based C++ parser callable from Python via the mental1104_export_layer, plus the observed performance after wiring pybind11 to return Python objects directly.

Environment & build
-------------------
- Python venv: `python/.venv` (created by `./dev setup-python`) installs pybind11, pytest-benchmark, ujson, orjson, etc.
- CMake configuration:
  - `./dev build-export-cpp` enables `-DPYBIND11_FINDPYTHON=ON` and injects `pybind11_DIR` from the venv (`python -m pybind11 --cmakedir`) so CMake finds pybind11.
  - Build type comes from `./dev build-export-cpp --config ...` (default Debug); use Release to avoid Debug slowness.
- Python side logging: `mental1104_export_layer/__init__.py` sets up basicConfig if no handlers, default INFO (overridable via `EXPORT_LAYER_LOG_LEVEL`).
- Strategy selection: prefers pybind11, then ctypes; ENV overrides:
  - `EXPORT_LAYER_STRATEGY=pybind11|ctypes`
  - `EXPORT_LAYER_ALLOW_FALLBACK=0` to forbid fallback.

Binding behavior
----------------
- Pybind module (`mental1104_export_layer_pybind`) now exposes:
  - `parse_json`: legacy (ok, error, offset).
  - `parse_json_value`: RapidJSON DOM → Python object (dict/list/str/bool/int/float/None) and returns `(ok, value, error, offset)`.
- Python `JsonUtil` registers the cpp parser only if an mental1104_export_layer backend is available. It first tries pybind11; ctypes remains a fallback (value=None).
- `load_json(..., parser=JsonParserType.CPP)` now consumes the 4-tuple; if `value` is present (pybind11 path), it returns that directly without a second `json.loads`.

Repro steps
-----------
1) Install Python deps in venv (includes pybind11):
   - `./dev setup-python`  # requires network / PyPI mirror configured
2) Build export/cpp with pybind11 module:
   - `./dev clean-export-cpp`
   - `./dev build-export-cpp --config Release`
3) Run Python tests with DEBUG logs for mental1104_export_layer:
   - `./dev test python --filter json`
   - Logs will show chosen backend (pybind11 expected if mental1104_export_layer_pybind built).
4) Run benchmarks:
   - `./dev bench python --filter json_parser_bench`
   - Artifacts under `artifacts/bench/python/...` and plots under `artifacts/bench/python/plots/`.

Performance snapshot (Release build)
------------------------------------
Pytest-benchmark means (ms). Lower is better.

- load_json_from_str:
  - cpp (RapidJSON→PyObject): 658 ms
  - orjson: 573 ms
  - ujson: 590 ms
  - stdlib json: 812 ms
- load_json_from_binary_stream:
  - cpp: 694 ms
  - orjson: 558 ms
  - ujson: 558 ms
  - stdlib json: 844 ms
- load_json_from_text_stream:
  - cpp: 1,029 ms
  - orjson: 915 ms
  - ujson: 985 ms
  - stdlib json: 1,171 ms
- load_json_huge_binary_stream:
  - cpp: 2,007 ms
  - orjson: 1,677 ms
  - ujson: 1,446 ms
  - stdlib json: 2,417 ms

Conclusion
----------
- Enabling pybind11 with RapidJSON and returning Python objects eliminated the double-parse and moved cpp ahead of stdlib json in all measured cases.
- orjson/ujson remain faster; the remaining gap comes from converting RapidJSON DOM to Python objects node-by-node. Further speedups would require a more direct PyObject construction (e.g., SAX-style conversion or tighter allocation strategy).
