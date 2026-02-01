from __future__ import annotations

import json
import logging
import os
from enum import Enum
from typing import Any, Callable, Optional

from mental1104 import check_required_env_vars
from .abstract_message_queue import AbstractConsumer, AbstractMessageQueue, AbstractProducer


def _load_confluent():
    try:
        from confluent_kafka import (
            Consumer as KafkaConsumerLib,
            Producer as KafkaProducerLib,
            KafkaError,
            KafkaException,
            TopicPartition,
        )
        from confluent_kafka.admin import AdminClient, NewTopic
    except Exception as exc:
        raise ImportError("confluent-kafka is required for KafkaMessageQueue") from exc
    return (
        KafkaConsumerLib,
        KafkaProducerLib,
        KafkaError,
        KafkaException,
        TopicPartition,
        AdminClient,
        NewTopic,
    )


class KafkaEnvironment(str, Enum):
    KAFKA_PORT = "KAFKA_PORT"
    KAFKA_EXTERNAL_PORT = "KAFKA_EXTERNAL_PORT"
    KAFKA_CONTROLLER_PORT = "KAFKA_CONTROLLER_PORT"
    KAFKA_ADVERTISED_HOST = "KAFKA_ADVERTISED_HOST"


class KafkaConnector:
    @staticmethod
    def get_bootstrap_servers() -> str:
        check_required_env_vars(
            [
                KafkaEnvironment.KAFKA_ADVERTISED_HOST.value,
                KafkaEnvironment.KAFKA_EXTERNAL_PORT.value,
            ]
        )
        host = os.environ[KafkaEnvironment.KAFKA_ADVERTISED_HOST.value]
        port = os.environ[KafkaEnvironment.KAFKA_EXTERNAL_PORT.value]
        return f"{host}:{port}"

    @staticmethod
    def build_topic(tenant: str, namespace: str, topic: str) -> str:
        parts = [p for p in (tenant, namespace, topic) if p]
        return ".".join(parts)


def _serialize_record(record: Any) -> Optional[bytes]:
    if record is None:
        return None
    if isinstance(record, bytes):
        return record
    if isinstance(record, str):
        return record.encode("utf-8")
    if isinstance(record, (dict, list, tuple, int, float, bool)):
        return json.dumps(record, ensure_ascii=True).encode("utf-8")
    return str(record).encode("utf-8")


class KafkaMessageQueue(AbstractMessageQueue):
    def __init__(self, config: Optional[dict[str, Any]] = None):
        self.__config = dict(config or {})
        self.__is_closed = False

    def create_producer(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        schema: Any,
        batching_enabled: bool = True,
    ) -> "Producer":
        _ = schema
        if self.__is_closed:
            raise RuntimeError("KafkaMessageQueue is closed.")
        config = dict(self.__config)
        config.setdefault("bootstrap.servers", KafkaConnector.get_bootstrap_servers())
        if not batching_enabled:
            config.setdefault("linger.ms", 0)
        return Producer(KafkaConnector.build_topic(tenant, namespace, topic), config=config)

    def create_consumer(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        subscription: str,
        schema: Any,
        subscription_type=None,
        message_listener: Optional[Callable[..., Any]] = None,
        **kwargs: Any,
    ) -> "Consumer":
        _ = schema
        _ = subscription_type
        if self.__is_closed:
            raise RuntimeError("KafkaMessageQueue is closed.")
        config = dict(self.__config)
        config.setdefault("bootstrap.servers", KafkaConnector.get_bootstrap_servers())
        config.setdefault("group.id", subscription)
        config.setdefault("auto.offset.reset", "earliest")
        config.update(kwargs)
        return Consumer(
            KafkaConnector.build_topic(tenant, namespace, topic),
            config=config,
            message_listener=message_listener,
        )

    def close(self) -> None:
        self.__is_closed = True


class Consumer(AbstractConsumer):
    def __init__(
        self,
        topic: str,
        *,
        config: dict[str, Any],
        message_listener: Optional[Callable[..., Any]] = None,
    ):
        (
            KafkaConsumerLib,
            _KafkaProducerLib,
            _KafkaError,
            KafkaException,
            TopicPartition,
            _AdminClient,
            _NewTopic,
        ) = _load_confluent()
        self.__consumer = KafkaConsumerLib(config)
        self.__topics = [topic]
        self.__message_listener = message_listener
        self.__topic_partition = TopicPartition
        self.__kafka_exception = KafkaException
        self.__consumer.subscribe(self.__topics)

    def __del__(self):
        self.close()

    def receive(self, timeout_millis: Optional[int] = None) -> Any:
        timeout = 1.0 if timeout_millis is None else max(0.0, timeout_millis / 1000.0)
        msg = self.__consumer.poll(timeout)
        if msg is None:
            raise TimeoutError("No message received from Kafka.")
        if msg.error() is not None:
            raise RuntimeError(f"Kafka consume failed: {msg.error()}")
        if self.__message_listener is not None:
            self.__message_listener(msg)
        return msg

    def acknowledge(self, record: Any) -> None:
        try:
            self.__consumer.commit(message=record, asynchronous=False)
        except self.__kafka_exception as exc:
            raise RuntimeError(f"Kafka commit failed: {exc}") from exc

    def negative_acknowledge(self, record: Any) -> None:
        try:
            tp = self.__topic_partition(record.topic(), record.partition(), record.offset())
            self.__consumer.seek(tp)
        except self.__kafka_exception as exc:
            raise RuntimeError(f"Kafka seek failed: {exc}") from exc

    def unsubscribe(self) -> None:
        self.__consumer.unsubscribe()

    def resubscribe(self) -> None:
        self.__consumer.subscribe(self.__topics)

    def close(self) -> None:
        if self.__consumer is not None:
            self.__consumer.close()
            self.__consumer = None


