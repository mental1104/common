# Python utilities

Package name: `mental1104`.

Install from this directory with `pip install . --upgrade` after building or updating package exports. The top-level package is generated from public exports, so if a new public function/class should be imported as `from mental1104 import ...`, regenerate the package init before release.

## Maintenance rule

When adding a public function, class, enum, protocol, reusable method, script, CLI entry, or package-level utility, update this README in the same change.

Each entry needs category, name, purpose, import path, minimal usage, notes, and Python REPL usage. If the symbol is exported but its intended stability is unclear, add `Needs review` in the notes.

## Categories

- Serialization and conversion
- Text, time, random, environment, and encryption
- File and path utilities
- App helpers
- Context, ASGI, and FastAPI
- Concurrency
- SQL database, DAO, and unit of work
- Redis
- MongoDB
- Messaging
- i18n
- Plotting and benchmark reports
- Debugging and networking
- Schema helpers
- CLI and scripts
- Candidate or example-only APIs

## Usage index

| Category | Name | Type | Import / Path | Purpose |
|---|---|---|---|---|
| Serialization and conversion | `JsonParserType`, `JsonUtil`, `load_json`, `dump_json` | enum/class/functions | `from mental1104 import ...` | Read and write JSON strings or streams through available parser backends. |
| Serialization and conversion | `YamlUtil`, `parse_yaml`, `dump_yaml` | class/functions | `from mental1104 import ...` | Read and write YAML strings or streams. |
| Serialization and conversion | `json_to_yaml`, `yaml_to_json` | functions | `from mental1104 import ...` | Convert between JSON and YAML. |
| Text and time | `replace_space_with`, `insert_newlines` | functions | `from mental1104 import ...` | Rewrite whitespace and insert simple line breaks. |
| Text and time | `timed`, `async_timed`, `get_current_time`, `parse_time` | decorators/functions | `from mental1104 import ...` | Time function calls and parse/format datetimes. |
| Random and encryption | `random_pick`, `encrypt`, `decrypt`, `generate_salt` | functions | `from mental1104 import ...` | Pick random list/dict entries and run AES-CBC helpers. |
| Environment | `MissingEnvVarError`, `check_required_env_vars` | exception/function | `from mental1104 import ...` | Validate required environment variables. |
| File and path | `file_iterator`, `csv_writer`, `export_csv_from_database` | functions | `from mental1104 import ...` | Process files and CSV rows. |
| File and path | `RenameOp`, `list_files`, `build_rename_plan`, `build_indexed_rename_plan`, `plan_directory_rename`, `plan_directory_rename_indexed`, `apply_rename_plan`, `rename_with_suffix`, `rename_with_regex_group`, `rename_with_index`, `validate_rename_plan` | dataclass/functions | `from mental1104.utils.batch_rename import ...` | Plan and apply collision-safe batch file renames. |
| App helpers | `extract_page_range` | function | `from mental1104 import extract_page_range` | Extract a page range from a PDF. |
| App helpers | `AnkiApkgGenerator` | class | `from mental1104 import AnkiApkgGenerator` | Build a simple Anki `.apkg` deck from JSON input. |
| Context and ASGI | `RequestCtx`, `ctx`, `set_ctx`, `reset_ctx`, `ctx_diag` | class/functions | `from mental1104 import ...` | Store request context in a `ContextVar`. |
| Context and ASGI | `request_ctx_from_headers`, `RequestCtxMiddlewareFactory`, `RequestCtxContextVarMiddlewareFactory`, `register_request_ctx_middleware`, `register_all_request_ctx_middlewares` | functions/classes | `from mental1104 import ...` | Populate request context from FastAPI/Starlette requests. |
| Concurrency | `CoroutinePool`, `GatherStrategy`, `AsCompletedStrategy`, `FirstSuccessfulStrategy`, `ThreadExecutorCoroutinePool`, `ProcessExecutorCoroutinePool`, `TaskExecutionStrategy` | classes | `from mental1104 import ...` | Run batches of async callables with configurable result strategies. |
| Concurrency | `ThreadWorkerPool`, `ProcessWorkerPool`, `MPStartMethod`, `delay`, `async_delay` | classes/enum/functions | `from mental1104 import ...` | Run sync worker pools and simple delays. |
| SQL database | `DBKind`, `ClickHouseProfile`, `ConnParams`, `SASettings`, `conn_params_from_env` | enums/classes/function | `from mental1104.db import ...` | Build DB connection parameters. |
| SQL database | `SQLAlchemyClient`, `AsyncSQLAlchemyClient`, `make_sqlalchemy_client`, `make_async_sqlalchemy_client` | classes/functions | `from mental1104.db import ...` | Create SQLAlchemy clients with session scopes. |
| SQL database | `DBRegistry`, `register_db`, `get_engine`, `get_async_engine`, `get_session_factory`, `get_async_session_factory`, `get_clickhouse_executor` | class/functions | `from mental1104.db import ...` | Register and retrieve engines/session factories. |
| SQL database | `session_scope`, `tx_scope`, `async_session_scope`, `async_tx_scope`, `pg_session_scope`, `mysql_session_scope`, `sqlite_session_scope`, `ck_session_scope`, `pg_tx_scope`, `mysql_tx_scope`, `sqlite_tx_scope`, `ck_tx_scope` | context managers | `from mental1104.db import ...` | Open read/write DB sessions. |
| SQL database | `Base`, `TimestampMixin`, `SoftDeleteMixin`, `SessionAwareDAO`, `AutoSessionDAO`, `singleton_dao`, `make_async_dao`, `UnitOfWork`, `AsyncUnitOfWork` | classes/functions | `from mental1104.db import ...` | Build ORM models, DAOs, and service transaction scopes. |
| SQL database | `create_all`, `create_all_async`, `drop_all`, `drop_all_async`, `register_db_and_create`, `register_db_and_create_async`, `set_migration_handler`, `run_migrations` | functions | `from mental1104.db import ...` | Register schemas and run migration hooks. |
| ClickHouse | `ClickHouseExecutor`, `ClickHouseSessionAware`, `make_clickhouse_executor`, `clickhouse_session_scope`, `clickhouse_tx_scope`, `resolve_clickhouse_profile`, `apply_clickhouse_profile` | classes/functions | `from mental1104.db import ...` | Run ClickHouse queries through configured executors. |
| Redis | `RedisMode`, `RedisConnParams`, `redis_params_from_env`, `RedisConnection`, `RedisLock`, `RedisBloom`, `RedisSessionAware`, `RedisRegistry`, `register_redis`, `get_redis_client`, `redis_session_scope`, `redis_tx_scope`, `ctx_redis_client`, `require_ctx_redis_client` | classes/functions | `from mental1104.db import ...` | Configure Redis clients, scopes, locks, and Bloom keys. |
| MongoDB | `MongoConnParams`, `mongo_params_from_env`, `MongoConnection`, `AsyncMongoConnection`, `MongoSession`, `AsyncMongoSession`, `MongoSessionAware`, `AsyncMongoSessionAware`, `AutoMongoSessionDAO`, `MongoRegistry`, `register_mongo`, `get_mongo_client`, `get_async_mongo_client`, `mongo_session_scope`, `mongo_tx_scope`, `async_mongo_session_scope`, `async_mongo_tx_scope`, `ctx_mongo_session`, `ctx_async_mongo_session` | classes/functions | `from mental1104.db import ...` or `from mental1104.db.nosql import ...` | Configure Mongo clients and sync/async scopes. |
| Messaging | `AbstractProducer`, `AbstractConsumer`, `AbstractMessageQueue` | abstract classes | `from mental1104.mq import ...` | Define producer/consumer/message-queue contracts. |
| Messaging | `PulsarEnvironment`, `PulsarConnector`, `PulsarMessageQueue`, `PulsarAdminHelper`, `AsyncPulsarAdminHelper`, `Producer`, `Consumer` | classes/enums | `from mental1104.mq.pulsar import ...` | Use Pulsar clients, producers, consumers, and admin helpers. |
| Messaging | `KafkaEnvironment`, `KafkaConnector`, `KafkaMessageQueue`, `KafkaAdminHelper`, `Producer`, `Consumer` | classes/enums | `from mental1104.mq.kafka import ...` | Use Kafka clients, producers, consumers, and admin helpers. |
| i18n | `I18n`, `FileMoProvider`, `I18nResourceProvider`, `get_locale`, `activate`, `reset_locale`, `locale_context`, `normalize_locale` | classes/protocol/functions | `from mental1104.common.i18n import ...` | Load MO resources and resolve locale-aware translations. |
| i18n | `extract_placeholders`, `compare_placeholders`, `localize_json`, `I18nMiddleware`, `LocaleResolver`, `ChainResolver`, `QueryResolver`, `HeaderResolver`, `CookieResolver` | functions/classes | `from mental1104.common.i18n import ...` or submodules | Validate placeholders, localize JSON, and resolve FastAPI locales. |
| i18n tools | `PoEntry`, `parse_po`, `write_mo`, `po_text_to_mo_bytes`, `compile_po_tree`, `check_po_tree`, `main` | class/functions/CLI | `from mental1104.common.i18n.tools...` | Parse, compile, and check PO/MO trees. |
| Plotting | `BenchTestType`, `BenchmarkRecord`, `BenchmarkSuite`, `PytestBenchmarkSuite`, `GoogleBenchmarkSuite`, `load_benchmark_suite`, `BenchmarkPlotter`, `TrendPlotBase`, `TimeBasedTrendPlot` | classes/functions | `from mental1104 import ...` | Load benchmark payloads and render plots. |
| Debugging and networking | `trace_if`, `deciprobe`, `fetch_status` | decorators/functions | `from mental1104 import ...` | Trace calls conditionally and fetch HTTP status codes. |
| Schema helpers | `JsonSerializable` | class | `from mental1104 import JsonSerializable` | Convert simple objects to dicts. |
| CLI and scripts | `python/tools/render_bench_plots.py`, `python/tools/assemble_bench_gallery.py`, `mental1104.common.i18n.tools.cli.main` | scripts/functions | run with Python or import `main` | Render benchmark assets and run i18n tooling. |
| Candidate or example-only APIs | `User`, `UserDAO`, `AsyncUserDAO`, `bootstrap`, `example_read`, `example_write`, `example_read_then_write`, `example_threads`, `example_chunk_read`, `example_async_read`, `example_async_write` | example classes/functions | `from mental1104.db.examples import ...` | Demonstrate DB registration, DAO, and scope usage. |

