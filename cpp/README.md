# C++ 工具库

公共 C++ 头文件位于 `cpp/include/mental1104`。下面的包含示例假设已经通过仓库 CMake 安装，或包含路径中已有 `cpp/include`。

## 从源码构建

仓库根目录和 `cpp/` 目录都支持标准 CMake 流程。机器已有 CMake、C/C++ 编译器及构建工具后，在仓库根目录执行：

```sh
mkdir -p build && cd build
cmake ..
cmake --build . --parallel 4
ctest --output-on-failure
```

**命令结果：** 配置阶段自动初始化缺失的 Git 子模块，准备 hiredis/redis++ SDK，并探测可用能力；编译生成 `lib/` 与 `bin/`，CTest 成功时退出码为 `0`。`cmake ..` 完成依赖准备与构建系统生成，后两条命令完成编译、测试。默认标准为 C++23，可用 `-DCMAKE_CXX_STANDARD=11` 等选项覆盖。

Debian/Ubuntu 本机独立构建默认用 apt 补齐 Git、Python 3、pkg-config、Boost 及启用功能所需的 GMP/MPFR、lcov、MQ 客户端 SDK；无需手动下载 SDK，也不新增 apt 源。首次准备需要网络，安装系统包需要 root 或可非交互执行 apt 的 `sudo -n`。其他平台需预先提供系统开发包。未配置 Redis/Kafka/Pulsar 服务时，相关真实收发测试按原有约定跳过。

`M1104_AUTO_SETUP` 控制自动源码/SDK 准备，独立本机构建默认开启；被其他工程包含、设置了 `CI` 环境变量或交叉编译时默认关闭。离线构建可用 `-DM1104_AUTO_SETUP=OFF` 复用已经提供的依赖；仅关闭系统包安装可用 `-DM1104_INSTALL_SYSTEM_DEPS=OFF`，源码准备仍然执行。`AUTO_FETCH_SUBMODULES=OFF` 禁止自动拉取缺失源码。SDK 二进制按构建树存于 `_deps/`，避免不同 C++ 标准的 SDK 相互覆盖；`M1104_DEPENDENCY_JOBS` 默认 `4`，可调整准备阶段并发数。

单元/行为测试及其辅助文件位于 `cpp/test/unit/`，性能基准位于 `cpp/test/bench/`。默认构建只编译单元测试，普通 `ctest` 也只执行单元测试；基准目标保留在同一构建图中，按需构建。

在仓库根目录执行 `./dev test-cpp`，始终只运行 `unit` 标签的测试；`./dev bench-cpp` 构建 `m1104_benchmarks` 并运行 `bench/` 当前配置启用的程序，沿用 `--file` 和 `--filter` 筛选。基准结果写入 `artifacts/bench/cpp/`。两类测试链接同一份业务库，共享的 JSON 消费端辅助代码也只通过对象库编译一次。此约定以同一构建目录和编译配置为单位，不跨不同标准或 Debug/Release 复用对象。

若要让普通构建与 CTest 显式包含基准，在原构建目录配置：

```sh
cmake -DM1104_REGISTER_BENCHMARK_TESTS=ON .
cmake --build . --parallel 4
ctest -L bench --output-on-failure
```

**命令结果：** 构建缺失的基准产物，并仅执行 `bench` 标签的性能用例；此配置下直接 `ctest` 会执行两类测试，`./dev test-cpp` 仍只执行单元测试。用 `-DM1104_REGISTER_BENCHMARK_TESTS=OFF` 恢复默认注册方式，已有库和测试对象保持复用。

### JSON parser 基准

`cpp/test/bench/bench_json_gbench.cpp` 使用同一程序比较已启用的 cJSON、RapidJSON 和 nlohmann JSON。一次操作为 `load_json`、成功状态检查及文档释放，包含公共校验、DOM 分配和销毁，不含输入生成、文件读取、业务字段遍历或序列化。参数 `0` 是短对象，`64/256/1024` 是嵌套文档中的用户数；实际输入长度由 `input_bytes` 给出。

在已准备仓库依赖的环境中，从仓库根目录运行下列命令。NAS 工作区在 `code-server` 容器内执行，Linux 可在最后一条命令前加 `taskset -c 2` 固定到一个可用 CPU：

```sh
cmake -S cpp -B cpp/build/json-bench-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23 \
  -DENABLE_COVERAGE=OFF -DCOVERAGE=OFF \
  -DM1104_AUTO_SETUP=OFF -DAUTO_FETCH_SUBMODULES=OFF \
  -DM1104_ENABLE_WEB=OFF -DM1104_ENABLE_REDIS_LOCK=OFF \
  -DM1104_ENABLE_HIGH_PRECISION=OFF -DM1104_ENABLE_BOOST_ASIO=OFF \
  -DM1104_ENABLE_KAFKA=OFF -DM1104_ENABLE_PULSAR=OFF
cmake --build cpp/build/json-bench-release --target bench_bench_json_gbench --parallel 4
mkdir -p artifacts/bench/cpp/json-parser
cpp/build/json-bench-release/bin/bench_bench_json_gbench \
  --benchmark_enable_random_interleaving=true \
  --benchmark_out=artifacts/bench/cpp/json-parser/results.json \
  --benchmark_out_format=json
```

**命令结果：** 运行三个后端各四种输入规模；每组预热至少 `0.1s`，每个吞吐 trial 至少 `0.3s`，重复五次并交错顺序。标准输出及 JSON 包含每次 trial 和 `mean/median/stddev/cv` 汇总，其中 `cv` 为变异系数。可用 `--benchmark_filter='BM_iface_.*/0/'` 只测短对象。现有 `./dev bench-cpp --file json` 也能发现这些用例，但使用其固定的 `cpp/build` 配置；不要把 Debug 或覆盖率结果用于后端性能排名。

- `ops_per_second` 和 `bytes_per_second` 为单线程墙钟吞吐。每个 trial 的吞吐计时结束后，另测 `512` 次相同操作，输出 `p50_ns/p95_ns/p99_ns/max_ns`；延迟包含读钟成本，尤其对短对象有影响，不能拿采样 p50 的倒数替代吞吐。
- POSIX 平台同时输出吞吐批次的 `user_s/sys_s/cpu_pct`、自愿及非自愿上下文切换次数。`rss_peak_kib` 是整个进程累计峰值，包含前面的后端，不能据此比较各后端独立内存占用；非 POSIX 平台不输出这些资源指标。
- 同一进程、相同输入和优化参数便于比较当前统一 API 下的后端成本。频率调整、其他进程争用、缓存和分配器状态仍会影响结果，应结合吞吐与 p99 的 CV 判断稳定性。这组基准没有旧版本或原生 SDK 对照，不能单独证明 NVI 改动增加或减少了多少开销。

## 声明、实现与链接

本库采用模板头文件与已编译运行时核心组合的交付方式。使用运行时能力需要链接 `mental1104`；仅添加 `cpp/include` 不能替代链接。通过仓库 CMake target 使用时，链接 `mental1104` 会携带它公开的依赖和编译定义；安装后的手动消费需要显式配置安装目录中的 include、lib 与运行时库搜索路径。

源码树手动消费时，还须把生成配置头目录加入搜索路径：从 `cpp/` 构建为 `cpp/build/include`，从仓库根目录构建为 `build/cpp/include`。两个执行器适配器头直接要求这个配置文件，避免缺失配置时静默使用不同的基类布局。自包含模板与编译期工具仍可直接包含源码头文件。

| 保留在头文件 | 由库提供的实现 |
|---|---|
| 特性宏、类型别名、traits、LRU/LFU 模板、开放类型包装 | 无需为编译期类型声明单独调用运行时入口 |
| `contains`、`scope_exit` 等自包含模板 | 这些自包含入口仍可仅包含头文件使用 |
| `ExponentialBackoff` 的泛型重载、随机引擎适配、`Timed`、Bloom filter 的 Key/Hash 适配 | 退避状态推进、随机种子、计时输出、Bloom 参数计算 |
| 日志参数及容器格式化、`ThreadPool::submit` 的结果类型适配 | 统一日志等级与输出、线程池入队及工作线程管理 |
| M:N 池组合模板、协程 promise 类型、执行器/调度器声明 | 执行器、调度循环、等待、Task 帧管理；协程实现需要 C++20+ |
| JSON、Redis 锁、高精度计算器、文件/描述符 RAII 的公开声明 | 对应解析、SDK 协议、算法、资源释放等普通函数与成员 |