class Producer(AbstractProducer):
    def __init__(self, topic: str, *, config: dict[str, Any]):
        (
            _KafkaConsumerLib,
            KafkaProducerLib,
            _KafkaError,
            KafkaException,
            _TopicPartition,
            _AdminClient,
            _NewTopic,
        ) = _load_confluent()
        self.__producer = KafkaProducerLib(config)
        self.__topic = topic
        self.__closed = False

    def __del__(self):
        self.close()

    def send(self, record: Any) -> None:
        if self.__closed:
            raise RuntimeError("Cannot send message; producer is already closed.")
        payload = _serialize_record(record)
        error_holder = {}

        def _on_delivery(err, msg):
            if err is not None:
                error_holder["err"] = err

        self.__producer.produce(self.__topic, value=payload, on_delivery=_on_delivery)
        remaining = self.__producer.flush(10.0)
        if remaining:
            raise RuntimeError("Kafka send timed out; messages still in queue.")
        if error_holder.get("err") is not None:
            raise RuntimeError(f"Kafka send failed: {error_holder['err']}")

    def send_async(self, record: Any, callback: Optional[Callable[..., Any]] = None) -> None:
        if self.__closed:
            raise RuntimeError("Cannot send message; producer is already closed.")
        payload = _serialize_record(record)
        if callback is None:
            callback = self.__default_callback(record)
        self.__producer.produce(self.__topic, value=payload, on_delivery=callback)
        self.__producer.poll(0)

    @classmethod
    def __default_callback(cls, record):
        def callback(err, msg):
            if err is not None:
                logging.warning("kafka send failed: %s", err)

        return callback

    def close(self) -> None:
        if self.__closed:
            return
        if self.__producer is not None:
            self.__producer.flush(10.0)
            self.__producer = None
        self.__closed = True


class KafkaAdminHelper:
    @staticmethod
    def _admin_client(config: Optional[dict[str, Any]] = None):
        (
            _KafkaConsumerLib,
            _KafkaProducerLib,
            _KafkaError,
            _KafkaException,
            _TopicPartition,
            AdminClient,
            _NewTopic,
        ) = _load_confluent()
        base = {"bootstrap.servers": KafkaConnector.get_bootstrap_servers()}
        if config:
            base.update(config)
        return AdminClient(base)

    @staticmethod
    def create_topic(
        topic: str,
        partitions: int = 1,
        replication_factor: int = 1,
        *,
        config: Optional[dict[str, Any]] = None,
    ) -> None:
        (
            _KafkaConsumerLib,
            _KafkaProducerLib,
            KafkaError,
            KafkaException,
            _TopicPartition,
            AdminClient,
            NewTopic,
        ) = _load_confluent()
        base = {"bootstrap.servers": KafkaConnector.get_bootstrap_servers()}
        if config:
            base.update(config)
        admin = AdminClient(base)
        futures = admin.create_topics(
            [NewTopic(topic, num_partitions=partitions, replication_factor=replication_factor)]
        )
        future = futures.get(topic)
        if future is None:
            return
        try:
            future.result()
        except KafkaException as exc:
            if exc.args and exc.args[0].code() == KafkaError.TOPIC_ALREADY_EXISTS:
                return
            raise

    @staticmethod
    def delete_topic(topic: str) -> None:
        (
            _KafkaConsumerLib,
            _KafkaProducerLib,
            _KafkaError,
            KafkaException,
            _TopicPartition,
            AdminClient,
            _NewTopic,
        ) = _load_confluent()
        admin = AdminClient({"bootstrap.servers": KafkaConnector.get_bootstrap_servers()})
        futures = admin.delete_topics([topic])
        future = futures.get(topic)
        if future is None:
            return
        try:
            future.result()
        except KafkaException as exc:
            raise RuntimeError(f"Kafka delete topic failed: {exc}") from exc

    @staticmethod
    def list_topics(timeout: float = 5.0) -> list[str]:
        admin = KafkaAdminHelper._admin_client()
        meta = admin.list_topics(timeout=timeout)
        return sorted(meta.topics.keys())

    @staticmethod
    def is_topic_exists(topic: str) -> bool:
        return topic in KafkaAdminHelper.list_topics()

    @staticmethod
    def ensure_topic(
        topic: str,
        partitions: int = 1,
        replication_factor: int = 1,
    ) -> None:
        if KafkaAdminHelper.is_topic_exists(topic):
            return
        KafkaAdminHelper.create_topic(
            topic, partitions=partitions, replication_factor=replication_factor
        )
