#include "mental1104/concurrency/token_bucket.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {

std::string token_bucket_repo_root() {
  const char *workspace = std::getenv("GITHUB_WORKSPACE");
  if (workspace != NULL && workspace[0] != '\0') {
    return workspace;
  }

  const std::string source_file(__FILE__);
  const std::string unix_marker("/cpp/test/");
  const std::string windows_marker("\\cpp\\test\\");
  std::string::size_type pos = source_file.find(unix_marker);
  if (pos != std::string::npos) {
    return source_file.substr(0, pos);
  }
  pos = source_file.find(windows_marker);
  if (pos != std::string::npos) {
    return source_file.substr(0, pos);
  }
  return ".";
}

std::string token_bucket_join(const std::string &left, const char *right) {
#ifdef _WIN32
  return left + "\\" + right;
#else
  return left + "/" + right;
#endif
}

} // namespace

TEST(TokenBucketTransport, CopyReadmeToCoverageArtifact) {
  const std::string repo_root = token_bucket_repo_root();
  const std::string artifact_dir = token_bucket_join(repo_root, "_cov");
#ifdef _WIN32
  _mkdir(artifact_dir.c_str());
#else
  mkdir(artifact_dir.c_str(), 0755);
#endif

  std::ifstream input(
      token_bucket_join(repo_root, "cpp/README.md").c_str(), std::ios::binary);
  ASSERT_TRUE(input.good());
  std::ofstream output(
      token_bucket_join(artifact_dir, "cpp-token-bucket-readme.md").c_str(),
      std::ios::binary);
  ASSERT_TRUE(output.good());
  output << input.rdbuf();
  ASSERT_TRUE(output.good());
}

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
