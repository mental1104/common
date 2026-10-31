# Python 工具库

包名：`mental1104`。

构建或更新包导出后，可在此目录中使用 `pip install . --upgrade` 安装。顶层包由公共导出生成，因此如果新增公共函数 / 类需要通过 `from mental1104 import ...` 导入，请在发布前重新生成包 init。

## 维护规则

新增公共函数、类、枚举、protocol、可复用方法、脚本、CLI 入口或包级工具时，必须在同一次变更中更新此 README。

每个条目都需要写明类别、名称、用途、导入路径、最小用法、备注和 Python REPL 用法。如果符号已导出但预期稳定性不明确，请在备注中加入 `待复核`。

## 分类

- 序列化与格式转换
- 文本、时间、随机、环境和加密
- 文件与路径工具
- 应用辅助工具
- 上下文、ASGI 和 FastAPI
- 并发
- SQL 数据库、DAO 和工作单元
- Redis
- MongoDB
- 消息队列
- i18n
- 绘图与基准报告
- 调试与网络
- Schema 辅助工具
- CLI 与脚本
- 候选或仅示例 API

## 用法索引

| 类别 | 名称 | 类型 | 导入 / 路径 | 用途 |
|---|---|---|---|---|
| 序列化与格式转换 | `JsonParserType`, `JsonUtil`, `load_json`, `dump_json` | 枚举 / 类 / 函数 | `from mental1104 import ...` | 通过可用解析后端读写 JSON 字符串或流。 |
| 序列化与格式转换 | `YamlUtil`, `parse_yaml`, `dump_yaml` | 类 / 函数 | `from mental1104 import ...` | 读写 YAML 字符串或流。 |
| 序列化与格式转换 | `json_to_yaml`, `yaml_to_json` | 函数 | `from mental1104 import ...` | 在 JSON 和 YAML 之间转换。 |
| 文本与时间 | `replace_space_with`, `insert_newlines` | 函数 | `from mental1104 import ...` | 重写空白字符并插入简单换行。 |
| 文本与时间 | `timed`, `async_timed`, `get_current_time`, `parse_time` | 装饰器 / 函数 | `from mental1104 import ...` | 计时函数调用，并解析 / 格式化日期时间。 |
| 随机与加密 | `random_pick`, `encrypt`, `decrypt`, `generate_salt` | 函数 | `from mental1104 import ...` | 随机选择 list/dict 条目，并运行 AES-CBC 辅助函数。 |
| 环境变量 | `MissingEnvVarError`, `check_required_env_vars` | 异常 / 函数 | `from mental1104 import ...` | 校验必需环境变量。 |
| 文件与路径 | `file_iterator`, `csv_writer`, `export_csv_from_database` | 函数 | `from mental1104 import ...` | 处理文件和 CSV 行。 |
| 文件与路径 | `RenameOp`, `list_files`, `build_rename_plan`, `build_indexed_rename_plan`, `plan_directory_rename`, `plan_directory_rename_indexed`, `apply_rename_plan`, `rename_with_suffix`, `rename_with_regex_group`, `rename_with_index`, `validate_rename_plan` | dataclass / 函数 | `from mental1104.utils.batch_rename import ...` | 规划并执行避免冲突的批量文件重命名。 |
| 应用辅助工具 | `extract_page_range` | 函数 | `from mental1104 import extract_page_range` | 从 PDF 中提取页面范围。 |
| 应用辅助工具 | `AnkiApkgGenerator` | 类 | `from mental1104 import AnkiApkgGenerator` | 基于 JSON 输入构建简单的 Anki `.apkg` 卡组。 |
| 上下文与 ASGI | `RequestCtx`, `ctx`, `set_ctx`, `reset_ctx`, `ctx_diag` | 类 / 函数 | `from mental1104 import ...` | 在 `ContextVar` 中保存请求上下文。 |
| 上下文与 ASGI | `request_ctx_from_headers`, `RequestCtxMiddlewareFactory`, `RequestCtxContextVarMiddlewareFactory`, `register_request_ctx_middleware`, `register_all_request_ctx_middlewares` | 函数 / 类 | `from mental1104 import ...` | 从 FastAPI/Starlette 请求填充请求上下文。 |
| 并发 | `CoroutinePool`, `GatherStrategy`, `AsCompletedStrategy`, `FirstSuccessfulStrategy`, `ThreadExecutorCoroutinePool`, `ProcessExecutorCoroutinePool`, `TaskExecutionStrategy` | 类 | `from mental1104 import ...` | 用可配置结果策略运行 async callable 批次。 |
| 并发 | `ThreadWorkerPool`, `ProcessWorkerPool`, `MPStartMethod`, `delay`, `async_delay` | 类 / 枚举 / 函数 | `from mental1104 import ...` | 运行同步 worker pool 和简单延迟。 |
| 并发与韧性 | `CircuitBreaker`, `CircuitBreakerConfig`, `CircuitPermit`, `CircuitOpenError`, `CircuitBreakerSnapshot` | 枚举 / dataclass / 类 / 异常 | `from mental1104.concurrency.circuit_breaker import ...` | 基于失败率和慢调用率保护本地的下游接口调用。 |
| SQL 数据库 | `DBKind`, `ClickHouseProfile`, `ConnParams`, `SASettings`, `conn_params_from_env` | 枚举 / 类 / 函数 | `from mental1104.db import ...` | 构建数据库连接参数。 |
| SQL 数据库 | `SQLAlchemyClient`, `AsyncSQLAlchemyClient`, `make_sqlalchemy_client`, `make_async_sqlalchemy_client` | 类 / 函数 | `from mental1104.db import ...` | 创建带 session scope 的 SQLAlchemy client。 |
| SQL 数据库 | `DBRegistry`, `register_db`, `get_engine`, `get_async_engine`, `get_session_factory`, `get_async_session_factory`, `get_clickhouse_executor` | 类 / 函数 | `from mental1104.db import ...` | 注册并获取引擎 / session factory。 |
| SQL 数据库 | `session_scope`, `tx_scope`, `async_session_scope`, `async_tx_scope`, `pg_session_scope`, `mysql_session_scope`, `sqlite_session_scope`, `ck_session_scope`, `pg_tx_scope`, `mysql_tx_scope`, `sqlite_tx_scope`, `ck_tx_scope` | 上下文管理器 | `from mental1104.db import ...` | 打开读 / 写数据库 session。 |
| SQL 数据库 | `Base`, `TimestampMixin`, `SoftDeleteMixin`, `SessionAwareDAO`, `AutoSessionDAO`, `singleton_dao`, `make_async_dao`, `UnitOfWork`, `AsyncUnitOfWork` | 类 / 函数 | `from mental1104.db import ...` | 构建 ORM model、DAO 和服务事务作用域。 |
| SQL 数据库 | `create_all`, `create_all_async`, `drop_all`, `drop_all_async`, `register_db_and_create`, `register_db_and_create_async`, `set_migration_handler`, `run_migrations` | 函数 | `from mental1104.db import ...` | 注册 schema 并运行迁移钩子。 |
| ClickHouse | `ClickHouseExecutor`, `ClickHouseSessionAware`, `make_clickhouse_executor`, `clickhouse_session_scope`, `clickhouse_tx_scope`, `resolve_clickhouse_profile`, `apply_clickhouse_profile` | 类 / 函数 | `from mental1104.db import ...` | 通过已配置 executor 运行 ClickHouse 查询。 |
| Redis | `RedisMode`, `RedisConnParams`, `redis_params_from_env`, `RedisConnection`, `RedisLock`, `RedisBloom`, `RedisSessionAware`, `RedisRegistry`, `register_redis`, `get_redis_client`, `redis_session_scope`, `redis_tx_scope`, `ctx_redis_client`, `require_ctx_redis_client` | 类 / 函数 | `from mental1104.db import ...` | 配置 Redis client、作用域、锁和 Bloom key。 |
| MongoDB | `MongoConnParams`, `mongo_params_from_env`, `MongoConnection`, `AsyncMongoConnection`, `MongoSession`, `AsyncMongoSession`, `MongoSessionAware`, `AsyncMongoSessionAware`, `AutoMongoSessionDAO`, `MongoRegistry`, `register_mongo`, `get_mongo_client`, `get_async_mongo_client`, `mongo_session_scope`, `mongo_tx_scope`, `async_mongo_session_scope`, `async_mongo_tx_scope`, `ctx_mongo_session`, `ctx_async_mongo_session` | 类 / 函数 | `from mental1104.db import ...` 或 `from mental1104.db.nosql import ...` | 配置 Mongo client 和同步 / 异步作用域。 |
| 消息队列 | `AbstractProducer`, `AbstractConsumer`, `AbstractMessageQueue` | 抽象类 | `from mental1104.mq import ...` | 定义 producer / consumer / message queue 契约。 |
| 消息队列 | `PulsarEnvironment`, `PulsarConnector`, `PulsarMessageQueue`, `PulsarAdminHelper`, `AsyncPulsarAdminHelper`, `Producer`, `Consumer` | 类 / 枚举 | `from mental1104.mq.pulsar import ...` | 使用 Pulsar client、producer、consumer 和 admin 辅助工具。 |
| 消息队列 | `KafkaEnvironment`, `KafkaConnector`, `KafkaMessageQueue`, `KafkaAdminHelper`, `Producer`, `Consumer` | 类 / 枚举 | `from mental1104.mq.kafka import ...` | 使用 Kafka client、producer、consumer 和 admin 辅助工具。 |
| i18n | `I18n`, `FileMoProvider`, `I18nResourceProvider`, `get_locale`, `activate`, `reset_locale`, `locale_context`, `normalize_locale` | 类 / protocol / 函数 | `from mental1104.common.i18n import ...` | 加载 MO 资源并解析区域相关翻译。 |
| i18n | `extract_placeholders`, `compare_placeholders`, `localize_json`, `I18nMiddleware`, `LocaleResolver`, `ChainResolver`, `QueryResolver`, `HeaderResolver`, `CookieResolver` | 函数 / 类 | `from mental1104.common.i18n import ...` 或子模块 | 校验占位符、本地化 JSON，并解析 FastAPI locale。 |
| i18n 工具 | `PoEntry`, `parse_po`, `write_mo`, `po_text_to_mo_bytes`, `compile_po_tree`, `check_po_tree`, `main` | 类 / 函数 / CLI | `from mental1104.common.i18n.tools...` | 解析、编译并检查 PO/MO 树。 |
| 绘图 | `BenchTestType`, `BenchmarkRecord`, `BenchmarkSuite`, `PytestBenchmarkSuite`, `GoogleBenchmarkSuite`, `load_benchmark_suite`, `BenchmarkPlotter`, `TrendPlotBase`, `TimeBasedTrendPlot` | 类 / 函数 | `from mental1104 import ...` | 加载基准测试 payload 并渲染图表。 |
| 调试与网络 | `trace_if`, `deciprobe`, `fetch_status` | 装饰器 / 函数 | `from mental1104 import ...` | 条件式跟踪调用并获取 HTTP 状态码。 |
| Schema 辅助工具 | `JsonSerializable` | 类 | `from mental1104 import JsonSerializable` | 将简单对象转换为 dict。 |
| CLI 与脚本 | `python/tools/render_bench_plots.py`, `python/tools/assemble_bench_gallery.py`, `mental1104.common.i18n.tools.cli.main` | 脚本 / 函数 | 通过 Python 运行或导入 `main` | 渲染基准测试资源并运行 i18n 工具。 |
| 候选或仅示例 API | `User`, `UserDAO`, `AsyncUserDAO`, `bootstrap`, `example_read`, `example_write`, `example_read_then_write`, `example_threads`, `example_chunk_read`, `example_async_read`, `example_async_write` | 示例类 / 函数 | `from mental1104.db.examples import ...` | 演示数据库注册、DAO 和作用域用法。 |

