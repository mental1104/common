# Checklist

- [ ] MO files placed under `<locale>/LC_MESSAGES/<domain>.mo`.
- [ ] Default locale configured and supported whitelist aligns with business plan.
- [ ] CI runs `compile` and `check` tools; runtime never touches PO.
- [ ] FastAPI middleware registered with resolvers in priority order.