## Details

### JSON utilities

**Category:** Serialization and conversion  
**Type:** enum, class, functions  
**Defined in:** `python/mental1104/utils/parse_json.py`  
**Import:** `from mental1104 import JsonParserType, JsonUtil, load_json, dump_json`  
**Purpose:** Parse and dump JSON through available parser backends.

**Basic usage:**

```python
from io import StringIO
from mental1104 import JsonParserType, JsonUtil, dump_json, load_json

data = load_json('{"name": "common"}', parser=JsonParserType.JSON)
text = dump_json(data, parser="json", ensure_ascii=False)
names = JsonUtil.get_parser_names()

buf = StringIO()
dump_json({"name": "common"}, buf)
```

**REPL usage:**

```python
>>> from mental1104 import JsonParserType, JsonUtil, dump_json, load_json
>>> load_json('{"ok": true}', parser=JsonParserType.JSON)
{'ok': True}
>>> dump_json({"ok": True}, parser="json")
'{"ok": true}'
>>> "json" in JsonUtil.get_parser_names()
True
```

**Notes:**

- Invalid JSON returns `None` and prints parse context where the backend exposes it.

### YAML utilities

**Category:** Serialization and conversion  
**Type:** class and functions  
**Defined in:** `python/mental1104/utils/parse_yaml.py`  
**Import:** `from mental1104 import YamlUtil, parse_yaml, dump_yaml`  
**Purpose:** Parse and dump YAML strings or streams.

