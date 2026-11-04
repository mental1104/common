#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "json/consumer.h"
#include "mental1104/json.h"

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

/// 两种原生后端共享同一组公共接口契约。
class JsonAccess : public testing::TestWithParam<JsonParser> {
protected:
    /// 用当前参数选择后端，不让场景依赖原生 JSON 类型。
    /// @param text 只读输入，不保存其引用。
    /// @return 独占解析结果。
    ParseResult parse(const std::string &text) const {
        return parse_json(text.data(), text.size(), GetParam());
    }
};

/// 已有成员、数组、嵌套读取及默认值入口保持可用。
TEST_P(JsonAccess, ExistingAccessAndTypedPaths) {
    auto parsed = this->parse(R"({"name":"Espeon","age":25,"active":true,
        "hobbies":["cpp","rust"],"address":{"city":"Shenzhen","zip":518000},
        "status":{"settings":{"uuid":"test-id"}}})");
    ASSERT_TRUE(parsed) << parsed.error;
    EXPECT_EQ(parsed.doc.backend(), GetParam());
    const auto root = parsed.doc.root();
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
    const auto settings = root.as_optional<json_consumer::Settings>();
    ASSERT_TRUE(settings);
    EXPECT_EQ(settings->uuid, "test-id");
}

/// false、0、空字符串不应与失败混淆；null 是实际存在的节点。
TEST_P(JsonAccess, MissingNullAndFalseValuesAreDistinct) {
    auto parsed = this->parse(R"({"no":false,"zero":0,"empty":"","nil":null})");
    ASSERT_TRUE(parsed);
    const auto root = parsed.doc.root();
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
    EXPECT_FALSE(JsonValueView{}.as_optional<int>());
}

/// 类型检查在适配层完成，不暴露后端断言、异常或隐式转换。
TEST_P(JsonAccess, WrongTypesAndIndicesFailWithoutConversion) {
    auto parsed = this->parse(R"({"number":1,"text":"1","array":[true]})");
    ASSERT_TRUE(parsed);
    const auto root = parsed.doc.root();
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
    EXPECT_FALSE(root.get("array").as_optional<int>());
    EXPECT_EQ(root.get("text").as_i64(99), 99);
}

/// 严格整数读取拒绝小数、符号错误、收窄和浮点边界溢出。
TEST_P(JsonAccess, NumericConversionsCheckBounds) {
    auto parsed = this->parse(R"({"fraction":1.5,"whole":3.0,"negative":-1,
        "small":127,"large":128,"unsigned":255,"overflow":256,
        "signed_limit":9223372036854775808,"unsigned_limit":18446744073709551616,
        "huge":1e100,"minimum":-9223372036854775808})");
    ASSERT_TRUE(parsed) << parsed.error;
    const auto root = parsed.doc.root();
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
    const auto root = parsed.doc.root();
    const auto child = root.get("value");
    const auto copy = child.as_optional<std::string>();
    auto moved = std::move(parsed.doc);
    EXPECT_FALSE(parsed.doc.valid());
    EXPECT_EQ(child.as_string_copy(), "owned");
    JsonDoc assigned;
    assigned = std::move(moved);
    // JsonDoc 明确保证移动后可查询空状态，此处专门验证该契约。
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
    EXPECT_FALSE(moved.valid());
    EXPECT_EQ(root.get_optional<std::string>("value").value_or(""), "owned");
    assigned = JsonDoc{};
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
    EXPECT_EQ(first.doc.root().as_optional<int>().value_or(0), 42);
    EXPECT_TRUE(second.doc.root().is_null());
    const auto third_value = third.doc.root().as_optional<bool>();
    ASSERT_TRUE(third_value);
    EXPECT_FALSE(*third_value);
    EXPECT_EQ(first.doc.root().get_optional<int>({}).value_or(0), 42);
}

/// 显式长度输入不要求结束符，错误也不能越界读取输入。
TEST_P(JsonAccess, BoundedBuffersAndErrorOffsets) {
    const char valid[] = {'{', '"', 'x', '"', ':', '1', '}'};
    auto parsed = parse_json(valid, sizeof(valid), GetParam());
    ASSERT_TRUE(parsed) << parsed.error;
    EXPECT_EQ(parsed.doc.root().get_optional<int>("x").value_or(0), 1);
    const char bad[] = {'{', '"', 'x', '"', ':', ']'};
    const auto failed = parse_json(bad, sizeof(bad), GetParam());
    EXPECT_FALSE(failed);
    EXPECT_FALSE(failed.doc.valid());
    EXPECT_FALSE(failed.error.empty());
    EXPECT_GT(failed.offset, 0u);
    EXPECT_LE(failed.offset, sizeof(bad));
    EXPECT_FALSE(parse_json(nullptr, 0, GetParam()));
    EXPECT_FALSE(parse_json(nullptr, 10, GetParam()));
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
    const auto root = parsed.doc.root();
    EXPECT_EQ(root.get_optional<int>(string_view{}).value_or(0), 1);
    EXPECT_EQ(root.get_optional<int>("Name").value_or(0), 2);
    EXPECT_FALSE(root.has("name"));
    EXPECT_EQ(root.get_optional<int>(string_view(key)).value_or(0), 3);
    EXPECT_FALSE(root.has(string_view(key.data(), key.size() - 1)));
    const std::string embedded("Name\0tail", 9);
    EXPECT_FALSE(root.has(string_view(embedded.data(), embedded.size())));
}

INSTANTIATE_TEST_SUITE_P(
    Backends, JsonAccess, testing::ValuesIn(enabled_backends()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(JsonAccess);

/// 调用方自有后端，不需要修改公共枚举分派或内置后端源文件。
class ConstantDocument final : public IJsonDocument {
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

/// 最小应用侧解析器，用于验证第三方扩展无需注册。
class ConstantParser final : public IJsonParser {
public:
    /// 创建固定文档，不解释输入内容。
    /// @param text 在此测试适配器中忽略的只读输入。
    /// @return 独占的自定义文档。
    ParseResult parse(const string_view &) const override {
        ParseResult result;
        result.doc =
            JsonDoc(std::unique_ptr<IJsonDocument>(new ConstantDocument));
        result.ok = true;
        return result;
    }
};

/// 基类分派、文档所有权和模板读取对应用自有后端同样有效。
TEST(JsonExtension, ApplicationBackendNeedsNoRegistry) {
    const std::unique_ptr<IJsonParser> parser(new ConstantParser);
    auto parsed = parser->parse(string_view("ignored"));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed.doc.backend(), JsonParser::Custom);
    EXPECT_EQ(parsed.doc.root().as_optional<int>().value_or(0), 42);
    EXPECT_FALSE(parse_json(string_view("42"), JsonParser::Custom));
}

/// 独立翻译单元可组合调用方自定义类型与运行时核心。
TEST(JsonExtension, CallerDefinedModelLinksAcrossTranslationUnits) {
    EXPECT_TRUE(json_consumer::verify());
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
    EXPECT_EQ(parsed.doc.root().get_optional<long long>("max").value_or(0),
        (std::numeric_limits<long long>::max)());
    EXPECT_EQ(
        parsed.doc.root().get_optional<unsigned long long>("umax").value_or(0),
        (std::numeric_limits<unsigned long long>::max)());
    EXPECT_FALSE(parsed.doc.root().get_optional<long long>("umax"));
    EXPECT_EQ(parsed.doc.root().get_optional<std::string>("s").value_or(""),
        std::string("a\0b", 3));
}
#endif
}  // namespace
