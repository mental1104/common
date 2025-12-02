# Redis Bloom Filter Benchmarks (miss-heavy workloads)

This document summarizes the benchmark design, parameters, runs, and conclusions for comparing Redis Bloom–guarded lookups vs plain GET across different execution modes.

## What we measured
- Workload: miss-heavy GET access where most keys do not exist in Redis.
- Goal: quantify how Bloom filtering reduces actual GETs and how that affects latency/throughput and server metrics.
- Modes:
  - Single-command (no pipeline): `redis_bloom_miss`
  - Pipelined batch: `redis_bloom_pipeline`
  - Multithreaded concurrent (ThreadPool): `redis_bloom_concurrent`

## Test setup
- Code: `test_redis_bloom_bench.py`
- Key parameters (env-tunable):
  - `BLOOM_BENCH_QUERY_COUNT` (default 500000)
  - `BLOOM_BENCH_EXISTING_COUNT` (default 100000)
  - `BLOOM_BENCH_MISS_RATIO` (default 0.999)
  - `BLOOM_BENCH_CAPACITY` (default 1000000)
  - `BLOOM_BENCH_ERROR_RATE` (default 1e-6)
  - `BLOOM_BENCH_BATCH_SIZE` (default 500, for pipeline mode)
  - `BLOOM_BENCH_WORKERS` (default 20, for concurrent mode)
- Bloom uses `RedisBloom` with low error rate to minimize false positives.
- Redis RTT observed via `redis-cli --latency`: ~0.11 ms (single host WSL2).

## How to run
From `common/python`:
```
make bench-python FILE=test_redis_bloom_bench.py
```
Example with heavier load and concurrency:
```
BLOOM_BENCH_QUERY_COUNT=1000000 \
BLOOM_BENCH_MISS_RATIO=0.9995 \
BLOOM_BENCH_BATCH_SIZE=1000 \
BLOOM_BENCH_WORKERS=50 \
make bench-python FILE=test_redis_bloom_bench.py
```
Artifacts: `common/artifacts/bench/python/test_benchmark__test_redis_bloom_bench.json` (+ plots).

## Observed results (key run)
Parameters: 500k queries, 100k existing keys, miss_ratio=0.999, capacity=1,000,000, error_rate=1e-6, batch_size=500, workers=20.

### Single-command (redis_bloom_miss)
- `plain_get`: ~29.84s, QPS ~16,755; misses=1,498,500; total_cmd ~1,500,001; net_in ~71 MB; net_out ~7.5 MB.
- `bloom_guard`: ~29.05s, QPS ~17,212; skipped_gets=499,500 (skip_pct=99.9%); hits reported via BF.EXISTS 1,501,500; total_cmd ~1,501,501; net_in ~106 MB; net_out ~6.0 MB.
- Interpretation: Bloom blocks almost all miss GETs, but per-request BF.EXISTS adds a round trip; latency difference is small in low-RTT environment.

### Pipeline (redis_bloom_pipeline, batch=500)
- `plain_get_pipeline`: ~1.81s, QPS ~276,950.
- `bloom_guard_pipeline`: ~1.80s, QPS ~277,640; skipped_gets=499,500 (99.9%).
- Interpretation: batching amortizes BF.EXISTS/GET so timings converge; Bloom still avoids GETs (reduced net_out, different cmd mix).

### Concurrent threads (redis_bloom_concurrent, workers=20)
- `plain_get_concurrent`: ~48.60s, QPS ~10,287.
- `bloom_guard_concurrent`: ~47.49s, QPS ~10,528; skipped_gets=499,500 (99.9%).
- Interpretation: even under thread contention, Bloom’s time is close to plain GET because BF.EXISTS is also a command; benefit is mainly fewer actual GETs/higher skip_pct.

## Conclusions
- In a low-RTT single-host Redis, Bloom’s main value is reducing real GETs and preventing miss traffic (skip_pct ~99.9% in miss-heavy load). Latency/throughput gains are small because BF.EXISTS adds its own command cost.
- Pipeline makes both paths equally fast; Bloom still trims GET volume (check `skipped_gets/skip_pct` and command deltas).
- Concurrency shows similar behavior: Bloom keeps skip_pct high but total time remains close to plain GET.
- Bloom is most useful when:
  - Miss traffic is high (prevent cache penetration/back-end hits).
  - Network/RTT is higher, or downstream cost (DB, storage) is significant.
  - You can batch requests or combine with local Bloom to cut round trips.