头文件中仍可能需要公开返回值或按值成员依赖的 Boost 等具体类型；JSON 原生后端类型已隐藏。库与消费方必须采用匹配的标准、optional/string_view 类型和 async_simple 等接口宏。生成并安装的 `mental1104/build_config.h` 记录可选能力及 async_simple 布局配置，公共兼容头会自动读取它；启用相关能力时同时安装使用到的仓库内 Boost 和 async_simple 公开头文件。当前交付不承诺一个库二进制跨所有 C++ 标准混用。

以下 CMake 开关默认开启：`M1104_ENABLE_JSON`、`M1104_ENABLE_REDIS_LOCK`、`M1104_ENABLE_HIGH_PRECISION`、`M1104_ENABLE_BOOST_ASIO`。关闭开关时相应实现与测试不参与构建。JSON 公共核心不依赖原生 SDK；`M1104_ENABLE_JSON_CJSON`、`M1104_ENABLE_JSON_RAPIDJSON` 在依赖可用时独立启用后端。`M1104_HAS_JSON`、`M1104_HAS_JSON_CJSON`、`M1104_HAS_JSON_RAPIDJSON` 记录公共核心与后端可用性。其他能力仍按实际依赖决定 `M1104_HAS_REDIS_LOCK`、`M1104_HAS_HIGH_PRECISION`、`M1104_HAS_BOOST_ASIO`：分别需要 hiredis/redis++、MPFR/GMP 与 Boost、Boost.Asio。

日志全局等级及后端输出由库中的一个运行时核心管理。`MENTAL1104_LOG_LEVEL` 环境变量与 `set_log_level` 仍在运行时生效；`M1104_LOG_DEFAULT_LEVEL` 和可选 spdlog 后端按构建库时的配置决定，消费端重新定义宏不能重编译已经链接的日志实现。计时器原有的每翻译单元构造互斥量保留，未改成全局单例。

`test_build_config` 刻意不继承主库 target 的编译宏，仅使用公开头、生成配置头和链接文件，验证条件基类与已编译构造/析构函数的布局一致。

独立 `export/cpp` 构建复用 `cpp/src/json/` 的运行时源文件并生成自己的后端配置，不要求链接整个主库。普通测试之外，`test_linkage` 和 `test_json_consumer` 通过多个翻译单元验证调用方模板与已编译核心。消费端按库标准编译；若新版 GoogleTest 把驱动提升至 C++17，C++11/14 通过不依赖 GoogleTest 的 `test_json_consumer` 验证 JSON，避免 optional/string_view 布局不匹配。高精度实现也会按标准试编译，不兼容时仅排除该能力。

## 独立 HTTP / WebSocket 服务端

此模块使用 C++17+，与原有 `mental1104`、`EpollServer`、cJSON/RapidJSON 能力独立。
公共声明位于 `include/mental1104/web/`，运行时实现位于 `src/web/`。
应用处理代码只包含 `mental1104/web/server.h`；装配代码通过
`mental1104/web/drogon_server.h` 的工厂选择后端。更换框架时实现同一 `Server` /
`WebSocketSession` 契约，复用应用路由与处理函数，仅替换装配处的工厂和链接目标。

### 构建、依赖与安装

先提供 Drogon 1.8+ 的开发包及其 CMake 配置。Ubuntu 24.04 的发行版包可用以下命令准备，
发行版 Drogon CMake 包还会查找构建它时启用的可选 SDK：

```sh
sudo apt-get update
sudo apt-get install -y libdrogon-dev libjsoncpp-dev libyaml-cpp-dev libbrotli-dev \
  libsqlite3-dev libmariadb-dev libpq-dev uuid-dev zlib1g-dev libhiredis-dev
cmake -S cpp -B build-web -DCMAKE_BUILD_TYPE=Debug -DM1104_ENABLE_WEB=ON
cmake --build build-web --parallel 4
ctest --test-dir build-web -R '^(test_web_lifecycle|web\.)' --output-on-failure
cmake --install build-web --component Web --prefix /tmp/common-web-install
```

**命令结果：** 生成 `mental1104_web`、`mental1104_web_drogon` 及两个测试程序；
CTest 成功时退出码为 `0`。构建定义集中在 `cmake/web.cmake`，复用原有依赖准备流程。
安装目录含公共头、共享库与 `mental1104WebConfig.cmake`。
`--component Web` 只安装 Web 模块，不执行原有主库和第三方 SDK 的安装规则。

Ubuntu 24.04 的 Trantor 发行版二进制未启用 TLS，以上 apt 命令可验证 HTTP/WebSocket；
HTTPS 需要链接启用 OpenSSL 的 SDK，否则启动明确报错。当前 code-server 镜像已从固定的
发行版源码构建 TLS 版本到 `/usr/local`，不需要在容器内临时安装或重编译。
网络场景测试额外需要 Boost 1.70+、OpenSSL 开发包及 `openssl` 命令；
这些测试依赖已包含在 code-server 镜像中，不进入公共 Web 库的依赖。
配置时生成构建目录内的临时证书，并单独执行 HTTPS 测试。
没有 TLS 的 SDK 会跳过该用例；code-server 镜像构建另外强制检查 TLS 能力。

已有 `./dev` / 主库构建也可配置 `-DM1104_ENABLE_WEB=ON` 后构建；该开关默认关闭，
因此 C++11/14 和未安装 Drogon 的原有构建不受影响。Web 模块配置选项如下：

| 选项 | 默认值 | 用途 |
|---|---|---|
| `M1104_WEB_WITH_DROGON` | `ON` | 构建 Drogon 后端；关闭时只构建公共 API 运行时，不查找 Drogon |
| `M1104_WEB_BUILD_TESTS` | `ON` | 构建公共 API 测试及真实 HTTP/WebSocket 契约测试 |

编辑 Web 后端时，开发构建须启用 `M1104_ENABLE_WEB=ON`，使
`cpp/build/compile_commands.json` 包含后端的真实编译参数。仓库 `.clangd` 指向该目录，
VS Code CMake 配置默认启用 Web 目标；此编辑器选项不改变命令行 CMake 开关的默认值。
编辑器构建目录显式固定为 `cpp/build`，打开工程及保存 CMake 文件时自动配置，
避免自动配置写入另一份数据库。新增源文件、切换功能开关或 SDK 后应确认配置成功；
编译参数变化后若已打开文件仍保留旧诊断，执行 `clangd: Restart language server`。
若编辑器报 `json/json.h` 缺失，随后出现 `registerHandlerViaRegex` 参数变成 `const int&`
或 `<recovery-expr>`，先重新配置 CMake 并检查编译数据库，不要手工向公共头添加 SDK 依赖。

安装消费使用 `find_package(mental1104Web CONFIG REQUIRED)`，应用接口代码链接
`mental1104::web`；选择 Drogon 的最终程序链接 `mental1104::web_drogon`，其公开依赖
自动包含前者。Drogon/Trantor 是后端共享库的私有依赖，消费工程不需要包含 SDK 头、
查找 Drogon CMake 包或定义其编译宏，但运行机器仍须提供对应 SDK 动态库。
库与调用者仍需使用匹配的编译器 ABI 和标准库配置，不承诺跨 ABI 二进制兼容。

### 公共 API 与最小用法

以下名称均位于 `mental1104::web`，状态为 **待复核**；这是第一版契约。

