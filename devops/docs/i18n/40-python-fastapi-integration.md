# Python FastAPI Integration

- Middleware injects locale per request using pluggable resolvers (query/header/cookie/custom).
- Resolved locale stored in `contextvars` and `request.state` for handlers and background tasks.
- Business handlers call translation APIs without threading language through parameters.
