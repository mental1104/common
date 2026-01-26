import importlib
import sys
from importlib import util

__path__ = []
if __spec__ is not None:
    __spec__.submodule_search_locations = __path__

_ALIASES = {
    __name__ + ".pulsar": "mental1104.mq.pulsar",
    __name__ + ".redis_client": "mental1104.db.redis",
    __name__ + ".redis_client.redis_bloom_kv": "mental1104.db.redis.redis_bloom_kv",
}

_OPTIONAL_DEPS = {
    __name__ + ".pulsar": "pulsar",
}

for alias, target in _ALIASES.items():
    if alias not in sys.modules:
        optional_dep = _OPTIONAL_DEPS.get(alias)
        if optional_dep and util.find_spec(optional_dep) is None:
            continue
        sys.modules[alias] = importlib.import_module(target)
