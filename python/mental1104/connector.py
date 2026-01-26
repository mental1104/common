import importlib
import sys

__path__ = []
if __spec__ is not None:
    __spec__.submodule_search_locations = __path__

_ALIASES = {
    __name__ + ".pulsar": "mental1104.mq.pulsar",
    __name__ + ".redis_client": "mental1104.redis_client",
    __name__ + ".redis_client.redis_bloom_kv": "mental1104.redis_client.redis_bloom_kv",
}

for alias, target in _ALIASES.items():
    if alias not in sys.modules:
        sys.modules[alias] = importlib.import_module(target)
