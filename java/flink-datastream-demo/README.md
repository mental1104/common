# Java utilities and Flink demo

Maven project: `com.mental1104:flink-datastream-demo`.

## Maintenance rule

When adding a public class, method, reusable Flink pipeline, example entry point, or package-level utility, update this README with its category, package, purpose, minimal usage example, and notes. If the API is public but not clearly stable, mark it `Needs review`.

## Categories

- Collection and string containment
- Flink demo pipeline
- CLI entry

## Usage index

| Category | Name | Type | Package | Purpose |
|---|---|---|---|---|
| Collection and string containment | `Contains.contains` | static method | `com.mental1104.common` | Convenience containment check for strings, chars, iterables, arrays, and map keys. |
| Collection and string containment | `Contains.inString` | static method | `com.mental1104.common` | Check whether one character sequence contains another. |
| Collection and string containment | `Contains.inChar` | static method | `com.mental1104.common` | Check whether a character sequence contains a char. |
| Collection and string containment | `Contains.inIterable` | static method | `com.mental1104.common` | Type-safe lookup in an `Iterable`. |
| Collection and string containment | `Contains.inArray` | static method | `com.mental1104.common` | Type-safe lookup in an object array. |
| Collection and string containment | `Contains.inMapKey` | static method | `com.mental1104.common` | Type-safe map-key lookup. |
| Collection and string containment | `Contains.inMapValue` | static method | `com.mental1104.common` | Type-safe map-value lookup. |
| Flink demo pipeline | `DemoPipeline.build` | static method | `com.mental1104.flink.impl` | Build the demo `DataStream<Integer>` pipeline. |
| Flink demo pipeline | `DemoPipeline.jobName` | static method | `com.mental1104.flink.impl` | Return the demo job name. |
| CLI entry | `SimpleJob.main` | static method | `com.mental1104.flink.examples` | Run the demo Flink job. |

## Details

### `Contains.contains`

**Category:** Collection and string containment  
**Type:** static method  
**Defined in:** `src/main/java/com/mental1104/common/Contains.java`  
**Package:** `com.mental1104.common`  
**Purpose:** Use one method for common containment checks.

**Basic usage:**

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

**Notes:**

- Map handling checks keys. Use `inMapValue` for value lookup.

### Typed `Contains` helpers

**Category:** Collection and string containment  
**Type:** static methods  
**Defined in:** `src/main/java/com/mental1104/common/Contains.java`  
**Package:** `com.mental1104.common`  
**Purpose:** Use explicit helper methods when the container type is known.

**Basic usage:**

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

### `DemoPipeline.build`

**Category:** Flink demo pipeline  
**Type:** static method  
**Defined in:** `src/main/java/com/mental1104/flink/impl/DemoPipeline.java`  
**Package:** `com.mental1104.flink.impl`  
**Purpose:** Build the demo stream from a `StreamExecutionEnvironment`.

**Basic usage:**

```java
import com.mental1104.flink.impl.DemoPipeline;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public class Example {
  public static void main(String[] args) throws Exception {
    StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
    DemoPipeline.build(env).print();
    env.execute(DemoPipeline.jobName());
  }
}
```

**Notes:**

- Requires Flink dependencies from this Maven project.

### `DemoPipeline.jobName`

**Category:** Flink demo pipeline  
**Type:** static method  
**Defined in:** `src/main/java/com/mental1104/flink/impl/DemoPipeline.java`  
**Package:** `com.mental1104.flink.impl`  
**Purpose:** Provide the job name passed to `env.execute(...)`.

**Basic usage:**

```java
import com.mental1104.flink.impl.DemoPipeline;

public class Example {
  public static void main(String[] args) {
    System.out.println(DemoPipeline.jobName());
  }
}
```

### `SimpleJob.main`

**Category:** CLI entry  
**Type:** static method  
**Defined in:** `src/main/java/com/mental1104/flink/examples/SimpleJob.java`  
**Package:** `com.mental1104.flink.examples`  
**Purpose:** Run the demo pipeline as the Maven or shaded-jar main class.

**Basic usage:**

```bash
cd java/flink-datastream-demo
mvn -q -DskipTests compile exec:java
```

**Docker/Flink cluster usage:**

```bash
cd java/flink-datastream-demo
mvn -q -DskipTests package
docker compose -f ../../devops/images/flink/docker-compose.yaml up -d
docker compose -f ../../devops/images/flink/docker-compose.yaml exec flink-jobmanager \
  ./bin/flink run -c com.mental1104.flink.examples.SimpleJob \
  /opt/flink/usrlib/flink-datastream-demo.jar
```

## Dev commands

From the repository root:

```bash
./dev setup-java
./dev build-java
./dev test-java
./dev coverage-java
./dev run-java
./dev run-java-docker
```
