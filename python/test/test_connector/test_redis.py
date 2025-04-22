import os
import time
import pytest
import multiprocessing
from mental1104.connector.redis import RedisLock
from mental1104.connector.redis import RedisConnection


# -------------------------------
# 辅助函数：清理指定前缀的 key
# -------------------------------
def clear_keys(client, prefix):
    """
    清除 Redis 中所有以指定 prefix 开头的 key
    """
    for key in client.scan_iter(f"{prefix}*"):
        client.delete(key)


# -------------------------------
# RedisLock 测试类
# -------------------------------
class TestRedisLock:
    @pytest.fixture(scope="module")
    def redis_client(self):
        """
        使用 RedisConnection 获取 Redis 客户端。
        如果环境变量未设置，则跳过测试。
        同时仅清理测试用到的 key 前缀，避免影响其他数据。
        """
        redis_host = os.environ.get("REDIS_HOST")
        redis_port = os.environ.get("REDIS_PORT")
        if not redis_host or not redis_port:
            pytest.skip("REDIS_HOST and REDIS_PORT environment variables are not set, skipping tests")
        try:
            with RedisConnection() as client:
                # 清理测试相关的 key（只清理 test:* 和 mp_test:*）
                clear_keys(client, "test:")
                clear_keys(client, "mp_test:")
                yield client
        except Exception as e:
            pytest.skip("Cannot connect to Redis: " + str(e))

    def test_connection_context_manager(self, redis_client):
        """测试 RedisConnection 上下文管理器能否正常返回 Redis 客户端"""
        with RedisConnection() as client:
            assert client.ping() is True, "RedisConnection 未能正确连接到 Redis"

    def test_single_thread_lock(self, redis_client):
        """测试单线程下的 try_lock 与 unlock API"""
        key = "test::{distributed_lock}"
        # 清理指定 key
        redis_client.delete(key)
        time.sleep(1)
        lock = RedisLock(redis_client, key, lock_expire=5)
        acquired = lock.try_lock(wait_timeout=1)  # 立即尝试获取锁
        assert acquired, "第一次应能获取锁"

        # 同时另一个锁实例尝试获取同一把锁应该失败
        lock2 = RedisLock(redis_client, key, lock_expire=5)
        acquired2 = lock2.try_lock(wait_timeout=1)
        assert not acquired2, "锁已被占用，第二次应无法获取锁"

        # 释放锁后，另一个实例即可获取
        lock.unlock()
        acquired2 = lock2.try_lock(wait_timeout=1)
        assert acquired2, "释放后应能获取锁"
        lock2.unlock()

    def test_multiprocess_lock(self, redis_client):
        """测试多进程下使用 try_lock 与 unlock 保证互斥性"""
        def worker(counter, redis_params):
            with RedisConnection(
                host=redis_params["host"],
                port=redis_params["port"],
                password=redis_params["auth"]
            ) as client:
                key = "mp_test:distributed_lock"
                # 每个进程清理自己用到的 key
                client.delete(key)
                lock = RedisLock(client, key, lock_expire=5)
                if lock.try_lock(wait_timeout=5):
                    try:
                        time.sleep(0.05)
                        counter.value += 1
                    finally:
                        lock.unlock()

        redis_params = {
            "host": os.environ.get("REDIS_HOST", "localhost"),
            "port": int(os.environ.get("REDIS_PORT", 6379)),
            "auth": os.environ.get("REDISCLI_AUTH", '')
        }
        process_count = 20
        counter = multiprocessing.Value('i', 0)
        processes = []
        for _ in range(process_count):
            p = multiprocessing.Process(target=worker, args=(counter, redis_params))
            processes.append(p)
            p.start()
        for p in processes:
            p.join()

        assert counter.value == process_count, f"多进程测试失败：最终计数应等于进程数，当前计数为 {counter.value}"

    def test_multi_process_redis_resource(self, redis_client):
        """多进程测试：多个进程访问同一个 Redis 资源，确保分布式锁生效"""

        def access_redis_resource(redis_client, key):
            """高并发访问 Redis 共享资源，确保同一时刻只有一个进程操作"""
            lock = RedisLock(redis_client, "distributed_lock", lock_expire=3)
            
            max_retries = 50  # 允许尝试获取锁的最大次数
            retry_delay = 0.01  # 每次尝试失败后的重试间隔
            cnt = 0

            while cnt != max_retries:
                if lock.try_lock():
                    try:
                        # 进入临界区
                        current_value = redis_client.get(key)
                        current_value = int(current_value) if current_value else 0
                        new_value = current_value + 1
                        
                        print(f"[进程 {multiprocessing.current_process().pid}] 🔄 访问资源 {key}，当前值: {current_value} -> {new_value}")
                        
                        redis_client.set(key, new_value)  # 更新 Redis 资源
                        time.sleep(0.01)  # 模拟业务逻辑（减少阻塞时间）
                        cnt += 1
                    finally:
                        lock.unlock()
                        print(f"[进程 {multiprocessing.current_process().pid}] 🔓 释放锁")
                else:
                    print(f"[进程 {multiprocessing.current_process().pid}] ⏳ 锁被占用，重试中...")
                    time.sleep(retry_delay)  # 等待一段时间再尝试获取锁



        key = "shared_counter"
        redis_client.set(key, 0)  # 初始化资源

        process_count = 5  # 5个进程同时竞争访问 Redis 资源
        processes = []
        for _ in range(process_count):
            p = multiprocessing.Process(target=access_redis_resource, args=(redis_client, key))
            processes.append(p)
            p.start()

        for p in processes:
            p.join()

        # 获取最终 Redis 资源值
        final_value = int(redis_client.get(key))
        print(f"🔍 资源 {key} 最终值: {final_value}（应等于成功获取锁的进程数）")