**Basic usage:**

```python
from io import StringIO
from mental1104 import YamlUtil, dump_yaml, parse_yaml

data = parse_yaml("name: common\n")
text = dump_yaml({"name": "common"}, indent=2)
buf = StringIO()
dump_yaml({"name": "common"}, buf)
```

**REPL usage:**

```python
>>> from mental1104 import YamlUtil, dump_yaml, parse_yaml
>>> parse_yaml("name: common\\n")
{'name': 'common'}
>>> "yaml" in YamlUtil.get_parser_names()
True
```

### Format conversion

**Category:** Serialization and conversion  
**Type:** functions  
**Defined in:** `python/mental1104/app/convert.py`  
**Import:** `from mental1104 import json_to_yaml, yaml_to_json`  
**Purpose:** Convert JSON text/streams to YAML and YAML text/streams to JSON.

**Basic usage:**

```python
from mental1104 import json_to_yaml, yaml_to_json

yaml_text = json_to_yaml('{"name": "common"}', sort_keys=False)
json_text = yaml_to_json("name: common\n", ensure_ascii=False)
```

**REPL usage:**

```python
>>> from mental1104 import json_to_yaml, yaml_to_json
>>> "name:" in json_to_yaml('{"name": "common"}')
True
>>> yaml_to_json("name: common\\n", ensure_ascii=False)
'{"name": "common"}'
```

### Text, time, random, environment, and encryption

