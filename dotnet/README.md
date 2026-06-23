# .NET utilities

Project: `dotnet/src/Mental1104/Mental1104.csproj` targeting `net8.0`.

## Maintenance rule

When adding a public class, method, extension method, struct, enum, interface, or reusable utility, update this README with its category, namespace, purpose, minimal usage example, and notes. If the API is public but not clearly stable, mark it `Needs review`.

## Categories

- Executable file validation

## Usage index

| Category | Name | Type | Namespace | Purpose |
|---|---|---|---|---|
| Executable file validation | `ExeChecker` | static class | `Mental1104.Executables` | Validate whether a path points to a supported Windows executable image rather than a DLL or malformed file. |
| Executable file validation | `ExeChecker.IsValidExe` | static method | `Mental1104.Executables` | Return `true` for valid executable files and `false` for missing, invalid, DLL, or unsupported files. |

## Details

### `ExeChecker`

**Category:** Executable file validation  
**Type:** static class  
**Defined in:** `dotnet/src/Mental1104/Executables/ExeChecker.cs`  
**Namespace:** `Mental1104.Executables`  
**Purpose:** Group executable file validation helpers.

**Basic usage:**

```csharp
using Mental1104.Executables;

bool ok = ExeChecker.IsValidExe("app.exe");
Console.WriteLine(ok);
```

### `ExeChecker.IsValidExe`

**Category:** Executable file validation  
**Type:** static method  
**Defined in:** `dotnet/src/Mental1104/Executables/ExeChecker.cs`  
**Namespace:** `Mental1104.Executables`  
**Purpose:** Check whether a file path is a valid executable image.

**Basic usage:**

```csharp
using Mental1104.Executables;

var fileName = args.Length > 0 ? args[0] : "app.exe";
if (ExeChecker.IsValidExe(fileName))
{
    Console.WriteLine("valid executable");
}
```

**Notes:**

- Returns `false` for missing files and malformed inputs.
- Internal PE header structs are not documented as public APIs because they are `internal`.
