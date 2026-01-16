import json
from pathlib import Path


def _strip_jsonc(text: str) -> str:
    out: list[str] = []
    i = 0
    in_string = False
    escape = False
    while i < len(text):
        ch = text[i]
        if in_string:
            out.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "\"":
                in_string = False
            i += 1
            continue

        if ch == "\"":
            in_string = True
            out.append(ch)
            i += 1
            continue

        if ch == "/" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "/":
                i += 2
                while i < len(text) and text[i] != "\n":
                    i += 1
                if i < len(text) and text[i] == "\n":
                    out.append("\n")
                    i += 1
                continue
            if nxt == "*":
                i += 2
                while i + 1 < len(text) and not (text[i] == "*" and text[i + 1] == "/"):
                    if text[i] == "\n":
                        out.append("\n")
                    i += 1
                if i + 1 < len(text):
                    i += 2
                continue

        out.append(ch)
        i += 1

    return "".join(out)


def _remove_trailing_commas(text: str) -> str:
    out: list[str] = []
    i = 0
    in_string = False
    escape = False
    while i < len(text):
        ch = text[i]
        if in_string:
            out.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "\"":
                in_string = False
            i += 1
            continue

        if ch == "\"":
            in_string = True
            out.append(ch)
            i += 1
            continue

        if ch == ",":
            j = i + 1
            while j < len(text) and text[j].isspace():
                j += 1
            if j < len(text) and text[j] in ("]", "}"):
                i += 1
                continue

        out.append(ch)
        i += 1

    return "".join(out)


def _parse_jsonc(text: str) -> dict:
    cleaned = _remove_trailing_commas(_strip_jsonc(text))
    data = json.loads(cleaned)
    if not isinstance(data, dict):
        raise SystemExit("extensions.json must be a JSON object")
    return data


def _extract_key_comments(text: str) -> tuple[dict[str, list[str]], list[str]]:
    comments: dict[str, list[str]] = {}
    header_comments: list[str] = []
    pending: list[str] = []
    stack: list[str] = []
    i = 0
    in_string = False
    escape = False
    while i < len(text):
        ch = text[i]
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "\"":
                in_string = False
            i += 1
            continue

        if ch == "/" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "/":
                j = i + 2
                while j < len(text) and text[j] != "\n":
                    j += 1
                pending.append(text[i:j])
                i = j
                continue
            if nxt == "*":
                j = i + 2
                while j + 1 < len(text) and not (text[j] == "*" and text[j + 1] == "/"):
                    j += 1
                j = j + 2 if j + 1 < len(text) else len(text)
                pending.append(text[i:j])
                i = j
                continue

        if ch == "\"":
            j = i + 1
            escape = False
            while j < len(text):
                cj = text[j]
                if escape:
                    escape = False
                elif cj == "\\":
                    escape = True
                elif cj == "\"":
                    break
                j += 1
            if j >= len(text):
                break
            key = text[i + 1 : j]
            k = j + 1
            while k < len(text) and text[k].isspace():
                k += 1
            if k < len(text) and text[k] == ":" and stack and stack[-1] == "object":
                depth = len(stack)
                if depth == 2:
                    if pending:
                        comments[key] = pending
                    pending = []
                else:
                    if pending:
                        header_comments.extend(pending)
                    pending = []
            i = j + 1
            continue

        if ch == "{":
            stack.append("object")
            i += 1
            continue
        if ch == "[":
            stack.append("array")
            i += 1
            continue
        if ch == "}" or ch == "]":
            if stack:
                stack.pop()
            if len(stack) < 2 and pending:
                header_comments.extend(pending)
                pending = []
            i += 1
            continue

        i += 1

    if pending:
        header_comments.extend(pending)
    return comments, header_comments


def _normalize_comment_lines(comment: str) -> list[str]:
    return [line.lstrip() for line in comment.splitlines()]


def _format_settings_jsonc(
    settings: dict, comments: dict[str, list[str]], header_comments: list[str]
) -> str:
    indent = "  "
    lines = ["{"]
    for comment in header_comments:
        for line in _normalize_comment_lines(comment):
            lines.append(indent + line)

    keys = list(settings.keys())
    for idx, key in enumerate(keys):
        if key in comments:
            for comment in comments[key]:
                for line in _normalize_comment_lines(comment):
                    lines.append(indent + line)
        value_json = json.dumps(settings[key], indent=2, ensure_ascii=False)
        value_lines = value_json.splitlines()
        if len(value_lines) == 1:
            line = f'{indent}"{key}": {value_lines[0]}'
            if idx != len(keys) - 1:
                line += ","
            lines.append(line)
        else:
            lines.append(f'{indent}"{key}": {value_lines[0]}')
            for vl in value_lines[1:]:
                lines.append(indent + vl)
            if idx != len(keys) - 1:
                lines[-1] = lines[-1] + ","

    lines.append("}")
    return "\n".join(lines) + "\n"


def main() -> None:
    src = Path("/root/vscode-extensions.json")
    raw = src.read_text(encoding="utf-8")
    data = _parse_jsonc(raw)
    comments, header_comments = _extract_key_comments(raw)

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
        _format_settings_jsonc(settings, comments, header_comments),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