## What to look at in results
- `skip_pct` / `skipped_gets`: how many GETs were avoided.
- `stats_delta`:
  - `cmd_get` vs `cmd_bf.exists` (command mix)
  - `net_in` / `net_out` (request/response bytes)
  - `hits`/`misses` counters
- `elapsed_s` / `qps` for overall throughput; `batch_size` / `workers` to understand mode.

## Notes / next steps
- For clearer performance gaps, test against higher RTT or remote Redis, increase concurrency, or push larger batches.
- Consider pipelined Bloom (as implemented) or local Bloom to avoid extra network commands.
- Bloom helps as a “existence filter” to stop cache穿透; it is not an extra value cache layer.


```json
{
    "machine_info": {
        "node": "Blue-Espeon",
        "processor": "x86_64",
        "machine": "x86_64",
        "python_compiler": "GCC 13.3.0",
        "python_implementation": "CPython",
        "python_implementation_version": "3.12.3",
        "python_version": "3.12.3",
        "python_build": [
            "main",
            "Nov  6 2025 13:44:16"
        ],
        "release": "6.6.87.2-microsoft-standard-WSL2",
        "system": "Linux",
        "cpu": {
            "python_version": "3.12.3.final.0 (64 bit)",
            "cpuinfo_version": [
                9,
                0,
                0
            ],
            "cpuinfo_version_string": "9.0.0",
            "arch": "X86_64",
            "bits": 64,
            "count": 20,
            "arch_string_raw": "x86_64",
            "vendor_id_raw": "GenuineIntel",
            "brand_raw": "13th Gen Intel(R) Core(TM) i5-13600KF",
            "hz_advertised_friendly": "3.4944 GHz",
            "hz_actual_friendly": "3.4944 GHz",
            "hz_advertised": [
                3494400000,
                0
            ],
            "hz_actual": [
                3494400000,
                0
            ],
            "stepping": 1,
            "model": 183,
            "family": 6,
            "flags": [
                "3dnowprefetch",
                "abm",
                "adx",
                "aes",
                "apic",
                "arch_capabilities",
                "avx",
                "avx2",
                "avx_vnni",
                "bmi1",
                "bmi2",
                "clflush",
                "clflushopt",
                "clwb",
                "cmov",
                "constant_tsc",
                "cpuid",
                "cx16",
                "cx8",
                "de",
                "ept",
                "ept_ad",
                "erms",
                "f16c",
                "flush_l1d",
                "fma",
                "fpu",
                "fsgsbase",
                "fsrm",
                "fxsr",
                "gfni",
                "ht",
                "hypervisor",
                "ibpb",
                "ibrs",
                "ibrs_enhanced",
                "invpcid",
                "lahf_lm",
                "lm",
                "mca",
                "mce",
                "md_clear",
                "mmx",
                "movbe",
                "movdir64b",
                "movdiri",
                "msr",
                "mtrr",
                "nonstop_tsc",
                "nopl",
                "nx",
                "osxsave",
                "pae",
                "pat",
                "pcid",
                "pclmulqdq",
                "pdpe1gb",
                "pge",
                "pni",
                "popcnt",
                "pse",
                "pse36",
                "rdpid",
                "rdrand",
                "rdrnd",
                "rdseed",
                "rdtscp",
                "rep_good",
                "sep",
                "serialize",
                "sha",
                "sha_ni",
                "smap",
                "smep",
                "ss",
                "ssbd",
                "sse",
                "sse2",
                "sse4_1",
                "sse4_2",
                "ssse3",
                "stibp",
                "syscall",
                "tpr_shadow",
                "tsc",
                "tsc_adjust",
                "tsc_deadline_timer",
                "tsc_known_freq",
                "tsc_reliable",
                "tscdeadline",
                "umip",
                "vaes",
                "vme",
                "vmx",
                "vnmi",
                "vpclmulqdq",
                "vpid",
                "waitpkg",
                "x2apic",
                "xgetbv1",
                "xsave",
                "xsavec",
                "xsaveopt",
                "xsaves",
                "xtopology"
            ],
            "l3_cache_size": 25165824,
            "l2_cache_size": 20971520,
            "l1_data_cache_size": 491520,
            "l1_instruction_cache_size": 327680,
            "l2_cache_line_size": 2048,
            "l2_cache_associativity": 7
        }
    },
    "commit_info": {
        "id": "c7087eb576c8337c4dc8aa378a53ba27e64efe2c",
        "time": "2025-12-01T20:02:30+08:00",
        "author_time": "2025-12-01T20:02:30+08:00",
        "dirty": true,
        "project": "common",
        "branch": "main"
    },
    "benchmarks": [
        {
            "group": "redis_bloom_miss",
            "name": "test_bloom_vs_plain[plain_get]",
            "fullname": "test_benchmark/test_redis_bloom_bench.py::TestRedisBloomPerformance::test_bloom_vs_plain[plain_get]",
            "params": {
                "path": "plain_get"
            },
            "param": "plain_get",
            "extra_info": {
                "workload_total": 500000,
                "miss_ratio": 0.999,
                "error_rate": 1e-06,
                "capacity": 1000000,
                "existing_count": 100000,
                "query_count": 500000,
                "seed": 42,
                "path": "plain_get",
                "elapsed_s": 29.841326871001,
                "qps": 16755,
                "stats_delta": {
                    "hits": 1500,
                    "misses": 1498500,
                    "total_cmd": 1500001,
                    "net_in": 71666516,
                    "net_out": 7517445,
                    "mem_used": 0
                }
            },
            "options": {
                "disable_gc": false,
                "timer": "perf_counter",
                "min_rounds": 5,
                "max_time": 1.0,
                "min_time": 5e-06,
                "warmup": false
            },
            "stats": {
                "min": 29.841328598999098,
                "max": 30.774366862000534,
                "mean": 30.168126735666494,
                "stddev": 0.5255530127854112,
                "rounds": 3,
                "median": 29.888684745999853,
                "iqr": 0.6997786972510767,
                "q1": 29.853167635749287,
                "q3": 30.552946333000364,
                "iqr_outliers": 0,
                "stddev_outliers": 1,
                "outliers": "1;0",
                "ld15iqr": 29.841328598999098,
                "hd15iqr": 30.774366862000534,
                "ops": 0.033147566925915306,
                "total": 90.50438020699949,
                "data": [
                    29.888684745999853,
                    30.774366862000534,
                    29.841328598999098
                ],
                "iterations": 1
            }
        },
        {
            "group": "redis_bloom_miss",
            "name": "test_bloom_vs_plain[bloom_guard]",
            "fullname": "test_benchmark/test_redis_bloom_bench.py::TestRedisBloomPerformance::test_bloom_vs_plain[bloom_guard]",
            "params": {
                "path": "bloom_guard"
            },
            "param": "bloom_guard",
            "extra_info": {
                "workload_total": 500000,
                "miss_ratio": 0.999,
                "error_rate": 1e-06,
                "capacity": 1000000,
                "existing_count": 100000,
                "query_count": 500000,
                "seed": 42,
                "path": "bloom_guard",
                "elapsed_s": 29.04849047099924,
                "qps": 17212,
                "skipped_gets": 499500,
                "skip_pct": 0.999,
                "stats_delta": {
                    "hits": 1501500,
                    "misses": 0,
                    "total_cmd": 1501501,
                    "net_in": 106238339,
                    "net_out": 6024946,
                    "mem_used": 0
                }
            },
            "options": {
                "disable_gc": false,
                "timer": "perf_counter",
                "min_rounds": 5,
                "max_time": 1.0,
                "min_time": 5e-06,
                "warmup": false
            },
            "stats": {
                "min": 29.048493280999537,
                "max": 30.52315644400005,
                "mean": 29.59773058433287,
                "stddev": 0.806099394315043,
                "rounds": 3,
                "median": 29.22154202799902,
                "iqr": 1.1059973722503855,
                "q1": 29.091755467749408,
                "q3": 30.197752839999794,
                "iqr_outliers": 0,
                "stddev_outliers": 1,
                "outliers": "1;0",
                "ld15iqr": 29.048493280999537,
                "hd15iqr": 30.52315644400005,
                "ops": 0.03378637416644827,
                "total": 88.79319175299861,
                "data": [
                    29.22154202799902,
                    30.52315644400005,
                    29.048493280999537
                ],
                "iterations": 1
            }
        },
        {
            "group": "redis_bloom_pipeline",
            "name": "test_bloom_pipeline[plain_get_pipeline]",
            "fullname": "test_benchmark/test_redis_bloom_bench.py::TestRedisBloomPipeline::test_bloom_pipeline[plain_get_pipeline]",
            "params": {
                "path": "plain_get_pipeline"
            },
            "param": "plain_get_pipeline",
            "extra_info": {
                "workload_total": 500000,
                "miss_ratio": 0.999,
                "error_rate": 1e-06,
                "capacity": 1000000,
                "existing_count": 100000,
                "query_count": 500000,
                "seed": 42,
                "batch_size": 500,
                "path": "plain_get_pipeline",
                "elapsed_s": 1.8053780359987286,
                "qps": 276950,
                "stats_delta": {
                    "hits": 1500,
                    "misses": 1498500,
                    "total_cmd": 1506001,
                    "net_in": 71753516,
                    "net_out": 21050447,
                    "mem_used": 7168
                }
            },
            "options": {
                "disable_gc": false,
                "timer": "perf_counter",
                "min_rounds": 5,
                "max_time": 1.0,
                "min_time": 5e-06,
                "warmup": false
            },
            "stats": {
                "min": 1.8053813650003576,
                "max": 1.8237661229995865,
                "mean": 1.8126220700002402,
                "stddev": 0.009794229212972372,
                "rounds": 3,
                "median": 1.8087187220007763,
                "iqr": 0.013788568499421672,
                "q1": 1.8062157042504623,
                "q3": 1.820004272749884,
                "iqr_outliers": 0,
                "stddev_outliers": 1,
                "outliers": "1;0",
                "ld15iqr": 1.8053813650003576,
                "hd15iqr": 1.8237661229995865,
                "ops": 0.5516869823834085,
                "total": 5.4378662100007205,
                "data": [
                    1.8237661229995865,
                    1.8087187220007763,
                    1.8053813650003576
                ],
                "iterations": 1
            }
        },
        {
            "group": "redis_bloom_pipeline",
            "name": "test_bloom_pipeline[bloom_guard_pipeline]",
            "fullname": "test_benchmark/test_redis_bloom_bench.py::TestRedisBloomPipeline::test_bloom_pipeline[bloom_guard_pipeline]",
            "params": {
                "path": "bloom_guard_pipeline"
            },
            "param": "bloom_guard_pipeline",
            "extra_info": {
                "workload_total": 500000,
                "miss_ratio": 0.999,
                "error_rate": 1e-06,
                "capacity": 1000000,
                "existing_count": 100000,
                "query_count": 500000,
                "seed": 42,
                "batch_size": 500,
                "path": "bloom_guard_pipeline",
                "elapsed_s": 1.8008875640007318,
                "qps": 277640,
                "skipped_gets": 499500,
                "skip_pct": 0.999,
                "stats_delta": {
                    "hits": 1501500,
                    "misses": 0,
                    "total_cmd": 1509919,
                    "net_in": 106360400,
                    "net_out": 19582335,
                    "mem_used": -3072
                }
            },
            "options": {
                "disable_gc": false,
                "timer": "perf_counter",
                "min_rounds": 5,
                "max_time": 1.0,
                "min_time": 5e-06,
                "warmup": false
            },
            "stats": {
                "min": 1.8008910700009437,
                "max": 1.8275376359997608,
                "mean": 1.8120202236665743,
                "stddev": 0.013854691641547432,
                "rounds": 3,
                "median": 1.8076319649990182,
                "iqr": 0.01998492449911282,
                "q1": 1.8025762937504624,
                "q3": 1.8225612182495752,
                "iqr_outliers": 0,
                "stddev_outliers": 1,
                "outliers": "1;0",
                "ld15iqr": 1.8008910700009437,
                "hd15iqr": 1.8275376359997608,
                "ops": 0.5518702202873469,
                "total": 5.436060670999723,
                "data": [
                    1.8076319649990182,
                    1.8275376359997608,
                    1.8008910700009437
                ],
                "iterations": 1
            }
        },
        {
            "group": "redis_bloom_concurrent",
            "name": "test_bloom_concurrent[plain_get_concurrent]",
            "fullname": "test_benchmark/test_redis_bloom_bench.py::TestRedisBloomConcurrent::test_bloom_concurrent[plain_get_concurrent]",
            "params": {
                "path": "plain_get_concurrent"
            },
            "param": "plain_get_concurrent",
            "extra_info": {
                "workload_total": 500000,
                "miss_ratio": 0.999,
                "error_rate": 1e-06,
                "capacity": 1000000,
                "existing_count": 100000,
                "query_count": 500000,
                "seed": 42,
                "workers": 20,
                "path": "plain_get_concurrent",
                "elapsed_s": 48.60202542600018,
                "qps": 10287,
                "stats_delta": {
                    "hits": 1500,
                    "misses": 1498500,
                    "total_cmd": 1500039,
                    "net_in": 71668606,
                    "net_out": 7517643,
                    "mem_used": 430968
                }
            },
            "options": {
                "disable_gc": false,
                "timer": "perf_counter",
                "min_rounds": 5,
                "max_time": 1.0,
                "min_time": 5e-06,
                "warmup": false
            },
            "stats": {
                "min": 48.48000242799935,
                "max": 48.60204844400141,
                "mean": 48.56018876566668,
                "stddev": 0.0694658701607591,
                "rounds": 3,
                "median": 48.59851542499928,
                "iqr": 0.0915345120015445,
                "q1": 48.50963067724933,
                "q3": 48.60116518925088,
                "iqr_outliers": 0,
                "stddev_outliers": 1,
                "outliers": "1;0",
                "ld15iqr": 48.48000242799935,
                "hd15iqr": 48.60204844400141,
                "ops": 0.02059300067439248,
                "total": 145.68056629700004,
                "data": [
                    48.48000242799935,
                    48.59851542499928,
                    48.60204844400141
                ],
                "iterations": 1
            }
        },
        {
            "group": "redis_bloom_concurrent",
            "name": "test_bloom_concurrent[bloom_guard_concurrent]",
            "fullname": "test_benchmark/test_redis_bloom_bench.py::TestRedisBloomConcurrent::test_bloom_concurrent[bloom_guard_concurrent]",
            "params": {
                "path": "bloom_guard_concurrent"
            },
            "param": "bloom_guard_concurrent",
            "extra_info": {
                "workload_total": 500000,
                "miss_ratio": 0.999,
                "error_rate": 1e-06,
                "capacity": 1000000,
                "existing_count": 100000,
                "query_count": 500000,
                "seed": 42,
                "workers": 20,
                "path": "bloom_guard_concurrent",
                "elapsed_s": 47.49141378200147,
                "qps": 10528,
                "skipped_gets": 499500,
                "skip_pct": 0.999,
                "stats_delta": {
                    "hits": 1501500,
                    "misses": 0,
                    "total_cmd": 1501501,
                    "net_in": 106238339,
                    "net_out": 6024952,
                    "mem_used": 0
                }
            },
            "options": {
                "disable_gc": false,
                "timer": "perf_counter",
                "min_rounds": 5,
                "max_time": 1.0,
                "min_time": 5e-06,
                "warmup": false
            },
            "stats": {
                "min": 47.49144548700133,
                "max": 48.97442217000207,
                "mean": 48.27657467933508,
                "stddev": 0.7453311560409601,
                "rounds": 3,
                "median": 48.363856381001824,
                "iqr": 1.1122325122505572,
                "q1": 47.70954821050145,
                "q3": 48.82178072275201,
                "iqr_outliers": 0,
                "stddev_outliers": 1,
                "outliers": "1;0",
                "ld15iqr": 47.49144548700133,
                "hd15iqr": 48.97442217000207,
                "ops": 0.02071397995077834,
                "total": 144.82972403800522,
                "data": [
                    48.97442217000207,
                    48.363856381001824,
                    47.49144548700133
                ],
                "iterations": 1
            }
        }
    ],
    "datetime": "2025-12-02T08:17:35.091114+00:00",
    "version": "5.2.3"
}
```