from __future__ import annotations

import os
from importlib import import_module

import pytest
import requests

pulsar = pytest.importorskip(
    "pulsar", reason="pulsar-client is not available on this platform"
)
ConsumerType = pulsar.ConsumerType

_pulsar_mod = import_module("mental1104.connector.pulsar")
Consumer = _pulsar_mod.Consumer
Producer = _pulsar_mod.Producer
PulsarAdminHelper = _pulsar_mod.PulsarAdminHelper
PulsarConnector = _pulsar_mod.PulsarConnector
PulsarEnvironment = _pulsar_mod.PulsarEnvironment


def _pulsar_admin_reachable() -> tuple[bool, str]:
    host = os.getenv(PulsarEnvironment.PULSAR_HOST.value)
    broker_port = os.getenv(PulsarEnvironment.PULSAR_BROKER_PORT.value)
    admin_port = os.getenv(PulsarEnvironment.PULSAR_ADMIN_PORT.value)
    if not host or not broker_port or not admin_port:
        return False, "Pulsar env vars not set"
    admin_url = f"http://{host}:{admin_port}"
    try:
        resp = requests.get(f"{admin_url}/admin/v2/brokers/health", timeout=2)
    except Exception as exc:
        return False, f"Pulsar admin not reachable: {exc}"
    if resp.status_code != 200:
        return False, f"Pulsar admin unhealthy: status={resp.status_code}"
    return True, ""


@pytest.fixture(scope="session", autouse=True)
def require_pulsar_admin():
    ok, reason = _pulsar_admin_reachable()
    if not ok:
        pytest.skip(reason)


pytestmark = pytest.mark.usefixtures("require_pulsar_admin")


@pytest.fixture(autouse=True)
def remove_env_vars():
    # 在测试之前删除环境变量
    if "HTTP_PROXY" in os.environ:
        del os.environ["HTTP_PROXY"]

    if "HTTPS_PROXY" in os.environ:
        del os.environ["HTTPS_PROXY"]
    yield


@pytest.mark.skipif(
    not all(
        env in os.environ
        for env in [
            PulsarEnvironment.PULSAR_HOST.value,
            PulsarEnvironment.PULSAR_BROKER_PORT.value,
            PulsarEnvironment.PULSAR_ADMIN_PORT.value,
        ]
    ),
    reason="Environment variables for Pulsar are not set.",
)
class TestPulsarConnector:
    @pytest.fixture(scope="class")
    def admin_url(self):
        return PulsarConnector.get_admin_url()

    @pytest.fixture(scope="class")
    def broker_url(self):
        return PulsarConnector.get_broker_url()

    @pytest.fixture(scope="function")
    def tenant_namespace_topic(self, admin_url):
        tenant = "test-tenant"
        namespace = f"{tenant}/test-namespace"
        topic = f"persistent://{namespace}/test-topic"

        # Create tenant
        tenant_url = f"{admin_url}/admin/v2/tenants/{tenant}"
        requests.put(tenant_url, json={"allowedClusters": ["standalone"]})
        assert requests.get(tenant_url).status_code in (200, 409)

        # Create namespace
        namespace_url = f"{admin_url}/admin/v2/namespaces/{namespace}"
        requests.put(namespace_url)
        assert requests.get(namespace_url).status_code == 200

        # Create topic
        topic_url = f"{admin_url}/admin/v2/persistent/{namespace}/test-topic"
        requests.put(topic_url)
        assert requests.get(f"{topic_url}/stats").status_code == 200

        yield tenant, namespace, topic

        # Cleanup topic
        requests.delete(topic_url)
        assert requests.get(f"{topic_url}/stats").status_code == 404

        # Cleanup namespace
        requests.delete(namespace_url)
        assert requests.get(namespace_url).status_code == 404

        # Cleanup tenant
        requests.delete(tenant_url)
        assert requests.get(tenant_url).status_code == 404

    def test_make_client(self, tenant_namespace_topic):
        """
        【场景背景】验证 PulsarConnector.make_client() 创建的 client 能完成
        生产与消费的闭环。
        【步骤输入】在 fixture 创建的租户/命名空间/主题下, 创建 producer 与
        consumer, 发送一条字节消息再同步消费。
        【期望输出】consumer 收到的 payload 等于原始内容并成功 acknowledge,
        说明客户端配置、身份与 topic 元数据均正确。
        """
        _, _, topic = tenant_namespace_topic
        client = PulsarConnector.make_client()
        producer = client.create_producer(topic)
        consumer = client.subscribe(
            topic, subscription_name="test-sub", consumer_type=pulsar.ConsumerType.Shared
        )

        # Test produce and consume
        producer.send(b"Test message")
        msg = consumer.receive(timeout_millis=5000)
        assert msg.data() == b"Test message"

        # Acknowledge the message
        consumer.acknowledge(msg)

        # Cleanup
        producer.close()
        consumer.close()
        client.close()

    def test_get_broker_url(self, broker_url):
        """
        【场景背景】broker URL 应符合 Pulsar 客户端地址规范。
        【步骤输入】调用 fixture 提供的 broker_url。
        【期望输出】字符串以 pulsar:// 开头, 保证后续客户端能识别协议。
        """
        assert broker_url.startswith("pulsar://")

    def test_get_admin_url(self, admin_url):
        """
        【场景背景】Pulsar 管理接口需通过 HTTP 访问。
        【步骤输入】读取 admin_url fixture。
        【期望输出】URL 以 http:// 开头, 确保管理 API 可以访问。
        """
        assert admin_url.startswith("http://")


