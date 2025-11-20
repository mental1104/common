# PostgreSQL Session Management Tests

该目录下的 `test_postgres_session_management.py` 汇总了针对共享连接器的所有
session 相关场景（单上下文复用、pool pre-ping 自动重连、懒加载会话、SessionAwareMixin、
并发 session 隔离等）。为了便于本地复现和观察日志，可以按以下步骤操作。

## 1. 准备环境变量

测试依赖 PostgreSQL 环境变量（`PGUSER/PGPASSWORD/PGHOST/PGPORT/PGDATABASE`
以及可选的 `PGAPPNAME`）。建议在 `common/.env` 内维护，并在执行前运行：

```bash
cd common/python
set -a
source ../.env    # 或你的自定义 env 文件
set +a
```

`set -a` 可确保 `.env` 中的变量被 `export`，pytest 才能读取。

## 2. 运行单个用例并打印日志

所有用例都支持通过 `pytest -k <keyword> -s --log-cli-level=INFO` 的方式单独执行。
常用示例：

| 场景 | 命令 |
| --- | --- |
| 验证 pool pre-ping 自动重连并打印前后 PID | `pytest test/test_connector/test_postgres_session_management.py -k pool_pre_ping -s --log-cli-level=INFO` |
| 验证懒加载 session 必须手动 close / rollback | `pytest ... -k lazy_session -s --log-cli-level=INFO` |
| 验证 SessionAwareMixin 自动注入 | `pytest ... -k session_aware_mixin -s --log-cli-level=INFO` |
| 验证并发 open_session 隔离 + 连接池复用 | `pytest ... -k concurrent_sessions_are_isolated_and_pool_reused -s --log-cli-level=INFO` |
| 验证池容量不足时的 Timeout 行为 | `pytest ... -k pool_timeout -s --log-cli-level=INFO` |

命令执行后终端会显示 `logger.info` 记录的 PID、session id 等关键数据，可直接用来验证假设。

## 3. 常见问题

1. **显示 “PG* 环境变量未配置完整”**  
   说明 pytest 检测不到变量，请确认已 `set -a` 并 `source` 你的 `.env`，或手动 `export` 每个变量。

2. **需要查看所有 session 相关日志**  
   可一次性跑多个用例，例如  
   `pytest ... -k "pre_ping or lazy_session or concurrent_sessions" -s --log-cli-level=INFO`。

3. **执行 pool_timeout 用例时看到 WARNING**  
   该用例会故意把池子缩小到 `pool_size=1/max_overflow=0`，并让多线程同时抢连接，
   以触发 `QueuePool limit ... timeout` 异常。日志里会出现 `WARNING mental1104.connector.postgres...`
   和 SQLAlchemy 的 timeout 提示，这是预期结果，用于证明超出池容量时会阻塞/超时。

4. **需要重置测试表数据**  
   测试会在 fixture 中自动清理 `test_connector_session_probe` 与
   `test_connector_auto_session`，不需要手工干预；如需彻底清库，手动执行
   `DROP TABLE` 即可。

通过以上命令即可快速验证连接池行为、session 生命周期以及 mixin 注入逻辑。
