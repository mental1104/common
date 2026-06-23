# Rust utilities

Crate name: `mental1104`.

## Maintenance rule

When adding a public function, struct, enum, trait, type alias, module, prelude export, or reusable example, update this README with its category, path, purpose, minimal usage example, and notes. If the API is public but not clearly stable, mark it `Needs review`.

## Categories

- Collection containment
- Error handling
- Prelude and feature exports

## Usage index

| Category | Name | Type | Use path | Purpose |
|---|---|---|---|---|
| Collection containment | `contains` | function | `mental1104::collections::contains` or `mental1104::prelude::contains` | Check whether a supported container contains a key/value. |
| Collection containment | `HasKey` | trait | `mental1104::collections::HasKey` | Trait implemented by containers accepted by `contains`. |
| Collection containment | `SortedSlice` | struct | `mental1104::collections::SortedSlice` or `mental1104::prelude::SortedSlice` | Mark an already-sorted slice for sorted lookup semantics. |
| Error handling | `MentalError` | enum | `mental1104::error::MentalError` | Common error enum for reusable modules. |
| Error handling | `Result<T>` | type alias | `mental1104::error::Result` | Repository result alias using `MentalError`. |
| Prelude and feature exports | `prelude`, `FastHashMap`, `FastHashSet` | module/type re-exports | `mental1104::prelude::*`, `mental1104::FastHashMap` | Convenient imports and optional fast hash map/set aliases. |

## Details

### `contains`

**Category:** Collection containment  
**Type:** function  
**Defined in:** `rust/mental1104/src/collections/contains.rs`  
**Use:** `use mental1104::collections::contains;`  
**Purpose:** Check containment through a single function for supported containers.

**Basic usage:**

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

**Notes:**

- Use `SortedSlice` when the slice is already sorted and you want sorted lookup behavior.

### `HasKey`

**Category:** Collection containment  
**Type:** trait  
**Defined in:** `rust/mental1104/src/collections/contains.rs`  
**Use:** `use mental1104::collections::HasKey;`  
**Purpose:** Let custom container wrappers participate in `contains`.

**Basic usage:**

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

### `SortedSlice`

**Category:** Collection containment  
**Type:** struct  
**Defined in:** `rust/mental1104/src/collections/contains.rs`  
**Use:** `use mental1104::collections::SortedSlice;`  
**Purpose:** Wrap a sorted slice for containment checks.

**Basic usage:**

```rust
use mental1104::collections::{contains, SortedSlice};

fn main() {
    let values = [1, 2, 3, 5, 8];
    assert!(contains(SortedSlice(&values), &5));
}
```

**Notes:**

- The caller is responsible for passing a sorted slice.

### `MentalError` and `Result<T>`

**Category:** Error handling  
**Type:** enum and type alias  
**Defined in:** `rust/mental1104/src/error/mod.rs`  
**Use:** `use mental1104::error::{MentalError, Result};`  
**Purpose:** Share common error/result types across crate modules.

**Basic usage:**

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

### Prelude and optional hash aliases

**Category:** Prelude and feature exports  
**Type:** module and re-exported type aliases  
**Defined in:** `rust/mental1104/src/prelude.rs`, `rust/mental1104/src/lib.rs`  
**Use:** `use mental1104::prelude::*;`  
**Purpose:** Import common collection helpers; when the `fast-hash` feature is enabled, use `FastHashMap` and `FastHashSet`.

**Basic usage:**

```rust
use mental1104::prelude::*;

fn main() {
    let values = [1, 2, 3];
    assert!(contains(values.as_slice(), &2));
}
```

**Notes:**

- `FastHashMap` and `FastHashSet` require the `fast-hash` feature.