**Category:** Text, time, random, environment, and encryption  
**Type:** functions, decorators, exception  
**Defined in:** `python/mental1104/string`, `python/mental1104/timed.py`, `python/mental1104/utils`, `python/mental1104/env`  
**Import:** `from mental1104 import ...`  
**Purpose:** Common small helpers for strings, delays, timestamps, random selection, environment checks, and encryption.

**Basic usage:**

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

**REPL usage:**

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

**Notes:**

- `encrypt`/`decrypt` use AES-CBC and expect key/salt sizes accepted by the crypto backend.
- `random_pick` supports `list` and `dict`; unsupported types raise `NotImplementedError`.

### File, CSV, and batch rename utilities

**Category:** File and path utilities  
**Type:** dataclass and functions  
**Defined in:** `python/mental1104/file`, `python/mental1104/utils/batch_rename.py`  
**Import:** `from mental1104 import file_iterator, csv_writer, export_csv_from_database`; `from mental1104.utils.batch_rename import ...`  
**Purpose:** Process files and build/apply safe rename plans.

**Basic usage:**

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

**REPL usage:**

```python
>>> from pathlib import Path
>>> from mental1104.utils.batch_rename import RenameOp, build_rename_plan, rename_with_suffix
>>> build_rename_plan([Path("clip.wav")], rename_with_suffix(".m4a"))
[RenameOp(src=PosixPath('clip.wav'), dst=PosixPath('clip.m4a'))]
```

**Notes:**

- `apply_rename_plan(..., dry_run=True)` returns the operations without touching files.
- `export_csv_from_database` expects a DB query object compatible with its implementation.

### App helpers

**Category:** App helpers  
**Type:** class and functions  
**Defined in:** `python/mental1104/app`  
**Import:** `from mental1104 import AnkiApkgGenerator, extract_page_range`  
**Purpose:** Generate simple Anki packages and extract PDF page ranges.

**Basic usage:**

```python
from mental1104 import AnkiApkgGenerator, extract_page_range

generator = AnkiApkgGenerator(model_name="Basic", deck_name="Demo")
generator.add_notes_from_json("cards.json")
generator.save_to_file("demo.apkg")

extract_page_range("source.pdf", "chapter.pdf", 1, 3)
```

**REPL usage:**

```python
>>> from mental1104 import AnkiApkgGenerator
>>> generator = AnkiApkgGenerator(deck_name="Demo")
>>> hasattr(generator, "save_to_file")
True
```

**Notes:**

- `AnkiApkgGenerator` requires `genanki`; `extract_page_range` requires `pypdf`.
- PDF examples require existing local files.

### Context and FastAPI helpers

**Category:** Context, ASGI, and FastAPI  
**Type:** class, factories, functions  
**Defined in:** `python/mental1104/utils/context.py`, `python/mental1104/asgi/fastapi`  
**Import:** `from mental1104 import RequestCtx, ctx, set_ctx, reset_ctx, request_ctx_from_headers, register_request_ctx_middleware`  
**Purpose:** Store request metadata in context variables and wire it into FastAPI middleware.

**Basic usage:**

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

**REPL usage:**

```python
>>> from mental1104 import RequestCtx, ctx, reset_ctx, set_ctx
>>> token = set_ctx(RequestCtx(language="en-US", time_zone="UTC"))
>>> ctx().language
'en-US'
>>> reset_ctx(token)
```

**Notes:**

- FastAPI helpers require FastAPI/Starlette request objects at runtime.

### Concurrency helpers

**Category:** Concurrency  
**Type:** classes, enum, functions  
**Defined in:** `python/mental1104/concurrency`  
**Import:** `from mental1104 import CoroutinePool, GatherStrategy, AsCompletedStrategy, FirstSuccessfulStrategy, ThreadExecutorCoroutinePool, ProcessExecutorCoroutinePool, ThreadWorkerPool, ProcessWorkerPool, MPStartMethod`  
**Purpose:** Run async batches, executor-backed coroutine batches, and sync worker pools.

**Basic usage:**

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

**REPL usage:**

```python
>>> from mental1104 import GatherStrategy, MPStartMethod
>>> isinstance(GatherStrategy(), GatherStrategy)
True
>>> MPStartMethod.SPAWN.value
'spawn'
```

**Notes:**

- `ProcessExecutorCoroutinePool` and `ProcessWorkerPool` need callables that can be pickled.

### SQL database registry, scopes, DAO, and unit of work