## 详情

### JSON 工具

- **类别：** 序列化与格式转换
- **类型：** 枚举、类、函数
- **定义位置：** `python/mental1104/utils/parse_json.py`
- **导入：** `from mental1104 import JsonParserType, JsonUtil, load_json, dump_json`
- **用途：** 通过可用解析后端解析和输出 JSON。

**基础用法：**

```python
from io import StringIO
from mental1104 import JsonParserType, JsonUtil, dump_json, load_json

data = load_json('{"name": "common"}', parser=JsonParserType.JSON)
text = dump_json(data, parser="json", ensure_ascii=False)
names = JsonUtil.get_parser_names()

buf = StringIO()
dump_json({"name": "common"}, buf)
```

**REPL 用法：**

```python
>>> from mental1104 import JsonParserType, JsonUtil, dump_json, load_json
>>> load_json('{"ok": true}', parser=JsonParserType.JSON)
{'ok': True}
>>> dump_json({"ok": True}, parser="json")
'{"ok": true}'
>>> "json" in JsonUtil.get_parser_names()
True
```

**备注：**

- 无效 JSON 会返回 `None`，并在后端提供上下文时打印解析上下文。

### YAML 工具

- **类别：** 序列化与格式转换
- **类型：** 类和函数
- **定义位置：** `python/mental1104/utils/parse_yaml.py`
- **导入：** `from mental1104 import YamlUtil, parse_yaml, dump_yaml`
- **用途：** 解析和输出 YAML 字符串或流。

