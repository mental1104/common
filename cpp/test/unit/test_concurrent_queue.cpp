#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX17

#include "mental1104/concurrency/concurrent_queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

// 验证无界队列的 FIFO 顺序、原地构造及基础状态查询。
TEST(ConcurrentQueueTest, UnboundedQueuePreservesFifoAndSupportsEmplace) {
  mental1104::ConcurrentQueue<int> queue;

  EXPECT_EQ(queue.capacity(), 0U);
  EXPECT_TRUE(queue.empty());
  EXPECT_TRUE(queue.push(1));
  EXPECT_TRUE(queue.emplace(2));
  EXPECT_EQ(queue.size(), 2U);
  EXPECT_EQ(queue.pop().value(), 1);
  EXPECT_EQ(queue.pop().value(), 2);
  EXPECT_TRUE(queue.empty());
}

// 验证队列支持不可复制的值类型，并通过移动语义返回元素。
TEST(ConcurrentQueueTest, SupportsMoveOnlyValues) {
  mental1104::ConcurrentQueue<std::unique_ptr<int>> queue;
  EXPECT_TRUE(queue.push(std::unique_ptr<int>(new int(7))));

  std::optional<std::unique_ptr<int>> item = queue.pop();
  EXPECT_TRUE(item.has_value());
  if (item.has_value()) {
    ASSERT_TRUE(item->get() != nullptr);
    EXPECT_EQ(**item, 7);
  }
}

