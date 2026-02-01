from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Callable, Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from .pulsar import Consumer, Producer


class AbstractMessageQueue(ABC):
    @abstractmethod
    def create_producer(
        self,
        tenant: str,
        namespace: str,
        topic: str,
        schema: dict,
        batching_enabled: bool = True,
    ) -> "Producer":
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
    ) -> "Consumer":
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        raise NotImplementedError
