Python layout & habits
- Main package: python/mental1104 (exposed via mental1104/__init__.py). Utilities include parse_json/load_json/dump_json and bench helpers in mental1104/utils/bench_tasks.py.
- Benchmarks live under python/test_benchmark/ (deserialization & concurrency). Make sure new benches follow test_*.py naming so ./dev bench python discovers them.
- Unit tests under python/test/.
- Virtualenv: python/.venv used by dev CLI; commands set PATH to venv automatically. python/requirements.txt includes pybind11, ujson, orjson, pytest-benchmark, etc.
- Bench flow: ./dev bench python runs pytest-benchmark; use --filter/--file to narrow via pytest -k, and render plots via python/tools/render_bench_plots.py as needed.
- CPP-backed JSON parser: load_json supports parsers json/ujson/orjson/cpp; cpp uses mental1104_export_layer bindings.
