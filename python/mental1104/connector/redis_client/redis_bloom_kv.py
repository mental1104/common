#!/usr/bin/env python3

import contextlib

import redis

# Redis 客户端请复用同目录下 __init__.py 中的 RedisConnection 上下文管理器。


# -------------------------------
# Redis 布隆过滤器封装
# -------------------------------


class RedisBloom:
    """
    负责：
      - 检测 Redis 是否支持 Bloom 模块（bf）
      - 初始化一个布隆过滤器 key（BF.RESERVE）
      - 提供 add / exists 接口

    注意：
      - 如果服务器没有 Bloom 模块, enabled=False,
        exists() 退化为“总是返回 True”（不影响正确性, 只影响性能）。
    """

    def __init__(
        self,
        client: redis.Redis,
        filter_key: str = "bf:kv",
        error_rate: float = 0.01,
        capacity: int = 1_000_000,
    ) -> None:
        self.client = client
        self.filter_key = filter_key
        self.error_rate = error_rate
        self.capacity = capacity
        self.enabled = self._check_and_init_bloom()

    def _check_and_init_bloom(self) -> bool:
        # 1. 检查模块列表中是否有 bf
        try:
            modules = self.client.execute_command("MODULE", "LIST")
        except redis.RedisError:
            # 连接失败/权限问题等, 直接认为不可用
            return False

        has_bf = False
        for m in modules:
            # m 形如 [ "name", "bf", "ver", 20816, "path", "..." ]
            # m[1] 理论上是模块名
            if len(m) >= 2 and str(m[1]).lower() == "bf":
                has_bf = True
                break

        if not has_bf:
            return False

        # 2. 初始化布隆过滤器 key（BF.RESERVE）
        try:
            # 如果 key 已存在, 这里会抛 ResponseError, 可以忽略
            self.client.execute_command(
                "BF.RESERVE",
                self.filter_key,
                self.error_rate,
                self.capacity,
            )
        except redis.ResponseError as e:
            # 常见情况：key 已存在, 直接忽略
            msg = str(e).lower()
            if "item exists" in msg or "exists" in msg:
                pass
            else:
                # 其他异常：保守起见直接关闭 Bloom
                return False
        except redis.RedisError:
            return False

        return True

    def add(self, item: str) -> None:
        if not self.enabled:
            return
        # 失败不会影响主逻辑, 这里简单忽略异常即可
        with contextlib.suppress(redis.RedisError):
            self.client.execute_command("BF.ADD", self.filter_key, item)

    def exists(self, item: str) -> bool:
        """
        返回：
          - True：可能存在, 需要访问 Redis GET 验证
          - False：肯定不存在, 可以直接认为 miss

        当 Bloom 不可用时, 返回 True, 等价于“不做任何优化”。
        """
        if not self.enabled:
            return True

        try:
            res = self.client.execute_command("BF.EXISTS", self.filter_key, item)
            return bool(res)
        except redis.RedisError:
            # 出问题时直接退化为“总是 True”, 避免误删有效 key
            return True
