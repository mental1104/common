#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import time
import uuid
import redis



# -------------------------------
# Redis 连接上下文管理器
# -------------------------------
class RedisConnection:
    """
    redis连接类
    使用上下文管理器管理 Redis 连接。
    连接参数默认从环境变量读取：
      REDIS_HOST: Redis 主机（默认为 localhost）
      REDIS_PORT: Redis 端口（默认为 6379）
      REDISCLI_AUTH: Redis 密码（默认为 None）
    """
    def __init__(self, host=None, port=None, password=None, decode_responses=True):
        self.host = host or os.environ.get("REDIS_HOST", "localhost")
        self.port = port or int(os.environ.get("REDIS_PORT", 6379))
        self.password = password or os.environ.get("REDISCLI_AUTH", None)
        self.decode_responses = decode_responses
        self.client = None

    def __enter__(self):
        self.client = redis.Redis(
            host=self.host,
            port=self.port,
            password=self.password if self.password not in (None, '') else None,
            decode_responses=self.decode_responses
        )
        return self.client

    def __exit__(self, exc_type, exc_val, exc_tb):
        try:
            # 如果 client 提供 close() 方法，则关闭连接
            if self.client and hasattr(self.client, "close"):
                self.client.close()
        except Exception:
            pass
# -------------------------------
# 分布式锁实现（API: try_lock, unlock）
# -------------------------------
class RedisLock:
    def __init__(self, redis_client, name, lock_expire=10):
        """
        :param redis_client: redis.Redis 实例
        :param name: 锁的名称（即 redis 的 key）
        :param lock_expire: 锁超时时间（秒），防止死锁
        """
        self.redis_client = redis_client
        self.name = name
        self.lock_expire = lock_expire
        self.lock_value = str(uuid.uuid4())

    def try_lock(self, wait_timeout=5, retry_delay=0.01):
        """
        尝试获取锁，最多等待 wait_timeout 秒（默认 5 秒）。
        :param wait_timeout: 等待的最长时间（秒）
        :param retry_delay: 每次重试间隔（秒）
        :return: 获取到锁返回 True，超时返回 False
        """
        end_time = time.time() + wait_timeout
        while time.time() < end_time:
            if self.redis_client.set(self.name, self.lock_value, nx=True, ex=self.lock_expire):
                return True
            time.sleep(retry_delay)
        return False

    def unlock(self):
        """
        使用 Lua 脚本保证只有锁的持有者才能释放锁
        :return: 脚本执行结果
        """
        lua_script = """
        if redis.call("get", KEYS[1]) == ARGV[1] then 
            return redis.call("del", KEYS[1])
        else 
            return 0 
        end
        """
        script = self.redis_client.register_script(lua_script)
        return script(keys=[self.name], args=[self.lock_value])

    def __enter__(self):
        # 默认阻塞等待直到获取锁
        if not self.try_lock():
            raise RuntimeError("获取锁超时")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.unlock()