**基础用法：**

```python
from io import StringIO
from mental1104 import YamlUtil, dump_yaml, parse_yaml

data = parse_yaml("name: common\n")
text = dump_yaml({"name": "common"}, indent=2)
buf = StringIO()
dump_yaml({"name": "common"}, buf)
```

**REPL 用法：**

```python
>>> from mental1104 import YamlUtil, dump_yaml, parse_yaml
>>> parse_yaml("name: common\\n")
{'name': 'common'}
>>> "yaml" in YamlUtil.get_parser_names()
True
```

### 格式转换

- **类别：** 序列化与格式转换
- **类型：** 函数
- **定义位置：** `python/mental1104/app/convert.py`
- **导入：** `from mental1104 import json_to_yaml, yaml_to_json`
- **用途：** 将 JSON 文本 / 流转换为 YAML，或将 YAML 文本 / 流转换为 JSON。

**基础用法：**

```python
from mental1104 import json_to_yaml, yaml_to_json

yaml_text = json_to_yaml('{"name": "common"}', sort_keys=False)
json_text = yaml_to_json("name: common\n", ensure_ascii=False)
```

**REPL 用法：**

```python
>>> from mental1104 import json_to_yaml, yaml_to_json
>>> "name:" in json_to_yaml('{"name": "common"}')
True
>>> yaml_to_json("name: common\\n", ensure_ascii=False)
'{"name": "common"}'
```

