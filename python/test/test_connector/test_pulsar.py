import os
import pytest
import requests
import pulsar
from mental1104.connector.pulsar import PulsarConnector, PulsarEnvironment, Consumer, Producer


@pytest.fixture(autouse=True)
def remove_env_vars():
    # 在测试之前删除环境变量
    if 'HTTP_PROXY' in os.environ:
        del os.environ['HTTP_PROXY']
    
    if 'HTTPS_PROXY' in os.environ:
        del os.environ['HTTPS_PROXY']
    yield

        

@pytest.mark.skipif(not all(env in os.environ for env in [
    PulsarEnvironment.PULSAR_HOST.value,
    PulsarEnvironment.PULSAR_BROKER_PORT.value,
    PulsarEnvironment.PULSAR_ADMIN_PORT.value
]), reason="Environment variables for Pulsar are not set.")
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
        tenant, namespace, topic = tenant_namespace_topic
        client = PulsarConnector.make_client()
        producer = client.create_producer(topic)
        consumer = client.subscribe(topic, subscription_name="test-sub", consumer_type=pulsar.ConsumerType.Shared)

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
        assert broker_url.startswith("pulsar://")

    def test_get_admin_url(self, admin_url):
        assert admin_url.startswith("http://")
        

from pulsar import ConsumerType

# 动态生成请求头
def get_headers():
    token = os.getenv("PULSAR_TOKEN")
    if token:
        return {"Authorization": f"Bearer {token}"}
    return {}


@pytest.mark.skipif(
    not os.getenv("PULSAR_HOST") or not os.getenv("PULSAR_BROKER_PORT"),
    reason="Pulsar environment variables are not set"
)
class TestPulsarIntegration:
    tenant = "test-tenant"
    namespace = "test-namespace"
    topic = "test-topic"

    @classmethod
    def setup_class(cls):
        """Setup Pulsar tenant and namespace before tests."""
        tenant_url = f"{PulsarConnector.get_admin_url()}/admin/v2/tenants/{cls.tenant}"
        namespace_url = f"{PulsarConnector.get_admin_url()}/admin/v2/namespaces/{cls.tenant}/{cls.namespace}"

        # Create tenant
        requests.put(
            tenant_url,
            json={"allowedClusters": ["standalone"]},
            headers=get_headers()
        )
        assert requests.get(tenant_url, headers=get_headers()).status_code == 200

        # Create namespace
        requests.put(
            namespace_url,
            json={},
            headers=get_headers()
        )
        assert requests.get(namespace_url, headers=get_headers()).status_code == 200

    @classmethod
    def teardown_class(cls):
        """Clean up Pulsar tenant and namespace after tests."""
        namespace_url = f"{PulsarConnector.get_admin_url()}/admin/v2/namespaces/{cls.tenant}/{cls.namespace}"
        tenant_url = f"{PulsarConnector.get_admin_url()}/admin/v2/tenants/{cls.tenant}"

        # Delete namespace
        requests.delete(namespace_url, headers=get_headers())
        assert requests.get(namespace_url, headers=get_headers()).status_code == 404

        # Delete tenant
        requests.delete(tenant_url, headers=get_headers())
        assert requests.get(tenant_url, headers=get_headers()).status_code == 404

    def test_producer_creation(self):
        """Test Producer creation and message sending."""
        schema = {"type": "record", "name": "TestRecord", "fields": [{"name": "field1", "type": "string"}]}
        producer = Producer(
            tenant=self.tenant,
            namespace=self.namespace,
            topic=self.topic,
            schema=schema
        )

        assert producer is not None
        message = {"field1": "test message"}
        producer.send(message)

        # Cleanup producer
        producer.close()

    def test_consumer_creation(self):
        """Test Consumer creation, message receiving, and acknowledgment."""
        schema = {"type": "record", "name": "TestRecord", "fields": [{"name": "field1", "type": "string"}]}
        producer = Producer(
            tenant=self.tenant,
            namespace=self.namespace,
            topic=self.topic,
            schema=schema
        )
        consumer = Consumer(
            tenant=self.tenant,
            namespace=self.namespace,
            topic=self.topic,
            subscription="test-subscription",
            schema=schema,
            subscription_type=ConsumerType.Shared
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
        """Test topic creation and deletion lifecycle."""
        topic_url = f"{PulsarConnector.get_admin_url()}/admin/v2/persistent/{self.tenant}/{self.namespace}/{self.topic}"

        # Create topic
        requests.put(topic_url, headers=get_headers())
        assert requests.get(f"{topic_url}/stats", headers=get_headers()).status_code == 200

        # Delete topic
        requests.delete(topic_url, headers=get_headers())
        assert requests.get(f"{topic_url}/stats", headers=get_headers()).status_code == 404