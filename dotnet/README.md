# .NET 工具库

## 文件与路径移动

| 类别 | 名称 | 类型 | 定义 / 命名空间 | 用途 |
|---|---|---|---|---|
| 文件处理 | `FileOperations.MoveFile`, `MoveResult`, `MoveStatus` | 静态方法 / record / 枚举 | `src/Mental1104/Files/FileOperations.cs`；`Mental1104.Files` | 文件、目录和链接移动及部分成功报告 |

```csharp
using Mental1104.Files;

// 使用新的临时目录，写入后关闭源。
string root = Path.Combine(Path.GetTempPath(), "move-example-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    string source = Path.Combine(root, "source"), target = Path.Combine(root, "target");
    File.WriteAllText(source, "hello");
    MoveResult result = FileOperations.MoveFile(source, target);
    System.Diagnostics.Debug.Assert(result.Status == MoveStatus.Completed);
    System.Diagnostics.Debug.Assert(result.SourceRemoved && result.Error is null);
    System.Diagnostics.Debug.Assert(File.ReadAllText(target) == "hello" && !File.Exists(source));
}
finally { Directory.Delete(root, true); }
```

示例结果：Linux x64/arm64 无标准输出，目标曾包含 `hello`，源删除，最后回收目标。
`MoveFile(string source, string target, bool overwrite = false)` 返回 `MoveResult`；
显式覆盖传 `overwrite: true`。发布前异常与发布后的 `Status`、`SourceRemoved`、`Error` 分开处理。
首例见 `tests/Mental1104.Tests/MoveFileTests.cs`。

平台：Linux x64/arm64 依赖内核/libc 的 `statx` 和 `renameat2`；Windows 只提供原生同卷移动，
成功后报告 `DurabilityUncertain`。macOS 及其他 Linux ABI 当前明确拒绝并保持路径不变，
不使用布局不兼容的 POSIX stat 结构模拟支持。

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
