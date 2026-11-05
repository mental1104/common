#include "linkage_consumer.h"

#include "mental1104/bloom_filter.h"
#include "mental1104/c_api_raii.h"
#include "mental1104/concurrency/coroutine/task.h"
#include "mental1104/concurrency/thread/thread_pool_executor.h"
#include "mental1104/core/cache.h"
#include "mental1104/json/codec.h"
#include "mental1104/random.h"
#include "mental1104/util.h"

#include <cstdio>
#include <future>

namespace linkage_test {
namespace {

/// 仅在消费端存在的键类型，库无法提前为其生成模板实例。
struct PrivateKey {
    int value;
};

/// 对消费端私有键进行哈希，不依赖库内注册或显式实例化。
struct PrivateHash {
    /// 计算键哈希。
    /// @param key 只读键，不保存引用。
    /// @return 键的哈希值。
    std::size_t operator()(const PrivateKey &key) const {
        return std::hash<int>{}(key.value);
    }
};

#if M1104_HAS_CXX20
/// 在消费端编译协程体，由已编译 Task 实现恢复和销毁帧。
/// @param steps 借用的计数器，必须在任务完成前保持存活。
/// @return 独占协程帧的任务。
mental1104::Task consumer_task(int &steps) {
    ++steps;
    co_await std::suspend_always{};
    ++steps;
}
#endif

}  // namespace

/// 组合调用方私有类型和库中的固定签名实现，检验真实的分离编译边界。
/// @return 不含模板内部对象的可观察结果。
Observation observe_consumer() {
    auto cache = mental1104::make_lru_cache<int, int>(
        2, [](int value) { return value + 1; });
    BasicBloomFilter<PrivateKey, PrivateHash> filter(100, 0.01);
    filter.insert(PrivateKey{7});

    std::promise<int> result;
    auto future = result.get_future();
    mental1104::ThreadPoolExecutor executor(1);
    // promise 的借用持续到 future.get 完成，执行器在离开作用域时回收线程。
    executor.execute([&result, &cache] { result.set_value(cache(41)); });

#if M1104_HAS_JSON_CJSON || M1104_HAS_JSON_RAPIDJSON || M1104_HAS_JSON_NLOHMANN
    const char payload[] = "{\"ok\":true}";
#endif
    // 按实际库能力访问
    // JSON，消费端由独立翻译单元按库标准编译，避免类型布局不匹配。
    bool parse_ok = true;
#if M1104_HAS_JSON_RAPIDJSON
    auto parsed = mental1104::load_json(
        payload, sizeof(payload) - 1, mental1104::JsonParser::RapidJSON);
    // 先取得视图再移动拥有者，验证视图不会因独占指针转移而失效。
    const auto root = parsed.get();
    auto moved = std::move(parsed);
    parse_ok = static_cast<bool>(moved) &&
               root.get_optional<bool>("ok").value_or(false);
#endif
#if M1104_HAS_JSON_CJSON
    auto parsed_cjson = mental1104::load_json(
        payload, sizeof(payload) - 1, mental1104::JsonParser::CJSON);
    parse_ok = parse_ok && parsed_cjson.ok &&
               parsed_cjson.get().get_optional<bool>("ok").value_or(false);
#endif
#if M1104_HAS_JSON_NLOHMANN
    const auto parsed_nlohmann = mental1104::load_json(
        payload, sizeof(payload) - 1, mental1104::JsonParser::NlohmannJSON);
    parse_ok = parse_ok && parsed_nlohmann &&
               parsed_nlohmann.get().get_optional<bool>("ok").value_or(false);
#endif
    mental1104::unique_file file(std::tmpfile());
    mental1104::ExponentialBackoff backoff(10, 100);
    int steps = 0;
#if M1104_HAS_CXX20
    auto task = consumer_task(steps);
    task.resume();
    task.resume();
#endif
    return {mental1104::to_lower_copy("MiXeD"), future.get(),
        filter.contains(PrivateKey{7}), parse_ok, static_cast<bool>(file),
        !mental1104::random_hex<std::minstd_rand0>().empty(),
        static_cast<int>(backoff.next().count()), steps,
        M1104_HAS_STRING_VIEW != 0};
}

/// 读取已编译日志核心持有的等级，验证跨翻译单元状态。
/// @return 当前日志等级。
mental1104::LogLevel consumer_log_level() {
    return mental1104::get_log_level();
}

/// 提交固定文本的 Info 日志，输出由库中的等级与后端控制。
void write_consumer_log() {
    mental1104::log(mental1104::LogLevel::Info, "linked-message");
}

}  // namespace linkage_test
