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
cd java/flink-datastream-demo
mvn -q -DskipTests package
docker compose -f ../../devops/images/flink/docker-compose.yaml up -d
docker compose -f ../../devops/images/flink/docker-compose.yaml exec flink-jobmanager \
  ./bin/flink run -c com.mental1104.flink.examples.SimpleJob \
  /opt/flink/usrlib/flink-datastream-demo.jar
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