// 验证有界队列满时生产者阻塞，并在消费者取走元素后继续入队。
TEST(ConcurrentQueueTest, BoundedPushBlocksUntilSpaceIsAvailable) {
  mental1104::ConcurrentQueue<int> queue(1);
  ASSERT_TRUE(queue.push(1));

  std::promise<void> started;
  std::future<void> started_future = started.get_future();
  std::future<bool> producer = std::async(std::launch::async, [&queue, &started] {
    started.set_value();
    return queue.push(2);
  });

  EXPECT_EQ(started_future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  const std::future_status blocked_status =
      producer.wait_for(std::chrono::milliseconds(50));

  const std::optional<int> first = queue.pop();
  const std::future_status finished_status =
      producer.wait_for(std::chrono::seconds(1));
  if (finished_status != std::future_status::ready) {
    // 即使被测实现未在 pop 后唤醒生产者，也先关闭队列，避免失败用例挂住测试进程。
    queue.close();
  }

  EXPECT_EQ(blocked_status, std::future_status::timeout);
  EXPECT_EQ(finished_status, std::future_status::ready);
  EXPECT_TRUE(first.has_value());
  if (first.has_value()) {
    EXPECT_EQ(*first, 1);
  }
  if (producer.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
    const bool pushed = producer.get();
    EXPECT_TRUE(pushed);
    if (pushed) {
      const std::optional<int> second = queue.pop();
      EXPECT_TRUE(second.has_value());
      if (second.has_value()) {
        EXPECT_EQ(*second, 2);
      }
    }
  }
}

// 验证空队列上的消费者会等待，直到有元素入队。
TEST(ConcurrentQueueTest, PopBlocksUntilAnItemIsAvailable) {
  mental1104::ConcurrentQueue<int> queue;
  std::promise<void> started;
  std::future<void> started_future = started.get_future();
  std::future<std::optional<int>> consumer =
      std::async(std::launch::async, [&queue, &started] {
        started.set_value();
        return queue.pop();
      });

  EXPECT_EQ(started_future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  const std::future_status blocked_status =
      consumer.wait_for(std::chrono::milliseconds(50));
  EXPECT_TRUE(queue.push(42));

  const std::future_status finished_status =
      consumer.wait_for(std::chrono::seconds(1));
  if (finished_status != std::future_status::ready) {
    // 关闭会唤醒仍在等待的消费者，确保测试失败时也能回收异步任务。
    queue.close();
  }
  EXPECT_EQ(blocked_status, std::future_status::timeout);
  EXPECT_EQ(finished_status, std::future_status::ready);
  if (consumer.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
    const std::optional<int> item = consumer.get();
    EXPECT_TRUE(item.has_value());
    if (item.has_value()) {
      EXPECT_EQ(*item, 42);
    }
  }
  queue.close();
}

// 验证关闭后拒绝新元素，但保留并排空关闭前已入队的元素。
TEST(ConcurrentQueueTest, CloseRejectsNewItemsAndDrainsExistingItems) {
  mental1104::ConcurrentQueue<int> queue(2);
  ASSERT_TRUE(queue.push(1));
  ASSERT_TRUE(queue.push(2));

  queue.close();
  queue.close();
  EXPECT_TRUE(queue.is_closed());
  EXPECT_FALSE(queue.push(3));
  EXPECT_EQ(queue.pop().value(), 1);
  EXPECT_EQ(queue.pop().value(), 2);
  EXPECT_FALSE(queue.pop().has_value());
}

// 验证关闭操作会唤醒因队列已满而阻塞的生产者。
TEST(ConcurrentQueueTest, CloseWakesBlockedProducer) {
  mental1104::ConcurrentQueue<int> queue(1);
  ASSERT_TRUE(queue.push(1));

  std::promise<void> started;
  std::future<void> started_future = started.get_future();
  std::future<bool> producer = std::async(std::launch::async, [&queue, &started] {
    started.set_value();
    return queue.push(2);
  });

  EXPECT_EQ(started_future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  EXPECT_EQ(producer.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout);
  queue.close();
  const std::future_status close_status =
      producer.wait_for(std::chrono::seconds(1));
  EXPECT_EQ(close_status, std::future_status::ready);
  std::optional<int> first;
  if (close_status != std::future_status::ready) {
    // 关闭未唤醒生产者时，释放一个槽位作为兜底，避免失败用例永久阻塞。
    first = queue.pop();
  }
  if (producer.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
    EXPECT_FALSE(producer.get());
  }
  if (!first.has_value()) {
    first = queue.pop();
  }
  EXPECT_TRUE(first.has_value());
  if (first.has_value()) {
    EXPECT_EQ(*first, 1);
  }
  EXPECT_FALSE(queue.pop().has_value());
}

// 验证关闭操作会唤醒因队列为空而阻塞的消费者。
TEST(ConcurrentQueueTest, CloseWakesBlockedConsumer) {
  mental1104::ConcurrentQueue<int> queue;
  std::promise<void> started;
  std::future<void> started_future = started.get_future();
  std::future<std::optional<int>> consumer =
      std::async(std::launch::async, [&queue, &started] {
        started.set_value();
        return queue.pop();
      });

  EXPECT_EQ(started_future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  EXPECT_EQ(consumer.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout);
  queue.close();
  EXPECT_EQ(consumer.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  EXPECT_FALSE(consumer.get().has_value());
}

// 验证多个生产者和消费者并发运行时没有元素丢失或重复交付。
TEST(ConcurrentQueueTest, MultipleProducersAndConsumersDeliverEachItemOnce) {
  constexpr int producer_count = 4;
  constexpr int consumer_count = 4;
  constexpr int items_per_producer = 500;
  constexpr int total_items = producer_count * items_per_producer;
  mental1104::ConcurrentQueue<int> queue(32);

  std::vector<std::atomic<int>> seen(static_cast<std::size_t>(total_items));
  for (std::atomic<int> &count : seen) {
    count.store(0);
  }
  std::atomic<int> consumed_count{0};
  std::atomic<bool> invalid_item{false};
  std::vector<std::thread> consumers;
  std::vector<std::thread> producers;

  for (int i = 0; i < consumer_count; ++i) {
    consumers.emplace_back([&queue, &seen, &consumed_count, &invalid_item] {
      while (std::optional<int> item = queue.pop()) {
        if (*item < 0 || *item >= total_items) {
          invalid_item.store(true);
          continue;
        }
        seen[static_cast<std::size_t>(*item)].fetch_add(1);
        consumed_count.fetch_add(1);
      }
    });
  }

  std::atomic<bool> push_failed{false};
  for (int i = 0; i < producer_count; ++i) {
    producers.emplace_back([&queue, &push_failed, i] {
      for (int j = 0; j < items_per_producer; ++j) {
        if (!queue.push(i * items_per_producer + j)) {
          push_failed.store(true);
          return;
        }
      }
    });
  }

  for (std::thread &producer : producers) {
    producer.join();
  }
  queue.close();
  for (std::thread &consumer : consumers) {
    consumer.join();
  }

  EXPECT_FALSE(push_failed.load());
  EXPECT_FALSE(invalid_item.load());
  EXPECT_EQ(consumed_count.load(), total_items);
  bool each_item_seen_once = true;
  for (const std::atomic<int> &count : seen) {
    if (count.load() != 1) {
      each_item_seen_once = false;
      break;
    }
  }
  EXPECT_TRUE(each_item_seen_once);
}

#endif // M1104_HAS_CXX17