**Category:** SQL database, DAO, and unit of work  
**Type:** classes, enums, context managers, functions  
**Defined in:** `python/mental1104/db`  
**Import:** `from mental1104.db import ...`  
**Purpose:** Register database clients and open sync/async read/write scopes.

**Basic usage:**

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

**REPL usage:**

```python
>>> from mental1104.db import DBKind, ConnParams, conn_params_from_env
>>> DBKind.SQLITE.value
'sqlite'
>>> ConnParams(ip=":memory:").ip
':memory:'
```

**Notes:**

- See [mental1104/db/README.md](./mental1104/db/README.md) for DB-specific examples.
- SQLAlchemy scopes require the relevant driver packages and reachable databases.

### ClickHouse helpers

**Category:** SQL database  
**Type:** classes and functions  
**Defined in:** `python/mental1104/db/clickhouse_adapter.py`, `python/mental1104/db/clickhouse_profiles.py`  
**Import:** `from mental1104.db import ClickHouseExecutor, make_clickhouse_executor, clickhouse_session_scope`  
**Purpose:** Execute ClickHouse statements through a lightweight executor and session-aware helpers.

**Basic usage:**

```python
from mental1104.db import make_clickhouse_executor, clickhouse_session_scope

executor = make_clickhouse_executor("clickhouse://default:@localhost:8123/default")
with clickhouse_session_scope(executor) as ch:
    rows = ch.select("SELECT 1")
```

**REPL usage:**

```python
>>> from mental1104.db import ClickHouseProfile
>>> ClickHouseProfile.DISTRIBUTED.value
'distributed'
```

**Notes:**

- Requires a ClickHouse client dependency and a reachable ClickHouse service.

### Redis helpers

**Category:** Redis  
**Type:** classes, enum, functions, context managers  
**Defined in:** `python/mental1104/db/redis`  
**Import:** `from mental1104.db import register_redis, redis_params_from_env, redis_session_scope, RedisLock, RedisBloom`  
**Purpose:** Configure Redis clients and use session scopes, lock helpers, and Bloom key helpers.

**Basic usage:**

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

**REPL usage:**

```python
>>> from mental1104.db import RedisMode, RedisConnParams
>>> RedisMode.STANDALONE.value
'standalone'
>>> RedisConnParams(host="localhost", port=6379).host
'localhost'
```

**Notes:**

- Requires `redis` dependencies and a reachable Redis service for live calls.

### MongoDB helpers

**Category:** MongoDB  
**Type:** classes, functions, context managers  
**Defined in:** `python/mental1104/db/nosql`  
**Import:** `from mental1104.db import register_mongo, mongo_params_from_env, mongo_session_scope, async_mongo_session_scope`  
**Purpose:** Configure sync/async Mongo clients and inject current sessions through context variables.

**Basic usage:**

```python
from mental1104.db import mongo_params_from_env, mongo_session_scope, register_mongo

register_mongo(params=mongo_params_from_env())
with mongo_session_scope() as mongo:
    collection = mongo.db["demo_users"]
    collection.insert_one({"name": "alice"})
```

**REPL usage:**

```python
>>> from mental1104.db import MongoConnParams
>>> MongoConnParams(host="localhost", port=27017).host
'localhost'
```

**Notes:**

- Requires MongoDB dependencies and a reachable MongoDB service for live calls.
- Transaction scopes require MongoDB replica set or sharded transaction support.

### Messaging helpers

**Category:** Messaging  
**Type:** abstract classes, connectors, producers, consumers, admin helpers  
**Defined in:** `python/mental1104/mq`  
**Import:** `from mental1104.mq import AbstractMessageQueue`; `from mental1104.mq.pulsar import PulsarMessageQueue`; `from mental1104.mq.kafka import KafkaMessageQueue`  
**Purpose:** Create producers/consumers and use admin helpers for Kafka or Pulsar.

**Basic usage:**

```python
from mental1104.mq.pulsar import PulsarConnector, PulsarMessageQueue
from mental1104.mq.kafka import KafkaConnector, KafkaMessageQueue

pulsar_client = PulsarConnector.make_client()
pulsar_queue = PulsarMessageQueue(pulsar_client)

kafka_queue = KafkaMessageQueue(
    {"bootstrap.servers": KafkaConnector.get_bootstrap_servers()}
)
```

**REPL usage:**

```python
>>> from mental1104.mq.pulsar import PulsarEnvironment
>>> PulsarEnvironment.PULSAR_HOST.value
'PULSAR_HOST'
```

**Notes:**

