#include "mental1104/json.h"
#include <cstring>
#include <gtest/gtest.h>

using namespace mental1104;

static const char *kJson = R"JSON(
{
  "name": "Espeon",
  "age": 25,
  "active": true,
  "hobbies": ["cpp", "rust"],
  "address": { "city": "Shenzhen", "zip": 518000 }
}
)JSON";

static void run_case(JsonParser backend) {
  auto r = parse_json(kJson, std::strlen(kJson), backend);
  ASSERT_TRUE(r.ok) << "parse failed: " << r.error << " @" << r.offset;

  auto root = r.doc.root();
  ASSERT_TRUE(root.is_object());

  EXPECT_EQ(root.get("name").as_string(), std::string_view("Espeon"));
  EXPECT_EQ(root.get("age").as_i64(), 25);
  EXPECT_TRUE(root.get("active").as_bool());

  auto hobbies = root.get("hobbies");
  ASSERT_TRUE(hobbies.is_array());
  ASSERT_EQ(hobbies.size(), 2u);
  EXPECT_EQ(hobbies.at(0).as_string(), std::string_view("cpp"));
  EXPECT_EQ(hobbies.at(1).as_string(), std::string_view("rust"));

  auto addr = root.get("address");
  ASSERT_TRUE(addr.is_object());
  EXPECT_EQ(addr.get("city").as_string(), std::string_view("Shenzhen"));
  EXPECT_EQ(addr.get("zip").as_i64(), 518000);
}

TEST(JsonIface, RapidJSON) { run_case(JsonParser::RapidJSON); }
TEST(JsonIface, CJSON) { run_case(JsonParser::CJSON); }

TEST(JsonIface, ParseErrorBoth) {
  const char *bad = "{ \"a\": [1,2, }";
  auto r1 = parse_json(bad, std::strlen(bad), JsonParser::RapidJSON);
  EXPECT_FALSE(r1.ok);
  auto r2 = parse_json(bad, std::strlen(bad), JsonParser::CJSON);
  EXPECT_FALSE(r2.ok);
}
