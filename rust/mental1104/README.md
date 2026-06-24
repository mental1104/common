# Rust 工具库

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