| 类别 | 名称 | 类型 | 定义位置 / 包含 | 用途 |
|---|---|---|---|---|
| HTTP | `Method`, `Status`, `Header` | 枚举 / 结构体 | `mental1104/web/server.h` | 请求方法、响应状态与头字段 |
| HTTP | `Request`, `Request::header` | 结构体 / 方法 | `mental1104/web/server.h` | 拥有请求快照；大小写无关查询头，返回 `optional<string>` |
| HTTP | `Response`, `FileBody` | 结构体 | `mental1104/web/server.h` | 互斥的内存响应或文件响应 |
| HTTP | `Respond`, `HttpHandler` | 回调类型 | `mental1104/web/server.h` | 异步处理及至多一次响应提交 |
| WebSocket | `MessageType`, `Message`, `SendResult` | 枚举 / 结构体 | `mental1104/web/server.h` | 文本/二进制消息及本地发送结果 |
| WebSocket | `WebSocketSession::send/close` | 虚接口方法 | `mental1104/web/server.h` | 跨线程发送及幂等关闭 |
| WebSocket | `WebSocketHandler` | 回调结构体 | `mental1104/web/server.h` | 握手前鉴权、建连、完整消息和断连通知 |
| 服务配置 | `TlsOptions`, `Listener`, `ServerOptions`, `Endpoint` | 结构体 | `mental1104/web/server.h` | TLS、监听地址、资源限制与实际绑定地址 |
| 服务生命周期 | `Server::add_route/add_websocket/start/request_stop/wait` | 非虚公共方法 | `mental1104/web/server.h` | 固定注册校验、配置冻结、启动、停止幂等与线程回收流程 |
| 后端装配 | `make_drogon_server` | 工厂函数 | `mental1104/web/drogon_server.h` | 返回 bool，成功时通过输出参数交付 `unique_ptr<Server>` |

HTTP、文件下载和 WebSocket 的调用见 `test/unit/web/server_scenarios.cc`。
本仓库不再维护 C++ examples 目录。

其余能力的调用与可观察结果：

工厂、路由注册、`start(endpoint)` 和 `wait()` 的外层以 bool 报告结果，
内部异常捕获后返回 false，调用方无需为这些入口编写 try/catch。
路由参数通过只读引用传入，在函数内部复制并捕获复制失败。
工厂输出参数必须为空；创建或启动失败均保持输出参数原值。
`request_stop()` 返回 bool，重复停止同样成功；会话 `close()` 为不抛异常的幂等操作；
`Respond` 提交失败返回 false，WebSocket 发送继续保留细分的 `SendResult`。

配置阶段使用哈希索引检查重复：HTTP 以 `(method, path)` 为键，WebSocket 以路径为键，
两者独立。完整路由仍按注册顺序保存在列表中；列表保存失败时撤销索引插入，允许重试。
索引在启动冻结配置或启动前取消时释放，不长期占用运行阶段的路径副本与哈希桶。

```cpp
// 装配入口检查返回值，失败时交由应用决定退出、记录或降级。
std::unique_ptr<mental1104::web::Server> server;
if (!mental1104::web::make_drogon_server(options, server)) return 1;
if (!server->add_route(mental1104::web::Method::Get, "/health", handler)) return 1;
mental1104::web::Endpoint endpoint{};
if (!server->start(endpoint)) return 1;
server->request_stop();
if (!server->wait()) return 1;
```

**示例结果：** 已定义 `options` 和 `handler` 的应用入口中，成功时取得非零监听端口并回收线程；
失败时返回退出码 1，无须捕获框架内部异常，无标准输出。

| 场景 | 最小用法 | 结果 |
|---|---|---|
| 请求读取 | `request.header("Content-Type")` | 存在时返回独立字符串；缺失时为空；字段值为空时仍有值 |
| 原始响应 | 设置 `response.body` 为字符串后调用 `respond(std::move(response))` | 首次提交返回 `true`；同一请求后续提交返回 `false` |
| 文件下载 | 设置 `response.body = FileBody{path, "report.bin"}` | 成功时发送文件，设置附件名；不存在返回 404 |
| WebSocket | `handler.message` 中调用 `session->send(message)` | 返回 `Accepted`、`Closed`、`TooLarge` 或 SDK 异常对应的 `Failed`，不抛异常 |
| 关闭连接 | `session->close()` | 发起正常关闭；重复调用无副作用，无标准输出 |
| 握手鉴权 | `handler.handshake` 返回空或一个 `Response` | 空表示允许升级；返回响应时拒绝升级并发送其 HTTP 状态 |
| 临时端口 | 默认 `ServerOptions` 后调用 `start(endpoint)` | 成功返回 true，endpoint 为 `127.0.0.1` 和非零端口 |
| HTTPS | 为 `Listener::tls` 提供 `TlsOptions{certificate, private_key}` | 该监听器使用 TLS；证书/私钥由应用配置 |
| 停止 | 管理线程调用 `request_stop()`，随后 `wait()` | 不再接受异步提交，回收后台线程；无标准输出 |

一个 `Server` 只管理一个监听器：通过 `ServerOptions::listener` 配置地址与端口，
`start(endpoint)` 成功时写入单个 `Endpoint`，后端 `do_listen()` 返回该地址。默认监听本机 HTTP 的临时端口；
HTTP 路由和 WebSocket 路由共用该监听器。若启用已有 TLS 选项，则该监听器使用 HTTPS。
这里不预留监听器集合；后续存在明确的多监听需求时再调整契约。

### 实现其他 Web 后端

`Server` 采用模板方法（NVI），不使用 C++ 类模板。应用继续调用非虚公共方法；
后端继承 `Server`，构造时传入 `ServerOptions`，只重写以下受保护钩子：

| 扩展点 | 后端职责 | 基类保证 |
|---|---|---|
| `do_configure(const Configuration&)` | 应用 SDK 配置，注册路由与监听参数，不启动监听 | 配置冻结，配置失败后仍执行清理 |
| `do_listen()` | 启动监听，等实际就绪后返回监听地址 | 基类统一发布就绪并结束 `start()` 等待，后端不接收启动回调 |
| `do_run_event_loop()` | 驱动或等待后端事件循环退出 | 基类固定在监听成功和就绪发布后进入该阶段 |
| `do_cleanup() noexcept` | 清理部分初始化资源，停止并回收全部后端线程和回调 | 配置、监听或运行任一阶段失败均执行一次，启动错误在清理后报告 |
| `do_request_stop() noexcept` | 唤醒并停止 SDK，允许与运行结束竞争 | 启动成功后至多提交一次；启动期间取消由清理阶段回收资源 |
| `do_is_io_thread() const noexcept` | 判断当前是否为后端 I/O 回调线程 | `wait` 拒绝 I/O 线程和管理线程的自等待 |
| `shutdown()` | 派生类析构函数中调用，须在成员销毁前执行 | 公共停止与等待，析构不传播已经记录的运行异常 |

`Configuration`、`HttpRoute`、`WebSocketRoute` 是受保护的后端契约类型。
公共协议规则位于 `server.h` / `server.cpp` 的 `detail` 辅助接口：
响应头校验与名称规范化、最终状态范围校验、下载名称校验、文件大小查询，
以及单段 Range 请求筛选和字节范围解析。各后端复用这些规则，
负责 SDK 类型转换、响应构造及回调适配，并在提交边界捕获内部辅助函数的异常。
字面路径转正则用于适配 Drogon 的路由注册，保留在 Drogon 实现中。

启动完成前收到 `request_stop()` 时，基类跳过事件循环阶段，先清理再让
`start(endpoint)` 返回 false，`wait()` 回收线程后也返回 false，不向调用方传播异常。
取消检查与启动成功提交共用状态锁；成功提交后收到的停止请求按运行期停止处理，
因此成功返回的监听地址不保证服务在调用方使用时仍在运行。
配置和路由由基类拥有，后端在整个服务运行期间可只读借用。
`Server::run()` 固定执行“配置 → 监听 → 发布就绪 → 运行事件循环 → 清理”，
成功与异常路径都由基类控制，不再把整段流程交给一个后端 `do_run`。
公共状态锁不会跨后端虚调用持有，停止钩子可重入 `request_stop()` 而不会重复提交。
后端的清理钩子必须在返回前停止其工作线程及回调，不能继续访问派生类成员。
基类析构不调用虚钩子，因此新后端必须在派生类析构中调用 `shutdown()`；
从服务回调中析构或遗漏线程回收属于契约违例，将终止进程。

无需 SDK 的参考后端与可观察结果见 `test/unit/web/lifecycle_scenarios.cc`：执行重复路由拒绝、
启动前/启动中取消、阶段顺序、各阶段失败清理、异常传播、自等待保护及析构回收测试。

