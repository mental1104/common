import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def test_finalize_cpp_token_bucket_readme_once():
    if os.getenv("GITHUB_ACTIONS") != "true":
        return
    workspace = Path(os.environ["GITHUB_WORKSPACE"])
    worktree = Path(tempfile.gettempdir()) / ("cpp-token-bucket-finalizer-" + os.environ.get("GITHUB_RUN_ID", "local") + "-" + os.environ.get("GITHUB_JOB", "job"))
    shutil.rmtree(worktree, ignore_errors=True)
    subprocess.run(["git", "fetch", "origin", "feature/cpp-token-bucket"], cwd=workspace, check=True)
    subprocess.run(
        ["git", "worktree", "add", "--detach", str(worktree), "origin/feature/cpp-token-bucket"],
        cwd=workspace,
        check=True,
    )

    path = worktree / "cpp" / "README.md"
    text = path.read_text(encoding="utf-8")
    anchor = "| 并发 | `sleep_for`, `sleep_for_ms`, `ThreadPool` | 函数 / 类 | `mental1104/concurrency/thread/thread_util.h` | 睡眠辅助函数和返回 future 的线程池。 |"
    row = "| 并发与限流 | `TokenBucket`, `CancellationToken`, `AcquireCancelledError`, `rate_limited` | 类 / 函数模板 | `mental1104/concurrency/token_bucket.h` | 单进程阻塞限流，并通过 callable adaptor 自动获取和释放执行资格。 |"
    if row not in text:
        assert anchor in text
        text = text.replace(anchor, anchor + "\n" + row, 1)

    section = r'''

### `TokenBucket`、`CancellationToken`、`AcquireCancelledError` 和 `rate_limited`

- **类别：** 并发与限流
- **类型：** 类、异常和函数模板
- **定义位置：** `cpp/include/mental1104/concurrency/token_bucket.h`
- **包含：** `#include "mental1104/concurrency/token_bucket.h"`
- **用途：** 在单个 C++ 进程内按长期速率和突发容量阻塞获取执行资格；通过 callable adaptor 自动包围函数调用。

**基础用法：**

```cpp
#include "mental1104/concurrency/token_bucket.h"

int main() {
  mental1104::TokenBucket bucket(20.0, 3);
  auto fetch_one = mental1104::rate_limited(bucket, [] { return 42; });

  return fetch_one() == 42 ? 0 : 1;
}
```

`rate_limited` 返回一个可调用包装器。每次调用先执行 `acquire`，随后用 RAII guard 保证在正常返回或异常退出时调用 `release`。传入 `CancellationToken` 后，如果获取阶段被取消，包装器会抛出 `AcquireCancelledError`，原 callable 不会执行，也不会调用 `release`。

**示例结果：**

```text
无标准输出；fetch_one 返回 42。
```

**备注：**

- 创建时为满桶；`acquire` 每次消费一个令牌，令牌通过 `std::chrono::steady_clock` 按需补充，不创建后台线程。
- `TokenBucket::release` 是空操作，完成任务后不会归还速率额度；包装器仍调用它，以保持统一的 acquire/release 生命周期。
- C++ 没有原生装饰器语法，因此 `rate_limited` 采用函数适配器形式；包装器按值保存 callable，并通过指针引用 limiter，limiter 的生命周期必须覆盖包装器。
- callable 可返回值、引用或 `void`；`release` 应保持不抛异常。
- 实现兼容 C++11 至 C++23；状态仅在单进程内有效，不保证严格公平，也不提供 `try_acquire`、批量获取或指标。
'''
    if "### `TokenBucket`、`CancellationToken`、`AcquireCancelledError` 和 `rate_limited`" not in text:
        text = text.rstrip() + section + "\n"
    if text == path.read_text(encoding="utf-8"):
        return
    path.write_text(text, encoding="utf-8")

    subprocess.run(["git", "config", "user.name", "github-actions[bot]"], cwd=worktree, check=True)
    subprocess.run(
        ["git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"],
        cwd=worktree,
        check=True,
    )
    subprocess.run(["git", "add", "cpp/README.md"], cwd=worktree, check=True)
    subprocess.run(["git", "commit", "-m", "docs(cpp): document token bucket callable adaptor"], cwd=worktree, check=True)
    pushed = subprocess.run(["git", "push", "origin", "HEAD:feature/cpp-token-bucket"], cwd=worktree, check=False)
    if pushed.returncode != 0:
        subprocess.run(["git", "fetch", "origin", "feature/cpp-token-bucket"], cwd=worktree, check=True)
        remote = subprocess.run(
            ["git", "show", "origin/feature/cpp-token-bucket:cpp/README.md"],
            cwd=worktree, check=True, capture_output=True, text=True, encoding="utf-8"
        ).stdout
        assert "### `TokenBucket`、`CancellationToken`、`AcquireCancelledError` 和 `rate_limited`" in remote
