#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "json/consumer.h"
/// @file
/// 两种内置后端复用的行为场景，覆盖读取、错误、视图生命周期及编解码往返。
/// 应用自有后端场景只依赖公开抽象，不通过原生类型强转验证实现细节。
#include "mental1104/json/codec.h"

using namespace mental1104;

namespace {
/// 只为实际编入库的后端运行同一套公共行为场景。
/// @return 当前配置启用的后端列表。
std::vector<JsonParser> enabled_backends() {
    std::vector<JsonParser> result;
#if M1104_HAS_JSON_CJSON
    result.push_back(JsonParser::CJSON);
#endif
#if M1104_HAS_JSON_RAPIDJSON
    result.push_back(JsonParser::RapidJSON);
#endif
    return result;
}

/// 按后端枚举参数化同一组黑盒场景，失败必须反映统一接口的可观察行为。
class JsonAccess : public testing::TestWithParam<JsonParser> {
protected:
    /// 用当前参数选择后端，不让场景依赖原生 JSON 类型。
    /// @param text 只读输入，不保存其引用。
    /// @return 独占解析结果。
    ParseResult parse(const std::string &text) const {
        return load_json(text.data(), text.size(), GetParam());
    }
};

/// 最小完整用法：加载对象、从根访问数组、读取元素，再直接序列化加载结果。
TEST_P(JsonAccess, LoadAccessAndDumpExample) {
    // 显式后端覆盖默认的 cJSON，让同一用法验证各个已启用实现。
    const auto loaded =
        mental1104::load_json(R"({"items":[{"id":1},{"id":2}]})", GetParam());
    // 加载失败时立即结束用例，避免空范围掩盖解析问题。
    ASSERT_TRUE(loaded) << loaded.error;

    // 元素视图借用 loaded 内部文档，整个遍历期间保持拥有者存活。
    std::vector<int> ids;
    for (const auto item : loaded.get().get("items")) {
        ids.push_back(item.get_optional<int>("id").value_or(-1));
    }
    EXPECT_EQ(ids, (std::vector<int>{1, 2}));

    // 序列化直接接收 loaded，自动使用其后端，返回独立的完整文本。
    const auto dumped = mental1104::dump_json(loaded);
    ASSERT_TRUE(dumped) << dumped.error;
    EXPECT_EQ(dumped.text, R"({"items":[{"id":1},{"id":2}]})");
}

/// 省略后端时，所有输入重载都固定选择 cJSON；未构建 cJSON 时明确失败。
TEST(JsonConfiguration, DefaultParserIsCJson) {
    const std::string text = R"({"value":42})";
    const auto from_view = load_json(string_view(text));
    const auto from_string = load_json(text);
    const auto from_literal = load_json(R"({"value":42})");
    // 显式长度路径也必须使用同一个默认后端，不依赖末尾的 NUL。
    const char buffer[] = {
        '{', '"', 'v', 'a', 'l', 'u', 'e', '"', ':', '4', '2', '}'};
    const auto from_buffer = load_json(buffer, sizeof(buffer));
    for (const auto *loaded :
        {&from_view, &from_string, &from_literal, &from_buffer}) {
#if M1104_HAS_JSON_CJSON
        ASSERT_TRUE(*loaded) << loaded->error;
        EXPECT_EQ(loaded->backend(), JsonParser::CJSON);
        EXPECT_EQ(loaded->get().get_optional<int>("value").value_or(-1), 42);
        const auto dumped = dump_json(*loaded);
        ASSERT_TRUE(dumped) << dumped.error;
        EXPECT_EQ(dumped.text, text);
#else
        EXPECT_FALSE(*loaded);
        EXPECT_FALSE(loaded->get().is_valid());
        EXPECT_FALSE(loaded->error.empty());
#endif
    }
    EXPECT_FALSE(load_json(nullptr));
}

/// 已有成员、数组、嵌套读取及默认值入口保持可用。
TEST_P(JsonAccess, ExistingAccessAndTypedPaths) {
    auto parsed = this->parse(R"({"name":"Espeon","age":25,"active":true,
        "hobbies":["cpp","rust"],"address":{"city":"Shenzhen","zip":518000},
        "status":{"settings":{"uuid":"test-id"}}})");
    ASSERT_TRUE(parsed) << parsed.error;
    EXPECT_EQ(parsed.backend(), GetParam());
    const auto root = parsed.get();
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root.get("name").as_string(), string_view("Espeon"));
    EXPECT_EQ(root.get("age").as_i64(), 25);
    EXPECT_TRUE(root.get("active").as_bool());
    EXPECT_EQ(root.get("hobbies").size(), 2u);
    EXPECT_EQ(root.get("hobbies").at(1).as_string_copy(), "rust");
    EXPECT_EQ(root.get({"address", "city"}).as_string_copy(), "Shenzhen");
    EXPECT_EQ(root.get_optional<int>({"address", "zip"}).value_or(-1), 518000);
    EXPECT_EQ(root.get_optional<std::string>({"status", "settings", "uuid"})
                  .value_or(""),
        "test-id");
    const auto settings = root.get_optional<json_consumer::Settings>();
    ASSERT_TRUE(settings);
    EXPECT_EQ(settings->uuid, "test-id");
}

