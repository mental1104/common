import os
import pulsar
import logging
import requests
from pulsar import ConsumerType
from pulsar.schema import AvroSchema, BytesSchema
from enum import Enum
from mental1104.util import Environment
import functools


class PulsarEnvironment(str, Enum):
    PULSAR_HOST = "PULSAR_HOST"
    PULSAR_BROKER_PORT = "PULSAR_BROKER_PORT"
    PULSAR_TOKEN = "PULSAR_TOKEN"
    PULSAR_ADMIN_PORT = "PULSAR_ADMIN_PORT"


class PulsarConnector:
    @staticmethod
    def make_client():
        Environment.check_required_env_vars([
            PulsarEnvironment.PULSAR_HOST.value,
            PulsarEnvironment.PULSAR_BROKER_PORT.value
        ])

        return pulsar.Client(
            PulsarConnector.get_broker_url(),
            authentication=pulsar.AuthenticationToken(
                os.environ[PulsarEnvironment.PULSAR_TOKEN.value]) if PulsarEnvironment.PULSAR_TOKEN.value in os.environ else None
        )

    @staticmethod
    def get_broker_url():
        Environment.check_required_env_vars([
            PulsarEnvironment.PULSAR_HOST.value,
            PulsarEnvironment.PULSAR_BROKER_PORT.value
        ])
        return f"pulsar://{os.environ[PulsarEnvironment.PULSAR_HOST.value]}:{os.environ[PulsarEnvironment.PULSAR_BROKER_PORT.value]}"

    @staticmethod
    def get_admin_url():
        Environment.check_required_env_vars([
            PulsarEnvironment.PULSAR_HOST.value,
            PulsarEnvironment.PULSAR_ADMIN_PORT.value
        ])
        return f"http://{os.environ[PulsarEnvironment.PULSAR_HOST.value]}:{os.environ[PulsarEnvironment.PULSAR_ADMIN_PORT.value]}"


class Consumer:
    def __init__(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        subscription: str,
        schema: dict,
        client=None,
        subscription_type=ConsumerType.Shared,
        message_listener=None,
        **kwargs
    ):
        """
        :kwargs 用于传入pulsar consumer的各种可选的参数配置，如
        negative_ack_redelivery_delay_ms 否认重传时间间隔
        receiver_queue_size 消费者消息队列大小，默认值为1000
        unacked_messages_timeout_ms 消息超时否认时间，默认设置为240s，单位ms
        更多的可选参数可参考pulsar的api文档 
        """
        self.__is_close = True
        if not client:
            self.__client = PulsarConnector.make_client()
            client = self.__client
            self.__is_close = False

        if False:
            real_schema = BytesSchema()
        else:
            real_schema = AvroSchema(None, schema)

        # 指定租户、命名空间
        self.__subscrifunc = functools.partial(client.subscribe,
                                               'persistent://{tenant}/{namespace}/{topic}'.format(
                                                   tenant=tenant,
                                                   namespace=namespace,
                                                   topic=topic
                                               ),
                                               subscription_name=subscription,
                                               consumer_type=subscription_type,
                                               message_listener=message_listener,
                                               batch_index_ack_enabled=True,
                                               schema=real_schema,
                                               **kwargs)

        self.__consumer = self.__subscrifunc()

    def __del__(self):
        if self.__consumer:
            self.__consumer.close()
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__consumer.close()
        self.close()

    def receive(self, timeout_millis: int = None):
        # 如果指定了时间，那么在指定时间内，没有接收到消息回抛错，单位为毫秒
        record = self.__consumer.receive(timeout_millis=timeout_millis)

        return record

    # 消费者需要确认消息处理成功，以便Pulsar broker删除消息。
    def acknowledge(self, record):
        self.__consumer.acknowledge(record)

    # 共享消息的情况下，如果不是自己消费的消息，将消息跳过，pulsar继续发给其他订阅者
    def negative_acknowledge(self, record):
        self.__consumer.negative_acknowledge(record)

    def close(self):
        if self.__is_close:
            return

        self.__client.close()
        self.__is_close = True

    def unsubscribe(self):
        """
        unsubscribe 删除当前consumer所属订阅的订阅，如果该订阅还存在其他消费者，那么会抛出异常，取消失败
        """
        try:
            if self.__consumer:
                self.__consumer.unsubscribe()
        except Exception as e:
            logging.exception(f"pulsar unsubscribe failed, exception:{e}")

    def resuscribe(self):
        """
        resuscribe 重新以该订阅名订阅topic，首先会删除该订阅，然后再重新订阅，相当于从当前最新的消息开始消费
        """
        self.unsubscribe()
        if self.__consumer:
            self.__consumer.close()
        self.__consumer = self.__subscrifunc()


