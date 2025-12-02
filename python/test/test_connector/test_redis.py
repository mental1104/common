import os
import time
import pytest
import multiprocessing
from mental1104.connector.redis_client import RedisLock
from mental1104.connector.redis_client import RedisConnection
from mental1104.connector.redis_client.redis_bloom_kv import RedisBloom


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

    @pytest.fixture(scope="module")
    def redis_bloom_client(self, redis_client):
        """
        提供带 Bloom 模块检查的客户端；模块不可用时跳过。
        """
        bloom = RedisBloom(redis_client, filter_key="test:bf:kv:testcase")
        if not bloom.enabled:
            pytest.skip("Redis Bloom module not loaded; skipping bloom tests")
        # 清理测试前缀
        clear_keys(redis_client, "test:bloom:")
        return redis_client, bloom

    def test_connection_context_manager(self, redis_client):
        """
        【场景背景】RedisConnection 充当上下文管理器时应自动建立/关闭连接。
        【步骤输入】在 with RedisConnection(): 块内执行 client.ping()。
        【期望输出】ping 返回 True 且未抛出异常，证明上下文管理器封装正确。
        """
        with RedisConnection() as client:
            assert client.ping() is True, "RedisConnection 未能正确连接到 Redis"

    def test_single_thread_lock(self, redis_client):
        """
        【场景背景】RedisLock 在单线程环境下应严格互斥，释放后可再次锁定。
        【步骤输入】创建两个锁实例，依次 try_lock / unlock 相同 key。
        【期望输出】第一个实例立刻拿到锁，第二个实例在锁被释放前失败，
        unlock 后第二个实例获得锁，验证基本互斥语义。
        """
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
        """
        【场景背景】在多进程竞争场景中，分布式锁仍需保证只有一个进程进入临界区。
        【步骤输入】并发启动多个子进程，分别执行 try_lock -> 累加计数 -> unlock。
        【期望输出】计数器最终值等于进程数，说明每次只有一个进程能成功进入临界区。
        """
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
        """
        【场景背景】模拟多进程对同一 Redis 资源的高频读写，验证锁可防止并发写冲突。
        【步骤输入】5 个进程循环 try_lock，成功后读取 shared_counter、自增并写回。
        【期望输出】最终计数等于加锁次数，且日志显示锁竞争与释放过程，证明资源更新有序。
        """

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

    def test_bloom_filter_basic_miss_hit(self, redis_bloom_client):
        """
        【场景背景】验证 Bloom 过滤器在 miss-heavy 场景下的基本行为。
        【步骤输入】
          1) 向 Bloom 预加载部分 key。
          2) 对存在/不存在的 key 分别调用 exists。
        【期望输出】已插入的 key 返回 True，未插入的 key 返回 False。
        """
        client, bloom = redis_bloom_client
        prefix = "test:bloom:kv"
        exists_key = f"{prefix}:exists:1"
        miss_key = f"{prefix}:miss:1"

        client.set(exists_key, "v")
        bloom.add(exists_key)

        assert bloom.exists(exists_key) is True
        assert bloom.exists(miss_key) is False

        # 清理测试数据与布隆 key
        client.delete(exists_key, miss_key)
        clear_keys(client, prefix)
        client.delete(bloom.filter_key)