/// 加载结果直接暴露根视图；数组范围遍历保留顺序、嵌套值和严格类型读取。
TEST_P(JsonAccess, LoadedRootSupportsArrayTraversal) {
    auto loaded = this->parse(R"([1,false,null,{"value":"nested"},[2,3]])");
    ASSERT_TRUE(loaded) << loaded.error;
    ASSERT_TRUE(loaded.get().is_array());
    EXPECT_EQ(loaded.get().size(), 5u);
    EXPECT_EQ(loaded.get().at(0).get_optional<int>().value_or(-1), 1);
    EXPECT_FALSE(loaded.get().at(0).get_optional<bool>());

    std::vector<JsonType> types;
    std::string nested;
    int sum = 0;
    for (const auto item : loaded.get()) {
        types.push_back(item.type());
        if (item.is_object()) {
            nested = item.get_optional<std::string>("value").value_or("");
        } else if (item.is_array()) {
            for (const auto number : item) {
                sum += number.get_optional<int>().value_or(0);
            }
        }
    }
    EXPECT_EQ(types, (std::vector<JsonType>{JsonType::Number, JsonType::Bool,
                         JsonType::Null, JsonType::Object, JsonType::Array}));
    EXPECT_EQ(nested, "nested");
    EXPECT_EQ(sum, 5);
    EXPECT_TRUE(loaded.get().at(2).is_null());
    const auto flag = loaded.get().at(1).get_optional<bool>();
    ASSERT_TRUE(flag);
    EXPECT_FALSE(*flag);

    // 借用的游标与已有节点都跟随后端文档，不受加载结果移动影响。
    const auto first = loaded.get().begin();
    auto moved = std::move(loaded);
    EXPECT_FALSE(loaded);
    EXPECT_FALSE(loaded.get().is_valid());
    EXPECT_EQ((*first).as_i64(), 1);
    const auto dumped = dump_json(moved);
    ASSERT_TRUE(dumped) << dumped.error;
    const auto restored = this->parse(dumped.text);
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored.get().size(), 5u);
}

/// 空数组和非数组根产生空遍历范围；失败结果始终提供 Missing 根。
TEST_P(JsonAccess, EmptyAndNonArrayRootsHaveEmptyTraversal) {
    const std::vector<std::string> inputs{
        "[]", "{}", "42", "null", "false", "\"text\""};
    for (const auto &input : inputs) {
        const auto loaded = this->parse(input);
        ASSERT_TRUE(loaded) << input;
        EXPECT_EQ(loaded.get().begin(), loaded.get().end());
        std::size_t count = 0;
        for (const auto item : loaded.get()) {
            (void)item;
            ++count;
        }
        EXPECT_EQ(count, 0u);
    }
    const auto failed = this->parse("[");
    EXPECT_FALSE(failed);
    EXPECT_FALSE(failed.get().is_valid());
    EXPECT_FALSE(failed.get().get_optional<int>());
    EXPECT_EQ(failed.get().begin(), failed.get().end());
    EXPECT_FALSE(dump_json(failed));
}

