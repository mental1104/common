# .NET 工具库

项目：`dotnet/src/Mental1104/Mental1104.csproj`，目标框架为 `net8.0`。

## 维护规则

新增公共类、方法、扩展方法、结构体、枚举、接口或可复用工具时，必须更新此 README，写明类别、命名空间、用途、最小用法示例和备注。如果 API 是公开的但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 可执行文件校验

## 用法索引

| 类别 | 名称 | 类型 | 命名空间 | 用途 |
|---|---|---|---|---|
| 可执行文件校验 | `ExeChecker` | 静态类 | `Mental1104.Executables` | 校验路径是否指向受支持的 Windows 可执行镜像，而不是 DLL 或格式错误的文件。 |
| 可执行文件校验 | `ExeChecker.IsValidExe` | 静态方法 | `Mental1104.Executables` | 对有效可执行文件返回 `true`；对缺失、无效、DLL 或不受支持的文件返回 `false`。 |

## 详情

### `ExeChecker`

- **类别：** 可执行文件校验
- **类型：** 静态类
- **定义位置：** `dotnet/src/Mental1104/Executables/ExeChecker.cs`
- **命名空间：** `Mental1104.Executables`
- **用途：** 归集可执行文件校验辅助函数。

**基础用法：**

```csharp
using Mental1104.Executables;

bool ok = ExeChecker.IsValidExe("app.exe");
Console.WriteLine(ok);
```

### `ExeChecker.IsValidExe`

- **类别：** 可执行文件校验
- **类型：** 静态方法
- **定义位置：** `dotnet/src/Mental1104/Executables/ExeChecker.cs`
- **命名空间：** `Mental1104.Executables`
- **用途：** 检查文件路径是否指向有效的可执行镜像。

**基础用法：**

```csharp
using Mental1104.Executables;

var fileName = args.Length > 0 ? args[0] : "app.exe";
if (ExeChecker.IsValidExe(fileName))
{
    Console.WriteLine("valid executable");
}
```

**备注：**

- 对缺失文件和格式错误的输入返回 `false`。
- 内部 PE 头结构体是 `internal`，因此不作为公共 API 记录。
