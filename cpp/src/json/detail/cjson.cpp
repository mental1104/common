/// @file
/// cJSON 后端：独占原生树，在同一处实现节点访问和 JSON 编解码。
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "backend.h"
#include "mental1104/json/codec.h"

#if M1104_HAS_JSON_CJSON
#include "mental1104/common/c_api_compat.h"
COMMON_EXTERN_C_BEGIN
#if M1104_HAS_INCLUDE("cJSON/cJSON.h")
#include <cJSON/cJSON.h>
#else
#include <cJSON.h>
#endif
COMMON_EXTERN_C_END
#endif

namespace mental1104 {
#if M1104_HAS_JSON_CJSON
namespace {
/// 原生根节点的独占所有权，解析后任何 C++ 失败路径都由 RAII 回收。
/// 删除函数读取全局 hooks，因此整个树存活期间必须保持分配/释放配置一致。
using CJsonTree = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

/// 检查原生树中的非有限数值，避免 cJSON 打印时将其静默替换为 null。
/// @param node 借用当前层首节点，可为空；同时检查其兄弟与子节点。
/// @return 包含 NaN 或无穷值时为 true。
bool contains_non_finite(const cJSON *node) {
    // 按兄弟链和子树遍历，任何一处非法数值都会阻止整份输出，不能只检查根。
    for (const auto *current = node; current; current = current->next) {
        if ((cJSON_IsNumber(current) && !std::isfinite(current->valuedouble)) ||
            contains_non_finite(current->child)) {
            return true;
        }
    }
    return false;
}

/// 使用当前后端的紧凑格式打印，只读访问原生树。
/// @param document 非空借用根节点，不修改或接管节点。
/// @return 独立文本或错误，不将非有限数值静默转换为 null。
DumpResult dump_cjson_document(const cJSON *document) {
    // cJSON 会把非有限数值写成 null，提前拒绝以免静默改变原值的类型。
    if (contains_non_finite(document)) {
        return json_detail::dump_error(
            "cannot serialize a non-finite JSON number");
    }
    // 原生缓冲始终由 cJSON_free 释放，std::string 复制抛异常时也不会泄漏。
    const std::unique_ptr<char, decltype(&cJSON_free)> buffer(
        cJSON_PrintUnformatted(document), &cJSON_free);
    // 打印失败时不构造成功文本；仍让原文档保持可读且由原拥有者管理。
    if (!buffer) {
        return json_detail::dump_error("cJSON serialization failed");
    }
    DumpResult result;
    result.text = buffer.get();
    result.ok = true;
    return result;
}

/// cJSON 文档实现；每个句柄都借用 root_ 独占的树。
/// 只在此文件解释原生指针；JsonDoc 移动不会移动树，析构通过 cJSON_Delete
/// 回收子树。
class CJsonDocument final : public IJsonDocument {
public:
    /// 接管成功解析的树，不复制节点。
    /// @param root 转移根节点所有权。
    explicit CJsonDocument(CJsonTree root) : root_(std::move(root)) {}

private:
    /// 使用当前后端的打印规则生成文本，保持原生树只读。
    /// @return 独立文本或序列化错误。
    DumpResult do_dump() const override {
        return dump_cjson_document(this->root_.get());
    }
    /// 借用当前文档独占的根节点。
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
        // 非对象统一表现为缺失，不把数组或标量当作对象成员链解释。
        if (!cJSON_IsObject(value)) return nullptr;
        // 比较调用方键的完整字节数；原生键自身仍受 cJSON 的 NUL 结束语义限制。
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
        // 收窄前拦截无法由 int 表示的下标，避免 size_t 被截断或变为负数。
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
    /// 唯一拥有的原生根；释放根会递归释放所有被视图借用的子节点。
    CJsonTree root_;
};
}  // namespace

/// 使用有界输入创建 cJSON 树，检查尾部后才提交成功文档。
/// @param text 已通过公共字节校验的只读输入；解析后不保留对输入存储的借用。
/// @return 成功时独占文档；失败时文档为空，提供相对于输入起点的错误字节偏移。
/// cJSON 仍访问进程级状态，不承诺并发解析；C++ 分配异常沿调用链传播。
ParseResult CJsonParser::do_parse(const string_view &text) const {
    // end 只属于本次调用；原生树一创建就进入 RAII，后续失败也不会遗留资源。
    const char *end = nullptr;
    CJsonTree tree(
        cJSON_ParseWithLengthOpts(text.data(), text.size(), &end, false),
        &cJSON_Delete);
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
    // 找到根不等于消费了完整输入；尾部垃圾必须按统一契约报告错误。
    if (offset != text.size()) {
        return json_detail::parse_error(
            "unexpected content after JSON root", offset);
    }
    // 最后把树转入地址固定的多态文档，由结果构造函数确认成功所有权。
    return ParseResult(JsonDoc(
        std::unique_ptr<IJsonDocument>(new CJsonDocument(std::move(tree))),
        JsonParser::CJSON));
}

#else
/// 当前构建缺少 cJSON，接收但不读取传入的只读视图，也不保留其存储。
/// @param text 已通过公共字节校验的只读输入，在本分支中忽略。
/// @return 明确的后端不可用错误，不回退到其他 JSON 库。
ParseResult CJsonParser::do_parse(const string_view &) const {
    return json_detail::parse_error("cJSON backend is not built");
}
#endif
}  // namespace mental1104
