from __future__ import annotations

from typing import Any, Dict, Mapping, Optional, Tuple, Union

from .config import ClickHouseProfile

_CONNECT_DRIVERS = ("connect", "clickhouse-connect", "native")


def resolve_clickhouse_profile(
    options: Optional[Mapping[str, Any]],
    profile: Optional[Union[ClickHouseProfile, str]],
) -> Tuple[Optional[ClickHouseProfile], Optional[str], Dict[str, Any]]:
    merged = dict(options or {})
    if profile is None:
        profile = merged.pop("profile", None)
    cluster = None
    for key in ("cluster", "on_cluster", "clickhouse_cluster"):
        if key in merged:
            value = merged.pop(key)
            cluster = str(value) if value not in (None, "") else None
            break
    if profile is None:
        return None, cluster, merged
    if not isinstance(profile, ClickHouseProfile):
        try:
            profile = ClickHouseProfile(str(profile))
        except ValueError as exc:
            raise ValueError(f"unsupported ClickHouse profile: {profile}") from exc

    if profile == ClickHouseProfile.DEFAULT:
        return profile, cluster, merged

    if profile == ClickHouseProfile.DISTRIBUTED:
        driver = str(merged.get("driver", "sqlalchemy")).lower()
        if driver in _CONNECT_DRIVERS:
            settings = dict(merged.get("settings", {}) or {})
            settings.setdefault("prefer_global_in_and_join", 1)
            settings.setdefault("distributed_product_mode", "global")
            merged["settings"] = settings
        else:
            merged.setdefault("distributed_force_global", True)
            merged.setdefault("distributed_product_mode", "global")

    driver = str(merged.get("driver", "sqlalchemy")).lower()
    if driver in _CONNECT_DRIVERS:
        settings = dict(merged.get("settings", {}) or {})
        if merged.pop("distributed_force_global", None):
            settings.setdefault("prefer_global_in_and_join", 1)
        if "distributed_product_mode" in merged:
            settings.setdefault("distributed_product_mode", merged.pop("distributed_product_mode"))
        if settings:
            merged["settings"] = settings

    return profile, cluster, merged


def apply_clickhouse_profile(
    options: Optional[Mapping[str, Any]],
    profile: Optional[Union[ClickHouseProfile, str]],
) -> Dict[str, Any]:
    _, _, merged = resolve_clickhouse_profile(options, profile)
    return merged