class Producer:
    def __init__(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        schema: dict,
        client=None,
        batching_enabled=True
    ):
        self.__is_close = False  # 初始状态为未关闭
        if not client:
            self.__client = PulsarConnector.make_client()
            client = self.__client

        if False:
            real_schema = BytesSchema()
        else:
            real_schema = AvroSchema(None, schema)

        # 指定租户、命名空间
        self.__producer = client.create_producer(
            topic='persistent://{tenant}/{namespace}/{topic}'.format(
                tenant=tenant,
                namespace=namespace,
                topic=topic
            ),
            block_if_queue_full=True,
            batching_enabled=batching_enabled,
            schema=real_schema
        )

    def __del__(self):
        self.close()  # 调用关闭方法

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()  # 退出时关闭生产者

    @classmethod
    def __default_callback(cls, message):
        """
        default_callback pulsar 异步发送的默认回调函数，发送错误时打印相关日志，包括
        1. 发送的内容
        2. 发送错误的原因
        3. 发送的消息id
        """
        def callback(result, msg_id_obj):
            if result != pulsar.Result.Ok:
                logging.warning(f"pulsar send msg fail, result:{result}, msg_id: {msg_id_obj}, message:{message}")

        return callback

    def send(self, record):
        if self.__is_close:
            raise RuntimeError("Cannot send message; producer is already closed.")
        self.__producer.send(record)

    @classmethod
    def check_send_status(cls, result, msg_id_obj):
        if result != pulsar.Result.Ok:
            logging.warning(
                "pulsar send msg fail:{}, msg_id: {}"
                .format(result, msg_id_obj)
            )
            return False
        return True

    @classmethod
    def check_send_status_exception(cls, result, msg_id_obj):
        if result == pulsar.Result.Ok:
            return
        resend_error = [pulsar.Result.Timeout, pulsar.Result.NotConnected,
                        pulsar.Result.AlreadyClosed, pulsar.Result.ConnectError]
        if result in resend_error:
            logging.error(f"pulsar send msg fail:{result}, msg_id: {msg_id_obj}")
        raise RuntimeError(f"pulsar send msg fail:{result}, msg_id: {msg_id_obj}")

    def send_async(self, record, callback=None):
        if self.__is_close:
            raise RuntimeError("Cannot send message; producer is already closed.")
        if callback is None:
            callback = Producer.__default_callback(record)
        self.__producer.send_async(record, callback)

    def close(self):
        if self.__is_close:  # 如果已经关闭，直接返回
            return

        if self.__producer:
            self.__producer.close()  # 关闭生产者
        if hasattr(self, '__client') and self.__client:
            self.__client.close()  # 关闭客户端
        self.__is_close = True  # 更新为已关闭状态


