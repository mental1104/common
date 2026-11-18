# Rust 工具库

## 文件与路径移动

| 类别 | 名称 | 类型 | 定义 / 使用 | 用途 |
|---|---|---|---|---|
| 文件处理 | `file::move_file`, `MoveResult`, `MoveStatus` | 函数 / 结构体 / 枚举 | `src/file.rs`；`use mental1104::prelude::{move_file, MoveStatus};` | 文件、目录和链接移动及部分成功报告 |

```rust
use mental1104::prelude::{move_file, MoveStatus};

/// 在调用方独占的空目录 root 中演示公开移动，返回发布前错误。
fn example(root: &std::path::Path) -> std::io::Result<()> {
    let source = root.join("source");
    let target = root.join("target");
    std::fs::write(&source, b"hello")?;
    let result = move_file(&source, &target, false)?;
    assert_eq!(result.status, MoveStatus::Completed);
    assert!(result.source_removed && result.error.is_none());
    assert_eq!(std::fs::read(&target)?, b"hello");
    assert!(!source.exists());
    std::fs::remove_file(target)?;
    Ok(())
}
```

示例结果：POSIX 返回 `Ok(())`，无标准输出，断言通过，资源已回收。
`move_file(source: impl AsRef<Path>, target: impl AsRef<Path>, overwrite: bool)`
返回 `io::Result<MoveResult>`；默认禁止覆盖通过第三参数 `false` 表达，显式替换传 `true`。
发布前为 `Err`；发布后的 `Ok` 仍须检查 `status`、`source_removed`、`error`。
首例见 `src/file.rs` 内测试和 `tests/file_it.rs`。

平台：Linux/macOS 提供原生排他命名；Windows 仅支持同卷，目录同步不支持时返回
`DurabilityUncertain`。Unix 使用 libc，Windows 使用 windows-sys，不依赖可选加密 feature。
最低版本沿用仓库 Rust 1.85 合同。

**状态与恢复：** 发布前失败按语言惯例抛异常或返回 error，保留源和旧目标，尽力删除本次临时项。
发布后通过结果区分 `Completed`（完成）、`PublishedSourceRemains`（目标已发布，源可能只清理了一部分）、
`DurabilityUncertain`（目录同步失败/不支持）；各语言枚举拼写见示例。
结果同时携带源根是否删除及失败原因。目标发布后的同步失败会停止源清理；
源删除后的同步失败仍报告不确定，并标明源已删除。不能对部分成功结果盲目重试，应先检查两端路径。

**调用约束：** `target` 是最终路径，父目录必须存在，不追加文件名、不合并目录。
默认禁止覆盖，由内核原子排他重命名裁决竞争；能力不支持时明确失败，不退化为“先检查再覆盖”。
显式覆盖遵守原生类型规则：POSIX 可替换空目录，非空目录不会被合并。
同路径、硬链接别名和将源目录移入自身后代均拒绝；顶层特殊类型及跨卷树内的 FIFO、设备、socket 等明确拒绝。

**跨卷与元信息：** 只有明确的跨卷错误才进入复制流程。在目标父目录独占创建私有临时容器，
按 64 KiB 分块复制、核对长度并逐字节读回，恢复普通 rwx 权限和 mtime，完成文件/目录同步后原子发布，
再按已核验清单后序删除源并同步目录。不跟随符号链接，仅保留链接文本和 mtime。
跨卷不具备端到端原子性；不保证 uid/gid、ACL、xattr、特权位、稀疏布局或硬链接拓扑。
同卷保留原生 rename 的行为；完成状态确认目录同步，不额外替调用方刷新之前尚未持久化的源内容。

**并发与中断：** 调用方必须独占源树，并保证祖先目录和目标父目录稳定可信。
身份、大小和时间快照只能发现可观察变化，无法证明复制期间从未发生写入，不能代替排他访问。
清理时不递归删除清单外的新项；清理错误可能留下部分源树。进程崩溃或断电可能遗留私有临时目录，
需要人工核对后回收；本 API 不提供跨卷事务日志或自动重试。