Drogon 的 `app.run()` 同时启动监听和运行事件循环。适配器在监听阶段启动私有 SDK 线程，
`BeginningAdvice` 只确认绑定完成，实际 `listen` 尚未执行；适配器还会通过 SDK
建立一次不发送业务数据的本地 TCP 连接，确认端口可连接后才返回地址。
探测最多等待五秒，超时启动返回 false；事件循环阶段等待 SDK 线程退出。
因此该后端有一条基类管理线程、一条 SDK 主循环线程，以及配置数量的网络 I/O 线程。
这项 SDK 差异封装在适配器内部，应用和公共基类均不依赖 Drogon 的启动回调。
Linux 上多个 I/O 线程的内部监听记录必须指向同一公共地址；使用临时端口时，
适配器先保留共享绑定，待所有 SDK 监听器绑定同一端口后释放，避免各线程取得不同端口。

### 复用后端场景测试

`test/unit/web/server_scenarios.cc` 只使用公共 Server 接口，HTTP/HTTPS/WebSocket 请求由
`wire_client.cc` 中独立的 Beast 客户端发送。场景覆盖路由、HEAD、文件和 Range、
错误隔离、延迟响应、请求大小限制、WebSocket 鉴权/消息/断连以及 TLS。
默认使用两个 I/O 线程和临时端口，网络操作有三秒截止时间。

接入新的后端只需新增一个接入文件，按 `test/unit/web/backend.h` 定义 `web_test::backend()`，
提供后端名称、返回 bool 的 Server 工厂及 TLS 能力，再在构建中调用
`m1104_add_web_backend_scenarios(测试目标 后端库目标 接入文件绝对路径)`。
参考 `drogon_backend.cc`；SDK 专属限制的测试放在该接入文件，不放进公共场景。
公共场景编译为可复用 OBJECT 目标，新的测试程序直接链接同一组场景，无需复制测试源码。

通过 CTest 执行上述筛选命令。每个网络场景运行于独立进程，允许并行执行，
不要求后端支持进程内重启。当前共 19 个 CTest 项，其中生命周期项包含多项故障注入测试；
Linux 后端专属场景会人为阻塞监听线程，验证 start 不会提前返回成功。
无 TLS 的后端显式跳过 HTTPS 场景。关闭 `M1104_WEB_WITH_DROGON` 后仍可运行生命周期测试。
不要直接运行整个 Drogon 测试程序来批量执行场景，因为 Drogon 每进程只能启动一次。

### 行为边界

- HTTP 按方法和字面路径匹配，区分大小写；Get 默认提供 HEAD，复用 Get 回调并省略响应体。
  HEAD 以 Get 方法交付回调，暂不支持独立 Head handler，注册 Head 返回 false。
  GET/HEAD 应保持只读业务语义；需要改变业务状态的操作应使用 POST 等合适方法。
  未注册方法不会进入其他方法的 handler，当前未统一后端返回 404 或 405 的策略。
  不提供框架专属路径模板，
  动态资源标识可暂由 query/body 承载。未显式注册的静态文件不自动对外提供。
- 请求体和原始 query 拷贝进 `Request`，可转移给应用执行器；本模块不绑定 JSON 库，
  不解析 multipart，也不替代 schema 解码或业务校验。请求体上限默认 16 MiB，超限由后端拒绝。
- HTTP/WebSocket 回调在 I/O 线程执行，不允许直接做阻塞 HTTP 调用、长时间磁盘扫描或重计算。
  可在回调内调用 `request_stop()`，不得调用 `wait()` 或销毁服务。管理线程负责 start/wait/销毁。
- `Respond` 可复制保存并从其他线程提交；首次响应生效，停止后提交返回 `false`。
  handler 抛异常或所有未提交句柄释放时发送空体 500；保留未完成句柄的责任由应用承担，
  模块不提供每请求的业务截止时间。返回 `true` 不保证对端收到，也不代表传输完成。
- SDK 内部上传缓存使用服务独占的系统临时目录，停止后清理，不在调用方工作目录创建 uploads。
- 响应状态允许 200..599，头字段名称要求唯一，禁止控制字符及手写消息定界/连接字段；
  非法响应转换成 500。第一版不提供多个 `Set-Cookie` 字段的建模。
- 文件支持 GET 单段 `Range`（闭区间、开放结尾、后缀）、206/416 与 GET 默认提供的 HEAD。
  多段 Range、其他范围单位及带 If-Range 的请求返回完整实体；尚无 ETag/条件请求机制。
  应用负责路径权限和传输期间文件稳定性；后端发送失败不能承诺已经开始响应后的状态改写。
- WebSocket 只交付完整文本/二进制消息；控制帧由后端处理。默认单条消息上限 1 MiB。
  会话句柄通过弱引用访问后端连接，关闭后保留句柄不会继续持有连接。
  `Accepted` 只表示提交给 SDK；第一版不提供发送背压或对端消费确认，高频推送由应用限速。
  已验证服务端主动关闭与客户端 TCP 断连通知；Drogon 1.8.7 对客户端发起的关闭握手
  会直接关闭传输，Beast 报 EOF，当前不承诺该方向的完整关闭帧握手。
- `request_stop()` 为非排空停止：不等待在途 HTTP 或 WebSocket 消息完成，也不承诺逐连接断开通知。
  析构自动请求停止并等待。Drogon 为进程单例，每进程仅能启动一次，不支持多实例或重启，
  不可与其他直接配置 `drogon::app()` 的代码混用；绑定失败等 SDK 致命错误仍遵循 Drogon 行为。

## MQ SDK 依赖准备

Kafka 与 Pulsar 通过真实 SDK 实现：CMake 分别建立 `M1104::KafkaSDK`、`M1104::PulsarSDK` 导入目标，并作为 `mental1104` 的私有链接依赖。公共 MQ 类型不要求调用方包含 SDK 头文件。

Debian/Ubuntu 原生独立构建默认在配置阶段用 apt 准备已启用的后端，无需额外选项。关闭自动准备后，也可以仅显式请求 MQ SDK 安装：

```sh
# 其余源码依赖已提供时，只显式准备 MQ SDK。
cmake -S cpp -B cpp/build -DM1104_AUTO_SETUP=OFF -DM1104_INSTALL_MQ_DEPS=ON
cmake --build cpp/build --parallel 4
ctest --test-dir cpp/build -R '^test_mq$' --output-on-failure
```

**命令结果：** SDK 准备成功后，CMake 输出 `Kafka MQ backend enabled` 和 `Pulsar MQ backend enabled`；构建与测试命令成功时退出码为 `0`。未配置 broker 时，实际收发用例会跳过；不依赖 broker 的域模型、缺 SDK 分支、Kafka SDK 配置校验及 Pulsar SDK topic 校验仍会执行适用的用例。

| SDK | apt 准备路径 | 限制 |
|---|---|---|
| librdkafka | 发行版仓库中的 `librdkafka-dev`，包含 C/C++ 客户端 | 当前 Ubuntu 24.04 验证版本为 2.3.0；仓库版本由发行版决定 |
| Pulsar C++ | 优先使用已配置 apt 仓库的 `apache-pulsar-client` / `apache-pulsar-client-dev`；不可用时下载 Apache 官方 4.2.0 DEB 后交给 apt 安装 | 官方 DEB 路径支持 amd64/arm64，下载时校验官方 SHA512；后端要求 C++17+ |

依赖缺失时准备流程会执行 `apt-get update/install`，需要 root 或允许非交互执行 apt 的 `sudo -n`；它不等待密码。已安装的 DEB 包直接复用，下载缓存位于构建目录的 `mq-packages/pulsar-4.2.0`。安装失败时配置失败，不悄悄降级为 SDK 不可用。关闭 `M1104_ENABLE_KAFKA` / `M1104_ENABLE_PULSAR` 时不准备对应 SDK；C++11/14 不准备 Pulsar。交叉编译或非 Debian/Ubuntu 平台请保留 apt 安装选项关闭，并通过系统安装或 CMake SDK 路径提供目标平台依赖。

