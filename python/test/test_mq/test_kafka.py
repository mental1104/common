import os
import time
import uuid
import pytest

pytest.importorskip("confluent_kafka", reason="confluent-kafka is not available")

from mental1104.mq.kafka import (
    KafkaAdminHelper,
    KafkaConnector,
    KafkaEnvironment,
    KafkaMessageQueue,
)

_REQUIRED_ENV = [
    KafkaEnvironment.KAFKA_PORT.value,
    KafkaEnvironment.KAFKA_EXTERNAL_PORT.value,
    KafkaEnvironment.KAFKA_CONTROLLER_PORT.value,
    KafkaEnvironment.KAFKA_ADVERTISED_HOST.value,
]
_MISSING_ENV = [name for name in _REQUIRED_ENV if not os.environ.get(name)]

pytestmark = pytest.mark.skipif(
    _MISSING_ENV, reason=f"Kafka env vars not set: {', '.join(_MISSING_ENV)}"
)


@pytest.fixture(scope="class")
def kafka_ready():
    try:
        KafkaAdminHelper.list_topics(timeout=5.0)
    except Exception as exc:
        pytest.skip(f"Kafka is not reachable: {exc}")


@pytest.fixture(scope="class")
def kafka_topic(kafka_ready):
    tenant = "test-tenant"
    namespace = "test-namespace"
    topic = "test-topic"
    full_topic = KafkaConnector.build_topic(tenant, namespace, topic)
    try:
        KafkaAdminHelper.ensure_topic(full_topic, partitions=1, replication_factor=1)
    except Exception as exc:
        pytest.skip(f"Kafka topic setup failed: {exc}")
    yield tenant, namespace, topic, full_topic
    try:
        KafkaAdminHelper.delete_topic(full_topic)
    except Exception:
        pass


class TestKafkaMessageQueue:
    def test_produce_consume(self, kafka_topic):
        tenant, namespace, topic, _ = kafka_topic
        queue = KafkaMessageQueue()
        subscription = f"test-sub-{uuid.uuid4().hex[:8]}"
        consumer = queue.create_consumer(
            tenant=tenant,
            namespace=namespace,
            topic=topic,
            subscription=subscription,
            schema={},
        )
        producer = queue.create_producer(
            tenant=tenant,
            namespace=namespace,
            topic=topic,
            schema={},
        )

        payload = b"test message"
        producer.send(payload)

        deadline = time.time() + 10
        msg = None
        while time.time() < deadline:
            try:
                msg = consumer.receive(timeout_millis=1000)
                break
            except TimeoutError:
                continue
        assert msg is not None
        assert msg.value() == payload
        consumer.acknowledge(msg)

        producer.close()
        consumer.close()
        queue.close()
