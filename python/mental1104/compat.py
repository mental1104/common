from __future__ import annotations

try:
    from zoneinfo import ZoneInfo as _ZoneInfo
except Exception:
    from backports.zoneinfo import ZoneInfo as _ZoneInfo

ZoneInfo = _ZoneInfo

try:
    from typing import Self as _Self
except Exception:
    from typing_extensions import Self as _Self

Self = _Self

try:
    from typing import TypeGuard as _TypeGuard
except Exception:
    from typing_extensions import TypeGuard as _TypeGuard

TypeGuard = _TypeGuard

try:
    from typing import TypeIs as _TypeIs
except Exception:
    from typing_extensions import TypeIs as _TypeIs

TypeIs = _TypeIs

__all__ = ["Self", "TypeGuard", "TypeIs", "ZoneInfo"]