Pulsar 的 Producer 创建接口通过实际头文件与库进行编译链接探测，新 SDK 使用 `createProducerV2`，较旧 SDK 保留旧接口路径。安装路径依据 [Apache 官方 C++ 客户端安装说明](https://pulsar.apache.org/docs/client-libraries/cpp-setup/)。

真实服务用例需要以下环境变量：Kafka 使用 `KAFKA_ADVERTISED_HOST`、`KAFKA_EXTERNAL_PORT`、`KAFKA_TEST_TOPIC`；Pulsar 使用 `PULSAR_HOST`、`PULSAR_BROKER_PORT`、`PULSAR_TEST_TENANT`、`PULSAR_TEST_NAMESPACE`、`PULSAR_TEST_TOPIC`。SDK 安装与 broker 服务准备是两个独立步骤。

## 维护规则

新增公共函数、类、结构体、枚举、别名、宏、可复用方法或公共头文件时，必须更新此 README，写明类别、包含路径、用途、最小调用示例和备注。如果 API 已公开但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 容器与字符串
- 缓存包装器
- JSON
- 日志
- C API 兼容
- 资源管理
- 随机键
- 语义基类型
- 计时
- 并发
- 网络与服务
- 调试
- 数值计算器

## 用法索引

| 类别 | 名称 | 类型 | 包含 | 用途 |
|---|---|---|---|---|
| 容器与字符串 | `contains` | 函数模板 | `mental1104/util.h` | 检查 STL 容器成员关系和 map 键是否存在。 |
| 容器与字符串 | `to_lower_copy` | 函数 | `mental1104/util.h` | 返回小写化后的 `std::string` 副本。 |
| 容器与字符串 | `ExponentialBackoff` | 类 | `mental1104/util.h` | 按上限推进重试延迟，可显式加入对称随机抖动。 |
| 缓存包装器 | `LRUCache`, `make_lru_cache`, `make_cache` | 类 / 函数 | `mental1104/core/cache.h` | 用 tuple 键 LRU 或无界缓存包装可调用对象。 |
| 缓存包装器 | `LFUCache`, `make_lfu_cache` | 类 / 函数 | `mental1104/core/cache.h` | 用 tuple 键 LFU 缓存包装可调用对象。 |
| JSON | `IJsonParser`, `IJsonDocument`, `CJsonParser`, `RapidJsonParser`, `NlohmannJsonParser`, `JsonParser`, `load_json`, `dump_json`, `ParseResult`, `DumpResult`, `JsonDoc`, `JsonValueView`, `JsonType`, `JsonOptional`, `JsonValueReader` | 接口 / 类 / 枚举 / 函数 / 结构体 / 模板 | `mental1104/json/codec.h`、`mental1104/json/access.h` | 可替换后端编解码、只读节点访问、类型化可选读取及调用方模型转换。 |
| 日志 | `LogLevel`, `get_log_level`, `set_log_level`, `log`, `logf`, `M1104_LOG_*` | 枚举 / 函数 / 宏 | `mental1104/log.h` | 通过 stdout/stderr 输出分级日志，可用时使用 spdlog。 |
| C API 兼容 | `COMMON_EXTERN_C_BEGIN`, `COMMON_EXTERN_C_END`, `COMMON_EXTERN_C` | 宏 | `mental1104/common/c_api_compat.h` | 包装 C 声明，使同一头文件可被 C 和 C++ 包含。 |
| 资源管理 | `unique_fd` | 类 | `mental1104/c_api_raii.h` | 持有 POSIX 文件描述符。 |
| 资源管理 | `unique_file`, `open_file` | 类 / 函数 | `mental1104/c_api_raii.h` | 持有 C `FILE*`。 |
| 资源管理 | `scope_exit`, `make_scope_exit` | 类 / 函数 | `mental1104/c_api_raii.h` | 在作用域退出时执行清理代码。 |
| 随机键 | `random_hex`, `key_with_random_suffix` | 函数模板 | `mental1104/random.h` | 生成随机十六进制字符串和带前缀的键。 |
| 语义基类型 | `NonCopyable`, `Movable`, `Immovable`, `MENTAL1104_MOVE_ONLY`, `is_move_only_v` | 结构体 / 宏 / trait | `mental1104/semantic.h` | 为持有资源的类型声明复制 / 移动语义。 |
| 计时 | `Timed`, `make_timed` | 类模板 / 函数 | `mental1104/timed.h` | 包装函数指针，并在调用前后输出简单计时信息。 |
| 并发 | `IExecutor` | 接口类 | `mental1104/concurrency/executor.h` | 通用 fire-and-forget 执行器接口。 |
| 并发 | `sleep_for`, `sleep_for_ms`, `ThreadPool` | 函数 / 类 | `mental1104/concurrency/thread/thread_util.h` | 睡眠辅助函数和返回 future 的线程池。 |
| 并发 | `ThreadPoolExecutor`, `BoostAsioExecutor` | 类 | `mental1104/concurrency/thread/*.h` | 基于本地线程池或 Boost.Asio 线程池的 `IExecutor` 适配器。 |
| 并发 | `Task`, `ICoroutineScheduler`, `BasicCoroutineScheduler`, `MnCoroutinePoolT`, `MnCoroutinePool`, `BoostMnCoroutinePool` | 类 / 别名 | `mental1104/concurrency/coroutine/*.h`, `mental1104/concurrency/mn/*.h` | 在执行器适配器上调度 C++20 coroutine。 |
| 容器 | `BasicBloomFilter`, `BloomFilter`, `CoarseLockBloomFilter`, `CoarseLockStringBloomFilter` | 类 / 别名 | `mental1104/bloom_filter.h` | 用于字符串或自定义键成员判断的 Bloom filter 变体。 |
| 网络与服务 | `RedisLock`, `create_redis_from_env` | 类 / 函数 | `mental1104/redis_lock.h` | 基于 Redis 的锁辅助工具。 |
| 网络与服务 | `EpollServer` | 类 | `mental1104/net/epoll_server.h` | 注册文件描述符并分发事件回调。 |
| 调试 | `st_options_t`, `st_init`, `st_shutdown`, `st_dump_current_thread` | C API | `mental1104/debug/stacktrace.h` | 初始化并输出原生 stacktrace。 |
| 数值计算器 | `high_precision`, `InfiniteDecimalCalculator`, `ECalculator`, `FixedPointCalculator`, `PiCalculator` | 别名 / 类 | `mental1104/high_precision_decimal.h` | 从高精度计算器生成十进制字符串。 |

## 详情

### `contains`, `to_lower_copy`, `ExponentialBackoff`

- **类别：** 容器与字符串
- **类型：** 函数和类
- **定义位置：** `cpp/include/mental1104/util.h`
- **包含：** `#include "mental1104/util.h"`
- **用途：** 提供通用成员判断、小写转换和重试延迟推进。

**基础用法：**

```cpp
#include "mental1104/util.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

int main() {
  std::vector<int> values{1, 2, 3};
  bool has_two = mental1104::contains(values, 2);
  std::string lower = mental1104::to_lower_copy("Hello");

  mental1104::ExponentialBackoff backoff(
      std::chrono::milliseconds(10),
      std::chrono::milliseconds(100));
  auto wait = backoff.next();
  std::mt19937 rng(123);
  auto jittered_wait = backoff.next(0.2, rng);
  backoff.reset();

  std::cout << has_two << " " << lower << " " << wait.count() << " "
            << jittered_wait.count() << "\n";
}
```

**示例输出：**

```text
1 hello 10 24
```

**备注：**

- 对 map 使用时，`contains` 检查键是否存在。
- `ExponentialBackoff::next()` 保持确定性指数退避；`next(jitter_ratio, rng)` 先按原序列推进，再把本次等待均匀采样到 `[(1-ratio)d, (1+ratio)d]`。
- 调用方显式传入随机引擎，便于测试复现，也避免工具类隐藏全局随机状态。

### `LRUCache`、`LFUCache` 和缓存工厂

- **类别：** 缓存包装器
- **类型：** 类模板和工厂函数
- **定义位置：** `cpp/include/mental1104/core/cache.h`
- **包含：** `#include "mental1104/core/cache.h"`
- **用途：** 按参数 tuple 缓存可调用对象的结果。

**基础用法：**

```cpp
#include "mental1104/core/cache.h"
#include <iostream>

int slow_add(int a, int b) { return a + b; }

int main() {
  auto lru = mental1104::make_lru_cache<int, int, int>(32, slow_add);
  auto lfu = mental1104::make_lfu_cache<int, int, int>(32, slow_add);
  auto unlimited = mental1104::make_cache<int, int, int>(slow_add);

  std::cout << lru(1, 2) << " " << lfu(1, 2) << " " << unlimited(1, 2);
}
```

**示例输出：**

```text
3 3 3
```

**备注：**

- 模板参数顺序为返回类型，然后是参数类型。

### JSON 编解码与对象访问

- **类别：** JSON
- **类型：** 枚举、函数、类、结构体
- **定义位置：** `cpp/include/mental1104/json/`；运行时实现位于 `cpp/src/json/`
- **包含：** `mental1104/json/codec.h` 提供文本转换入口和解析器，并包含对象访问接口；只需文档、节点访问或类型化读取时可单独包含 `mental1104/json/access.h`
- **用途：** 加载 JSON 文本为独占文档，通过统一视图读取对象，再使用文档所属后端输出紧凑 JSON 文本。

**基础用法：**

```cpp
#include "mental1104/json/codec.h"
#include <iostream>

/// 加载配置、读取嵌套成员并序列化为独立文本。
/// @return 成功返回 0，编解码失败返回 1。
int main() {
    auto result = mental1104::load_json(
        R"({"status":{"settings":{"uuid":"test-id"}},"nil":null})");
    if (!result) {
        std::cerr << result.error << " @" << result.offset << "\n";
        return 1;
    }
    const auto root = result.get();
    const auto uuid = root.get_optional<std::string>(
        {"status", "settings", "uuid"});
    std::cout << uuid.value_or("unknown") << "\n";
    std::cout << root.get("missing").is_valid() << " "
              << root.get("nil").is_null() << "\n";
    const auto dumped = mental1104::dump_json(result);
    if (!dumped) {
        std::cerr << dumped.error << "\n";
        return 1;
    }
    std::cout << dumped.text << "\n";
}
```

**示例输出：**

```text
test-id
0 1
{"status":{"settings":{"uuid":"test-id"}},"nil":null}
```

**备注：**

| 接口 | 结果与失败语义 |
|---|---|
| `load_json(text, backend)`、`load_json(data, length, backend)` | 返回内部独占文档的 `ParseResult`；以 `result.get()` 直接取得根视图，失败时提供 `error` 和错误字节 `offset`。省略后端时固定使用 cJSON；若未构建 cJSON 则返回错误，不自动切换。可显式指定 `JsonParser::RapidJSON` 或 `JsonParser::NlohmannJSON`。 |
| `load_json("{}")` | NUL 结尾字符串和字符串字面量可直接传入；nullptr 返回错误。不要求 NUL 结尾的缓冲应使用指针/长度重载。 |
| `dump_json(result)`、`dump_json(document)` | 接受加载结果或独占文档，返回 `DumpResult`；成功时 `ok=true`、`text` 为独立字符串且 `error` 为空；失败时 `text` 为空且 `error` 提供诊断。使用文档所属后端，无须再次指定枚举。 |
| `type()`、`is_valid()`、`is_null()`、`is_object()`、`is_array()`、`is_string()`、`is_bool()`、`is_number()` | 区分 `JsonType::Missing` 与有效的 JSON `null`，缺失成员不触发后端异常或断言。 |
| `get(key)`、`get({"a", "b"})`、`has(key)` | 区分大小写；非对象、成员缺失或路径中断返回空视图；空路径返回当前节点。 |
| `size()`、`at(index)` | 数组长度及元素；非数组长度为 0，非数组或越界返回空视图。 |
| `for (const auto item : result.get())` | 按顺序借用数组元素，嵌套数组视图也可遍历；空数组及非数组为空范围，不枚举对象成员。 |
| `get_optional<T>()`、`get_optional<T>(key/path)` | 无参读取当前节点，带键或路径读取成员。支持布尔、标准整数、浮点、`std::string`，C++17 起支持借用的 `std::string_view`。缺失、null、类型错误或越界返回空；false、0、空字符串仍包含值。 |
| `as_string()`、`as_string_copy()`、`as_bool()`、`as_i64()`、`as_f64()` | 保留默认值读取；`as_i64()` 仍向零截断小数，越界返回默认值；严格 optional 整数读取拒绝小数。 |
| `JsonValueReader<T>::read(view)` | 调用方特化可组合统一读取能力，生成自定义模型，无须修改库或显式实例化。 |

**所有权与扩展：**

数组根可以直接遍历，每个元素仍是统一的借用视图：

```cpp
const auto loaded = mental1104::load_json(
    mental1104::string_view(R"([{"id":1},{"id":2}])"),
    mental1104::JsonParser::NlohmannJSON);
// 成功后按顺序读取元素；加载失败时根为 Missing，范围为空。
if (loaded) {
    for (const auto item : loaded.get()) {
        std::cout << item.get_optional<int>("id").value_or(-1) << "\n";
    }
}
```

示例输出为两行 `1`、`2`。数组标量可用 `item.get_optional<int>()` 读取当前节点。

- 公开接口按两项职责组织：`codec.h/.cpp` 提供 `load_json`、`dump_json`、编解码结果与统一入口；`access.h/.cpp` 提供文档所有权、节点视图、路径遍历和类型转换。调用方需要的模板留在 `access.h`，固定签名实现独立编译。两层都不包含原生 SDK 头文件。
- 具体后端集中在私有 `src/json/detail/`：`cjson.cpp`、`rapidjson.cpp` 与 `nlohmann.cpp` 各自完整管理原生 DOM、节点读取、解析和序列化，不再把同一个后端拆散到外层文件。`detail/backend.h` 只声明通用诊断和数值转换，不暴露原生类型；这些私有文件不安装给调用方。独立导出工程递归收集该目录中的源文件。

- `IJsonParser::parse` 每次产生独立文档，完成后可销毁解析器和输入。`ParseResult` 公开 `ok`、`error`、`offset`，内部独占文档，调用方通过 `get()` 访问根，通过 `backend()` 查询诊断标识。布尔判断同时检查解析成功和当前所有权；默认构造、失败及移出后的结果为 false，`get()` 返回 Missing。整个结果可移动、不可复制，不再公开 `doc` 成员。
- `JsonDoc` 独占 `IJsonDocument`，允许移动、禁止复制；`valid()`、`backend()`、`root()` 保持原用途。移动所有权不改变后端地址，已有视图仍有效；覆盖或销毁文档使其旧视图失效。视图和借用字符串不延长文档生命周期，`std::string` 结果是独立副本。
- 应用可派生 `IJsonParser` 和 `IJsonDocument`，通过 `ParseResult(JsonDoc(unique_ptr<IJsonDocument>))` 返回独占的成功结果。解析器只覆写非公开的 `do_parse(const string_view &) const`：公共 `parse` 为 `final` 内联入口，统一拒绝空输入和原始 NUL 后再分派；后端仅负责语法、完整消费与文档创建，无需重复公共校验。入口不包装或吞掉后端异常，错误字符串仅在失败路径构造。已有派生类应将 `parse` 覆写迁移为 `do_parse`，并与库一起重新编译；调用方式仍为 `parser.parse(text)`。文档实现 protected `do_root/type/get/size/at/string/bool/i64/u64/f64` 钩子；句柄只由所属文档解释，nullptr 表示缺失，非空句柄属于该文档；DOM 只读。公共视图负责路径和模板转换，无须增加枚举分支或全局注册。默认诊断标识为 `JsonParser::Custom`。
- 公共输入校验先于后端可用性检查：即使对应后端未构建，空输入及原始 NUL 也返回统一输入错误；空输入偏移为 0，原始 NUL 偏移为首个 NUL 的字节位置。转义的 `\u0000` 不会被公共入口拒绝。
- 需要支持序列化的自定义文档覆写 `IJsonDocument::do_dump() const`，返回 `DumpResult`。默认钩子返回明确的不支持错误，因此只实现读取的后端仍可使用。内置文档的节点读取与文本生成均封装在对应的 `detail` 后端文件中。
- `dump_json` 不修改文档，不使已有借用视图失效；其文本可在源文档销毁后继续使用。默认空文档和移动后的空对象返回错误，有效的 JSON `null` 文档则成功输出 `null`。输出为紧凑格式，不保留输入空白或数字的原始拼写；不承诺不同后端输出逐字相同。
- 统一入口为 `load_json(text, backend)`，后端可选 `JsonParser::CJSON`、`JsonParser::RapidJSON` 或 `JsonParser::NlohmannJSON`，同时提供指针/长度重载，并委托给派生解析器；原 `parse_json` 已更名，不保留同义入口。旧 `JsonValueView::from_cjson/from_rapid` 原生节点注入接口已移除，应通过文档取得视图。
- 本次调整了公开对象布局及部分函数签名，需要重新编译库与调用方，不兼容旧版二进制 ABI。

**构建、链接与限制：**

- 链接既有 `mental1104` 共享库。公共 JSON 头不依赖 cJSON、RapidJSON 或 nlohmann JSON SDK，原生 SDK 为私有依赖。头与库仍须匹配 C++ 标准，不能混用 C++11/14 与 C++17+ 的 `string_view`/optional 配置。
- `M1104_ENABLE_JSON` 控制模块；`M1104_ENABLE_JSON_CJSON`、`M1104_ENABLE_JSON_RAPIDJSON`、`M1104_ENABLE_JSON_NLOHMANN` 独立控制内置后端，能力写入安装的 `mental1104/build_config.h`，nlohmann 的能力宏为 `M1104_HAS_JSON_NLOHMANN`。缺依赖或禁用的后端返回错误，不自动切换；全部内置后端关闭时仍可接入自定义实现。
- nlohmann JSON 依赖位于 `cpp/lib/nlohmann_json`，子模块固定为 v3.12.0。复用既有子模块初始化流程；也可用 CMake 的 `M1104_NLOHMANN_JSON_INCLUDE_DIR` 指向兼容的头文件根目录。独立 `export/cpp` 同样支持该后端和开关，头文件路径使用 `EXPORT_NLOHMANN_JSON_INCLUDE_DIR`；Python/C 导出的既有协议名称保持不变。
- `JsonOptional<T>` 在 C++17+ 等同 `std::optional<T>`，C++11/14 等同 `boost::optional<T>`；通过布尔判断和 `value_or` 使用。旧标准的 `string_view` 为 `std::string`，可能分配内存，这些路径不承诺 `noexcept`。
- 输入不要求 NUL 结尾；根后仅允许 JSON 空白。空输入及原始 NUL 返回错误，转义的 `\u0000` 由后端处理。语法诊断和偏移细节可能不同，cJSON 的宽松语法未被完整校验器替代。
- 严格数值读取不转换字符串或布尔，不截断小数，检查符号、范围及浮点有限性；浮点转换允许舍入。cJSON 数字以 double 保存，不能承诺原始完整 64 位整数精度；RapidJSON 与 nlohmann JSON 保留原生 64 位整数。cJSON 字符串和键没有独立长度元数据，不能完整读取含内嵌 NUL 的字符串和键，此类输入应选择 RapidJSON 或 nlohmann JSON。
- nlohmann JSON 后端使用只读的保序 DOM，紧凑输出保留对象成员的输入顺序；原生语法异常转换为 ASCII 诊断，错误字节偏移统一从 0 开始。数值溢出没有原生位置信息时偏移为 0；内存分配失败仍可抛异常。
- 数组范围游标与节点一样借用文档，移动加载结果后仍有效，销毁或覆盖拥有者后失效；每次解引用使用后端索引访问，cJSON 链表数组的完整遍历可能需要平方级时间。游标只提供 range-for 所需操作，不承诺标准库迭代器接口。
- 序列化不能恢复原生 DOM 已丢失的精度或字符串长度。cJSON 解析产生的非有限数值在 `dump_json` 中返回错误，避免被打印成 `null`；RapidJSON Writer 失败时也返回错误，不暴露未完成的文本前缀。
- 缺失和类型错误表现为空；分配失败及自定义转换异常仍可抛出。cJSON 使用进程级 hooks/错误状态，不承诺并发解析；需要并发解析时选择 RapidJSON。文档销毁、覆盖或自定义后端的可变操作不能与视图读取并发。

**验证入口：** 构建后运行 `./dev test-cpp --file '^test_json'`。三种后端复用相同场景，首例展示加载、数组访问和输出的最小完整用法；`test_json_consumer` 不链接 GoogleTest，按实际库标准编译多个翻译单元，验证调用方自定义类型、模板和运行时链接。

### 日志

- **类别：** 日志
- **类型：** 枚举、函数、宏
- **定义位置：** `cpp/include/mental1104/log.h`
- **包含：** `#include "mental1104/log.h"`
- **用途：** 输出分级消息和格式化消息。

**基础用法：**

```cpp
#include "mental1104/log.h"

int main() {
  mental1104::set_log_level(mental1104::LogLevel::Info);
  mental1104::log(mental1104::LogLevel::Info, "ready: ", 1);
  mental1104::logf(mental1104::LogLevel::Warning, "retry=%d", 3);

  M1104_LOG_INFO("plain macro log");
  M1104_LOG_ERRORF("code=%d", 500);
}
```

**示例输出：**

未启用 spdlog 时的回退输出如下；启用 spdlog 时会由 spdlog 添加自己的时间戳 / logger 格式。

```text
[info] ready: 1
[warning] retry=3
[info] plain macro log
[error] code=500
```

**备注：**

- `MENTAL1104_LOG_LEVEL` 可设置进程级初始日志级别。

### C API 兼容宏

- **类别：** C API 兼容
- **类型：** 宏
- **定义位置：** `cpp/include/mental1104/common/c_api_compat.h`
- **包含：** `#include "mental1104/common/c_api_compat.h"`
- **用途：** 在不重复编写 `extern "C"` 保护的情况下，让 C 声明可被 C++ 调用方共享。

**基础用法：**

```c
#include "mental1104/common/c_api_compat.h"

COMMON_EXTERN_C_BEGIN

int common_add(int lhs, int rhs);
void common_reset(void);

COMMON_EXTERN_C_END
```

**示例结果：**

```text
无标准输出；在 C++ 编译单元中，common_add/common_reset 以 C linkage 声明。
```

**单个声明用法：**

```c
#include "mental1104/common/c_api_compat.h"

COMMON_EXTERN_C int common_add(int lhs, int rhs);
```

**示例结果：**

```text
无标准输出；common_add 在 C++ 下等价于 extern "C" 声明，在 C 下保持普通函数声明。
```

**备注：**

- 这些宏只用于 C 兼容声明。
- 不要用它们包装 C++ 类、模板、重载函数或仅存在于命名空间中的 API。

### C API RAII 包装器

- **类别：** 资源管理
- **类型：** 类和工厂函数
- **定义位置：** `cpp/include/mental1104/c_api_raii.h`
- **包含：** `#include "mental1104/c_api_raii.h"`
- **用途：** 持有 C 文件句柄、POSIX 描述符和作用域清理回调。

**基础用法：**

```cpp
#include "mental1104/c_api_raii.h"

int main() {
  auto file = mental1104::open_file("example.txt", "w");
  if (file) {
    std::fputs("hello\n", file.get());
  }

  auto cleanup = mental1104::make_scope_exit([] {
    // cleanup code here
  });
  cleanup.dismiss();
}
```

**示例结果：**

```text
无标准输出；如果 example.txt 可写，文件内容为：
hello
```

**备注：**

- `unique_fd` 仅在非 Windows 平台可用。

### 随机键

- **类别：** 随机键
- **类型：** 函数模板
- **定义位置：** `cpp/include/mental1104/random.h`
- **包含：** `#include "mental1104/random.h"`
- **用途：** 为临时键和锁值构建随机后缀。

**基础用法：**

```cpp
#include "mental1104/random.h"
#include <iostream>

int main() {
  std::cout << mental1104::random_hex<>(2) << "\n";
  std::cout << mental1104::key_with_random_suffix<>("job", 1) << "\n";
}
```

**示例输出：**

```text
<hex-string>
job:<hex-string>
```

### 语义基类型

- **类别：** 语义基类型
- **类型：** 结构体、宏、trait
- **定义位置：** `cpp/include/mental1104/semantic.h`
- **包含：** `#include "mental1104/semantic.h"`
- **用途：** 为持有资源的公共类型表达复制 / 移动约束。

**基础用法：**

```cpp
#include "mental1104/semantic.h"
#include <type_traits>

class Handle : public mental1104::Movable {
public:
  Handle() = default;
};

static_assert(mental1104::is_move_only_v<Handle>, "Handle should be move-only");
```

**示例结果：**

```text
无标准输出；static_assert 通过，Handle 被识别为 move-only 类型。
```

**备注：**

- `is_move_only_v` 需要 C++17 inline variables。

### `Timed` 和 `make_timed`

- **类别：** 计时
- **类型：** 类模板和工厂函数
- **定义位置：** `cpp/include/mental1104/timed.h`
- **包含：** `#include "mental1104/timed.h"`
- **用途：** 包装函数指针，并在调用前后打印进入 / 退出计时。

**基础用法：**

```cpp
#include "mental1104/timed.h"

int add(int a, int b) { return a + b; }

int main() {
  auto timed_add = mental1104::make_timed(add, "add");
  return timed_add(1, 2);
}
```

**示例输出和返回值：**

```text
Entering add
Exiting add with <seconds> seconds
```

`timed_add(1, 2)` 返回 `3`；上面这个 `main` 会把进程退出码设为 `3`。

### 执行器和线程辅助工具

- **类别：** 并发
- **类型：** 接口、类、函数
- **定义位置：** `cpp/include/mental1104/concurrency/...`
- **包含：** `#include "mental1104/concurrency/thread/thread_pool_executor.h"`
- **用途：** 在线程池上运行 fire-and-forget 任务或返回 future 的工作。

**基础用法：**

```cpp
#include "mental1104/concurrency/thread/thread_pool_executor.h"
#include "mental1104/concurrency/thread/thread_util.h"
#include <iostream>

int main() {
  ThreadPool pool(2);
  auto result = pool.submit([] { return 42; });
  std::cout << result.get() << "\n";

  mental1104::ThreadPoolExecutor executor(2);
  executor.execute([] {});
  mental1104::sleep_for_ms(1);
}
```

**示例输出：**

```text
42
```

**备注：**

- `BoostAsioExecutor` 需要 Boost.Asio。

### C++20 coroutine 辅助工具

- **类别：** 并发
- **类型：** 类和别名
- **定义位置：** `cpp/include/mental1104/concurrency/coroutine/*.h`, `cpp/include/mental1104/concurrency/mn/*.h`
- **包含：** `#include "mental1104/concurrency/mn/mn_coroutine_pool.h"`
- **用途：** 在执行器支撑的池上调度仓库 `Task` coroutine。

**基础用法：**

```cpp
#include "mental1104/concurrency/mn/mn_coroutine_pool.h"

#if __cplusplus >= 202002L
mental1104::Task sample_task() { co_return; }

int main() {
  mental1104::MnCoroutinePool pool(2);
  pool.spawn(sample_task());
  pool.wait_all();
}
#endif
```

**示例结果：**

```text
无标准输出；sample_task 完成后 wait_all 返回。
```

**备注：**

- 需要 C++20。`AsyncSimpleCoroutineScheduler` 别名需要 `M1104_HAS_ASYNC_SIMPLE`。

### Bloom filter

- **类别：** 容器
- **类型：** 类模板和别名
- **定义位置：** `cpp/include/mental1104/bloom_filter.h`
- **包含：** `#include "mental1104/bloom_filter.h"`
- **用途：** 添加并测试可能存在的键，可选粗粒度锁保护。

**基础用法：**

```cpp
#include "mental1104/bloom_filter.h"

int main() {
  BloomFilter bf(1000, 0.01);
  bf.insert("alice");
  bool maybe = bf.contains("alice");

  CoarseLockStringBloomFilter locked(1000, 0.01);
  locked.insert("bob");
  return maybe && locked.contains("bob") ? 0 : 1;
}
```

**示例结果：**

```text
无标准输出；maybe 为 true，locked.contains("bob") 为 true，进程退出码为 0。
```

**备注：**

- 正结果表示“可能包含”；负结果表示一定不存在。

### Redis 锁

- **类别：** 网络与服务
- **类型：** 类和函数
- **定义位置：** `cpp/include/mental1104/redis_lock.h`
- **包含：** `#include "mental1104/redis_lock.h"`
- **用途：** 获取和释放基于 Redis 的锁。

**基础用法：**

```cpp
#include "mental1104/redis_lock.h"

int main() {
  auto redis = create_redis_from_env();
  if (!redis) {
    return 1;
  }
  RedisLock lock(redis, "common:lock");
  if (lock.try_lock(30000)) {
    lock.unlock();
  }
}
```

**示例结果：**

```text
未配置可连接的 Redis 时，create_redis_from_env() 失败并返回退出码 1。
连接成功且获取锁成功时无标准输出，try_lock(30000) 返回 true 后释放锁。
```

**备注：**

- 需要 Redis++ 和 `REDIS_HOST`/`REDIS_PORT`；`REDISCLI_AUTH` 可选。
- 待复核：此头文件暴露了 `using namespace sw::redis;` 和未放入命名空间的公共符号。

### `EpollServer`

- **类别：** 网络与服务
- **类型：** 类
- **定义位置：** `cpp/include/mental1104/net/epoll_server.h`
- **包含：** `#include "mental1104/net/epoll_server.h"`
- **用途：** 注册文件描述符回调并分发事件。

**基础用法：**

```cpp
#include "mental1104/net/epoll_server.h"

int main() {
  mental1104::EpollServer server;
  int fd = 0;
  server.add_fd(fd, EPOLLIN, [](int ready_fd) {
    (void)ready_fd;
  });
  server.dispatch_once(0);
  server.remove_fd(fd);
}
```

**示例结果：**

```text
无标准输出；dispatch_once(0) 执行一次非阻塞事件分发，然后 remove_fd(fd) 注销文件描述符。
```

**备注：**

- 待复核：平台行为取决于实现文件和可用事件 API。

### Stacktrace C API

- **类别：** 调试
- **类型：** C 结构体、枚举和函数
- **定义位置：** `cpp/include/mental1104/debug/stacktrace.h`
- **包含：** `#include "mental1104/debug/stacktrace.h"`
- **用途：** 配置原生 stacktrace 输出并转储当前线程。

**基础用法：**

```cpp
#include "mental1104/debug/stacktrace.h"

int main() {
  st_options_t opt{};
  opt.enable = 1;
  opt.max_frames = 32;
  st_init(&opt);
  st_dump_current_thread();
  st_shutdown();
}
```

**示例输出：**

```text
<当前线程的 stacktrace，具体帧名和路径取决于平台、编译选项和符号信息>
```

**备注：**

- 待复核：平台支持取决于链接的 stacktrace 实现。

### 高精度十进制计算器

- **类别：** 数值计算器
- **类型：** 别名和类
- **定义位置：** `cpp/include/mental1104/high_precision_decimal.h`
- **包含：** `#include "mental1104/high_precision_decimal.h"`
- **用途：** 构造计算器对象并读取格式化十进制字符串。

**基础用法：**

```cpp
#include "mental1104/high_precision_decimal.h"
#include <iostream>

int main() {
  PiCalculator pi(50);
  ECalculator e(50);
  FixedPointCalculator fixed(50);

  std::cout << pi.to_string() << "\n";
  std::cout << e.getDecimalSubstring(1, 8) << "\n";
  std::cout << fixed.to_string() << "\n";
}
```

**示例输出：**

```text
3.14159265358979323846264338327950288419716939937511
71828182
0.73908513...
```

**备注：**

- 需要带 MPFR/GMP 支持的 Boost.Multiprecision。
- 待复核：这些类目前位于全局命名空间，不同于本仓库多数 C++ API。
