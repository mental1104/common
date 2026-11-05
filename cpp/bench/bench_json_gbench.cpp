/// @file
/// 使用同一规模 JSON 输入对比内置加载后端；输入构造和缓存独立于解析接口。
#include <benchmark/benchmark.h>
#include <mutex>
#include <string>
#include <unordered_map>

#include "mental1104/json/codec.h"  // JSON 编解码入口。

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

/// 按用户规模缓存输入，在调用方进入基准迭代前取得稳定的只读文本。
/// @param users 输入中的用户记录数，其他规模参数沿用构造函数默认值。
/// @return 缓存拥有的文本引用，条目不删除，在基准进程存活期间有效。
const std::string &get_json_cached(std::size_t users) {
    static std::unordered_map<std::size_t, std::string> cache;
    static std::mutex mtx;
    // 查找、首次构造及插入共享同一把锁，防止多线程同时修改缓存容器。
    std::lock_guard<std::mutex> lk(mtx);
    auto it = cache.find(users);
    if (it != cache.end()) return it->second;
    auto em = cache.emplace(users, make_complex_json(users));
    return em.first->second;
}

/// 使用统一加载入口比较后端，第二个参数仅通过标签类型选择后端。
/// @tparam ParserTag 提供静态 parser 标识的后端标签。
/// @param state 基准框架的迭代状态，用于记录实际处理字节数。
template <typename ParserTag>
static void BM_iface_parse(benchmark::State &state, ParserTag) {
    const std::size_t users = static_cast<std::size_t>(state.range(0));
    const std::string &json = get_json_cached(users);

    for (auto _ : state) {
        auto res = mental1104::load_json(json, ParserTag::parser);
        // 失败输入不能作为有效吞吐样本；让框架记录错误并终止当前场景。
        if (!res.ok) {
            state.SkipWithError(res.error.c_str());
            break;
        }
        // 防止优化掉（不访问内部成员，以免触发释放等额外成本）
        benchmark::DoNotOptimize(res.ok);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(json.size()));
}

/// cJSON 的编译期后端标签，仅提供公共加载入口所需的枚举值。
struct UseCjson {
    static constexpr auto parser = mental1104::JsonParser::CJSON;
};
/// RapidJSON 的编译期后端标签，不让基准逻辑依赖原生 DOM 类型。
struct UseRapidjson {
    static constexpr auto parser = mental1104::JsonParser::RapidJSON;
};

/// 将 cJSON 标签交给统一基准流程。
/// @param s 框架提供的可变迭代状态，由统一流程记录处理字节数。
static void BM_iface_cjson(benchmark::State &s) {
    BM_iface_parse(s, UseCjson{});
}
/// 将 RapidJSON 标签交给统一基准流程。
/// @param s 框架提供的可变迭代状态，由统一流程记录处理字节数。
static void BM_iface_rapid(benchmark::State &s) {
    BM_iface_parse(s, UseRapidjson{});
}

}  // namespace

// 规模参数：users 控制整体大小
BENCHMARK(BM_iface_cjson)->Arg(64)->Arg(256)->Arg(1024);
BENCHMARK(BM_iface_rapid)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK_MAIN();
