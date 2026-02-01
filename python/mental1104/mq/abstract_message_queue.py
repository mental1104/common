from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Callable, Optional

try:
    from pulsar import ConsumerType
except Exception:
    class ConsumerType:
        Shared = "Shared"

class AbstractProducer(ABC):
    @abstractmethod
    def send(self, record: Any) -> None:
        raise NotImplementedError

    @abstractmethod
    def send_async(self, record: Any, callback: Optional[Callable[..., Any]] = None) -> None:
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        raise NotImplementedError

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()


class AbstractConsumer(ABC):
    @abstractmethod
    def receive(self, timeout_millis: Optional[int] = None) -> Any:
        raise NotImplementedError

    @abstractmethod
    def acknowledge(self, record: Any) -> None:
        raise NotImplementedError

    @abstractmethod
    def negative_acknowledge(self, record: Any) -> None:
        raise NotImplementedError

    @abstractmethod
    def unsubscribe(self) -> None:
        raise NotImplementedError

    @abstractmethod
    def resubscribe(self) -> None:
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        raise NotImplementedError

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()


class AbstractMessageQueue(ABC):
    @abstractmethod
    def create_producer(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        schema: dict,
        batching_enabled: bool = True,
    ) -> "AbstractProducer":
        raise NotImplementedError

    @abstractmethod
    def create_consumer(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        subscription: str,
        schema: dict,
        subscription_type: Any = None,
        message_listener: Optional[Callable[..., Any]] = None,
        **kwargs: Any,
    ) -> "AbstractConsumer":
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        raise NotImplementedError
