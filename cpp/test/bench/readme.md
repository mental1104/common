# MN Coroutine Pool Benchmarks

最近一次在本机跑的结果（Release 构建，固定 iterations=3）：

```text
$ ./bench_bench_mn_coroutine_pool --benchmark_min_time=200ms
Run on (20 X 3494.4 MHz CPU s)
CPU Caches:
  L1 Data 48 KiB (x10)
  L1 Instruction 32 KiB (x10)
  L2 Unified 2048 KiB (x10)
  L3 Unified 24576 KiB (x1)
Load Average: 0.60, 2.91, 2.00
--------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                      Time             CPU   Iterations
--------------------------------------------------------------------------------------------------------------------------------
BM_Pool<mental1104::MnCoroutinePool>/2/1000/2/iterations:3/repeats:1                        2.41 ms        0.092 ms            3
BM_Pool<mental1104::MnCoroutinePool>/4/2000/2/iterations:3/repeats:1                        2.39 ms        0.301 ms            3
BM_Pool<mental1104::MnCoroutinePool>/4/4000/1/iterations:3/repeats:1                        2.72 ms        0.507 ms            3
BM_Pool<mental1104::MnCoroutinePool>/4/10000/3/iterations:3/repeats:1                       7.52 ms         2.61 ms            3
BM_Pool<mental1104::MnCoroutinePool>/8/20000/3/iterations:3/repeats:1                       17.8 ms         8.92 ms            3
BM_Pool<mental1104::MnCoroutinePoolAsyncSimple>/2/1000/2/iterations:3/repeats:1             2.46 ms         1.05 ms            3
BM_Pool<mental1104::MnCoroutinePoolAsyncSimple>/4/2000/2/iterations:3/repeats:1             6.36 ms         3.89 ms            3
BM_Pool<mental1104::MnCoroutinePoolAsyncSimple>/4/4000/1/iterations:3/repeats:1             11.3 ms         7.39 ms            3
BM_Pool<mental1104::MnCoroutinePoolAsyncSimple>/4/10000/3/iterations:3/repeats:1            37.3 ms         17.3 ms            3
BM_Pool<mental1104::MnCoroutinePoolAsyncSimple>/8/20000/3/iterations:3/repeats:1            90.3 ms         58.1 ms            3
BM_Pool<mental1104::BoostMnCoroutinePool>/2/1000/2/iterations:3/repeats:1                   2.17 ms        0.156 ms            3
BM_Pool<mental1104::BoostMnCoroutinePool>/4/2000/2/iterations:3/repeats:1                   2.55 ms        0.268 ms            3
BM_Pool<mental1104::BoostMnCoroutinePool>/4/4000/1/iterations:3/repeats:1                   2.53 ms        0.556 ms            3
BM_Pool<mental1104::BoostMnCoroutinePool>/4/10000/3/iterations:3/repeats:1                  7.23 ms         3.12 ms            3
BM_Pool<mental1104::BoostMnCoroutinePool>/8/20000/3/iterations:3/repeats:1                  16.2 ms         5.86 ms            3
BM_Pool<mental1104::BoostMnCoroutinePoolAsyncSimple>/2/1000/2/iterations:3/repeats:1        1.11 ms        0.672 ms            3
BM_Pool<mental1104::BoostMnCoroutinePoolAsyncSimple>/4/2000/2/iterations:3/repeats:1        3.79 ms         2.16 ms            3
BM_Pool<mental1104::BoostMnCoroutinePoolAsyncSimple>/4/4000/1/iterations:3/repeats:1        5.59 ms         3.71 ms            3
BM_Pool<mental1104::BoostMnCoroutinePoolAsyncSimple>/4/10000/3/iterations:3/repeats:1       20.8 ms         12.1 ms            3
BM_Pool<mental1104::BoostMnCoroutinePoolAsyncSimple>/8/20000/3/iterations:3/repeats:1       75.0 ms         41.0 ms            3
```

命令说明：
- 固定 `iterations:3` / `repeats:1`，所以 `--benchmark_min_time` 仅作标记，不会延长运行时长。
- 任务模型是“原子计数 + 挂起”，主要考察调度/投递开销，业务负载越小越能放大调度差异。
