# Coroutine Pool Benchmarks

该目录将原 `test_coroutine_pool_bench.py` 拆分为多份更易理解的并发/协程池性能用例：

- `test_throughput.py`：A~F 场景，覆盖并发缩放、任务规模变化、零延迟极限、CPU 污染、超大并发与冷启动。
- `test_mixed_workloads.py`：G 场景，I/O 与 CPU 交错的混合负载。
- `test_async_heavy.py`：H 场景，CPU 重载并在前后插入额外异步 I/O。
- `test_blocking_io.py`：I 场景，对比异步池与线程/进程池处理同步阻塞 I/O 的差异。

## 执行方式

所有基准默认都依赖 `pytest-benchmark`，推荐的运行示例：

```bash
# 1) 运行全部并发基准
pytest python/test_benchmark/test_concurrency -q --benchmark-group-by=param:scenario --benchmark-sort=name

# 2) 仅关注混合负载（G）
pytest python/test_benchmark/test_concurrency/test_mixed_workloads.py -q --benchmark-group-by=param:scenario --benchmark-sort=name

# 3) 仅验证阻塞 I/O 场景并关闭 benchmark 统计（快速烟测）
pytest python/test_benchmark/test_concurrency/test_blocking_io.py --benchmark-disable
```

可根据需要附加 `--benchmark-name=short`、`--benchmark-columns=min,median,max` 等参数来获得更紧凑或更详尽的统计。