Crate 名称：`mental1104`。

## 维护规则

新增公共函数、结构体、枚举、trait、类型别名、模块、prelude 导出或可复用示例时，必须更新此 README，写明类别、路径、用途、最小用法示例和备注。如果 API 是公开的但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 集合包含判断
- 错误处理
- Prelude 与 feature 导出

## 用法索引

| 类别 | 名称 | 类型 | 使用路径 | 用途 |
|---|---|---|---|---|
| 集合包含判断 | `contains` | 函数 | `mental1104::collections::contains` 或 `mental1104::prelude::contains` | 检查受支持容器是否包含指定键 / 值。 |
| 集合包含判断 | `HasKey` | trait | `mental1104::collections::HasKey` | 供 `contains` 接受的容器实现的 trait。 |
| 集合包含判断 | `SortedSlice` | 结构体 | `mental1104::collections::SortedSlice` 或 `mental1104::prelude::SortedSlice` | 标记一个已排序切片，使其使用有序查找语义。 |
| 错误处理 | `MentalError` | 枚举 | `mental1104::error::MentalError` | 供可复用模块共享的通用错误枚举。 |
| 错误处理 | `Result<T>` | 类型别名 | `mental1104::error::Result` | 使用 `MentalError` 的仓库级结果别名。 |
| Prelude 与 feature 导出 | `prelude`, `FastHashMap`, `FastHashSet` | 模块 / 类型重导出 | `mental1104::prelude::*`, `mental1104::FastHashMap` | 便捷导入，以及可选的快速哈希 map/set 别名。 |

## 详情

### AES 加解密（可选 encryption feature）

- **类别 / 类型：** 对称加密；`encryption::CipherAlgorithm` 枚举及 `encryption::{encrypt, decrypt}` 函数。
- **定义位置 / 使用：** `src/encryption.rs`；`use mental1104::encryption::{encrypt, decrypt, CipherAlgorithm};`。
- **启用：** Cargo 依赖声明 `mental1104 = { ..., features = ["encryption"] }`；使用 RustCrypto `aes-gcm`、`cbc` 和 `base64`，默认构建不引入加密依赖。
- **用途：** 兼容 Python 历史 CBC，与其他五种语言互通版本化 AES-256-GCM。

```rust
use mental1104::encryption::{decrypt, encrypt, CipherAlgorithm};

// main 展示公开 API 往返；固定密钥仅用于示例，生产中从安全存储读取。
fn main() -> mental1104::error::Result<()> {
    let key = b"0123456789abcdef0123456789abcdef";
    let ciphertext = encrypt("secret", Some(key), None, CipherAlgorithm::AesGcm)?;
    assert_eq!(decrypt(&ciphertext, Some(key), None, None)?, "secret");
    Ok(())
}
```

示例结果：无标准输出，断言通过。`encrypt(&str, Option<&[u8]>, Option<&[u8]>, CipherAlgorithm)` 返回 Base64 `Vec<u8>`，
`decrypt(&[u8], Option<&[u8]>, Option<&[u8]>, Option<CipherAlgorithm>)` 返回 UTF-8 `String`，均使用仓库 `Result<T>`。
所有输入均为只读借用。`decrypt` 的算法 None 自动识别，或用 Some 显式指定。
`LegacyCbc` 的 None Key 使用 `0ePThPnLaJcWFcRc`，加密必须显式提供 `Some` 的 16 字节 salt，None 或空切片均拒绝。
每次 CBC 加密应生成新的随机 IV 并与密文保存，解密复用原 IV；仅历史解密允许 None salt 使用 Key 作为 IV。PKCS#7 与旧线格式不变。
`AesGcm` 必须显式提供 32 字节 Key、None salt，每次生成 12 字节随机 Nonce、16 字节 Tag，无 Padding。
统一封装为 Base64 编码的 `b"mental1104:AES:" + 0x01 + Nonce + Tag + Ciphertext`，Magic 与版本参与认证。
参数、格式、未知版本、截断、认证或 UTF-8 错误返回 `MentalError::InvalidInput`，已识别新格式绝不降级 CBC。
只接收 GCM 时显式传 `Some(CipherAlgorithm::AesGcm)`，拒绝损坏或缺失 Magic 的输入；调用方负责密钥管理。

