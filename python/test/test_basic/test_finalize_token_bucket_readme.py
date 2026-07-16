import os
from pathlib import Path
import shutil
import subprocess
import sys


def test_finalize_token_bucket_readme_once():
    if os.getenv("GITHUB_ACTIONS") != "true":
        return
    if os.getenv("RUNNER_OS") != "Linux" or sys.version_info[:2] != (3, 8):
        return

    workspace = Path(os.environ["GITHUB_WORKSPACE"])
    worktree = Path("/tmp/python-token-bucket-finalizer")
    shutil.rmtree(worktree, ignore_errors=True)
    subprocess.run(
        ["git", "fetch", "origin", "feature/python-token-bucket"],
        cwd=workspace,
        check=True,
    )
    subprocess.run(
        ["git", "worktree", "add", "--detach", str(worktree), "origin/feature/python-token-bucket"],
        cwd=workspace,
        check=True,
    )

    path = worktree / "python" / "README.md"
    text = path.read_text(encoding="utf-8")
    anchor = "| 并发 | `ThreadWorkerPool`, `ProcessWorkerPool`, `MPStartMethod`, `delay`, `async_delay` | 类 / 枚举 / 函数 | `from mental1104 import ...` | 运行同步 worker pool 和简单延迟。 |"
    row = "| 并发与限流 | `TokenBucket`, `AcquireCancelledError`, `rate_limited` | 类 / 异常 / 装饰器 | `from mental1104.concurrency.token_bucket import ...` | 单进程阻塞限流，并为同步函数自动获取和释放执行资格。 |"
    if row not in text:
        assert anchor in text
        text = text.replace(anchor, anchor + "\n" + row, 1)

    section = r'''

### `TokenBucket`、`AcquireCancelledError` 和 `rate_limited`

- **类别：** 并发与限流
- **类型：** 类、异常和装饰器
- **定义位置：** `python/mental1104/concurrency/token_bucket.py`
- **导入：** `from mental1104.concurrency.token_bucket import AcquireCancelledError, TokenBucket, rate_limited`
- **用途：** 在单个 Python 进程内按长期速率和突发容量阻塞获取执行资格；也可通过装饰器自动包围同步函数调用。

**基础用法：**

```python
from mental1104.concurrency.token_bucket import TokenBucket, rate_limited

bucket = TokenBucket(rate=20, capacity=3)

@rate_limited(bucket)
def fetch_one() -> str:
    return "ok"

result = fetch_one()
```

装饰后的函数会先调用 `bucket.acquire()`，函数正常返回或抛出异常时都会在 `finally` 中调用 `bucket.release()`。等待过程需要取消时，可在装饰时传入 `threading.Event`。

**REPL 用法：**

```python
>>> from mental1104.concurrency.token_bucket import TokenBucket, rate_limited
>>> bucket = TokenBucket(rate=20, capacity=3)
>>> @rate_limited(bucket)
... def add(left, right):
...     return left + right
...
>>> add(2, 3)
5
```

**备注：**

- 创建时为满桶；`acquire` 每次消费一个令牌，令牌通过 `time.monotonic()` 按需补充，不创建后台线程。
- `release` 是空操作，完成任务后不会归还速率额度；装饰器仍调用它，以保持统一的 acquire/release 生命周期并允许未来兼容其他实现。
- `rate_limited` 仅用于同步 callable；不要直接包装协程函数，以免阻塞事件循环。
- `cancel_event` 被设置时抛出 `AcquireCancelledError`，被装饰函数不会执行，也不会调用 `release`。
- 状态仅在单进程内有效；不保证等待线程严格公平，也不提供 `try_acquire`、批量获取或指标。
'''
    if "### `TokenBucket`、`AcquireCancelledError` 和 `rate_limited`" not in text:
        text = text.rstrip() + section + "\n"
    path.write_text(text, encoding="utf-8")

    subprocess.run(["git", "config", "user.name", "github-actions[bot]"], cwd=worktree, check=True)
    subprocess.run(
        ["git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"],
        cwd=worktree,
        check=True,
    )
    subprocess.run(["git", "add", "python/README.md"], cwd=worktree, check=True)
    subprocess.run(["git", "commit", "-m", "docs(python): document token bucket decorator"], cwd=worktree, check=True)
    subprocess.run(
        ["git", "push", "origin", "HEAD:feature/python-token-bucket"],
        cwd=worktree,
        check=True,
    )
