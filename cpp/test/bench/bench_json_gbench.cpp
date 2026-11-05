/// @file
/// 使用相同输入比较内置加载后端，计入完整解析与文档释放成本。
/// 吞吐计时与逐次延迟采样分开，避免把每次读钟的成本混入吞吐结果。
#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

#include "mental1104/json/codec.h"  // JSON 编解码入口。

#if M1104_HAS_JSON_CJSON || M1104_HAS_JSON_RAPIDJSON || M1104_HAS_JSON_NLOHMANN
namespace {

/// 构造可按固定规模重复使用的嵌套 JSON，覆盖文本、数字、布尔、null 和数组。
/// @param users 用户记录数；每条记录包含独立的资料与分数字段。
/// @param scores_len 每条用户记录的分数数量。
/// @param matrix_n 矩阵的行数和列数，生成 matrix_n 乘 matrix_n 个数值。
/// @param tags 元数据中的字符串标签数量。
/// @return 独立持有的 UTF-8 JSON 文本，生成过程不依赖被测解析后端。
std::string make_complex_json(std::size_t users, std::size_t scores_len = 16,
    std::size_t matrix_n = 16, std::size_t tags = 12) {
    std::string s;
    s.reserve(users * (256 + scores_len * 6) + matrix_n * matrix_n * 3 +
              tags * 16 + 4096);

    s +=
        R"({"meta":{"version":1,"title":"Benchmark Fixture","active":true,"ratio":0.12345,"text":"你好, 😊","tags":[)";
    for (size_t i = 0; i < tags; ++i) {
        s += "\"tag_";
        s += std::to_string(i);
        s += '"';
        if (i + 1 != tags) s += ',';
    }
    s += R"(]},"users":[)";

    for (size_t i = 0; i < users; ++i) {
        s += R"({"id":)";
        s += std::to_string(i);
        s += R"(,"name":")";
        s += "user_" + std::to_string(i);
        s += R"(","scores":[)";
        for (size_t j = 0; j < scores_len; ++j) {
            s += std::to_string((i * 131 + j * 17) % 1000);
            if (j + 1 != scores_len) s += ',';
        }
        s += R"(],"profile":{"email":")";
        s += "u" + std::to_string(i) + "@example.com";
        s +=
            R"(","flags":[true,false,null],"address":{"city":"Shenzhen","zip":518000,"lines":["Apt 1","Street 2"]}})";
        s += R"(,"created_at":")";
        s += "2025-01-01T00:00:00Z";
        s += R"("})";
        if (i + 1 != users) s += ',';
    }

    s += R"(],"matrix":[)";
    for (size_t r = 0; r < matrix_n; ++r) {
        s += '[';
        for (size_t c = 0; c < matrix_n; ++c) {
            s += std::to_string((r * 31 + c * 7) % 97);
            if (c + 1 != matrix_n) s += ',';
        }
        s += ']';
        if (r + 1 != matrix_n) s += ',';
    }
    s += "]}";
    return s;
}

/// 进程资源快照；POSIX 平台提供 CPU、上下文切换及进程累计峰值 RSS。
/// 非 POSIX 平台不填充资源计数器，仍可运行吞吐和延迟基准。
struct ProcessUsage {
    bool available{false};
    double user_seconds{0};
    double system_seconds{0};
    double peak_rss_kib{0};
    long voluntary_switches{0};
    long involuntary_switches{0};
};

/// 读取当前进程资源，供整批吞吐迭代前后求差，不在每次解析时调用。
/// @return POSIX 读取成功时 available=true；RSS 为进程存活至今的峰值。
ProcessUsage process_usage() {
    ProcessUsage result;
#if defined(__unix__) || defined(__APPLE__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return result;
    result.available = true;
    result.user_seconds = static_cast<double>(usage.ru_utime.tv_sec) +
                          static_cast<double>(usage.ru_utime.tv_usec) / 1e6;
    result.system_seconds = static_cast<double>(usage.ru_stime.tv_sec) +
                            static_cast<double>(usage.ru_stime.tv_usec) / 1e6;
    result.peak_rss_kib = static_cast<double>(usage.ru_maxrss);
#if defined(__APPLE__)
    // macOS 返回字节，其他受支持 POSIX 平台沿用 KiB 口径。
    result.peak_rss_kib /= 1024;
#endif
    result.voluntary_switches = usage.ru_nvcsw;
    result.involuntary_switches = usage.ru_nivcsw;
#endif
    return result;
}

/// 将吞吐批次的资源差写入框架输出，避免在计时循环中采集操作系统指标。
/// @param state 当前 trial 的计数器拥有者。
/// @param before 吞吐循环开始前的进程快照。
/// @param after 吞吐循环结束后、延迟采样之前的进程快照。
/// @param wall_seconds 此批迭代的墙钟秒数，包含框架启动和结束计时的微小成本。
void record_usage(benchmark::State &state, const ProcessUsage &before,
    const ProcessUsage &after, double wall_seconds) {
    // 不支持进程资源采集的平台不能用零值伪造测量结果。
    if (!before.available || !after.available) return;
    const double user = after.user_seconds - before.user_seconds;
    const double system = after.system_seconds - before.system_seconds;
    state.counters["user_s"] = user;
    state.counters["sys_s"] = system;
    state.counters["cpu_pct"] =
        wall_seconds > 0 ? 100 * (user + system) / wall_seconds : 0;
    state.counters["rss_peak_kib"] = after.peak_rss_kib;
    state.counters["ctx_voluntary"] = static_cast<double>(
        after.voluntary_switches - before.voluntary_switches);
    state.counters["ctx_involuntary"] = static_cast<double>(
        after.involuntary_switches - before.involuntary_switches);
}