### `contains`

- **类别：** 集合包含判断
- **类型：** 函数
- **定义位置：** `rust/mental1104/src/collections/contains.rs`
- **使用：** `use mental1104::collections::contains;`
- **用途：** 用单个函数为受支持容器执行包含判断。

**基础用法：**

```rust
use mental1104::collections::contains;
use std::collections::HashMap;

fn main() {
    let values = vec![1, 2, 3];
    assert!(contains(values.as_slice(), &2));

    let mut names = HashMap::new();
    names.insert(String::from("alice"), 1);
    assert!(contains(&names, "alice"));
}
```

**示例结果：**

```text
无标准输出；contains(values.as_slice(), &2) 返回 true，contains(&names, "alice") 返回 true。
```

**备注：**

- 当切片已经排序且希望使用有序查找行为时，使用 `SortedSlice`。

### `HasKey`

- **类别：** 集合包含判断
- **类型：** trait
- **定义位置：** `rust/mental1104/src/collections/contains.rs`
- **使用：** `use mental1104::collections::HasKey;`
- **用途：** 让自定义容器包装类型参与 `contains` 判断。

**基础用法：**

```rust
use mental1104::collections::{contains, HasKey};

struct Bag(Vec<i32>);

impl HasKey<i32> for &Bag {
    fn contains(&self, key: &i32) -> bool {
        self.0.iter().any(|value| value == key)
    }
}

fn main() {
    let bag = Bag(vec![1, 2, 3]);
    assert!(contains(&bag, &2));
}
```

**示例结果：**

```text
无标准输出；contains(&bag, &2) 返回 true。
```

### `SortedSlice`

- **类别：** 集合包含判断
- **类型：** 结构体
- **定义位置：** `rust/mental1104/src/collections/contains.rs`
- **使用：** `use mental1104::collections::SortedSlice;`
- **用途：** 包装已排序切片，用于包含判断。

**基础用法：**

```rust
use mental1104::collections::{contains, SortedSlice};

fn main() {
    let values = [1, 2, 3, 5, 8];
    assert!(contains(SortedSlice(&values), &5));
}
```

**示例结果：**

```text
无标准输出；contains(SortedSlice(&values), &5) 返回 true。
```

**备注：**

- 调用方负责确保传入的切片已经排序。

### `MentalError` and `Result<T>`

- **类别：** 错误处理
- **类型：** 枚举和类型别名
- **定义位置：** `rust/mental1104/src/error/mod.rs`
- **使用：** `use mental1104::error::{MentalError, Result};`
- **用途：** 在 crate 模块之间共享通用错误 / 结果类型。

**基础用法：**

```rust
use mental1104::error::{MentalError, Result};

fn require_name(name: &str) -> Result<&str> {
    if name.is_empty() {
        return Err(MentalError::InvalidInput("name is empty".to_string()));
    }
    Ok(name)
}

fn main() {
    let _ = require_name("common");
}
```

**示例返回值：**

```text
require_name("common") => Ok("common")
require_name("") => Err(MentalError::InvalidInput("name is empty"))
```

### Prelude 和可选哈希别名

- **类别：** Prelude 与 feature 导出
- **类型：** 模块和重导出的类型别名
- **定义位置：** `rust/mental1104/src/prelude.rs`, `rust/mental1104/src/lib.rs`
- **使用：** `use mental1104::prelude::*;`
- **用途：** 导入常用集合辅助函数；启用 `fast-hash` feature 时，可使用 `FastHashMap` 和 `FastHashSet`。

**基础用法：**

```rust
use mental1104::prelude::*;

fn main() {
    let values = [1, 2, 3];
    assert!(contains(values.as_slice(), &2));
}
```

**示例结果：**

```text
无标准输出；contains(values.as_slice(), &2) 返回 true。
```

**备注：**

- `FastHashMap` 和 `FastHashSet` 需要启用 `fast-hash` feature。
