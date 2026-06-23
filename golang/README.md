# Go utilities

Module path: `github.com/mental1104/common/golang`.

## Maintenance rule

When adding an exported function, type, method, package-level utility, command, or reusable lab entry, update this README with its category, package path, purpose, minimal usage example, and notes. If the API is exported but not clearly stable, mark it `Needs review`.

## Categories

- Collection and string containment
- CLI and labs

## Usage index

| Category | Name | Type | Import / Path | Purpose |
|---|---|---|---|---|
| Collection and string containment | `Contains` | function | `github.com/mental1104/common/golang` | Convenience containment check for string, rune, slices, arrays, map keys, and pointers to supported containers. |
| Collection and string containment | `InSlice` | generic function | `github.com/mental1104/common/golang` | Type-safe value lookup in slices. |
| Collection and string containment | `InMapKey` | generic function | `github.com/mental1104/common/golang` | Type-safe map-key lookup. |
| Collection and string containment | `InMapValue` | generic function | `github.com/mental1104/common/golang` | Type-safe map-value lookup. |
| Collection and string containment | `InString` | function | `github.com/mental1104/common/golang` | Substring lookup. |
| Collection and string containment | `InRune` | function | `github.com/mental1104/common/golang` | Rune lookup in a string. |
| CLI and labs | `labctl`, `labs/*` | commands | `./golang/cmd/labctl`, `./golang/labs/*` | Runnable lab/demo entry points. |

## Details

### `Contains`

**Category:** Collection and string containment  
**Type:** function  
**Defined in:** `golang/mental1104/contains.go`  
**Import:** `github.com/mental1104/common/golang`  
**Purpose:** Use one call shape for strings, runes, slices, arrays, map keys, and supported pointers.

**Basic usage:**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.Contains("golang", "go"))
	fmt.Println(mental1104.Contains([]int{3, 5, 8}, 5))
	fmt.Println(mental1104.Contains(map[string]int{"a": 1}, "a"))
}
```

**Notes:**

- `Contains` uses map-key semantics for maps.
- Prefer the generic helpers below on hot paths or when type safety matters.

### `InSlice`

**Category:** Collection and string containment  
**Type:** generic function  
**Defined in:** `golang/mental1104/contains.go`  
**Import:** `github.com/mental1104/common/golang`  
**Purpose:** Check whether a comparable value is in a slice.

**Basic usage:**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.InSlice([]string{"a", "b"}, "b"))
}
```

### `InMapKey`

**Category:** Collection and string containment  
**Type:** generic function  
**Defined in:** `golang/mental1104/contains.go`  
**Import:** `github.com/mental1104/common/golang`  
**Purpose:** Check whether a key exists in a map.

**Basic usage:**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	values := map[string]int{"a": 1}
	fmt.Println(mental1104.InMapKey(values, "a"))
}
```

### `InMapValue`

**Category:** Collection and string containment  
**Type:** generic function  
**Defined in:** `golang/mental1104/contains.go`  
**Import:** `github.com/mental1104/common/golang`  
**Purpose:** Check whether a comparable value exists among map values.

**Basic usage:**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	values := map[string]int{"a": 1}
	fmt.Println(mental1104.InMapValue(values, 1))
}
```

### `InString`

**Category:** Collection and string containment  
**Type:** function  
**Defined in:** `golang/mental1104/contains.go`  
**Import:** `github.com/mental1104/common/golang`  
**Purpose:** Check whether a string contains a substring.

**Basic usage:**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.InString("golang", "go"))
}
```

### `InRune`

**Category:** Collection and string containment  
**Type:** function  
**Defined in:** `golang/mental1104/contains.go`  
**Import:** `github.com/mental1104/common/golang`  
**Purpose:** Check whether a string contains a rune.

**Basic usage:**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.InRune("golang", 'g'))
}
```

### Lab commands

**Category:** CLI and labs  
**Type:** command packages  
**Defined in:** `golang/cmd/labctl`, `golang/labs/*`  
**Import / Path:** run with `go run ./cmd/labctl` or specific lab directories from `golang/`  
**Purpose:** Run local scheduler, network, memory, and GC lab demos.

**Basic usage:**

```bash
cd golang
go run ./cmd/labctl
go run ./labs/gc/minimal
```

**Notes:**

- Needs review: labs are runnable demos, not stable reusable library APIs.
