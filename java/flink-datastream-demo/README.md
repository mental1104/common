# Java 工具库和 Flink 演示

Maven 项目：`com.mental1104:flink-datastream-demo`。

## 维护规则

新增公共类、方法、可复用 Flink pipeline、示例入口或包级工具时，必须更新此 README，写明类别、包名、用途、最小用法示例和备注。如果 API 是公开的但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 集合与字符串包含判断
- Flink 演示 pipeline
- CLI 入口

## 用法索引

| 类别 | 名称 | 类型 | 包 | 用途 |
|---|---|---|---|---|
| 集合与字符串包含判断 | `Contains.contains` | 静态方法 | `com.mental1104.common` | 对字符串、字符、可迭代对象、数组和 map 键进行便捷包含判断。 |
| 集合与字符串包含判断 | `Contains.inString` | 静态方法 | `com.mental1104.common` | 检查一个字符序列是否包含另一个字符序列。 |
| 集合与字符串包含判断 | `Contains.inChar` | 静态方法 | `com.mental1104.common` | 检查字符序列是否包含指定字符。 |
| 集合与字符串包含判断 | `Contains.inIterable` | 静态方法 | `com.mental1104.common` | 在 `Iterable` 中执行类型安全查找。 |
| 集合与字符串包含判断 | `Contains.inArray` | 静态方法 | `com.mental1104.common` | 在对象数组中执行类型安全查找。 |
| 集合与字符串包含判断 | `Contains.inMapKey` | 静态方法 | `com.mental1104.common` | 以类型安全方式检查 map 键。 |
| 集合与字符串包含判断 | `Contains.inMapValue` | 静态方法 | `com.mental1104.common` | 以类型安全方式检查 map 值。 |
| Flink 演示 pipeline | `DemoPipeline.build` | 静态方法 | `com.mental1104.flink.impl` | 构建演示用 `DataStream<Integer>` pipeline。 |
| Flink 演示 pipeline | `DemoPipeline.jobName` | 静态方法 | `com.mental1104.flink.impl` | 返回演示作业名称。 |
| CLI 入口 | `SimpleJob.main` | 静态方法 | `com.mental1104.flink.examples` | 运行演示 Flink 作业。 |

## 详情

### AES 加解密

- **类别 / 类型：** 对称加密；`CipherAlgorithm` 枚举与 `Encryption` 静态工具类。
- **定义位置：** `src/main/java/com/mental1104/common/{CipherAlgorithm,Encryption}.java`。
- **导入：** `com.mental1104.common.Encryption`、`com.mental1104.common.CipherAlgorithm`。
- **用途：** 使用 Java 11 标准库兼容 Python 历史 CBC 和六种语言统一的 AES-256-GCM，无新增依赖。

```java
import com.mental1104.common.Encryption;
import com.mental1104.common.CipherAlgorithm;
import java.security.SecureRandom;

// main 展示显式密钥的 GCM 往返；调用方负责保存和分发密钥。
class Example {
  public static void main(String[] args) throws Exception {
    byte[] key = new byte[32];
    new SecureRandom().nextBytes(key);
    byte[] ciphertext = Encryption.encrypt("secret", key, null, CipherAlgorithm.AES_GCM);
    System.out.println(Encryption.decrypt(ciphertext, key, null, null));
  }
}
```

示例输出：`secret`。`encrypt(String, byte[] key, byte[] salt, CipherAlgorithm)` 返回标准 Base64 bytes，
`decrypt(byte[], byte[] key, byte[] salt, CipherAlgorithm)` 返回 UTF-8 String；解密算法 null 表示自动识别。
`encrypt(String)` 因缺少显式 IV 而抛异常，CBC 加密必须使用带 16 字节 salt 的重载，null 或空数组均拒绝。
CBC 的 null Key 使用 `0ePThPnLaJcWFcRc`；每次加密生成新随机 IV 并与密文保存，解密复用原 IV。
`decrypt(byte[])` 仍支持历史默认 Key/IV；仅历史解密允许 null salt 使用 Key 作为 IV。
JCE `PKCS5Padding` 对 AES 的线格式等同 PKCS#7，旧数据格式不变。
GCM 必须显式提供 32 字节 Key、null salt，每次生成 12 字节随机 Nonce，使用 16 字节 Tag，无 Padding。
封装为 Base64 编码的 `b"mental1104:AES:" + 0x01 + Nonce + Tag + Ciphertext`，Magic 与版本参与认证。
格式、参数、未知版本、截断和认证失败抛 `GeneralSecurityException`（认证失败为 `AEADBadTagException`），非法 Base64 抛 `IllegalArgumentException`，均不降级 CBC；无效 UTF-8 也会拒绝。
输入数组不修改。只接收 GCM 时显式传 `AES_GCM`，拒绝缺失或损坏 Magic 的输入；调用方负责密钥管理。

