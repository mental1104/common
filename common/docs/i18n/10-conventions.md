# Conventions

- Keep runtime lean: never parse/compile PO in runtime, only load MO bytes.
- Locale normalized to base language (e.g., `zh-CN` -> `zh`), with configurable defaults and whitelist.
- Domains split by concern (e.g., `ui`, `errors`) under `LC_MESSAGES`.
- Contextvars provide per-request locale without pushing language arguments through business code.
