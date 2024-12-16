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
            PulsarEnvironment.PULSAR_BROKER_PORT.value,
            PulsarEnvironment.PULSAR_BROKER_TOKEN.value
        ])

        return pulsar.Client('{host}:{port}'.format(
                host=os.environ[PulsarEnvironment.PULSAR_BROKER_HOST.value],
                port=os.environ[PulsarEnvironment.PULSAR_BROKER_PORT.value]
            ),
            authentication=pulsar.AuthenticationToken(os.environ[PulsarEnvironment.PULSAR_BROKER_TOKEN.value])
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
        pass

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
        pass