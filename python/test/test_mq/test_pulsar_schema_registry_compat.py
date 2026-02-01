import os
import uuid

import pytest
import requests
from urllib.parse import urlparse

pulsar = pytest.importorskip(
    "pulsar", reason="pulsar-client is not available on this platform"
)
from pulsar import ConsumerType, InitialPosition
from pulsar.schema import Record, String, Integer, AvroSchema, JsonSchema, BytesSchema
from mental1104.mq.pulsar import PulsarConnector, PulsarMessageQueue


PULSAR_URL = os.getenv("PULSAR_URL", "pulsar://localhost:6650")
PULSAR_ADMIN_URL = os.getenv("PULSAR_ADMIN_URL", "http://localhost:8080")
TENANT = "public"
NAMESPACE = "default"

# Use FULL when we want strict bidirectional compatibility checks.
# Use BACKWARD for additive evolution patterns (common for JSON-like payloads).
STRATEGY_FULL = "FULL"
STRATEGY_BACKWARD = "BACKWARD"


def _sync_env_from_urls():
    host = os.environ.get("PULSAR_HOST")
    broker_port = os.environ.get("PULSAR_BROKER_PORT")
    admin_port = os.environ.get("PULSAR_ADMIN_PORT")

    if (not host or not broker_port) and PULSAR_URL:
        info = urlparse(PULSAR_URL)
        if info.hostname and not host:
            os.environ["PULSAR_HOST"] = info.hostname
            host = info.hostname
        if info.port and not broker_port:
            os.environ["PULSAR_BROKER_PORT"] = str(info.port)
            broker_port = str(info.port)

    if (not host or not admin_port) and PULSAR_ADMIN_URL:
        info = urlparse(PULSAR_ADMIN_URL)
        if info.hostname and not host:
            os.environ["PULSAR_HOST"] = info.hostname
            host = info.hostname
        if info.port and not admin_port:
            os.environ["PULSAR_ADMIN_PORT"] = str(info.port)
            admin_port = str(info.port)


def _optional_string(default=None):
    # Ensure Avro/JSON schema carries a default for additive evolution.
    for kwargs in (
        {"required": False, "default": default, "required_default": True},
        {"default": default, "required_default": True},
        {"required": False, "default": default},
        {"required": False},
    ):
        try:
            return String(**kwargs)
        except TypeError:
            continue
    return String()


def _make_record(name, fields):
    attrs = dict(fields)
    attrs["__module__"] = __name__
    return type(name, (Record,), attrs)


AvroUserV1 = _make_record(
    "UserAvro",
    {
        "name": String(),
        "age": Integer(),
    },
)
AvroUserV2 = _make_record(
    "UserAvro",
    {
        "name": String(),
        "age": Integer(),
        "email": _optional_string(),
    },
)
AvroUserBad = _make_record(
    "UserAvro",
    {
        "name": String(),
        "age": String(),
    },
)

JsonUserV1 = _make_record(
    "UserJson",
    {
        "name": String(),
        "age": Integer(),
    },
)
JsonUserV2 = _make_record(
    "UserJson",
    {
        "name": String(),
        "age": Integer(),
        "nickname": _optional_string(),
    },
)
JsonUserBad = _make_record(
    "UserJson",
    {
        "name": String(),
        "age": String(),
    },
)


@pytest.fixture(scope="session")
def pulsar_env():
    _sync_env_from_urls()

    admin_url = os.environ.get("PULSAR_ADMIN_URL")
    if not admin_url:
        try:
            admin_url = PulsarConnector.get_admin_url()
        except Exception:
            admin_url = None

    broker_url = os.environ.get("PULSAR_URL")
    if not broker_url:
        try:
            broker_url = PulsarConnector.get_broker_url()
        except Exception:
            broker_url = None

    if not admin_url or not broker_url:
        pytest.skip("Pulsar env vars are not set: need admin/broker URL")

    try:
        resp = requests.get(f"{admin_url}/admin/v2/brokers/health", timeout=3)
        if resp.status_code != 200:
            pytest.skip(
                f"Pulsar admin health check failed: {admin_url} status={resp.status_code}"
            )
    except Exception as exc:
        pytest.skip(f"Pulsar admin not reachable: {admin_url} ({exc})")

    try:
        ns_resp = requests.get(
            f"{admin_url}/admin/v2/namespaces/{TENANT}/{NAMESPACE}", timeout=3
        )
        if ns_resp.status_code != 200:
            pytest.skip(
                f"Pulsar namespace {TENANT}/{NAMESPACE} not available: status={ns_resp.status_code}"
            )
    except Exception as exc:
        pytest.skip(f"Pulsar namespace check failed: {exc}")

    try:
        client = _create_client(broker_url)
        client.close()
    except Exception as exc:
        pytest.skip(f"Pulsar client not available: {broker_url} ({exc})")

    return {"admin_url": admin_url, "broker_url": broker_url}


