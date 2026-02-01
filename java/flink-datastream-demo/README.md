# Flink DataStream Demo (single-node, containerized)

This is a minimal, repeatable Flink DataStream skeleton that compiles and runs in under an hour.
It includes:
- A Java Flink job that finishes with exit code 0
- Docker Compose for a local Flink JobManager/TaskManager (reused from `common/devops/images/flink`)
- A Python tool package skeleton under `common/python`

## Layout

- `common/java/flink-datastream-demo`
  - `pom.xml` (Maven build)
  - `src/main/java/com/mental1104/flink/impl/DemoPipeline.java`
  - `src/main/java/com/mental1104/flink/examples/SimpleJob.java`
  - `src/test/java/com/mental1104/flink/tests/DemoPipelineTest.java`
- `common/devops/images/flink/docker-compose.yaml` (shared local Flink cluster)
- `common/python/mental1104/flink_demo`
  - `__init__.py`, `__main__.py`, `cli.py` (tooling skeleton)

## Start/stop (Docker Compose)

Start (build jar, bring up cluster, submit job):

```bash
cd java/flink-datastream-demo
mvn -q -DskipTests package

docker compose -f ../../devops/images/flink/docker-compose.yaml up -d
docker compose -f ../../devops/images/flink/docker-compose.yaml exec flink-jobmanager \
  ./bin/flink run -c com.mental1104.flink.examples.SimpleJob \
  /opt/flink/usrlib/flink-datastream-demo.jar
```

Stop (tear down cluster):

```bash
docker compose -f ../../devops/images/flink/docker-compose.yaml down
```

## Dev commands (recommended)

From repo root:

```bash
./dev setup-java
./dev build-java
./dev test-java
./dev coverage-java
./dev run-java
./dev run-java-docker
./dev docker-java-up
./dev docker-java-down
```

## Manual steps (local and containerized)

### 1) Build and test (local)

```bash
cd java/flink-datastream-demo
mvn -q test
```

### 2) Run job locally (no Docker)

```bash
cd java/flink-datastream-demo
mvn -q -DskipTests compile exec:java
```

### 3) Build jar and run on Docker Compose cluster

```bash
mvn -q -DskipTests package
docker compose -f ../../devops/images/flink/docker-compose.yaml up -d

docker compose -f ../../devops/images/flink/docker-compose.yaml exec flink-jobmanager \
  ./bin/flink run -c com.mental1104.flink.examples.SimpleJob \
  /opt/flink/usrlib/flink-datastream-demo.jar
```

Stop cluster:

```bash
docker compose -f ../../devops/images/flink/docker-compose.yaml down
```

## Python tool skeleton (optional smoke)

From repo root:

```bash
python -m mental1104.flink_demo
```

## Experiment checklist

1) Create directories and base build files
2) Write the minimal main/job skeleton
3) Build and run an empty topology locally
4) Record commands and outputs

## Recorded commands and sample output

Commands (local run):

```bash
cd java/flink-datastream-demo
mvn -q test
mvn -q -DskipTests compile exec:java
```

Sample output (your log lines may differ):

```
1
Job execution finished
```

## Retest plan

After 7 days, re-clone the repo into a clean directory and run the commands above.
Record total time-to-green and any diffs in output or setup steps.