/// false、0、空字符串不应与失败混淆；null 是实际存在的节点。
TEST_P(JsonAccess, MissingNullAndFalseValuesAreDistinct) {
    auto parsed = this->parse(R"({"no":false,"zero":0,"empty":"","nil":null})");
    ASSERT_TRUE(parsed);
    const auto root = parsed.get();
    EXPECT_TRUE(root.has("nil"));
    EXPECT_TRUE(root.get("nil").is_valid());
    EXPECT_TRUE(root.get("nil").is_null());
    EXPECT_EQ(root.get("absent").type(), JsonType::Missing);
    EXPECT_FALSE(root.has("absent"));
    ASSERT_TRUE(root.get_optional<bool>("no"));
    EXPECT_FALSE(*root.get_optional<bool>("no"));
    EXPECT_EQ(root.get_optional<int>("zero").value_or(-1), 0);
    ASSERT_TRUE(root.get_optional<std::string>("empty"));
    EXPECT_TRUE(root.get_optional<std::string>("empty")->empty());
    EXPECT_FALSE(root.get_optional<int>("nil"));
    EXPECT_FALSE(root.get_optional<int>("absent"));
    EXPECT_FALSE(root.get_optional<int>({"zero", "child"}));
    EXPECT_FALSE(root.get_optional<int>({"nil", "child"}));
    EXPECT_EQ(root.get({}).type(), JsonType::Object);
    EXPECT_FALSE(JsonValueView{}.get_optional<int>());
}

/// 类型检查在适配层完成，不暴露后端断言、异常或隐式转换。
TEST_P(JsonAccess, WrongTypesAndIndicesFailWithoutConversion) {
    auto parsed = this->parse(R"({"number":1,"text":"1","array":[true]})");
    ASSERT_TRUE(parsed);
    const auto root = parsed.get();
    EXPECT_FALSE(root.get_optional<bool>("number"));
    EXPECT_FALSE(root.get_optional<int>("text"));
    EXPECT_FALSE(root.get_optional<std::string>("number"));
    EXPECT_FALSE(root.get("array").get("key").is_valid());
    EXPECT_FALSE(root.at(0).is_valid());
    EXPECT_EQ(root.size(), 0u);
    EXPECT_FALSE(root.get("array").at(1).is_valid());
    EXPECT_FALSE(root.get("array")
                     .at((std::numeric_limits<std::size_t>::max)())
                     .is_valid());
    EXPECT_TRUE(root.get("array").at(0).as_bool());
    EXPECT_FALSE(root.get("array").get_optional<int>());
    EXPECT_EQ(root.get("text").as_i64(99), 99);
}

/// 严格整数读取拒绝小数、符号错误、收窄和浮点边界溢出。
TEST_P(JsonAccess, NumericConversionsCheckBounds) {
    auto parsed = this->parse(R"({"fraction":1.5,"whole":3.0,"negative":-1,
        "small":127,"large":128,"unsigned":255,"overflow":256,
        "signed_limit":9223372036854775808,"unsigned_limit":18446744073709551616,
        "huge":1e100,"minimum":-9223372036854775808})");
    ASSERT_TRUE(parsed) << parsed.error;
    const auto root = parsed.get();
    EXPECT_FALSE(root.get_optional<int>("fraction"));
    EXPECT_EQ(root.get("fraction").as_i64(), 1);
    EXPECT_EQ(root.get_optional<int>("whole").value_or(-1), 3);
    EXPECT_FALSE(root.get_optional<unsigned>("negative"));
    EXPECT_EQ(root.get_optional<std::int8_t>("small").value_or(0), 127);
    EXPECT_FALSE(root.get_optional<std::int8_t>("large"));
    EXPECT_EQ(root.get_optional<std::uint8_t>("unsigned").value_or(0), 255);
    EXPECT_FALSE(root.get_optional<std::uint8_t>("overflow"));
    EXPECT_FALSE(root.get_optional<long long>("signed_limit"));
    EXPECT_EQ(root.get("signed_limit").as_i64(7), 7);
    EXPECT_FALSE(root.get_optional<unsigned long long>("unsigned_limit"));
    EXPECT_FALSE(root.get_optional<long long>("huge"));
    EXPECT_FALSE(root.get_optional<float>("huge"));
    EXPECT_TRUE(root.get_optional<double>("huge"));
    EXPECT_EQ(root.get_optional<long long>("minimum").value_or(0),
        (std::numeric_limits<long long>::min)());
}

