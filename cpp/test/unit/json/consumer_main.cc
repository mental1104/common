/// @file
/// 不依赖 GoogleTest 的消费端入口，仅以退出码报告跨翻译单元验证结果。
#include "consumer.h"

/// 使用实际库标准编译的独立调用方，不受 GoogleTest 最低标准影响。
/// @return 公共模板和已编译核心能够组合时返回 0。
int main() { return json_consumer::verify() ? 0 : 1; }