### `Contains.contains`

- **类别：** 集合与字符串包含判断
- **类型：** 静态方法
- **定义位置：** `src/main/java/com/mental1104/common/Contains.java`
- **包：** `com.mental1104.common`
- **用途：** 用一个方法处理常见包含判断。

**基础用法：**

```java
import com.mental1104.common.Contains;
import java.util.List;
import java.util.Map;

public class Example {
  public static void main(String[] args) {
    System.out.println(Contains.contains("hello", "ell"));
    System.out.println(Contains.contains(List.of(1, 2, 3), 2));
    System.out.println(Contains.contains(Map.of("a", 1), "a"));
  }
}
```

**示例输出：**

```text
true
true
true
```

**备注：**

- map 处理逻辑检查键；如需查找值，请使用 `inMapValue`。

### 类型明确的 `Contains` 辅助方法

- **类别：** 集合与字符串包含判断
- **类型：** 静态方法
- **定义位置：** `src/main/java/com/mental1104/common/Contains.java`
- **包：** `com.mental1104.common`
- **用途：** 容器类型已知时，使用语义更明确的辅助方法。

**基础用法：**

```java
import com.mental1104.common.Contains;
import java.util.List;
import java.util.Map;

public class Example {
  public static void main(String[] args) {
    boolean inText = Contains.inString("hello", "ell");
    boolean inChar = Contains.inChar("hello", 'e');
    boolean inList = Contains.inIterable(List.of(1, 2, 3), 2);
    boolean inArray = Contains.inArray(new Integer[] {1, 2, 3}, 2);
    boolean hasKey = Contains.inMapKey(Map.of("a", 1), "a");
    boolean hasValue = Contains.inMapValue(Map.of("a", 1), 1);

    System.out.println(inText && inChar && inList && inArray && hasKey && hasValue);
  }
}
```

**示例输出：**

```text
true
```

### `DemoPipeline.build`

- **类别：** Flink 演示 pipeline
- **类型：** 静态方法
- **定义位置：** `src/main/java/com/mental1104/flink/impl/DemoPipeline.java`
- **包：** `com.mental1104.flink.impl`
- **用途：** 基于 `StreamExecutionEnvironment` 构建演示数据流。

**基础用法：**

```java
import com.mental1104.flink.impl.DemoPipeline;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Example {
  public static void main(String[] args) throws Exception {
    StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
    env.setParallelism(1);
    DemoPipeline.build(env).print();
    env.execute(DemoPipeline.jobName());
  }
}
```

**示例输出：**

忽略 Flink runtime 日志时，业务输出为：

```text
1
```

**备注：**

- 需要使用此 Maven 项目提供的 Flink 依赖。

### `DemoPipeline.jobName`

- **类别：** Flink 演示 pipeline
- **类型：** 静态方法
- **定义位置：** `src/main/java/com/mental1104/flink/impl/DemoPipeline.java`
- **包：** `com.mental1104.flink.impl`
- **用途：** 提供传入 `env.execute(...)` 的作业名称。

**基础用法：**

```java
import com.mental1104.flink.impl.DemoPipeline;

public class Example {
  public static void main(String[] args) {
    System.out.println(DemoPipeline.jobName());
  }
}
```

**示例输出：**

```text
Flink DataStream Demo
```

### `SimpleJob.main`

- **类别：** CLI 入口
- **类型：** 静态方法
- **定义位置：** `src/main/java/com/mental1104/flink/examples/SimpleJob.java`
- **包：** `com.mental1104.flink.examples`
- **用途：** 作为 Maven 或 shaded jar 的主类运行演示 pipeline。

**基础用法：**

```bash
cd java/flink-datastream-demo
mvn -q -DskipTests compile exec:java
```

**示例输出：**

忽略 Maven 和 Flink runtime 日志时，业务输出为：

```text
1
```

**Docker/Flink 集群用法：**

```bash
# 在 common 仓库根目录执行；默认使用平级 visitor。
export VISITOR_ROOT=/path/to/visitor
./dev build-java
./dev docker-java-up
./dev run-java-docker --no-build --no-up
```

**示例输出：**

Flink 集群日志会因环境不同而变化；pipeline 的业务输出为：

```text
1
```

## 开发命令

在仓库根目录运行：

```bash
./dev setup-java
./dev build-java
./dev test-java
./dev coverage-java
./dev run-java
./dev run-java-docker
```

**命令结果：**

```text
setup/build/test/coverage 成功时退出码为 0；run-java/run-java-docker 的业务输出为 1（运行时日志省略）。
```