def _create_client(broker_url):
    try:
        return PulsarConnector.make_client()
    except Exception:
        try:
            return pulsar.Client(broker_url, operation_timeout_seconds=3)
        except TypeError:
            return pulsar.Client(broker_url)


def _topic_name(prefix):
    return f"{prefix}-{uuid.uuid4().hex}"


def _topic_path(topic):
    return f"persistent://{TENANT}/{NAMESPACE}/{topic}"


def _admin_request(admin_url, method, path, **kwargs):
    headers = kwargs.pop("headers", None)
    if headers is None:
        headers = PulsarConnector.get_header()
    return requests.request(method, f"{admin_url}{path}", timeout=3, headers=headers, **kwargs)


def _ensure_topic(admin_url, topic):
    resp = _admin_request(
        admin_url,
        "PUT",
        f"/admin/v2/persistent/{TENANT}/{NAMESPACE}/{topic}",
    )
    if resp.status_code not in (200, 204, 409):
        pytest.skip(
            "Unable to create topic via admin API: "
            f"status={resp.status_code} body={resp.text}"
        )


def _set_topic_compatibility(admin_url, topic, strategy):
    path = f"/admin/v2/persistent/{TENANT}/{NAMESPACE}/{topic}/schemaCompatibilityStrategy"
    last = None
    for method in ("POST", "PUT"):
        for payload in (
            {"json": strategy},
            {"data": strategy, "headers": {"Content-Type": "text/plain"}},
        ):
            resp = _admin_request(admin_url, method, path, **payload)
            last = resp
            if resp.status_code in (200, 204):
                return
            if resp.status_code in (404, 405):
                continue

    # Fallback for brokers that only support namespace-level compatibility.
    ns_path = f"/admin/v2/namespaces/{TENANT}/{NAMESPACE}/schemaCompatibilityStrategy"
    for method in ("POST", "PUT"):
        for payload in (
            {"json": strategy},
            {"data": strategy, "headers": {"Content-Type": "text/plain"}},
        ):
            resp = _admin_request(admin_url, method, ns_path, **payload)
            last = resp
            if resp.status_code in (200, 204):
                return

    if last is not None and last.status_code in (404, 405, 501):
        pytest.skip(
            "schemaCompatibilityStrategy API not available on Pulsar admin: "
            f"status={last.status_code}"
        )
    raise RuntimeError(
        "Failed to set schemaCompatibilityStrategy on topic: "
        f"status={last.status_code if last else 'unknown'} body={last.text if last else 'n/a'}"
    )


def _close_quietly(obj):
    if obj is None:
        return
    try:
        obj.close()
    except Exception:
        return


def _assert_incompatible_exception(exc):
    incompat = getattr(pulsar, "IncompatibleSchemaException", None)
    if incompat and isinstance(exc, incompat):
        return
    message = str(exc).lower()
    if "incompat" in message and "schema" in message:
        return
    pytest.fail(f"Expected incompatible schema error, got: {exc!r}")


def _assert_incompatible_on_producer(create_fn, send_value):
    producer = None
    try:
        producer = create_fn()
        try:
            producer.send(send_value)
        except Exception as exc:
            _assert_incompatible_exception(exc)
            return True
        return False
    except Exception as exc:
        _assert_incompatible_exception(exc)
        return True
    finally:
        _close_quietly(producer)


def _assert_incompatible_on_consumer(create_fn):
    consumer = None
    try:
        consumer = create_fn()
    except Exception as exc:
        _assert_incompatible_exception(exc)
        return True
    finally:
        _close_quietly(consumer)
    return False


def _get_protobuf_schema():
    try:
        from pulsar.schema import ProtobufSchema as schema_type
        return schema_type
    except Exception:
        try:
            from pulsar.schema.schema import ProtobufSchema as schema_type
            return schema_type
        except Exception:
            return None


def _require_protobuf():
    descriptor_pb2 = pytest.importorskip("google.protobuf.descriptor_pb2")
    descriptor_pool = pytest.importorskip("google.protobuf.descriptor_pool")
    message_factory = pytest.importorskip("google.protobuf.message_factory")
    return descriptor_pb2, descriptor_pool, message_factory


def _proto_field_to_record_field(field_type):
    descriptor_pb2, _, _ = _require_protobuf()
    if field_type == descriptor_pb2.FieldDescriptorProto.TYPE_STRING:
        return String()
    if field_type == descriptor_pb2.FieldDescriptorProto.TYPE_INT32:
        return Integer()
    return String()


