from __future__ import annotations

from fastapi import Request

from mental1104.utils.context import RequestCtx


def request_ctx_from_headers(request: Request) -> RequestCtx:
    """Build RequestCtx from HTTP headers: lang -> language, timezone -> time_zone."""

    base = RequestCtx()
    language = request.headers.get("lang") or base.language
    time_zone = request.headers.get("timezone") or base.time_zone
    return RequestCtx(language=language, time_zone=time_zone)
