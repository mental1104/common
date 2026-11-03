#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include "mental1104/common/c_api_compat.h"
#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX17
#include <variant>
#else
#include <boost/variant2/variant.hpp>
#endif

COMMON_EXTERN_C_BEGIN
#if __has_include(<cJSON/cJSON.h>)
#include <cJSON/cJSON.h>
#else
#include "cJSON.h"
#endif
COMMON_EXTERN_C_END

#if __has_include(<rapidjson/document.h>)
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#else
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#endif

namespace mental1104 {

namespace json_detail {
#if M1104_HAS_CXX17
using std::get;
using std::get_if;
using std::holds_alternative;
using std::monostate;
template <typename... T> using variant = std::variant<T...>;
#else
using boost::variant2::get;
using boost::variant2::get_if;
using boost::variant2::holds_alternative;
using boost::variant2::monostate;
template <typename... T> using variant = boost::variant2::variant<T...>;
#endif
} // namespace json_detail

enum class JsonParser { CJSON, RapidJSON };

struct ParseResult;
/// 解析 JSON，成功结果独占后端文档。
/// @param text 只读输入，本函数不保存调用方视图。
/// @param parser 使用的解析后端。
/// @return 成功文档或后端错误；库与调用方必须使用兼容的标准和 variant 配置。
ParseResult parse_json(const string_view &text, JsonParser parser);
/// 按显式字节长度解析 JSON。
/// @param data 指向至少 len 字节的只读数据。
/// @param len 输入字节数。
/// @param parser 使用的解析后端。
/// @return 解析结果及独占文档。
ParseResult parse_json(const char *data, std::size_t len, JsonParser parser);
#if M1104_HAS_CXX17
/// 解析字符串，不保存输入引用。
/// @param text 只读 JSON 字符串。
/// @param parser 使用的解析后端。
/// @return 解析结果及独占文档。
ParseResult parse_json(const std::string &text, JsonParser parser);
#endif

class JsonValueView;
class JsonDoc;

/// 独占后端 JSON 文档；布局随当前标准的 variant 实现变化，库与调用方须匹配。
class JsonDoc {
public:
  JsonDoc() = default;
  /// 释放由当前文档持有的后端资源。
  ~JsonDoc();

  /// 接管另一个文档的后端资源。
  /// @param other 移动后置为空状态的源文档。
  JsonDoc(JsonDoc &&other) noexcept;
  /// 释放旧资源后接管源文档。
  /// @param other 移动后置为空状态的源文档。
  /// @return 当前文档引用。
  JsonDoc &operator=(JsonDoc &&other) noexcept;

  JsonDoc(const JsonDoc &) = delete;
  JsonDoc &operator=(const JsonDoc &) = delete;

  /// 查询是否持有后端文档。
  /// @return 非空文档返回 true。
  bool valid() const noexcept;
  JsonParser backend() const noexcept { return backend_; }

  /// 借用当前文档的根节点，不延长文档生命周期。
  /// @return 根视图；文档为空时返回空视图。
  JsonValueView root() const;

private:
  using Impl = json_detail::variant<json_detail::monostate, cJSON *,
                                    rapidjson::Document>;
  Impl impl_;
  JsonParser backend_{JsonParser::CJSON};

  /// 释放后端资源并恢复为空文档状态。
  void reset() noexcept;
  /// 接管源文档表示，不保留源对象所有权。
  /// @param other 移动后清空的源文档。
  void move_from(JsonDoc &&other) noexcept;

  friend ParseResult parse_json(const string_view &text, JsonParser parser);
  friend ParseResult parse_json(const char *data, std::size_t len,
                                JsonParser parser);
#if M1104_HAS_CXX17
  friend ParseResult parse_json(const std::string &, JsonParser);
#endif

  friend class JsonValueView;
};

/// 借用文档节点的视图，不延长所属 JsonDoc 的生命周期。
class JsonValueView {
public:
  JsonValueView() = default;

  /// 建立不拥有资源的 cJSON 节点视图。
  /// @param n 可为空的借用节点，所属文档必须在视图使用期间存活。
  /// @return 借用节点的视图。
  static JsonValueView from_cjson(const cJSON *n);
  /// 建立不拥有资源的 RapidJSON 节点视图。
  /// @param n 可为空的借用节点，所属文档必须在视图使用期间存活。
  /// @return 借用节点的视图。
  static JsonValueView from_rapid(const rapidjson::Value *n);

  /// 查询是否保存后端节点表示。
  /// @return 不是 monostate 时返回 true，沿用现有表示语义。
  bool is_valid() const noexcept;
  /// 查询节点是否为 object。
  /// @return 非空且类型匹配时为 true。
  bool is_object() const noexcept;
  /// 查询节点是否为 array。
  /// @return 非空且类型匹配时为 true。
  bool is_array() const noexcept;
  /// 查询节点是否为 string。
  /// @return 非空且类型匹配时为 true。
  bool is_string() const noexcept;
  /// 查询节点是否为 bool。
  /// @return 非空且类型匹配时为 true。
  bool is_bool() const noexcept;
  /// 查询节点是否为 number。
  /// @return 非空且类型匹配时为 true。
  bool is_number() const noexcept;

  /// 查询对象是否包含区分大小写的键。
  /// @param key 只读键，不保存视图。
  /// @return 非对象或键缺失时为 false。
  bool has(string_view key) const noexcept;

  /// 查询对象成员，不转移后端资源。
  /// @param key 只读键，不保存视图。
  /// @return 成员视图；类型不匹配或缺失时为空视图。
  JsonValueView get(string_view key) const noexcept;

  /// 查询数组长度。
  /// @return 数组元素数量；非数组返回 0。
  std::size_t size() const noexcept;

  /// 查询数组元素，不转移资源。
  /// @param i 从 0 开始的元素索引。
  /// @return 元素视图；非数组或越界时为空视图。
  JsonValueView at(std::size_t i) const noexcept;

  /// 读取字符串，沿用 string_view 在当前标准下的借用或复制语义。
  /// @param def 类型不匹配时返回的默认值。
  /// @return 字符串；借用结果要求文档或默认值的存储保持存活。
  string_view as_string(string_view def = {}) const noexcept;

  /// 读取布尔值。
  /// @param def 类型不匹配时的默认值。
  /// @return 节点值或默认值。
  bool as_bool(bool def = false) const noexcept;

  /// 按后端既有数值转换规则读取整数。
  /// @param def 类型不匹配时的默认值。
  /// @return 转换值或默认值。
  long long as_i64(long long def = 0) const noexcept;

  /// 读取浮点数。
  /// @param def 类型不匹配时的默认值。
  /// @return 数值或默认值。
  double as_f64(double def = 0.0) const noexcept;

  /// 复制节点字符串，不返回借用视图。
  /// @param def 类型不匹配时复制的默认值。
  /// @return 独立字符串副本。
  std::string as_string_copy(const std::string &def = {}) const;

private:
  using Node = json_detail::variant<json_detail::monostate, const cJSON *,
                                    const rapidjson::Value *>;
  explicit JsonValueView(Node n) : node_(n) {}
  Node node_;

  friend class JsonDoc;
};

/// 一次解析的状态、错误位置与独占文档。
struct ParseResult {
  bool ok{false};
  std::string error;
  std::size_t offset{0};
  JsonDoc doc;
  explicit operator bool() const noexcept { return ok; }
};

} // namespace mental1104
