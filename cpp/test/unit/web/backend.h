#pragma once

#include "mental1104/web/server.h"

namespace web_test {
/// 每个测试程序链接一个后端接入文件；场景只通过公共 Server 使用它。
struct Backend {
    const char *name;
    bool (*create)(const mental1104::web::ServerOptions &,
        std::unique_ptr<mental1104::web::Server> &) noexcept;
    bool tls;
};

/// 返回本测试程序选择的后端，定义由独立接入文件提供。
/// @return 进程内有效的只读后端描述。
const Backend &backend();
}  // namespace web_test
