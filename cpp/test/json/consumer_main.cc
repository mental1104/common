#include "consumer.h"

/// 使用实际库标准编译的独立调用方，不受 GoogleTest 最低标准影响。
/// @return 公共模板和已编译核心能够组合时返回 0。
int main() { return json_consumer::verify() ? 0 : 1; }