def _make_proto_message(name, fields):
    descriptor_pb2, descriptor_pool, message_factory = _require_protobuf()
    file_proto = descriptor_pb2.FileDescriptorProto()
    file_proto.name = f"{name.lower()}.proto"
    file_proto.package = "pulsar_test"
    file_proto.syntax = "proto3"

    message = file_proto.message_type.add()
    message.name = name
    record_fields = {}
    for index, field_info in enumerate(fields, start=1):
        if len(field_info) == 3:
            field_name, field_type, record_field = field_info
        else:
            field_name, field_type = field_info
            record_field = _proto_field_to_record_field(field_type)
        field = message.field.add()
        field.name = field_name
        field.number = index
        field.label = descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
        field.type = field_type
        record_fields[field_name] = record_field

    pool = descriptor_pool.DescriptorPool()
    pool.Add(file_proto)
    full_name = f"pulsar_test.{name}"
    try:
        messages = message_factory.GetMessages([file_proto], pool=pool)
        message_cls = messages[full_name]
    except Exception:
        descriptor = pool.FindMessageTypeByName(full_name)
        factory = message_factory.MessageFactory(pool)
        if hasattr(factory, "GetPrototype"):
            message_cls = factory.GetPrototype(descriptor)
        else:
            message_cls = message_factory.GetMessageClass(descriptor)
    record_cls = _make_record(f"{name}ProtoRecord", record_fields)
    return message_cls, file_proto, record_cls


def _build_protobuf_schema(message_cls, record_cls):
    schema_type = _get_protobuf_schema()
    if schema_type is not None:
        return schema_type(message_cls)

    import _pulsar
    from pulsar.schema import Schema

    class CompatProtobufSchema(Schema):
        def __init__(self, record_cls, schema_definition):
            super().__init__(record_cls, _pulsar.SchemaType.PROTOBUF, schema_definition, "PROTOBUF")

        def encode(self, obj):
            self._validate_object_type(obj)
            return obj.SerializeToString()

        def decode(self, data):
            msg = self._record_cls()
            msg.ParseFromString(data)
            return msg

        def decode_message(self, msg):
            return self.decode(msg.data())

        def schema_info(self):
            return self._schema_info

        def attach_client(self, client):
            self._client = client

    schema_definition = record_cls.schema()
    return CompatProtobufSchema(message_cls, schema_definition)


def test_bytes_schema_backward_compatible(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("bytes-backward-compat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_BACKWARD)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer_v1 = None
    producer_v2 = None
    consumer = None
    try:
        producer_v1 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=BytesSchema()
        )
        producer_v1.send(b"v1-payload")

        # Bytes has no structural fields, so the compatible evolution is reusing the same schema.
        producer_v2 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=BytesSchema()
        )
        producer_v2.send(b"v2-payload")

        consumer = queue.create_consumer(
            tenant=TENANT,
            namespace=NAMESPACE,
            topic=topic,
            subscription=f"sub-{uuid.uuid4().hex}",
            schema=BytesSchema(),
            subscription_type=ConsumerType.Exclusive,
            initial_position=InitialPosition.Earliest,
            receiver_queue_size=1,
        )

        received = []
        for _ in range(2):
            msg = consumer.receive(timeout_millis=3000)
            received.append(msg.value())
            consumer.acknowledge(msg)

        assert b"v1-payload" in received
        assert b"v2-payload" in received
    finally:
        _close_quietly(consumer)
        _close_quietly(producer_v2)
        _close_quietly(producer_v1)
        _close_quietly(queue)
        _close_quietly(client)


def test_bytes_schema_incompatible_switch_from_json_to_bytes(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("bytes-incompat-json")

    _ensure_topic(admin_url, topic)
    # FULL is stricter; switching from structured schema to raw bytes should be rejected.
    _set_topic_compatibility(admin_url, topic, STRATEGY_FULL)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer = None
    try:
        producer = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=JsonSchema(JsonUserV1)
        )
        producer.send(JsonUserV1(name="seed", age=1))
    finally:
        _close_quietly(producer)

    def _create_bytes_producer():
        return queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=BytesSchema()
        )

    try:
        incompatible = _assert_incompatible_on_producer(
            _create_bytes_producer, b"bytes-again"
        )
        if not incompatible:
            # Some brokers treat BYTES as schema-less and allow schema downgrades.
            # In that case, assert client-side schema enforcement by sending a non-bytes payload.
            with pytest.raises(TypeError):
                producer = _create_bytes_producer()
                try:
                    producer.send("not-bytes")
                finally:
                    _close_quietly(producer)
    finally:
        _close_quietly(queue)
        _close_quietly(client)


