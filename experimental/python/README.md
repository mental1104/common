# Experimental Python Hello

Minimal example that extracts `world` from `"hello world"`. Use a virtual environment to keep dependencies isolated.

```bash
python -m venv .venv
source .venv/bin/activate
pip install .[dev]
pytest
```