### 文本、时间、随机、环境和加密

- **类别：** 文本、时间、随机、环境和加密
- **类型：** 函数、装饰器、异常
- **定义位置：** `python/mental1104/string`, `python/mental1104/timed.py`, `python/mental1104/utils`, `python/mental1104/env`
- **导入：** `from mental1104 import ...`
- **用途：** 提供字符串、延迟、时间戳、随机选择、环境检查和加密相关的小型通用辅助函数。

**基础用法：**

```python
import asyncio
from mental1104 import (
    MissingEnvVarError,
    async_delay,
    async_timed,
    check_required_env_vars,
    decrypt,
    delay,
    encrypt,
    generate_salt,
    get_current_time,
    insert_newlines,
    parse_time,
    random_pick,
    replace_space_with,
    timed,
)

replace_space_with("a  b", "-")
insert_newlines("abcdef", 3)
delay(0)
asyncio.run(async_delay(0))
get_current_time()
parse_time("2026-06-23 00:00:00")
random_pick([1, 2, 3])

ciphertext = encrypt("secret")
plaintext = decrypt(ciphertext)
salt = generate_salt(16)

try:
    check_required_env_vars(["HOME"])
except MissingEnvVarError:
    pass

@timed
def sync_work():
    return "ok"

@async_timed
async def async_work():
    return "ok"
```

**REPL 用法：**

```python
>>> from mental1104 import decrypt, encrypt, generate_salt, random_pick, replace_space_with
>>> replace_space_with("a  b", "-")
'a-b'
>>> decrypt(encrypt("secret"))
'secret'
>>> len(generate_salt(4))
4
>>> random_pick({"a": 1})[0]
'a'
```

**备注：**

- `encrypt`/`decrypt` 使用 AES-CBC，并要求 key/salt 长度被加密后端接受。
- `random_pick` 支持 `list` 和 `dict`；不支持的类型会抛出 `NotImplementedError`。

### 文件、CSV 和批量重命名工具

- **类别：** 文件与路径工具
- **类型：** dataclass 和函数
- **定义位置：** `python/mental1104/file`, `python/mental1104/utils/batch_rename.py`
- **导入：** `from mental1104 import file_iterator, csv_writer, export_csv_from_database`; `from mental1104.utils.batch_rename import ...`
- **用途：** 处理文件，并构建 / 应用安全的重命名计划。

**基础用法：**

```python
from pathlib import Path
from mental1104 import csv_writer, file_iterator
from mental1104.utils.batch_rename import (
    RenameOp,
    apply_rename_plan,
    build_indexed_rename_plan,
    build_rename_plan,
    list_files,
    plan_directory_rename,
    plan_directory_rename_indexed,
    rename_with_index,
    rename_with_regex_group,
    rename_with_suffix,
    validate_rename_plan,
)

files = list_files("data", predicate=lambda path: path.suffix == ".txt")
plan = build_rename_plan(files, rename_with_suffix(".bak"))
manual = [RenameOp(src=Path("a.txt"), dst=Path("b.txt"))]
validate_rename_plan(manual)
```

