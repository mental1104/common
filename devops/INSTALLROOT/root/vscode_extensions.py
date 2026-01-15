import json
from pathlib import Path


def main() -> None:
    src = Path("/root/vscode-extensions.json")
    data = json.loads(src.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise SystemExit("extensions.json must be a JSON object")

    common = data.get("common", {})
    if common is None:
        common = {}
    if not isinstance(common, dict):
        raise SystemExit("common must be a JSON object")

    settings = dict(common)
    for key, value in data.items():
        if key == "common":
            continue
        if not isinstance(value, dict):
            raise SystemExit(f"extension '{key}' must map to a JSON object")
        settings.update(value)

    settings_path = Path("/root/.vscode-server/data/Machine/settings.json")
    settings_path.parent.mkdir(parents=True, exist_ok=True)
    settings_path.write_text(
        json.dumps(settings, indent=2, sort_keys=False) + "\n",
        encoding="utf-8",
    )

if __name__ == "__main__":
    main()
