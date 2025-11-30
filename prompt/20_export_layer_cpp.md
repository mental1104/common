Export layer & C++ notes
- C++ core JSON implementation in cpp/include/mental1104/json.h (cJSON/RapidJSON). export/cpp builds shared lib libexport_json.* and optional pybind11 module mental1104_export_layer_pybind.
- Python bindings in export/python/src/mental1104_export_layer/: strategies pybind11 (PyBindStrategy) and ctypes (CTypesStrategy). Default prefers pybind11 then ctypes; env overrides: EXPORT_LAYER_STRATEGY, EXPORT_LAYER_ALLOW_FALLBACK=0 to block fallback.
- _init_default logs missing backends at warning level. ctypes uses EXPORT_LAYER_CTYPE_LIB or searches export/cpp/build/libexport_json.*.
- Makefile: _configure_cpp auto-passes pybind11_DIR from python/.venv if available. Bench/test targets prepend PYTHONPATH with export/cpp/build and export/python/src to expose mental1104_export_layer.
- Current Python load_json cpp parser calls mental1104_export_layer.parse_json for validation then json.loads for data (double-parse, slower than orjson/ujson unless pybind returns objects).