**REPL 用法：**

```python
>>> from pathlib import Path
>>> from mental1104.utils.batch_rename import RenameOp, build_rename_plan, rename_with_suffix
>>> build_rename_plan([Path("clip.wav")], rename_with_suffix(".m4a"))
[RenameOp(src=PosixPath('clip.wav'), dst=PosixPath('clip.m4a'))]
```

**备注：**

- `apply_rename_plan(..., dry_run=True)` 会返回操作列表，但不改动文件。
- `export_csv_from_database` 需要传入与其实现兼容的数据库查询对象。

### 应用辅助工具

- **类别：** 应用辅助工具
- **类型：** 类和函数
- **定义位置：** `python/mental1104/app`
- **导入：** `from mental1104 import AnkiApkgGenerator, extract_page_range`
- **用途：** 生成简单 Anki 包并提取 PDF 页面范围。

**基础用法：**

```python
from mental1104 import AnkiApkgGenerator, extract_page_range

generator = AnkiApkgGenerator(model_name="Basic", deck_name="Demo")
generator.add_notes_from_json("cards.json")
generator.save_to_file("demo.apkg")

extract_page_range("source.pdf", "chapter.pdf", 1, 3)
```

**REPL 用法：**

```python
>>> from mental1104 import AnkiApkgGenerator
>>> generator = AnkiApkgGenerator(deck_name="Demo")
>>> hasattr(generator, "save_to_file")
True
```

**备注：**

- `AnkiApkgGenerator` 需要 `genanki`；`extract_page_range` 需要 `pypdf`。
- PDF 示例需要本地已有文件。

### 上下文和 FastAPI 辅助工具

- **类别：** 上下文、ASGI 和 FastAPI
- **类型：** 类、工厂、函数
- **定义位置：** `python/mental1104/utils/context.py`, `python/mental1104/asgi/fastapi`
- **导入：** `from mental1104 import RequestCtx, ctx, set_ctx, reset_ctx, request_ctx_from_headers, register_request_ctx_middleware`
- **用途：** 在上下文变量中存储请求元数据，并接入 FastAPI 中间件。

**基础用法：**

```python
from fastapi import FastAPI
from mental1104 import (
    RequestCtx,
    ctx,
    ctx_diag,
    register_all_request_ctx_middlewares,
    reset_ctx,
    set_ctx,
)

token = set_ctx(RequestCtx(language="en-US", time_zone="UTC"))
try:
    current = ctx()
    diagnostics = ctx_diag()
finally:
    reset_ctx(token)

app = FastAPI()
register_all_request_ctx_middlewares(app)
```

**REPL 用法：**

```python
>>> from mental1104 import RequestCtx, ctx, reset_ctx, set_ctx
>>> token = set_ctx(RequestCtx(language="en-US", time_zone="UTC"))
>>> ctx().language
'en-US'
>>> reset_ctx(token)
```

**备注：**

- FastAPI 辅助工具在运行时需要 FastAPI/Starlette request 对象。

### 并发辅助工具

- **类别：** 并发
- **类型：** 类、枚举、函数
- **定义位置：** `python/mental1104/concurrency`
- **导入：** `from mental1104 import CoroutinePool, GatherStrategy, AsCompletedStrategy, FirstSuccessfulStrategy, ThreadExecutorCoroutinePool, ProcessExecutorCoroutinePool, ThreadWorkerPool, ProcessWorkerPool, MPStartMethod`
- **用途：** 运行 async 批任务、基于 executor 的 coroutine 批任务和同步 worker pool。

**基础用法：**

```python
import asyncio
import functools
from mental1104 import CoroutinePool, GatherStrategy, MPStartMethod

async def task(value):
    return value

loop = asyncio.new_event_loop()
pool = CoroutinePool(loop, max_concurrent_task=4)
partials = [functools.partial(task, 1)]
result = loop.run_until_complete(pool.run_task_batch(partials, GatherStrategy()))
loop.close()
```

**REPL 用法：**

```python
>>> from mental1104 import GatherStrategy, MPStartMethod
>>> isinstance(GatherStrategy(), GatherStrategy)
True
>>> MPStartMethod.SPAWN.value
'spawn'
```

**备注：**

- `ProcessExecutorCoroutinePool` 和 `ProcessWorkerPool` 需要可被 pickle 的 callable。

### SQL 数据库注册表、作用域、DAO 和工作单元