/// 文档移动保留原生节点地址，解析器和输入均不参与结果生命周期。
TEST_P(JsonAccess, DocumentsAndViewsHaveIndependentLifetimes) {
    ParseResult parsed;
    {
        std::string input = R"({"value":"owned"})";
        if (GetParam() == JsonParser::CJSON) {
            const CJsonParser parser;
            parsed = parser.parse(string_view(input));
        } else {
            const RapidJsonParser parser;
            parsed = parser.parse(string_view(input));
        }
        input.assign(input.size(), 'x');
    }
    ASSERT_TRUE(parsed);
    const auto root = parsed.get();
    const auto child = root.get("value");
    const auto copy = child.get_optional<std::string>();
    auto moved = std::move(parsed);
    EXPECT_FALSE(parsed.get().is_valid());
    EXPECT_EQ(child.as_string_copy(), "owned");
    ParseResult assigned;
    assigned = std::move(moved);
    // 加载结果移动后根视图为空，布尔判断也必须反映文档已经移出。
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
    EXPECT_FALSE(moved.get().is_valid());
    EXPECT_FALSE(moved);
    EXPECT_EQ(root.get_optional<std::string>("value").value_or(""), "owned");
    assigned = ParseResult{};
    // 文档销毁后仅访问已复制的字符串，不再解引用借用视图。
    EXPECT_EQ(copy.value_or(""), "owned");
}

/// 同一解析器可连续产生多个独立文档，根可以是任意 JSON 类型。
TEST_P(JsonAccess, RepeatedParsingAndScalarRoots) {
    const std::unique_ptr<IJsonParser> parser(
        GetParam() == JsonParser::CJSON
            ? static_cast<IJsonParser *>(new CJsonParser)
            : static_cast<IJsonParser *>(new RapidJsonParser));
    const auto first = parser->parse(string_view("42"));
    const auto second = parser->parse(string_view("null"));
    const auto third = parser->parse(string_view("false"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);
    EXPECT_EQ(first.get().get_optional<int>().value_or(0), 42);
    EXPECT_TRUE(second.get().is_null());
    const auto third_value = third.get().get_optional<bool>();
    ASSERT_TRUE(third_value);
    EXPECT_FALSE(*third_value);
    EXPECT_EQ(first.get().get_optional<int>({}).value_or(0), 42);
}

/// 显式长度输入不要求结束符，错误也不能越界读取输入。
TEST_P(JsonAccess, BoundedBuffersAndErrorOffsets) {
    const char valid[] = {'{', '"', 'x', '"', ':', '1', '}'};
    auto parsed = load_json(valid, sizeof(valid), GetParam());
    ASSERT_TRUE(parsed) << parsed.error;
    EXPECT_EQ(parsed.get().get_optional<int>("x").value_or(0), 1);
    const char bad[] = {'{', '"', 'x', '"', ':', ']'};
    const auto failed = load_json(bad, sizeof(bad), GetParam());
    EXPECT_FALSE(failed);
    EXPECT_FALSE(failed.get().is_valid());
    EXPECT_FALSE(failed.error.empty());
    EXPECT_GT(failed.offset, 0u);
    EXPECT_LE(failed.offset, sizeof(bad));
    EXPECT_FALSE(load_json(nullptr, 0, GetParam()));
    EXPECT_FALSE(load_json(nullptr, 10, GetParam()));
    EXPECT_FALSE(this->parse(""));
    EXPECT_FALSE(this->parse("{} {}"));
    EXPECT_FALSE(this->parse(std::string("{}\0x", 4)));
    EXPECT_TRUE(this->parse("{} \t\r\n"));
}

/// 成员查找区分大小写和完整字节长度，长键、空键均可读取。
TEST_P(JsonAccess, KeysUseCaseAndLength) {
    const std::string key(100, 'k');
    auto parsed = this->parse("{\"\":1,\"Name\":2,\"" + key + "\":3}");
    ASSERT_TRUE(parsed);
    const auto root = parsed.get();
    EXPECT_EQ(root.get_optional<int>(string_view{}).value_or(0), 1);
    EXPECT_EQ(root.get_optional<int>("Name").value_or(0), 2);
    EXPECT_FALSE(root.has("name"));
    EXPECT_EQ(root.get_optional<int>(string_view(key)).value_or(0), 3);
    EXPECT_FALSE(root.has(string_view(key.data(), key.size() - 1)));
    const std::string embedded("Name\0tail", 9);
    EXPECT_FALSE(root.has(string_view(embedded.data(), embedded.size())));
}

/// 两种后端均能输出完整的紧凑 JSON，且序列化不修改原文档或视图。
TEST_P(JsonAccess, DumpRoundTripsNestedValuesAcrossEnabledBackends) {
    auto parsed = this->parse(
        R"({"message":"quote: \" slash: \\ line: \n tab: \t",)"
        R"("unicode":"\u4f60\u597d","empty":"","flag":false,"zero":0,)"
        R"("nested":[null,{"value":-7},3.25]})");
    ASSERT_TRUE(parsed) << parsed.error;
    const auto root = parsed.get();
    const auto message = root.get("message").as_string_copy();
    const auto unicode = root.get("unicode").as_string_copy();
    const auto dumped = dump_json(parsed);
    ASSERT_TRUE(dumped) << dumped.error;
    EXPECT_TRUE(dumped.error.empty());
    EXPECT_EQ(dumped.text.find('\n'), std::string::npos);
    EXPECT_EQ(dumped.text.find('\t'), std::string::npos);
    EXPECT_EQ(root.get("message").as_string_copy(), message);

    for (const auto backend : enabled_backends()) {
        const auto restored = load_json(string_view(dumped.text), backend);
        ASSERT_TRUE(restored) << restored.error;
        const auto value = restored.get();
        EXPECT_EQ(value.get("message").as_string_copy(), message);
        EXPECT_EQ(value.get("unicode").as_string_copy(), unicode);
        EXPECT_EQ(value.get("empty").as_string_copy("fallback"), "");
        EXPECT_FALSE(value.get("flag").as_bool(true));
        EXPECT_EQ(value.get("zero").as_i64(-1), 0);
        EXPECT_EQ(value.get("nested").size(), 3u);
        EXPECT_TRUE(value.get("nested").at(0).is_null());
        EXPECT_EQ(value.get("nested").at(1).get("value").as_i64(), -7);
        EXPECT_DOUBLE_EQ(value.get("nested").at(2).as_f64(), 3.25);
    }
}

