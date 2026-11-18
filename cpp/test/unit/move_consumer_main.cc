#include <cstdio>
#include <fstream>

#include "move_consumer.h"

/// 最小完整消费：准备并关闭源，公开移动、跨 TU 核验及回收；返回 0 表示通过。
int main() {
    const std::string source = "m1104-move-consumer-source.bin",
                      target = "m1104-move-consumer-target.bin";
    // CTest/安装验证在独占构建或临时目录运行，文件名由本消费者拥有。
    {
        std::ofstream output(source, std::ios::binary);
        output << "hello";
        if (!output.good()) return 1;
    }
    const auto result = mental1104::file::move_file(source, target);
    // 核对公开结果的跨翻译单元布局、目标内容及源消失，再释放目标文件。
    std::ifstream old(source, std::ios::binary);
    const bool okay = !old.good() && verify_move_consumer(result, target);
    old.close();
    return std::remove(target.c_str()) == 0 && okay ? 0 : 1;
}