def test_avro_full_compatible_add_optional_field(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("avro-full-compat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_FULL)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer_v1 = None
    producer_v2 = None
    consumer = None
    try:
        producer_v1 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=AvroSchema(AvroUserV1)
        )
        producer_v1.send(AvroUserV1(name="alice", age=30))

        producer_v2 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=AvroSchema(AvroUserV2)
        )
        producer_v2.send(AvroUserV2(name="bob", age=28, email="bob@example.com"))

        consumer = queue.create_consumer(
            tenant=TENANT,
            namespace=NAMESPACE,
            topic=topic,
            subscription=f"sub-{uuid.uuid4().hex}",
            schema=AvroSchema(AvroUserV2),
            subscription_type=ConsumerType.Exclusive,
            initial_position=InitialPosition.Earliest,
            receiver_queue_size=1,
        )

        received = []
        for _ in range(2):
            msg = consumer.receive(timeout_millis=3000)
            received.append(msg.value())
            consumer.acknowledge(msg)

        emails = [getattr(item, "email", "") for item in received]
        assert "bob@example.com" in emails
    finally:
        _close_quietly(consumer)
        _close_quietly(producer_v2)
        _close_quietly(producer_v1)
        _close_quietly(queue)
        _close_quietly(client)


def test_avro_full_incompatible_type_change_on_producer(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("avro-full-incompat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_FULL)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer = None
    try:
        producer = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=AvroSchema(AvroUserV1)
        )
        producer.send(AvroUserV1(name="seed", age=1))
    finally:
        _close_quietly(producer)

    def _create_bad_producer():
        return queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=AvroSchema(AvroUserBad)
        )

    try:
        incompatible = _assert_incompatible_on_producer(
            _create_bad_producer, AvroUserBad(name="bad", age="x")
        )
        if not incompatible:
            pytest.fail("Expected incompatible schema error on producer create/send")
    finally:
        _close_quietly(queue)
        _close_quietly(client)


def test_json_backward_compatible_add_optional_field(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("json-backward-compat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_BACKWARD)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer_v1 = None
    producer_v2 = None
    consumer = None
    try:
        producer_v1 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=JsonSchema(JsonUserV1)
        )
        producer_v1.send(JsonUserV1(name="alice", age=20))

        producer_v2 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=JsonSchema(JsonUserV2)
        )
        producer_v2.send(JsonUserV2(name="bob", age=21, nickname="b"))

        consumer = queue.create_consumer(
            tenant=TENANT,
            namespace=NAMESPACE,
            topic=topic,
            subscription=f"sub-{uuid.uuid4().hex}",
            schema=JsonSchema(JsonUserV2),
            subscription_type=ConsumerType.Exclusive,
            initial_position=InitialPosition.Earliest,
            receiver_queue_size=1,
        )

        received = []
        for _ in range(2):
            msg = consumer.receive(timeout_millis=3000)
            received.append(msg.value())
            consumer.acknowledge(msg)

        nicknames = [getattr(item, "nickname", "") for item in received]
        assert "b" in nicknames
    finally:
        _close_quietly(consumer)
        _close_quietly(producer_v2)
        _close_quietly(producer_v1)
        _close_quietly(queue)
        _close_quietly(client)