/// 在吞吐计时结束后单独采样每次加载与释放的墙钟延迟。
/// @tparam Parser 当前 trial 使用的内置后端。
/// @param state 当前 trial，失败时记录错误，成功时添加纳秒分位数计数器。
/// @param input 只读稳定输入，构造和持有成本不计入单次解析。
template <mental1104::JsonParser Parser>
void sample_latency(
    benchmark::State &state, const mental1104::string_view &input) {
    constexpr std::size_t samples = 512;
    std::vector<double> latency;
    latency.reserve(samples);
    for (std::size_t i = 0; i < samples; ++i) {
        const auto start = std::chrono::steady_clock::now();
        {
            auto parsed = mental1104::load_json(input, Parser);
            if (!parsed.ok) {
                state.SkipWithError(parsed.error.c_str());
                return;
            }
            benchmark::DoNotOptimize(parsed.ok);
            // 拥有者在本作用域结束时释放 DOM，延迟与吞吐使用相同工作单位。
        }
        const auto end = std::chrono::steady_clock::now();
        latency.push_back(
            std::chrono::duration<double, std::nano>(end - start).count());
    }
    // 最近秩分位数；排序和计数器写入均在吞吐计时之外执行。
    std::sort(latency.begin(), latency.end());
    state.counters["p50_ns"] = latency[(samples * 50 + 99) / 100 - 1];
    state.counters["p95_ns"] = latency[(samples * 95 + 99) / 100 - 1];
    state.counters["p99_ns"] = latency[(samples * 99 + 99) / 100 - 1];
    state.counters["max_ns"] = latency.back();
    state.counters["latency_samples"] = static_cast<double>(samples);
}

/// 使用统一加载入口比较已编入库的后端，每次迭代创建并释放一个独立 DOM。
/// @tparam Parser 后端枚举，所有后端均使用相同的输入和公共校验路径。
/// @param state 框架迭代状态，range(0)=0 为短对象，否则为嵌套文档用户数。
template <mental1104::JsonParser Parser>
void BM_iface_parse(benchmark::State &state) {
    const std::size_t users = static_cast<std::size_t>(state.range(0));
    const std::string json =
        users == 0 ? R"({"id":42,"name":"Espeon","active":true,"value":null})"
                   : make_complex_json(users);
    // 旧标准的 string_view 是拥有型 string，适配只做一次，不能混入重复解析。
    const mental1104::string_view input(json.data(), json.size());
    {
        const auto checked = mental1104::load_json(input, Parser);
        // 计时前检查输入确实完整可读，不能把解析失败或错误后端当作性能样本。
        if (!checked || checked.backend() != Parser ||
            !checked.get().is_object() ||
            (users == 0 ? checked.get().get("id").as_i64() != 42
                        : checked.get().get("users").size() != users)) {
            state.SkipWithError("JSON benchmark fixture validation failed");
            return;
        }
    }

    const auto before = process_usage();
    const auto start = std::chrono::steady_clock::now();
    for (auto _ : state) {
        auto parsed = mental1104::load_json(input, Parser);
        if (!parsed.ok) {
            // 错误样本没有成功吞吐含义，终止本次 trial，不输出伪造的性能数据。
            state.SkipWithError(parsed.error.c_str());
            return;
        }
        benchmark::DoNotOptimize(parsed.ok);
        // 每轮结束前销毁结果，吞吐包含 DOM 分配、解析与释放。
    }
    const auto end = std::chrono::steady_clock::now();
    const auto after = process_usage();
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(json.size()));
    state.counters["ops_per_second"] = benchmark::Counter(
        static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["input_bytes"] = static_cast<double>(json.size());
    record_usage(state, before, after,
        std::chrono::duration<double>(end - start).count());
    sample_latency<Parser>(state, input);
}

#if M1104_HAS_JSON_CJSON
/// 通过统一入口测量 cJSON；输入准备和资源采集在计时循环之外完成。
/// @param state 框架提供的当前规模及 trial 状态。
void BM_iface_cjson(benchmark::State &state) {
    BM_iface_parse<mental1104::JsonParser::CJSON>(state);
}
BENCHMARK(BM_iface_cjson)
    ->Arg(0)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024)
    ->UseRealTime()
    ->MinTime(0.3)
    ->MinWarmUpTime(0.1)
    ->Repetitions(5);
#endif

#if M1104_HAS_JSON_RAPIDJSON
/// 通过相同公共入口测量 RapidJSON，不借用或泄漏原生 DOM。
/// @param state 框架提供的当前规模及 trial 状态。
void BM_iface_rapid(benchmark::State &state) {
    BM_iface_parse<mental1104::JsonParser::RapidJSON>(state);
}
BENCHMARK(BM_iface_rapid)
    ->Arg(0)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024)
    ->UseRealTime()
    ->MinTime(0.3)
    ->MinWarmUpTime(0.1)
    ->Repetitions(5);
#endif

#if M1104_HAS_JSON_NLOHMANN
/// 通过相同公共入口测量 nlohmann JSON，与其他后端复用同一输入生成器。
/// @param state 框架提供的当前规模及 trial 状态。
void BM_iface_nlohmann(benchmark::State &state) {
    BM_iface_parse<mental1104::JsonParser::NlohmannJSON>(state);
}
BENCHMARK(BM_iface_nlohmann)
    ->Arg(0)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024)
    ->UseRealTime()
    ->MinTime(0.3)
    ->MinWarmUpTime(0.1)
    ->Repetitions(5);
#endif

}  // namespace
#endif

BENCHMARK_MAIN();
