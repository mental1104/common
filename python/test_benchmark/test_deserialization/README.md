# 反序列化性能基准

该目录收纳各类反序列化（JSON/YAML/TOML 等）基准测试。当前内容：

- `test_json_parser_bench.py`：大数据集场景下比较 `load_json` 封装（支持 json / ujson / orjson 等）以及 `json.loads` 的性能。

## 运行示例

```bash
# 1) 运行全部反序列化基准
pytest python/test_benchmark/test_deserialization -q --benchmark-sort=name

# 2) 仅运行 JSON 解析基准并展示自定义列
pytest python/test_benchmark/test_deserialization/test_json_parser_bench.py \
    --benchmark-group-by=name --benchmark-columns=min,median,max --benchmark-sort=median

# 3) 运行“大型对象”场景，放大 orjson 相对优势
pytest python/test_benchmark/test_deserialization/test_json_parser_bench.py -k huge \
    --benchmark-sort=min --benchmark-min-rounds=1

# 4) 快速回归（关闭 benchmark 收集，仅验证逻辑）
pytest python/test_benchmark/test_deserialization/test_json_parser_bench.py --benchmark-disable
```

如需新增其它格式，只需在此目录添加新的 `test_*` 文件，沿用现有 `DatasetFactory` 等工具即可。
