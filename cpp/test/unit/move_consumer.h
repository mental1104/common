#ifndef M1104_MOVE_CONSUMER_H
#define M1104_MOVE_CONSUMER_H
#include <mental1104/file/move.h>

/// 在独立翻译单元核验 result 及 target 内容；参数均为只读借用。
/// @return 状态符合平台合同且目标为 hello 时为 true，不获取持久资源。
bool verify_move_consumer(
    const mental1104::file::MoveResult& result, const std::string& target);
#endif