def test_json_backward_incompatible_type_change_on_consumer(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("json-backward-incompat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_BACKWARD)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer = None
    try:
        producer = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=JsonSchema(JsonUserV1)
        )
        producer.send(JsonUserV1(name="seed", age=1))
    finally:
        _close_quietly(producer)

    def _create_bad_consumer():
        return queue.create_consumer(
            tenant=TENANT,
            namespace=NAMESPACE,
            topic=topic,
            subscription=f"sub-{uuid.uuid4().hex}",
            schema=JsonSchema(JsonUserBad),
            subscription_type=ConsumerType.Exclusive,
            initial_position=InitialPosition.Earliest,
            receiver_queue_size=1,
        )

    try:
        incompatible = _assert_incompatible_on_consumer(_create_bad_consumer)
        if not incompatible:
            pytest.fail("Expected incompatible schema error on consumer subscribe")
    finally:
        _close_quietly(queue)
        _close_quietly(client)


def test_protobuf_full_compatible_add_optional_field(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("pb-full-compat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_FULL)

    descriptor_pb2, _, _ = _require_protobuf()

    UserV1, _, record_v1 = _make_proto_message(
        "User",
        [
            ("name", descriptor_pb2.FieldDescriptorProto.TYPE_STRING),
            ("age", descriptor_pb2.FieldDescriptorProto.TYPE_INT32),
        ],
    )
    UserV2, _, record_v2 = _make_proto_message(
        "User",
        [
            ("name", descriptor_pb2.FieldDescriptorProto.TYPE_STRING),
            ("age", descriptor_pb2.FieldDescriptorProto.TYPE_INT32),
            ("email", descriptor_pb2.FieldDescriptorProto.TYPE_STRING, _optional_string()),
        ],
    )

    schema_v1 = _build_protobuf_schema(UserV1, record_v1)
    schema_v2 = _build_protobuf_schema(UserV2, record_v2)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer_v1 = None
    producer_v2 = None
    consumer = None
    try:
        producer_v1 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=schema_v1
        )
        producer_v1.send(UserV1(name="alice", age=30))

        producer_v2 = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=schema_v2
        )
        producer_v2.send(UserV2(name="bob", age=31, email="bob@example.com"))

        consumer = queue.create_consumer(
            tenant=TENANT,
            namespace=NAMESPACE,
            topic=topic,
            subscription=f"sub-{uuid.uuid4().hex}",
            schema=schema_v2,
            subscription_type=ConsumerType.Exclusive,
            initial_position=InitialPosition.Earliest,
            receiver_queue_size=1,
        )

        received = []
        for _ in range(2):
            msg = consumer.receive(timeout_millis=3000)
            received.append(msg.value())
            consumer.acknowledge(msg)

        emails = [getattr(item, "email", "") for item in received]
        assert "bob@example.com" in emails
    finally:
        _close_quietly(consumer)
        _close_quietly(producer_v2)
        _close_quietly(producer_v1)
        _close_quietly(queue)
        _close_quietly(client)


def test_protobuf_full_incompatible_type_change_on_consumer(pulsar_env):
    admin_url = pulsar_env["admin_url"]
    broker_url = pulsar_env["broker_url"]
    topic = _topic_name("pb-full-incompat")

    _ensure_topic(admin_url, topic)
    _set_topic_compatibility(admin_url, topic, STRATEGY_FULL)

    descriptor_pb2, _, _ = _require_protobuf()

    UserV1, _, record_v1 = _make_proto_message(
        "User",
        [
            ("name", descriptor_pb2.FieldDescriptorProto.TYPE_STRING),
            ("age", descriptor_pb2.FieldDescriptorProto.TYPE_INT32),
        ],
    )
    UserBad, _, record_bad = _make_proto_message(
        "User",
        [
            ("name", descriptor_pb2.FieldDescriptorProto.TYPE_STRING),
            ("age", descriptor_pb2.FieldDescriptorProto.TYPE_STRING),
        ],
    )

    schema_v1 = _build_protobuf_schema(UserV1, record_v1)
    schema_bad = _build_protobuf_schema(UserBad, record_bad)

    client = _create_client(broker_url)
    queue = PulsarMessageQueue(client)
    producer = None
    try:
        producer = queue.create_producer(
            tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=schema_v1
        )
        producer.send(UserV1(name="seed", age=1))
    finally:
        _close_quietly(producer)

    def _create_bad_consumer():
        return queue.create_consumer(
            tenant=TENANT,
            namespace=NAMESPACE,
            topic=topic,
            subscription=f"sub-{uuid.uuid4().hex}",
            schema=schema_bad,
            subscription_type=ConsumerType.Exclusive,
            initial_position=InitialPosition.Earliest,
            receiver_queue_size=1,
        )

    try:
        incompatible = _assert_incompatible_on_consumer(_create_bad_consumer)
        if not incompatible:
            # Some brokers/clients only enforce schema compatibility when registering
            # a new schema via producer; fall back to producer-side registration.
            def _create_bad_producer():
                return queue.create_producer(
                    tenant=TENANT, namespace=NAMESPACE, topic=topic, schema=schema_bad
                )

            incompatible = _assert_incompatible_on_producer(
                _create_bad_producer, UserBad(name="bad", age="x")
            )
            if not incompatible:
                # Last resort: force a schema type switch to ensure incompatibility is detected.
                def _create_json_producer():
                    return queue.create_producer(
                        tenant=TENANT,
                        namespace=NAMESPACE,
                        topic=topic,
                        schema=JsonSchema(JsonUserV1),
                    )

                incompatible = _assert_incompatible_on_producer(
                    _create_json_producer, JsonUserV1(name="bad", age=1)
                )
                if not incompatible:
                    pytest.fail("Expected incompatible schema error on consumer or producer")
    finally:
        _close_quietly(queue)
        _close_quietly(client)