# 动态生成请求头


def get_headers():
    token = os.getenv("PULSAR_TOKEN")
    if token:
        return {"Authorization": f"Bearer {token}"}
    return {}


@pytest.mark.skipif(
    not os.getenv("PULSAR_HOST") or not os.getenv("PULSAR_BROKER_PORT"),
    reason="Pulsar environment variables are not set",
)
class TestPulsarIntegration:
    tenant = "test-tenant"
    namespace = "test-namespace"
    topic = "test-topic"

    @classmethod
    def setup_class(cls):
        """Setup Pulsar tenant and namespace before tests."""
        tenant_url = f"{PulsarConnector.get_admin_url()}/admin/v2/tenants/{cls.tenant}"
        namespace_url = (
            f"{PulsarConnector.get_admin_url()}/admin/v2/namespaces/{cls.tenant}/{cls.namespace}"
        )

        # Create tenant
        requests.put(tenant_url, json={"allowedClusters": ["standalone"]}, headers=get_headers())
        assert requests.get(tenant_url, headers=get_headers()).status_code == 200

        # Create namespace
        requests.put(namespace_url, json={}, headers=get_headers())
        assert requests.get(namespace_url, headers=get_headers()).status_code == 200

    @classmethod
    def teardown_class(cls):
        """Clean up Pulsar tenant and namespace after tests."""
        namespace_url = (
            f"{PulsarConnector.get_admin_url()}/admin/v2/namespaces/{cls.tenant}/{cls.namespace}"
        )
        tenant_url = f"{PulsarConnector.get_admin_url()}/admin/v2/tenants/{cls.tenant}"

        # Delete namespace
        requests.delete(namespace_url, headers=get_headers())
        assert requests.get(namespace_url, headers=get_headers()).status_code == 404

        # Delete tenant
        requests.delete(tenant_url, headers=get_headers())
        assert requests.get(tenant_url, headers=get_headers()).status_code == 404

    def test_producer_creation(self):
        """
        【场景背景】高层封装的 Producer 应根据 tenant/namespace/topic 自动关联
        schema 并能发送消息。
        【步骤输入】构建 Producer 实例, 向测试 topic 发送一条字段为 field1 的消息。
        【期望输出】Producer 对象正常构造且 send 不抛异常, 说明连接和 schema 合法。
        """
        schema = {
            "type": "record",
            "name": "TestRecord",
            "fields": [{"name": "field1", "type": "string"}],
        }
        producer = Producer(
            tenant=self.tenant, namespace=self.namespace, topic=self.topic, schema=schema
        )

        assert producer is not None
        message = {"field1": "test message"}
        producer.send(message)

        # Cleanup producer
        producer.close()

    def test_consumer_creation(self):
        """
        【场景背景】Consumer 封装需支持订阅、拉取和确认消息。
        【步骤输入】创建 Producer & Consumer, 先发消息再调用 consumer.receive,
        最后执行 acknowledge。
        【期望输出】能收到非空 record 并成功 ack, 表示 schema/订阅配置都正确。
        """
        schema = {
            "type": "record",
            "name": "TestRecord",
            "fields": [{"name": "field1", "type": "string"}],
        }
        producer = Producer(
            tenant=self.tenant, namespace=self.namespace, topic=self.topic, schema=schema
        )
        consumer = Consumer(
            tenant=self.tenant,
            namespace=self.namespace,
            topic=self.topic,
            subscription="test-subscription",
            schema=schema,
            subscription_type=ConsumerType.Shared,
        )

        # Send a message
        message = {"field1": "test message"}
        producer.send(message)

        # Receive and acknowledge the message
        record = consumer.receive(timeout_millis=5000)
        assert record is not None
        consumer.acknowledge(record)

        # Cleanup producer and consumer
        consumer.close()
        producer.close()

    def test_topic_lifecycle(self):
        """
        【场景背景】管理 API 应允许创建/删除持久化 topic, 并能在 stats 接口中反映。
        【步骤输入】使用 REST Admin API 创建 topic, 再查询 stats; 随后删除并再次查询。
        【期望输出】创建后 stats=200, 删除后 stats=404, 以证明生命周期操作可用。
        """
        topic_url = f"{PulsarConnector.get_admin_url()}/admin/v2/persistent/{self.tenant}/{self.namespace}/{self.topic}"

        # Create topic
        requests.put(topic_url, headers=get_headers())
        assert requests.get(f"{topic_url}/stats", headers=get_headers()).status_code == 200

        # Delete topic
        requests.delete(topic_url, headers=get_headers())
        assert requests.get(f"{topic_url}/stats", headers=get_headers()).status_code == 404


