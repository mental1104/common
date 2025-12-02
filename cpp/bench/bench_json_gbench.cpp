// bench/bench_json_gbench.cpp
#include <benchmark/benchmark.h>
#include <mutex>
#include <string>
#include <unordered_map>

#include "mental1104/json.h" // 你的统一接口：JsonParser / parse_json / ParseResult

namespace {

// 构造一个结构较复杂、可伸缩的 JSON；用 users 控制整体大小
std::string make_complex_json(std::size_t users, std::size_t scores_len = 16,
                              std::size_t matrix_n = 16,
                              std::size_t tags = 12) {
  std::string s;
  s.reserve(users * (256 + scores_len * 6) + matrix_n * matrix_n * 3 +
            tags * 16 + 4096);

  s +=
      R"({"meta":{"version":1,"title":"Benchmark Fixture","active":true,"ratio":0.12345,"text":"你好, 😊","tags":[)";
  for (size_t i = 0; i < tags; ++i) {
    s += "\"tag_";
    s += std::to_string(i);
    s += '"';
    if (i + 1 != tags)
      s += ',';
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
      if (j + 1 != scores_len)
        s += ',';
    }
    s += R"(],"profile":{"email":")";
    s += "u" + std::to_string(i) + "@example.com";
    s +=
        R"(","flags":[true,false,null],"address":{"city":"Shenzhen","zip":518000,"lines":["Apt 1","Street 2"]}})";
    s += R"(,"created_at":")";
    s += "2025-01-01T00:00:00Z";
    s += R"("})";
    if (i + 1 != users)
      s += ',';
  }

  s += R"(],"matrix":[)";
  for (size_t r = 0; r < matrix_n; ++r) {
    s += '[';
    for (size_t c = 0; c < matrix_n; ++c) {
      s += std::to_string((r * 31 + c * 7) % 97);
      if (c + 1 != matrix_n)
        s += ',';
    }
    s += ']';
    if (r + 1 != matrix_n)
      s += ',';
  }
  s += "]}";
  return s;
}

// 简单缓存：相同规模只生成一次，避免生成开销进入计时
const std::string &get_json_cached(std::size_t users) {
  static std::unordered_map<std::size_t, std::string> cache;
  static std::mutex mtx;
  std::lock_guard<std::mutex> lk(mtx);
  auto it = cache.find(users);
  if (it != cache.end())
    return it->second;
  auto em = cache.emplace(users, make_complex_json(users));
  return em.first->second;
}

// 通用模板：走你的 parse_json，选择不同后端
template <typename ParserTag>
static void BM_iface_parse(benchmark::State &state, ParserTag) {
  const std::size_t users = static_cast<std::size_t>(state.range(0));
  const std::string &json = get_json_cached(users);

  for (auto _ : state) {
    auto res = mental1104::parse_json(json, ParserTag::parser);
    if (!res.ok) {
      state.SkipWithError(res.error.c_str()); // 修正字段名
      break;
    }
    // 防止优化掉（不访问内部成员，以免触发释放等额外成本）
    benchmark::DoNotOptimize(res.ok);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(json.size()));
}

// 标签类型：在编译期选择不同枚举值
struct UseCjson {
  static constexpr auto parser = mental1104::JsonParser::CJSON;
};
struct UseRapidjson {
  static constexpr auto parser = mental1104::JsonParser::RapidJSON;
};

// gbench 入口
static void BM_iface_cjson(benchmark::State &s) {
  BM_iface_parse(s, UseCjson{});
}
static void BM_iface_rapid(benchmark::State &s) {
  BM_iface_parse(s, UseRapidjson{});
}

} // namespace

// 规模参数：users 控制整体大小
BENCHMARK(BM_iface_cjson)->Arg(64)->Arg(256)->Arg(1024);
BENCHMARK(BM_iface_rapid)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK_MAIN();
