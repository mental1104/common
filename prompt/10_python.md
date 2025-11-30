Python layout & habits
- Main package: python/mental1104 (exposed via mental1104/__init__.py). Utilities include parse_json/load_json/dump_json and bench helpers in mental1104/utils/bench_tasks.py.
- Benchmarks live under python/test_benchmark/ (deserialization & concurrency). Make sure new benches follow test_*.py naming so make bench-python discovers them.
- Unit tests under python/test/.
- Virtualenv: python/.venv used by Makefile; commands set PATH to venv automatically. python/requirements.txt includes pybind11, ujson, orjson, pytest-benchmark, etc.
- Bench flow: make bench-python -> finds test_benchmark files, writes artifacts to artifacts/bench/python, renders plots via tools/render_bench_plots.py. FILTER narrows files (regex) when FILE not set.
- CPP-backed JSON parser: load_json supports parsers json/ujson/orjson/cpp; cpp uses export_layer bindings.