/// null、布尔、数字、字符串及空容器都可作为合法序列化根节点。
TEST_P(JsonAccess, DumpSupportsEveryRootType) {
    const std::vector<std::string> inputs{"null", "false", "true", "0", "42",
        "-7", "3.25", "\"\"", "\"a\\nb\"", "[]", "{}"};
    for (const auto &input : inputs) {
        const auto parsed = this->parse(input);
        ASSERT_TRUE(parsed) << input << ": " << parsed.error;
        const auto dumped = dump_json(parsed);
        ASSERT_TRUE(dumped) << input << ": " << dumped.error;
        EXPECT_EQ(dumped.text, input);
        const auto restored = this->parse(dumped.text);
        ASSERT_TRUE(restored) << restored.error;
        EXPECT_EQ(restored.get().type(), parsed.get().type());
    }
}

/// 输出文本不借用源文档；移动源失效与合法的 null 文档保持区分。
TEST_P(JsonAccess, DumpOwnsItsTextAndRejectsMovedFromDocument) {
    DumpResult dumped;
    {
        auto parsed = this->parse(R"({"name":"owned"})");
        ASSERT_TRUE(parsed);
        auto document = std::move(parsed);
        const auto empty = dump_json(parsed);
        EXPECT_FALSE(empty);
        EXPECT_TRUE(empty.text.empty());
        EXPECT_FALSE(empty.error.empty());
        dumped = dump_json(document);
        ASSERT_TRUE(dumped) << dumped.error;
    }
    const auto restored = this->parse(dumped.text);
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored.get().get("name").as_string_copy(), "owned");
}

