import os
import pulsar
from pulsar import ConsumerType
from pulsar.schema import AvroSchema, BytesSchema
from enum import Enum
from util import Environment

class PulsarEnvironment(str, Enum):
    PULSAR_BROKER_HOST = "PULSAR_BROKER_HOST"
    PULSAR_BROKER_PORT = "PULSAR_BROKER_PORT"
    PULSAR_BROKER_TOKEN = "PULSAR_BROKER_TOKEN"

class PulsarConnector:
    @staticmethod
    def make_client():
        Environment.check_required_env_vars([
            PulsarEnvironment.PULSAR_BROKER_HOST.value,
            PulsarEnvironment.PULSAR_BROKER_PORT.value
        ])

        return pulsar.Client('{host}:{port}'.format(
                host=os.environ[PulsarEnvironment.PULSAR_BROKER_HOST.value],
                port=os.environ[PulsarEnvironment.PULSAR_BROKER_PORT.value]
            ),
            authentication=pulsar.AuthenticationToken(os.environ[PulsarEnvironment.PULSAR_BROKER_TOKEN.value]) if PulsarEnvironment.PULSAR_BROKER_TOKEN.value in os.environ else None
        )
        

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
            self.__client = make_client()
            client = self.__client
            self.__is_close = False

        if schema_t == 'Json':
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
        self.__is_close = True
        if not client:
            self.__client = make_client()
            client = self.__client
            self.__is_close = False

        if schema_t == 'Json':
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
        if self.__producer:
            self.__producer.close()
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__producer.close()
        self.close()

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
                        pulsar.Result.AlreadyClosed, pulsar.Result.ConnectError,
                        ]
        if result in resend_error:
            raise SendCustomError(f"pulsar send msg fail:{result}, msg_id: {msg_id_obj}")
        raise RuntimeError(f"pulsar send msg fail:{result}, msg_id: {msg_id_obj}")

    def send_async(self, record, callback=None):
        if callback is None:
            callback = Producer.__default_callback(record)
        self.__producer.send_async(record, callback)

    def close(self):
        if self.__is_close:
            return

        self.__client.close()
        self.__is_close = True