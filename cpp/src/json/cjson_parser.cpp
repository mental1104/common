#include "mental1104/json/cjson_parser.h"

#include "support.h"

#if M1104_HAS_JSON_CJSON
#include <cstring>
#include <limits>
#include <utility>

#include "mental1104/common/c_api_compat.h"
COMMON_EXTERN_C_BEGIN
#if M1104_HAS_INCLUDE("cJSON/cJSON.h")
#include <cJSON/cJSON.h>
#else
#include <cJSON.h>
#endif
COMMON_EXTERN_C_END

namespace mental1104 {
namespace {
/// 把原生树释放绑定到独占指针，覆盖解析后 C++ 分配失败的路径。
struct CJsonDeleter {
    /// 释放整棵原生树。
    /// @param node 被独占的根节点，允许 nullptr。
    void operator()(cJSON *node) const noexcept { cJSON_Delete(node); }
};
/// cJSON 文档实现；每个句柄都借用 root_ 拥有的树。
class CJsonDocument final : public IJsonDocument {
public:
    /// 接管成功解析的树，不复制节点。
    /// @param root 转移根节点所有权。
    explicit CJsonDocument(std::unique_ptr<cJSON, CJsonDeleter> root)
        : root_(std::move(root)) {}

private:
    /// @return 文档拥有的原生根节点。
    Node do_root() const noexcept override { return this->root_.get(); }
    /// 解释当前文档的节点类型。
    /// @param node 非空借用句柄。
    /// @return 后端无关的类型。
    JsonType do_type(Node node) const noexcept override {
        const auto *value = static_cast<const cJSON *>(node);
        if (cJSON_IsNull(value)) return JsonType::Null;
        if (cJSON_IsBool(value)) return JsonType::Bool;
        if (cJSON_IsNumber(value)) return JsonType::Number;
        if (cJSON_IsString(value)) return JsonType::String;
        if (cJSON_IsArray(value)) return JsonType::Array;
        if (cJSON_IsObject(value)) return JsonType::Object;
        return JsonType::Missing;
    }
    /// 查询区分大小写的成员，按长度比较而不构造临时 NUL 缓冲。
    /// @param node 非空借用句柄。
    /// @param key 不保存的只读键。
    /// @return 成员借用句柄，非对象或缺失为空。
    Node do_get(Node node, const string_view &key) const override {
        const auto *value = static_cast<const cJSON *>(node);
        if (!cJSON_IsObject(value)) return nullptr;
        for (auto *child = value->child; child; child = child->next) {
            if (child->string && std::strlen(child->string) == key.size() &&
                (key.empty() ||
                    std::memcmp(child->string, key.data(), key.size()) == 0)) {
                return child;
            }
        }
        return nullptr;
    }
    /// 查询数组长度。
    /// @param node 非空借用句柄。
    /// @return 数组长度，非数组返回 0。
    std::size_t do_size(Node node) const noexcept override {
        const auto *value = static_cast<const cJSON *>(node);
        return cJSON_IsArray(value)
                   ? static_cast<std::size_t>(cJSON_GetArraySize(value))
                   : 0;
    }
    /// 检查索引范围后访问原生数组。
    /// @param node 非空借用句柄。
    /// @param index 从 0 开始的索引。
    /// @return 元素句柄，非数组或越界为空。
    Node do_at(Node node, std::size_t index) const noexcept override {
        const auto *value = static_cast<const cJSON *>(node);
        if (!cJSON_IsArray(value) ||
            index >
                static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return nullptr;
        }
        return cJSON_GetArrayItem(value, static_cast<int>(index));
    }
    /// 借用 cJSON 的 NUL 结尾字符串，不恢复内嵌 NUL 后的字节。
    /// @param node 非空借用句柄。
    /// @return 字符串或空结果。
    JsonOptional<string_view> do_string(Node node) const override {
        const auto *value = static_cast<const cJSON *>(node);
        if (!cJSON_IsString(value) || !value->valuestring) return {};
        return string_view(value->valuestring);
    }
    /// 严格读取 JSON 布尔。
    /// @param node 非空借用句柄。
    /// @return 布尔值或空结果。
    JsonOptional<bool> do_bool(Node node) const override {
        const auto *value = static_cast<const cJSON *>(node);
        if (!cJSON_IsBool(value)) return {};
        return cJSON_IsTrue(value) != 0;
    }
    /// 按 double 当前值做整数范围检查，不承诺原始 64 位整数精度。
    /// @param node 非空借用句柄。
    /// @return 整数或空结果。
    JsonOptional<long long> do_i64(Node node) const override {
        const auto *value = static_cast<const cJSON *>(node);
        return cJSON_IsNumber(value)
                   ? json_detail::signed_integer(value->valuedouble)
                   : JsonOptional<long long>{};
    }
    /// 拒绝负数、小数和越界转换。
    /// @param node 非空借用句柄。
    /// @return 无符号整数或空结果。
    JsonOptional<unsigned long long> do_u64(Node node) const override {
        const auto *value = static_cast<const cJSON *>(node);
        return cJSON_IsNumber(value)
                   ? json_detail::unsigned_integer(value->valuedouble)
                   : JsonOptional<unsigned long long>{};
    }
    /// 读取 cJSON 保存的 double。
    /// @param node 非空借用句柄。
    /// @return 数值或空结果。
    JsonOptional<double> do_f64(Node node) const override {
        const auto *value = static_cast<const cJSON *>(node);
        if (!cJSON_IsNumber(value)) return {};
        return value->valuedouble;
    }
    std::unique_ptr<cJSON, CJsonDeleter> root_;
};
}  // namespace

ParseResult CJsonParser::parse(const string_view &text) const {
    if (const auto bad = json_detail::invalid_input(text)) {
        return json_detail::parse_error(
            "empty JSON input or embedded NUL byte", *bad);
    }
    const char *end = nullptr;
    std::unique_ptr<cJSON, CJsonDeleter> tree(
        cJSON_ParseWithLengthOpts(text.data(), text.size(), &end, false));
    if (!tree) {
        // 使用本次调用的 end，而不读取线程间共享的 cJSON_GetErrorPtr。
        return json_detail::parse_error("cJSON parse error",
            end ? static_cast<std::size_t>(end - text.data()) : 0);
    }
    std::size_t offset = static_cast<std::size_t>(end - text.data());
    // cJSON 默认接受根之后的额外内容；统一入口要求只剩 JSON 空白。
    while (offset < text.size() &&
           (text[offset] == ' ' || text[offset] == '\t' ||
               text[offset] == '\n' || text[offset] == '\r')) {
        ++offset;
    }
    if (offset != text.size()) {
        return json_detail::parse_error(
            "unexpected content after JSON root", offset);
    }
    ParseResult result;
    result.doc = JsonDoc(
        std::unique_ptr<IJsonDocument>(new CJsonDocument(std::move(tree))),
        JsonParser::CJSON);
    result.ok = true;
    return result;
}
}  // namespace mental1104
#else
namespace mental1104 {
ParseResult CJsonParser::parse(const string_view &) const {
    return json_detail::parse_error("cJSON backend is not built");
}
}  // namespace mental1104
#endif
