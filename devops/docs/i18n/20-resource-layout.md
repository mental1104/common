# Resource Layout

- PO source from business projects, compiled MO consumed at runtime only.
- Directory rule: `<mo_root>/<locale>/LC_MESSAGES/<domain>.mo`.
- Supports multiple domains to separate UI/errors/system messages.
- Locale fallback driven by default locale and optional supported whitelist.