- **类别：** SQL 数据库、DAO 和工作单元
- **类型：** 类、枚举、上下文管理器、函数
- **定义位置：** `python/mental1104/db`
- **导入：** `from mental1104.db import ...`
- **用途：** 注册数据库 client，并打开同步 / 异步读写作用域。

**基础用法：**

```python
from mental1104.db import (
    AutoSessionDAO,
    Base,
    DBKind,
    TimestampMixin,
    UnitOfWork,
    conn_params_from_env,
    register_db_and_create,
    session_scope,
    tx_scope,
)

register_db_and_create(
    DBKind.SQLITE,
    dsn="sqlite+pysqlite:///demo.db",
    db_name="demo",
    base=Base,
)

with session_scope(DBKind.SQLITE, "demo") as session:
    from sqlalchemy import text
    rows = session.execute(text("SELECT 1"))

with tx_scope(DBKind.SQLITE, "demo") as session:
    from sqlalchemy import text
    session.execute(text("SELECT 1"))
```

**REPL 用法：**

```python
>>> from mental1104.db import DBKind, ConnParams, conn_params_from_env
>>> DBKind.SQLITE.value
'sqlite'
>>> ConnParams(ip=":memory:").ip
':memory:'
```

**备注：**

- 数据库专用示例见 [mental1104/db/README.md](./mental1104/db/README.md)。
- SQLAlchemy 作用域需要相关驱动包和可访问的数据库。

### ClickHouse 辅助工具

- **类别：** SQL 数据库
- **类型：** 类和函数
- **定义位置：** `python/mental1104/db/clickhouse_adapter.py`, `python/mental1104/db/clickhouse_profiles.py`
- **导入：** `from mental1104.db import ClickHouseExecutor, make_clickhouse_executor, clickhouse_session_scope`
- **用途：** 通过轻量 executor 和 session-aware 辅助工具执行 ClickHouse 语句。

**基础用法：**

```python
from mental1104.db import make_clickhouse_executor, clickhouse_session_scope

executor = make_clickhouse_executor("clickhouse://default:@localhost:8123/default")
with clickhouse_session_scope(executor) as ch:
    rows = ch.select("SELECT 1")
```

**REPL 用法：**

```python
>>> from mental1104.db import ClickHouseProfile
>>> ClickHouseProfile.DISTRIBUTED.value
'distributed'
```

**备注：**

- 需要 ClickHouse client 依赖和可访问的 ClickHouse 服务。

### Redis 辅助工具

- **类别：** Redis
- **类型：** 类、枚举、函数、上下文管理器
- **定义位置：** `python/mental1104/db/redis`
- **导入：** `from mental1104.db import register_redis, redis_params_from_env, redis_session_scope, RedisLock, RedisBloom`
- **用途：** 配置 Redis client，并使用 session 作用域、锁辅助工具和 Bloom key 辅助工具。

**基础用法：**

```python
from mental1104.db import (
    RedisBloom,
    RedisLock,
    RedisMode,
    register_redis,
    redis_params_from_env,
    redis_session_scope,
)

register_redis(name="cache", params=redis_params_from_env())
with redis_session_scope("cache") as client:
    client.set("key", "value")

lock = RedisLock(client, "lock-key")
if lock.try_lock():
    lock.unlock()

bloom = RedisBloom(client, "bf")
bloom.add("alice")
bloom.exists("alice")
```

**REPL 用法：**

```python
>>> from mental1104.db import RedisMode, RedisConnParams
>>> RedisMode.STANDALONE.value
'standalone'
>>> RedisConnParams(host="localhost", port=6379).host
'localhost'
```

**备注：**

- 实际调用需要 `redis` 依赖和可访问的 Redis 服务。

### MongoDB 辅助工具

- **类别：** MongoDB
- **类型：** 类、函数、上下文管理器
- **定义位置：** `python/mental1104/db/nosql`
- **导入：** `from mental1104.db import register_mongo, mongo_params_from_env, mongo_session_scope, async_mongo_session_scope`
- **用途：** 配置同步 / 异步 Mongo client，并通过上下文变量注入当前 session。

**基础用法：**

```python
from mental1104.db import mongo_params_from_env, mongo_session_scope, register_mongo

register_mongo(params=mongo_params_from_env())
with mongo_session_scope() as mongo:
    collection = mongo.db["demo_users"]
    collection.insert_one({"name": "alice"})
```

**REPL 用法：**