class PulsarAdminHelper:

    @staticmethod
    def ensure_tenant_namespace_topic(tenant, namespace=None, topic=None):
        """
        确保租户、命名空间和主题按照顺序创建
        """
        pulsar_admin_url = PulsarConnector.get_admin_url()

        # 检查租户是否存在
        tenant_url = f"{pulsar_admin_url}/admin/v2/tenants/{tenant}"
        tenant_response = requests.get(tenant_url)
        if tenant_response.status_code != 200:
            logging.info(f"Tenant {tenant} does not exist. Creating it.")
            PulsarAdminHelper.create_tenant(tenant)

        # 检查命名空间是否存在
        if not namespace:
            return
        namespace_url = f"{pulsar_admin_url}/admin/v2/namespaces/{tenant}/{namespace}"
        namespace_response = requests.get(namespace_url)
        if namespace_response.status_code != 200:
            logging.info(f"Namespace {tenant}/{namespace} does not exist. Creating it.")
            PulsarAdminHelper.create_namespace(f"{tenant}/{namespace}")

        # 检查主题是否存在
        if not topic:
            return
        topic_url = f"{pulsar_admin_url}/admin/v2/persistent/{tenant}/{namespace}/{topic}"
        topic_response = requests.get(topic_url)
        if topic_response.status_code != 200:
            logging.info(f"Topic {tenant}/{namespace}/{topic} does not exist. Creating it.")
            PulsarAdminHelper.create_topic(f"{tenant}/{namespace}/{topic}")

    @staticmethod
    def create_tenant(tenant):
        """创建租户"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/tenants/{tenant}"
        response = requests.put(url, json={
            "allowedClusters": ["standalone"]
        })
        if response.status_code not in [200, 204, 409]:
            raise RuntimeError(
                f"Failed to create tenant {tenant}: {response.status_code}: {response.status_code} {response.text}")

    @staticmethod
    def create_namespace(namespace):
        """创建命名空间"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/namespaces/{namespace}"
        response = requests.put(url)
        if response.status_code not in [200, 204, 409]:
            raise RuntimeError(f"Failed to create namespace {namespace}: {response.status_code} {response.text}")

    @staticmethod
    def create_topic(topic):
        """创建主题"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/persistent/{topic}"
        response = requests.put(url)
        if response.status_code not in [200, 204, 409]:
            raise RuntimeError(f"Failed to create topic {topic}: {response.status_code} {response.text}")

    @staticmethod
    def get_tenant_namespaces(tenant):
        """获取租户的所有命名空间"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/namespaces/{tenant}"
        response = requests.get(url)
        if response.status_code != 200:
            raise RuntimeError(f"Failed to list namespaces for tenant {tenant}: {response.status_code} {response.text}")
        return response.json()

    @staticmethod
    def get_namespace_topics(namespace):
        """获取命名空间的所有主题"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/persistent/{namespace}"
        response = requests.get(url)
        if response.status_code != 200:
            raise RuntimeError(
                f"Failed to list topics for namespace {namespace}: {response.status_code} {response.text}")
        return response.json()

    @staticmethod
    def delete_topic(topic):
        """删除主题"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        topic_to_delete = topic
        prefix = "persistent://"
        if topic_to_delete.startswith(prefix):
            topic_to_delete = topic_to_delete[len(prefix):]  # 去除前缀
        url = f"{pulsar_admin_url}/admin/v2/persistent/{topic_to_delete}"
        response = requests.delete(url)
        if response.status_code not in [200, 204, 404]:
            raise RuntimeError(f"Failed to delete topic {topic_to_delete}: {response.status_code} {response.text}")

    @staticmethod
    def delete_namespace(namespace):
        """删除命名空间"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/namespaces/{namespace}"
        response = requests.delete(url)
        if response.status_code not in [200, 204, 404]:
            raise RuntimeError(f"Failed to delete namespace {namespace}: {response.status_code} {response.text}")

    @staticmethod
    def delete_tenant(tenant):
        """删除租户"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/tenants/{tenant}"
        response = requests.delete(url)
        if response.status_code not in [200, 204, 404]:
            raise RuntimeError(f"Failed to delete tenant {tenant}: {response.status_code} {response.text}")

    @staticmethod
    def cleanup_tenant(tenant):
        """清理租户，确保删除所有命名空间和主题"""
        try:
            # 获取所有命名空间
            namespaces = PulsarAdminHelper.get_tenant_namespaces(tenant)
            for namespace in namespaces:
                # 获取命名空间下的所有主题并删除
                topics = PulsarAdminHelper.get_namespace_topics(namespace)
                for topic in topics:
                    PulsarAdminHelper.delete_topic(topic)
                # 删除命名空间
                PulsarAdminHelper.delete_namespace(namespace)
            # 删除租户
            PulsarAdminHelper.delete_tenant(tenant)
            print(f"Successfully cleaned up tenant: {tenant}")
        except Exception as e:
            logging.exception(f"Failed to clean up tenant {tenant}: {e}")

    @staticmethod
    def is_tenant_exists(tenant):
        """检查租户是否存在"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/tenants"
        response = requests.get(url)

        if response.status_code != 200:
            raise RuntimeError(f"Failed to fetch tenants: {response.status_code} {response.text}")

        tenants = response.json()
        return tenant in tenants

    @staticmethod
    def is_namespace_exists(tenant, namespace):
        """检查租户下的命名空间是否存在"""
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/namespaces/{tenant}"
        response = requests.get(url)

        if response.status_code != 200:
            raise RuntimeError(
                f"Failed to fetch namespaces for tenant {tenant}: {response.status_code} {response.text}")

        namespaces = response.json()
        return f"{tenant}/{namespace}" in namespaces

    @staticmethod
    def is_topic_exists(tenant, namespace, topic, topic_type="persistent"):
        """
        检查特定租户、命名空间下的主题是否存在
        :param tenant: 租户名
        :param namespace: 命名空间名
        :param topic: 主题名
        :param topic_type: 主题类型（"persistent" 或 "non-persistent"）
        """
        pulsar_admin_url = PulsarConnector.get_admin_url()
        url = f"{pulsar_admin_url}/admin/v2/{topic_type}/{tenant}/{namespace}"
        response = requests.get(url)

        if response.status_code != 200:
            raise RuntimeError(
                f"Failed to fetch topics for namespace {tenant}/{namespace}: {response.status_code} {response.text}")

        topics = response.json()
        return f"{topic_type}://{tenant}/{namespace}/{topic}" in topics