@pytest.mark.skipif(
    not all(
        env in os.environ
        for env in [PulsarEnvironment.PULSAR_HOST.value, PulsarEnvironment.PULSAR_ADMIN_PORT.value]
    ),
    reason="Environment variables for Pulsar are not set.",
)
class TestPulsarAdminHelper:
    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """
        测试前的初始化和测试后的清理。
        确保租户、命名空间、主题创建和删除。
        """
        self.tenant = "test-tenant"
        self.namespace = "test-namespace"
        self.topic = "test-topic"

        # 测试前确保干净状态
        PulsarAdminHelper.cleanup_tenant(self.tenant)
        yield
        # 测试后清理租户
        PulsarAdminHelper.cleanup_tenant(self.tenant)

    def test_create_tenant(self):
        """
        【场景背景】PulsarAdminHelper.create_tenant 应负责租户初始化。
        【步骤输入】直接调用 create_tenant(self.tenant)。
        【期望输出】is_tenant_exists 返回 True, 说明租户元数据被创建。
        """
        PulsarAdminHelper.create_tenant(self.tenant)
        assert PulsarAdminHelper.is_tenant_exists(self.tenant), "Tenant creation failed."

    def test_create_namespace(self):
        """
        【场景背景】命名空间依赖已存在的租户, 应在 helper 中生成。
        【步骤输入】先建租户再 create_namespace(tenant/namespace)。
        【期望输出】is_namespace_exists 返回 True, 证明命名空间生效。
        """
        PulsarAdminHelper.create_tenant(self.tenant)
        PulsarAdminHelper.create_namespace(f"{self.tenant}/{self.namespace}")
        assert PulsarAdminHelper.is_namespace_exists(self.tenant, self.namespace), (
            "Namespace creation failed."
        )

    def test_create_topic(self):
        """
        【场景背景】Topic 依赖租户与命名空间, helper 应自动创建底层资源。
        【步骤输入】按顺序 create_tenant -> create_namespace -> create_topic。
        【期望输出】is_topic_exists 为 True, 说明 topic 元信息注册成功。
        """
        PulsarAdminHelper.create_tenant(self.tenant)
        PulsarAdminHelper.create_namespace(f"{self.tenant}/{self.namespace}")
        PulsarAdminHelper.create_topic(f"{self.tenant}/{self.namespace}/{self.topic}")
        assert PulsarAdminHelper.is_topic_exists(self.tenant, self.namespace, self.topic), (
            "Topic creation failed."
        )

    def test_cleanup_tenant(self):
        """
        【场景背景】cleanup_tenant 应递归删除租户下的命名空间与 topic。
        【步骤输入】先创建一套租户/命名空间/主题, 再调用 cleanup_tenant。
        【期望输出】清理前存在、清理后 is_tenant_exists 变 False, 验证深度删除。
        """
        PulsarAdminHelper.create_tenant(self.tenant)
        PulsarAdminHelper.create_namespace(f"{self.tenant}/{self.namespace}")
        PulsarAdminHelper.create_topic(f"{self.tenant}/{self.namespace}/{self.topic}")

        # 确保租户、命名空间和主题创建成功
        assert PulsarAdminHelper.is_tenant_exists(self.tenant), (
            "Tenant does not exist before cleanup."
        )
        assert PulsarAdminHelper.is_namespace_exists(self.tenant, self.namespace), (
            "Namespace does not exist before cleanup."
        )
        assert PulsarAdminHelper.is_topic_exists(self.tenant, self.namespace, self.topic), (
            "Topic does not exist before cleanup."
        )

        # 清理租户
        PulsarAdminHelper.cleanup_tenant(self.tenant)
        assert not PulsarAdminHelper.is_tenant_exists(self.tenant), "Tenant cleanup failed."

    def test_ensure_tenant_namespace_topic(self):
        """
        【场景背景】ensure_tenant_namespace_topic 应幂等地确保三种资源存在。
        【步骤输入】调用 ensure..., 然后依次检查租户/命名空间/主题存在性。
        【期望输出】三项检查均为 True, 证明 helper 能自动补齐缺失资源。
        """
        try:
            # 调用 ensure_tenant_namespace_topic 方法
            PulsarAdminHelper.ensure_tenant_namespace_topic(self.tenant, self.namespace, self.topic)

            # 验证租户、命名空间和主题是否被正确创建
            assert PulsarAdminHelper.is_tenant_exists(self.tenant), (
                "Tenant does not exist after ensure operation."
            )
            assert PulsarAdminHelper.is_namespace_exists(self.tenant, self.namespace), (
                "Namespace does not exist after ensure operation."
            )
            assert PulsarAdminHelper.is_topic_exists(self.tenant, self.namespace, self.topic), (
                "Topic does not exist after ensure operation."
            )
        finally:
            # 清理确保测试后的环境干净
            PulsarAdminHelper.cleanup_tenant(self.tenant)