```python
>>> from mental1104.db import MongoConnParams
>>> MongoConnParams(host="localhost", port=27017).host
'localhost'
```

**备注：**

- 实际调用需要 MongoDB 依赖和可访问的 MongoDB 服务。
- 事务作用域需要 MongoDB 副本集或分片事务支持。

### 消息队列辅助工具

- **类别：** 消息队列
- **类型：** 抽象类、connector、producer、consumer、admin 辅助工具
- **定义位置：** `python/mental1104/mq`
- **导入：** `from mental1104.mq import AbstractMessageQueue`; `from mental1104.mq.pulsar import PulsarMessageQueue`; `from mental1104.mq.kafka import KafkaMessageQueue`
- **用途：** 创建 producer / consumer，并使用 Kafka 或 Pulsar 的 admin 辅助工具。

**基础用法：**

```python
from mental1104.mq.pulsar import PulsarConnector, PulsarMessageQueue
from mental1104.mq.kafka import KafkaConnector, KafkaMessageQueue

pulsar_client = PulsarConnector.make_client()
pulsar_queue = PulsarMessageQueue(pulsar_client)

kafka_queue = KafkaMessageQueue(
    {"bootstrap.servers": KafkaConnector.get_bootstrap_servers()}
)
```

**REPL 用法：**

```python
>>> from mental1104.mq.pulsar import PulsarEnvironment
>>> PulsarEnvironment.PULSAR_HOST.value
'PULSAR_HOST'
```

**备注：**

- 需要对应 client 库和可访问的 broker。
- Kafka 和 Pulsar 模块中都存在 `Producer` 与 `Consumer` 名称，请从具体模块导入。

### i18n 运行时和工具

- **类别：** i18n
- **类型：** 类、protocol、函数、CLI 入口
- **定义位置：** `python/mental1104/common/i18n`
- **导入：** `from mental1104.common.i18n import I18n, FileMoProvider, locale_context`; 工具位于 `mental1104.common.i18n.tools`
- **用途：** 解析区域相关的 MO 翻译，并编译 / 检查 PO 树。

**基础用法：**

```python
from mental1104.common.i18n import FileMoProvider, I18n, locale_context
from mental1104.common.i18n.placeholder import compare_placeholders
from mental1104.common.i18n.tools.compile import compile_po_tree

provider = FileMoProvider("locale/mo")
i18n = I18n(provider, default_locale="en", supported={"en", "zh"})

with locale_context("en-US"):
    text = i18n.t("Hello", domain="ui")

missing, extra = compare_placeholders("Hello {name}", "Hi {name}")
```

**REPL 用法：**

```python
>>> from mental1104.common.i18n import normalize_locale
>>> normalize_locale("en-US")
'en'
>>> from mental1104.common.i18n.placeholder import extract_placeholders
>>> extract_placeholders("Hello {name}")
{'name'}
```

**备注：**

- 运行时翻译使用已编译的 MO 文件。
- FastAPI 中间件和 resolver 需要 FastAPI/Starlette request 对象。

### 绘图和基准报告

- **类别：** 绘图与基准报告
- **类型：** 类和函数
- **定义位置：** `python/mental1104/plot`, `python/tools`
- **导入：** `from mental1104 import BenchmarkPlotter, load_benchmark_suite, TrendPlotBase, TimeBasedTrendPlot`
- **用途：** 加载基准测试 payload，并渲染对比、排名和趋势图。

**基础用法：**

```python
from mental1104 import BenchmarkPlotter, BenchTestType, load_benchmark_suite

suite = load_benchmark_suite(test_type=BenchTestType.PYTEST_BENCHMARK, result_data={})
plotter = BenchmarkPlotter(suite)
metrics = plotter.available_metrics()
```

**REPL 用法：**

```python
>>> from mental1104 import BenchTestType
>>> BenchTestType.PYTEST_BENCHMARK
'pytest-benchmark'
```

**备注：**

- 实际绘图调用需要基准测试 payload 和绘图依赖。

### 调试、网络和 schema 辅助工具

- **类别：** 调试、网络、schema 辅助工具
- **类型：** 装饰器、异步函数、基类
- **定义位置：** `python/mental1104/debug`, `python/mental1104/network`, `python/mental1104/schema`
- **导入：** `from mental1104 import deciprobe, trace_if, fetch_status, JsonSerializable`
- **用途：** 跟踪调用、通过 aiohttp 获取 HTTP 状态，并将简单对象转换为字典。

**基础用法：**

