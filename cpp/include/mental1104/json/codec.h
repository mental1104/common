#pragma once

/// @file
/// JSON 编解码边界：加载文本为独占文档，并将只读文档序列化为独立文本。
#include "mental1104/json/access.h"

namespace mental1104 {
/// 单次序列化结果；成功时 ok=true，text 独占紧凑 JSON 文本。
/// 失败时 text 为空，error 提供 ASCII 诊断；结果不借用输入文档。
struct DumpResult {
    /// 仅完整文本生成成功时为 true，不能以 text 是否为空替代状态检查。
    bool ok{false};
    /// 成功时为空，失败时提供可读诊断，不要求调用方解析错误文案。
    std::string error;
    /// 独立持有的完整 JSON 文本；失败时为空，不借用文档或后端缓冲。
    std::string text;
    /// 判断是否成功生成完整 JSON 文本。
    /// @return 成功时为 true。
    explicit operator bool() const noexcept { return this->ok; }
};

/// 使用文档所属后端生成紧凑 JSON，不保留原始空白和数值拼写。
/// @param document 只读借用文档，不转移所有权；空文档返回错误。
/// @return 独立文本或错误；后端不支持或无法输出合法 JSON 时失败。
/// 内存分配失败仍可抛异常，调用方自定义后端异常不会被吞掉。
DumpResult dump_json(const JsonDoc &document);

/// 单次解析结果，内部独占文档，通过 get() 直接取得借用的根节点。
/// error 用于诊断，offset 是从输入起点计算的错误字节偏移。
struct ParseResult {
    /// 创建无文档的失败结果，供解析器填充诊断信息。
    ParseResult() noexcept = default;
    /// 接管成功解析的文档，禁止复制 DOM。
    /// @param document 按值转入独占所有权；空文档产生失败状态。
    explicit ParseResult(JsonDoc document) noexcept;
    /// 本次输入是否被完整接受；移动后另由布尔判断检查文档是否仍存在。
    bool ok{false};
    /// 成功时为空，失败时保存后端诊断的独立副本。
    std::string error;
    /// 错误相对于输入起点的字节偏移，仅失败时有意义；0 也可能是首字节错误。
    std::size_t offset{0};
    /// 直接访问根节点，不复制 DOM，不转移文档所有权。
    /// @return 借用根视图；失败或移出所有权后返回 Missing。
    JsonValueView get() const noexcept;
    /// 查询文档的后端标识，仅用于诊断，不驱动节点分派。
    /// @return 内置后端或 Custom；空结果的标识不代表具有可用文档。
    JsonParser backend() const noexcept;
    /// 判断解析成功且仍然持有文档，移出所有权后返回 false。
    /// @return 可以访问所持文档时为 true。
    explicit operator bool() const noexcept;

private:
    /// 独占后端文档；移动结果保留节点地址，销毁或覆盖结果使旧视图失效。
    JsonDoc document_;
    friend DumpResult dump_json(const ParseResult &result);
};

/// 直接序列化加载结果所持的文档，无需访问内部拥有者或再次指定后端。
/// @param result 只读借用的加载结果，不转移其文档所有权。
/// @return 独立文本或错误；失败或已移出的结果返回错误。
/// 内存分配或自定义后端异常仍传播到调用方。
DumpResult dump_json(const ParseResult &result);
/// 可替换的解析入口；每次产生独立文档，文档不依赖解析器存活。
/// 并发限制由具体后端决定；const 入口不代表第三方库没有进程级状态。
class IJsonParser {
public:
    /// 通过基类释放解析器。
    virtual ~IJsonParser();
    /// 解析完整输入，不借用输入、不改变之前返回的文档。
    /// @param text 只读 JSON 字节，允许没有 NUL 结束符。
    /// @return 独占文档或解析错误；分配失败仍可抛异常。
    virtual ParseResult parse(const string_view &text) const = 0;
};
/// 使用指定内置后端加载完整 JSON 文本，结果独占文档。
/// @param text 只读输入，不保存引用。
/// @param parser 内置后端，省略时使用 cJSON；Custom 应直接使用自定义解析器。
/// @return 文档或错误；未构建的后端返回明确错误。
ParseResult load_json(
    const string_view &text, JsonParser parser = JsonParser::CJSON);
/// 加载以 NUL 结尾的文本，避免字面量在 string 与 string_view 重载间产生歧义。
/// @param text 只读 NUL 结尾字符串；nullptr 返回输入错误，不保存指针。
/// @param parser 内置后端，省略时使用 cJSON；禁用时返回错误，不自动切换。
/// @return 独占文档或错误；需要处理完整字节区间时使用指针/长度重载。
ParseResult load_json(const char *text, JsonParser parser = JsonParser::CJSON);
/// 按显式字节长度解析。
/// @param data 指向至少 len 字节的只读输入；nullptr 返回输入错误。
/// @param len 输入字节数；零字节内容同样返回解析错误。
/// @param parser 内置后端，省略时使用 cJSON；禁用时返回错误，不自动切换。
/// @return 文档或错误。
ParseResult load_json(
    const char *data, std::size_t len, JsonParser parser = JsonParser::CJSON);
#if M1104_HAS_CXX17
/// 解析字符串，结果不借用输入存储。
/// @param text 只读 JSON 字符串。
/// @param parser 内置后端，省略时使用 cJSON；禁用时返回错误，不自动切换。
/// @return 文档或错误。
ParseResult load_json(
    const std::string &text, JsonParser parser = JsonParser::CJSON);
#endif

/// 无实例缓存的 cJSON 适配器，原生节点和释放规则仅存在于实现文件。
/// 底层仍写进程级错误状态，不承诺并发解析。
/// 原生文档及打印缓冲全部释放之前，不得切换 cJSON 全局内存 hooks。
class CJsonParser final : public IJsonParser {
public:
    /// 用 cJSON 解析完整输入，文档独立拥有原生树。
    /// @param text 不要求 NUL 结尾的只读字节。
    /// @return 独占文档或带偏移的错误；后端禁用时返回错误。
    ParseResult parse(const string_view &text) const override;
};

/// 无状态 RapidJSON 适配器，每次创建独立 Document 与 allocator。
/// 文档解析后保持只读，已有文档不会被后续 parse 调用复用或覆盖。
class RapidJsonParser final : public IJsonParser {
public:
    /// 解析完整输入，不使用借用输入存储的原地解析。
    /// @param text 不要求 NUL 结尾的只读字节。
    /// @return 独占文档或带偏移的错误；后端禁用时返回错误。
    ParseResult parse(const string_view &text) const override;
};
}  // namespace mental1104
