from types import SimpleNamespace

from mental1104.db.schema import (
    _restore_clickhouse_cluster_options,
    _set_clickhouse_cluster_options,
)


def test_clickhouse_cluster_option_defaults_to_none_when_absent():
    table = SimpleNamespace(dialect_options={"clickhouse": {}})

    applied = _set_clickhouse_cluster_options([table], None)
    assert table.dialect_options["clickhouse"]["cluster"] is None

    _restore_clickhouse_cluster_options(applied)
    assert table.dialect_options["clickhouse"]["cluster"] is None


def test_clickhouse_cluster_option_restores_previous_value():
    table = SimpleNamespace(dialect_options={"clickhouse": {"cluster": "old_cluster"}})

    applied = _set_clickhouse_cluster_options([table], "new_cluster")
    assert table.dialect_options["clickhouse"]["cluster"] == "new_cluster"

    _restore_clickhouse_cluster_options(applied)
    assert table.dialect_options["clickhouse"]["cluster"] == "old_cluster"