- Requires the corresponding client libraries and reachable brokers.
- `Producer` and `Consumer` names exist in both Kafka and Pulsar modules; import from the specific module.

### i18n runtime and tools

**Category:** i18n  
**Type:** classes, protocols, functions, CLI entry  
**Defined in:** `python/mental1104/common/i18n`  
**Import:** `from mental1104.common.i18n import I18n, FileMoProvider, locale_context`; tools under `mental1104.common.i18n.tools`  
**Purpose:** Resolve locale-aware MO translations and compile/check PO trees.

**Basic usage:**

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

**REPL usage:**

```python
>>> from mental1104.common.i18n import normalize_locale
>>> normalize_locale("en-US")
'en'
>>> from mental1104.common.i18n.placeholder import extract_placeholders
>>> extract_placeholders("Hello {name}")
{'name'}
```

**Notes:**

- Runtime translation uses compiled MO files.
- FastAPI middleware and resolvers require request objects from FastAPI/Starlette.

### Plotting and benchmark reports

**Category:** Plotting and benchmark reports  
**Type:** classes and functions  
**Defined in:** `python/mental1104/plot`, `python/tools`  
**Import:** `from mental1104 import BenchmarkPlotter, load_benchmark_suite, TrendPlotBase, TimeBasedTrendPlot`  
**Purpose:** Load benchmark payloads and render comparison/ranking/trend plots.

**Basic usage:**

```python
from mental1104 import BenchmarkPlotter, BenchTestType, load_benchmark_suite

suite = load_benchmark_suite(test_type=BenchTestType.PYTEST_BENCHMARK, result_data={})
plotter = BenchmarkPlotter(suite)
metrics = plotter.available_metrics()
```

**REPL usage:**

```python
>>> from mental1104 import BenchTestType
>>> BenchTestType.PYTEST_BENCHMARK
'pytest-benchmark'
```

**Notes:**

- Real plotting calls require benchmark payloads and plotting dependencies.

### Debugging, networking, and schema helpers

**Category:** Debugging, networking, schema helpers  
**Type:** decorators, async function, base class  
**Defined in:** `python/mental1104/debug`, `python/mental1104/network`, `python/mental1104/schema`  
**Import:** `from mental1104 import deciprobe, trace_if, fetch_status, JsonSerializable`  
**Purpose:** Trace calls, fetch HTTP status through aiohttp, and convert simple objects to dictionaries.

**Basic usage:**

```python
from mental1104 import JsonSerializable, deciprobe, trace_if

class Item(JsonSerializable):
    def __init__(self, name):
        self.name = name

@deciprobe
def build():
    return Item("common").to_dict()
```

**REPL usage:**

```python
>>> from mental1104 import JsonSerializable
>>> class Item(JsonSerializable):
...     def __init__(self):
...         self.name = "common"
>>> Item().to_dict()
{'name': 'common'}
```

**Notes:**

- `fetch_status` requires an `aiohttp.ClientSession`.

### CLI and scripts

**Category:** CLI and scripts  
**Type:** script entry points  
**Defined in:** `python/tools`, `python/mental1104/common/i18n/tools/cli.py`  
**Import / Path:** run scripts directly or import `main`  
**Purpose:** Render benchmark plots, assemble benchmark galleries, and run i18n tooling.

**Basic usage:**

```bash
python python/tools/render_bench_plots.py --help
python python/tools/assemble_bench_gallery.py --help
python -m mental1104.common.i18n.tools --help
```

**REPL usage:**

```python
>>> from mental1104.common.i18n.tools.cli import main
>>> callable(main)
True
```

### Candidate or example-only DB APIs

**Category:** Candidate or example-only APIs  
**Type:** classes and functions  
**Defined in:** `python/mental1104/db/examples.py`  
**Import:** `from mental1104.db.examples import User, UserDAO, AsyncUserDAO, bootstrap, example_read`  
**Purpose:** Demonstrate ORM, DAO, sync scopes, async scopes, and threaded usage.

**Basic usage:**

```python
from mental1104.db.examples import bootstrap, example_read, example_write

bootstrap("sqlite+pysqlite:///demo.db")
example_write("alice")
rows = example_read()
```

**REPL usage:**

```python
>>> from mental1104.db.examples import User
>>> User.__tablename__
'users'
```

**Notes:**

- Needs review: these are public exports but appear to be examples rather than stable reusable APIs.

## Dev commands

From the repository root:

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

The older package note still applies: after adding new top-level exports, regenerate `python/mental1104/__init__.py` with the repository's init generation workflow before packaging.
