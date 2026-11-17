#include <gtest/gtest.h>

#include "mental1104/common/c_api_compat.h"
#include "mental1104/debug/stacktrace.h"

COMMON_EXTERN_C int m1104_c_api_compat_add_from_c(int lhs, int rhs);

TEST(CApiCompatTest, CCompilerSeesCompatMacrosAndCppLinksCFunction) {
  EXPECT_EQ(m1104_c_api_compat_add_from_c(2, 3), 5);
}

TEST(CApiCompatTest, CppCanIncludeStacktraceCApiHeader) {
  st_options_t options = {};
  options.enable = 0;
  EXPECT_EQ(options.enable, 0);
}
