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

### AES 加解密

- **类别 / 类型：** 对称加密；`Mental1104.CipherAlgorithm` 枚举与 `Mental1104.Encryption` 静态类。
- **定义位置：** `dotnet/src/Mental1104/Encryption.cs`。
- **用途：** 使用 .NET 8 标准加密 API 兼容 Python 历史 CBC 和统一版本化 AES-256-GCM，无新增依赖。

```csharp
using Mental1104;
using System.Security.Cryptography;

byte[] key = RandomNumberGenerator.GetBytes(32); // 生成后妥善保存，解密必须使用同一密钥。
byte[] ciphertext = Encryption.Encrypt("secret", key, algorithm: CipherAlgorithm.AesGcm);
Console.WriteLine(Encryption.Decrypt(ciphertext, key));
```

示例输出：`secret`。`Encrypt(string, byte[]? key = null, byte[]? salt = null, CipherAlgorithm algorithm = LegacyCbc)`
返回标准 Base64 ASCII `byte[]`；`Decrypt(byte[], byte[]? key = null, byte[]? salt = null, CipherAlgorithm? algorithm = null)`
返回 UTF-8 `string`，默认按 Magic 自动识别。输入数组不修改，内部加密资源自动释放。
CBC 的 null Key 使用 `0ePThPnLaJcWFcRc`，加密必须显式提供 16 字节 salt，省略、null 或空数组均拒绝；`Encrypt("secret")` 不再受支持。
每次 CBC 加密应生成新的随机 IV 并与密文保存，解密复用原 IV；仅历史解密允许 null salt 使用 Key 作为 IV。PKCS#7 与旧线格式不变。
GCM 必须显式提供 32 字节 Key 和 null salt，每次生成 12 字节随机 Nonce，使用 16 字节 Tag，无 Padding。
统一封装为 Base64 编码的 `b"mental1104:AES:" + 0x01 + Nonce + Tag + Ciphertext`，Magic 与版本参与认证。
未知版本、截断、格式错误抛 `FormatException`，非法参数抛 `ArgumentException`，认证失败抛 `CryptographicException`，均不降级 CBC；无效 UTF-8 也会拒绝。
只接收 GCM 时显式传 `CipherAlgorithm.AesGcm`，拒绝缺失或损坏 Magic 的输入；调用方负责密钥管理。

### `ExeChecker`

- **类别：** 可执行文件校验
- **类型：** 静态类
- **定义位置：** `dotnet/src/Mental1104/Executables/ExeChecker.cs`
- **命名空间：** `Mental1104.Executables`
- **用途：** 归集可执行文件校验辅助函数。

**基础用法：**

```csharp
using Mental1104.Executables;

bool ok = ExeChecker.IsValidExe("missing.exe");
Console.WriteLine(ok);
```

**示例输出：**

```text
False
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

**示例输出：**

当 `fileName` 指向有效 Windows PE 可执行文件时：

```text
valid executable
```

当文件缺失、无效、是 DLL 或机器类型不受支持时，`IsValidExe(fileName)` 返回 `false`，上面的分支不会输出内容。

**备注：**

- 对缺失文件和格式错误的输入返回 `false`。
- 内部 PE 头结构体是 `internal`，因此不作为公共 API 记录。
