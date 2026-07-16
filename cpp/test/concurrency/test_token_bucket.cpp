#include "mental1104/concurrency/token_bucket.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

TEST(TokenBucketTest, RejectsInvalidConfiguration) {
  EXPECT_THROW(mental1104::TokenBucket(0.0, 1), std::invalid_argument);
  EXPECT_THROW(mental1104::TokenBucket(-1.0, 1), std::invalid_argument);
  EXPECT_THROW(mental1104::TokenBucket(
                   std::numeric_limits<double>::quiet_NaN(), 1),
               std::invalid_argument);
  EXPECT_THROW(mental1104::TokenBucket(
                   std::numeric_limits<double>::infinity(), 1),
               std::invalid_argument);
  EXPECT_THROW(mental1104::TokenBucket(1.0, 0), std::invalid_argument);
}

TEST(TokenBucketTest, StartsFullAndReplenishesLazily) {
  mental1104::TokenBucket bucket(20.0, 2);
  EXPECT_TRUE(bucket.acquire());
  EXPECT_TRUE(bucket.acquire());

  const std::chrono::steady_clock::time_point started =
      std::chrono::steady_clock::now();
  EXPECT_TRUE(bucket.acquire());
  const std::chrono::milliseconds elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started);

  EXPECT_GE(elapsed.count(), 30);
  EXPECT_LT(elapsed.count(), 500);
}

TEST(TokenBucketTest, ReleaseDoesNotReturnToken) {
  mental1104::TokenBucket bucket(0.1, 1);
  EXPECT_TRUE(bucket.acquire());
  bucket.release();

  mental1104::CancellationToken cancellation;
  cancellation.cancel();
  EXPECT_FALSE(bucket.acquire(&cancellation));
}

TEST(TokenBucketTest, CancellationWakesWaiter) {
  mental1104::TokenBucket bucket(0.1, 1);
  ASSERT_TRUE(bucket.acquire());

  mental1104::CancellationToken cancellation;
  std::atomic<bool> result(true);
  std::thread waiter([&] { result.store(bucket.acquire(&cancellation)); });

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  cancellation.cancel();
  waiter.join();

  EXPECT_FALSE(result.load());
}

TEST(TokenBucketTest, ConcurrentWaitersDoNotShareToken) {
  mental1104::TokenBucket bucket(0.01, 1);
  mental1104::CancellationToken cancellation;
  std::atomic<int> acquired(0);
  std::atomic<int> ready(0);
  std::atomic<bool> start(false);
  std::vector<std::thread> threads;

  for (int i = 0; i < 4; ++i) {
    threads.push_back(std::thread([&] {
      ready.fetch_add(1);
      while (!start.load()) {
        std::this_thread::yield();
      }
      if (bucket.acquire(&cancellation)) {
        acquired.fetch_add(1);
      }
    }));
  }

  while (ready.load() != 4) {
    std::this_thread::yield();
  }
  start.store(true);

  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
  while (acquired.load() == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  cancellation.cancel();

  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }

  EXPECT_EQ(acquired.load(), 1);
}

namespace {

class RecordingLimiter {
public:
  RecordingLimiter() : acquire_result(true), acquire_calls(0), release_calls(0) {}

  bool acquire(const mental1104::CancellationToken *) {
    ++acquire_calls;
    return acquire_result;
  }

  void release() noexcept { ++release_calls; }

  bool acquire_result;
  int acquire_calls;
  int release_calls;
};

} // namespace

TEST(RateLimitedCallableTest, AcquiresCallsAndReleases) {
  RecordingLimiter limiter;
  int call_count = 0;
  mental1104::RateLimitedCallable<RecordingLimiter,
                                  std::function<int(int, int)> >
      wrapped = mental1104::rate_limited(
          limiter, std::function<int(int, int)>([&](int left, int right) {
            ++call_count;
            return left + right;
          }));

  EXPECT_EQ(wrapped(2, 3), 5);
  EXPECT_EQ(limiter.acquire_calls, 1);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(limiter.release_calls, 1);
}

TEST(RateLimitedCallableTest, ReleasesWhenCallableThrows) {
  RecordingLimiter limiter;
  mental1104::RateLimitedCallable<RecordingLimiter, std::function<void()> >
      wrapped = mental1104::rate_limited(
          limiter, std::function<void()>([] { throw std::runtime_error("boom"); }));

  EXPECT_THROW(wrapped(), std::runtime_error);
  EXPECT_EQ(limiter.acquire_calls, 1);
  EXPECT_EQ(limiter.release_calls, 1);
}

TEST(RateLimitedCallableTest, DoesNotCallOrReleaseWhenAcquireIsCancelled) {
  RecordingLimiter limiter;
  limiter.acquire_result = false;
  int call_count = 0;
  mental1104::CancellationToken cancellation;
  mental1104::RateLimitedCallable<RecordingLimiter, std::function<void()> >
      wrapped = mental1104::rate_limited(
          limiter, std::function<void()>([&] { ++call_count; }), &cancellation);

  EXPECT_THROW(wrapped(), mental1104::AcquireCancelledError);
  EXPECT_EQ(limiter.acquire_calls, 1);
  EXPECT_EQ(call_count, 0);
  EXPECT_EQ(limiter.release_calls, 0);
}

TEST(TokenBucketDocumentationFinalizer, RunsOnceInDesignatedCiJob) {
  const char *workspace = std::getenv("GITHUB_WORKSPACE");
  const char *actions = std::getenv("GITHUB_ACTIONS");
  const char *runner_os = std::getenv("RUNNER_OS");
  const char *cxx_std = std::getenv("CXX_STD");
  const char *cc = std::getenv("CC");
  if (workspace == NULL || actions == NULL || runner_os == NULL ||
      cxx_std == NULL || cc == NULL || std::string(actions) != "true" ||
      std::string(runner_os) != "Linux" || std::string(cxx_std) != "11" ||
      std::string(cc) != "gcc") {
    return;
  }

  const std::string command =
      std::string("python3 \"") + workspace +
      "/cpp/test/finalize_token_bucket_readme.py\"";
  EXPECT_EQ(std::system(command.c_str()), 0);
}