```python
from mental1104 import JsonSerializable, deciprobe, trace_if

class Item(JsonSerializable):
    def __init__(self, name):
        self.name = name

@deciprobe
def build():
    return Item("common").to_dict()
```

**REPL 用法：**

```python
>>> from mental1104 import JsonSerializable
>>> class Item(JsonSerializable):
...     def __init__(self):
...         self.name = "common"
>>> Item().to_dict()
{'name': 'common'}
```

**备注：**

- `fetch_status` 需要 `aiohttp.ClientSession`。

### CLI 和脚本

- **类别：** CLI 与脚本
- **类型：** 脚本入口
- **定义位置：** `python/tools`, `python/mental1104/common/i18n/tools/cli.py`
- **导入 / 路径：** 直接运行脚本或导入 `main`
- **用途：** 渲染基准测试图表、组装基准测试 gallery，并运行 i18n 工具。

**基础用法：**

```bash
python python/tools/render_bench_plots.py --help
python python/tools/assemble_bench_gallery.py --help
python -m mental1104.common.i18n.tools --help
```

**REPL 用法：**

```python
>>> from mental1104.common.i18n.tools.cli import main
>>> callable(main)
True
```

### 候选或仅示例 DB API

- **类别：** 候选或仅示例 API
- **类型：** 类和函数
- **定义位置：** `python/mental1104/db/examples.py`
- **导入：** `from mental1104.db.examples import User, UserDAO, AsyncUserDAO, bootstrap, example_read`
- **用途：** 演示 ORM、DAO、同步作用域、异步作用域和多线程用法。

**基础用法：**

```python
from mental1104.db.examples import bootstrap, example_read, example_write

bootstrap("sqlite+pysqlite:///demo.db")
example_write("alice")
rows = example_read()
```

**REPL 用法：**

```python
>>> from mental1104.db.examples import User
>>> User.__tablename__
'users'
```

**备注：**

- 待复核：这些是公共导出，但看起来更像示例而非稳定的可复用 API。

## 开发命令

在仓库根目录运行：

```bash
./dev setup-python
./dev build-python
./dev test-python
./dev coverage-python
./dev fmt-python
./dev vet-python
./dev guard-python
./dev install-python
```

旧版包说明仍然适用：新增顶层导出后，请在打包前使用仓库的 init 生成流程重新生成 `python/mental1104/__init__.py`。

### `CircuitBreaker`

- **类别：** 并发与韧性
- **类型：** 状态枚举、配置、熔断器、许可、快照和异常
- **定义位置：** `python/mental1104/concurrency/circuit_breaker.py`
- **导入：** `from mental1104.concurrency.circuit_breaker import CircuitBreaker, CircuitBreakerConfig, CircuitOpenError`
- **用途：** 在调用方进程内按下游服务与接口维护 Closed / Open / Half-Open 状态，基于精确时间滑窗统计系统失败和慢调用。

**基础用法：**

```python
from mental1104.concurrency.circuit_breaker import (
    CircuitBreaker,
    CircuitBreakerConfig,
)

breaker = CircuitBreaker(CircuitBreakerConfig())
result = breaker.call(
    reserve_stock,
    is_failure=lambda exc: not isinstance(exc, InventoryShortage),
    fallback=lambda _: cached_unavailable_result(),
)
```

**REPL 用法：**

```python
>>> from mental1104.concurrency.circuit_breaker import CircuitBreaker, CircuitState
>>> breaker = CircuitBreaker()
>>> breaker.try_acquire().record_success()
True
>>> breaker.snapshot().state is CircuitState.CLOSED
True
```

**备注：**

- `CircuitOutcome.IGNORED` 用于库存不足、参数错误等正常业务结果；它不进入 Closed 统计窗口，在 Half-Open 中只要不超慢阈值就视为健康探针。
- 系统异常由 `is_failure` 分类；默认所有普通异常都计为失败。同步 `call` 和异步 `call_async` 只在本地拒绝时执行 fallback。
- Half-Open 每轮最多发放配置数量的探针；任一失败或慢探针立即重新 Open，达到成功条件且没有在途探针后 Closed。
- 状态基于 `time.monotonic`，线程安全且无后台线程；`snapshot` 与状态变更回调用于接入日志和指标。
- 熔断器不负责超时、限并发或重试。Open 状态应禁止重试，并与超时、Bulkhead、有限重试和 jitter 配套使用。
- 应按“下游服务 + 接口 + 调用类型”创建实例，不要做成 Redis 集中式熔断器，也不要按高基数业务 ID 建实例。