INSTANTIATE_TEST_SUITE_P(
    Backends, JsonAccess, testing::ValuesIn(enabled_backends()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(JsonAccess);

/// 调用方自有的只读标量后端，固定返回 42，用于验证开放扩展而非原生 SDK。
/// 各个匿名节点参数仅表达借用契约，在本替身中故意不解引用；它不支持对象或数组访问。
class ConstantDocument : public IJsonDocument {
private:
    /// @return 以当前文档地址作为唯一的标量句柄。
    Node do_root() const noexcept override { return this; }
    /// 标量后端始终提供数值类型。
    /// @param node 当前文档的借用句柄。
    /// @return Number。
    JsonType do_type(Node) const noexcept override { return JsonType::Number; }
    /// 标量没有对象成员。
    /// @param node 当前文档的借用句柄。
    /// @param key 未使用的只读键。
    /// @return nullptr。
    Node do_get(Node, const string_view &) const override { return nullptr; }
    /// 标量没有数组长度。
    /// @param node 当前文档的借用句柄。
    /// @return 0。
    std::size_t do_size(Node) const noexcept override { return 0; }
    /// 标量没有数组元素。
    /// @param node 当前文档的借用句柄。
    /// @param index 未使用的索引。
    /// @return nullptr。
    Node do_at(Node, std::size_t) const noexcept override { return nullptr; }
    /// 标量不转换为字符串。
    /// @param node 当前文档的借用句柄。
    /// @return 空结果。
    JsonOptional<string_view> do_string(Node) const override { return {}; }
    /// 标量不转换为布尔。
    /// @param node 当前文档的借用句柄。
    /// @return 空结果。
    JsonOptional<bool> do_bool(Node) const override { return {}; }
    /// 提供固定有符号整数。
    /// @param node 当前文档的借用句柄。
    /// @return 42。
    JsonOptional<long long> do_i64(Node) const override { return 42; }
    /// 提供固定无符号整数。
    /// @param node 当前文档的借用句柄。
    /// @return 42。
    JsonOptional<unsigned long long> do_u64(Node) const override { return 42; }
    /// 提供固定浮点值。
    /// @param node 当前文档的借用句柄。
    /// @return 42.0。
    JsonOptional<double> do_f64(Node) const override { return 42.0; }
};

/// 调用方只需覆写编解码钩子，即可为自有文档接入序列化。
class SerializableConstantDocument final : public ConstantDocument {
private:
    /// 输出此后端的固定数值，不依赖内置后端枚举或注册表。
    /// @return 独立文本 42。
    DumpResult do_dump() const override {
        DumpResult result;
        result.text = "42";
        result.ok = true;
        return result;
    }
};

/// 最小应用侧解析器，用于验证第三方扩展无需注册。
class ConstantParser final : public IJsonParser {
public:
    /// 创建固定文档，不解释输入内容。
    /// @param text 在此测试适配器中忽略的只读输入。
    /// @return 独占的自定义文档。
    ParseResult parse(const string_view &) const override {
        return ParseResult(
            JsonDoc(std::unique_ptr<IJsonDocument>(new ConstantDocument)));
    }
};

/// 基类分派、文档所有权和模板读取对应用自有后端同样有效。
TEST(JsonExtension, ApplicationBackendNeedsNoRegistry) {
    const std::unique_ptr<IJsonParser> parser(new ConstantParser);
    auto parsed = parser->parse(string_view("ignored"));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed.backend(), JsonParser::Custom);
    EXPECT_EQ(parsed.get().get_optional<int>().value_or(0), 42);
    EXPECT_FALSE(load_json(string_view("42"), JsonParser::Custom));
}

/// 独立翻译单元可组合调用方自定义类型与运行时核心。
TEST(JsonExtension, CallerDefinedModelLinksAcrossTranslationUnits) {
    EXPECT_TRUE(json_consumer::verify());
}

/// 空文档和不支持序列化的应用后端均给出明确错误，不输出伪造的 null。
TEST(JsonExtension, DumpRejectsEmptyAndUnsupportedDocuments) {
    const auto empty = dump_json(JsonDoc{});
    EXPECT_FALSE(empty);
    EXPECT_FALSE(empty.error.empty());
    EXPECT_TRUE(empty.text.empty());

    const ParseResult empty_result;
    EXPECT_FALSE(empty_result);
    EXPECT_FALSE(empty_result.get().is_valid());
    EXPECT_FALSE(dump_json(empty_result));
    const ParseResult empty_owner(JsonDoc{});
    EXPECT_FALSE(empty_owner.ok);
    EXPECT_FALSE(empty_owner);

    const auto parsed = ConstantParser{}.parse(string_view("ignored"));
    ASSERT_TRUE(parsed);
    const auto unsupported = dump_json(parsed);
    EXPECT_FALSE(unsupported);
    EXPECT_FALSE(unsupported.error.empty());
    EXPECT_TRUE(unsupported.text.empty());
}

/// dump_json 通过文档虚函数分派，不根据内置后端枚举强制转换。
TEST(JsonExtension, ApplicationBackendControlsSerialization) {
    const JsonDoc document(
        std::unique_ptr<IJsonDocument>(new SerializableConstantDocument));
    EXPECT_EQ(document.backend(), JsonParser::Custom);
    const auto dumped = dump_json(document);
    ASSERT_TRUE(dumped) << dumped.error;
    EXPECT_EQ(dumped.text, "42");
    EXPECT_EQ(document.root().as_i64(), 42);
}

/// 禁用的后端必须返回明确错误，不静默切换到另一个库。
TEST(JsonConfiguration, DisabledBackendsReportFailure) {
#if !M1104_HAS_JSON_CJSON
    const auto cjson = CJsonParser{}.parse(string_view("{}"));
    EXPECT_FALSE(cjson);
    EXPECT_FALSE(cjson.error.empty());
#endif
#if !M1104_HAS_JSON_RAPIDJSON
    const auto rapid = RapidJsonParser{}.parse(string_view("{}"));
    EXPECT_FALSE(rapid);
    EXPECT_FALSE(rapid.error.empty());
#endif
}

#if M1104_HAS_JSON_RAPIDJSON
/// RapidJSON 保留完整整数和内嵌 NUL；这些是明确的后端能力差异。
TEST(JsonRapidCapabilities, FullWidthIntegersAndEmbeddedNulStrings) {
    const auto parsed = RapidJsonParser{}.parse(string_view(
        R"({"max":9223372036854775807,"umax":18446744073709551615,"s":"a\u0000b"})"));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed.get().get_optional<long long>("max").value_or(0),
        (std::numeric_limits<long long>::max)());
    EXPECT_EQ(parsed.get().get_optional<unsigned long long>("umax").value_or(0),
        (std::numeric_limits<unsigned long long>::max)());
    EXPECT_FALSE(parsed.get().get_optional<long long>("umax"));
    EXPECT_EQ(parsed.get().get_optional<std::string>("s").value_or(""),
        std::string("a\0b", 3));
    const auto dumped = dump_json(parsed);
    ASSERT_TRUE(dumped) << dumped.error;
    EXPECT_EQ(dumped.text.find('\0'), std::string::npos);
    const auto restored = RapidJsonParser{}.parse(string_view(dumped.text));
    ASSERT_TRUE(restored) << restored.error;
    EXPECT_EQ(restored.get().get_optional<long long>("max").value_or(0),
        (std::numeric_limits<long long>::max)());
    EXPECT_EQ(
        restored.get().get_optional<unsigned long long>("umax").value_or(0),
        (std::numeric_limits<unsigned long long>::max)());
    EXPECT_EQ(restored.get().get_optional<std::string>("s").value_or(""),
        std::string("a\0b", 3));
}
#endif

#if M1104_HAS_JSON_CJSON
/// cJSON 能解析溢出的 double，但序列化必须报告错误而不是静默输出 null。
TEST(JsonCJsonCapabilities, NonFiniteNumbersCannotBeDumpedAsNull) {
    const std::vector<std::string> inputs{"1e9999", "{\"nested\":[1e9999]}"};
    for (const auto &input : inputs) {
        const auto parsed = CJsonParser{}.parse(string_view(input));
        ASSERT_TRUE(parsed) << parsed.error;
        const auto dumped = dump_json(parsed);
        EXPECT_FALSE(dumped);
        EXPECT_FALSE(dumped.error.empty());
        EXPECT_TRUE(dumped.text.empty());
    }
}
#endif
}  // namespace
