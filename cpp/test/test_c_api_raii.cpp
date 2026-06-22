#include <gtest/gtest.h>

#include "mental1104/c_api_raii.h"

#include <cstdio>
#include <stdexcept>
#include <type_traits>

#if !defined(_WIN32)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

struct NoopCleanup {
  void operator()() const noexcept {}
};

static_assert(!std::is_copy_constructible<mental1104::unique_file>::value,
              "unique_file must not be copy constructible");
static_assert(!std::is_copy_assignable<mental1104::unique_file>::value,
              "unique_file must not be copy assignable");
static_assert(std::is_move_constructible<mental1104::unique_file>::value,
              "unique_file must be move constructible");
static_assert(std::is_move_assignable<mental1104::unique_file>::value,
              "unique_file must be move assignable");

static_assert(!std::is_copy_constructible<
                  mental1104::scope_exit<NoopCleanup>>::value,
              "scope_exit must not be copy constructible");
static_assert(!std::is_copy_assignable<
                  mental1104::scope_exit<NoopCleanup>>::value,
              "scope_exit must not be copy assignable");
static_assert(std::is_move_constructible<
                  mental1104::scope_exit<NoopCleanup>>::value,
              "scope_exit must be move constructible");

#if !defined(_WIN32)
static_assert(!std::is_copy_constructible<mental1104::unique_fd>::value,
              "unique_fd must not be copy constructible");
static_assert(!std::is_copy_assignable<mental1104::unique_fd>::value,
              "unique_fd must not be copy assignable");
static_assert(std::is_move_constructible<mental1104::unique_fd>::value,
              "unique_fd must be move constructible");
static_assert(std::is_move_assignable<mental1104::unique_fd>::value,
              "unique_fd must be move assignable");

bool fd_is_open(int fd) {
  errno = 0;
  const int rc = ::fcntl(fd, F_GETFD);
  return !(rc == -1 && errno == EBADF);
}

void close_if_open(int fd) {
  if (fd >= 0 && fd_is_open(fd)) {
    (void)::close(fd);
  }
}

bool make_pipe(int fds[2]) { return ::pipe(fds) == 0; }
#endif

std::FILE *make_tmp_file() {
#if defined(_MSC_VER)
  std::FILE *file = nullptr;
  const int rc = tmpfile_s(&file);
  EXPECT_EQ(rc, 0);
#else
  std::FILE *file = std::tmpfile();
#endif
  EXPECT_NE(file, nullptr);
  return file;
}

} // namespace

#if !defined(_WIN32)
TEST(UniqueFdTest, DefaultConstructedIsInvalid) {
  mental1104::unique_fd fd;
  EXPECT_EQ(fd.get(), -1);
  EXPECT_FALSE(static_cast<bool>(fd));
}

TEST(UniqueFdTest, ClosesOwnedFdOnScopeExit) {
  int fds[2];
  ASSERT_TRUE(make_pipe(fds));
  const int read_fd = fds[0];
  {
    mental1104::unique_fd read_end(fds[0]);
    mental1104::unique_fd write_end(fds[1]);
    EXPECT_TRUE(fd_is_open(read_fd));
  }
  EXPECT_FALSE(fd_is_open(read_fd));
}

TEST(UniqueFdTest, MoveConstructTransfersOwnership) {
  int fds[2];
  ASSERT_TRUE(make_pipe(fds));
  const int read_fd = fds[0];
  {
    mental1104::unique_fd read_end(fds[0]);
    mental1104::unique_fd write_end(fds[1]);

    mental1104::unique_fd moved(std::move(read_end));

    EXPECT_EQ(moved.release(), read_fd);
  }
  EXPECT_TRUE(fd_is_open(read_fd));
  close_if_open(read_fd);
}

TEST(UniqueFdTest, MoveAssignmentClosesOldFdThenTransfersOwnership) {
  int first[2];
  int second[2];
  ASSERT_TRUE(make_pipe(first));
  ASSERT_TRUE(make_pipe(second));

  mental1104::unique_fd owner(first[0]);
  mental1104::unique_fd first_write(first[1]);
  const int old_fd = first[0];
  const int new_fd = second[0];
  {
    mental1104::unique_fd replacement(second[0]);
    mental1104::unique_fd second_write(second[1]);

    owner = std::move(replacement);
  }

  EXPECT_FALSE(fd_is_open(old_fd));
  EXPECT_EQ(owner.get(), new_fd);
  EXPECT_TRUE(fd_is_open(new_fd));
}

TEST(UniqueFdTest, ReleaseDoesNotCloseFd) {
  int fds[2];
  ASSERT_TRUE(make_pipe(fds));
  const int read_fd = fds[0];
  {
    mental1104::unique_fd read_end(fds[0]);
    mental1104::unique_fd write_end(fds[1]);
    EXPECT_EQ(read_end.release(), read_fd);
    EXPECT_EQ(read_end.get(), -1);
  }
  EXPECT_TRUE(fd_is_open(read_fd));
  close_if_open(read_fd);
}

TEST(UniqueFdTest, ResetNewFdClosesOldFdAndOwnsNewFd) {
  int first[2];
  int second[2];
  ASSERT_TRUE(make_pipe(first));
  ASSERT_TRUE(make_pipe(second));

  mental1104::unique_fd owner(first[0]);
  mental1104::unique_fd first_write(first[1]);
  mental1104::unique_fd second_write(second[1]);
  const int old_fd = first[0];
  const int new_fd = second[0];

  owner.reset(second[0]);

  EXPECT_FALSE(fd_is_open(old_fd));
  EXPECT_EQ(owner.get(), new_fd);
  EXPECT_TRUE(fd_is_open(new_fd));
}

TEST(UniqueFdTest, ResetWithoutArgumentClosesAndInvalidates) {
  int fds[2];
  ASSERT_TRUE(make_pipe(fds));
  mental1104::unique_fd read_end(fds[0]);
  mental1104::unique_fd write_end(fds[1]);
  const int read_fd = fds[0];

  read_end.reset();

  EXPECT_EQ(read_end.get(), -1);
  EXPECT_FALSE(fd_is_open(read_fd));
}

TEST(UniqueFdTest, DestroyingInvalidFdIsSafe) {
  mental1104::unique_fd fd;
  fd.reset();
  SUCCEED();
}
#endif

TEST(UniqueFileTest, DefaultConstructedIsNull) {
  mental1104::unique_file file;
  EXPECT_EQ(file.get(), nullptr);
  EXPECT_FALSE(static_cast<bool>(file));
}

TEST(UniqueFileTest, ClosesOwnedFileOnScopeExit) {
  std::FILE *raw = make_tmp_file();
  ASSERT_NE(raw, nullptr);
#if !defined(_WIN32)
  const int fd = ::fileno(raw);
  ASSERT_TRUE(fd_is_open(fd));
#endif
  {
    mental1104::unique_file file(raw);
    EXPECT_EQ(file.get(), raw);
  }
#if !defined(_WIN32)
  EXPECT_FALSE(fd_is_open(fd));
#endif
}

TEST(UniqueFileTest, MoveConstructTransfersOwnership) {
  std::FILE *raw = make_tmp_file();
  ASSERT_NE(raw, nullptr);
  mental1104::unique_file file(raw);

  mental1104::unique_file moved(std::move(file));

  EXPECT_EQ(moved.get(), raw);
}

TEST(UniqueFileTest, MoveAssignmentClosesOldFileThenTransfersOwnership) {
  std::FILE *first = make_tmp_file();
  std::FILE *second = make_tmp_file();
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
#if !defined(_WIN32)
  const int old_fd = ::fileno(first);
#endif
  mental1104::unique_file owner(first);
  mental1104::unique_file replacement(second);

  owner = std::move(replacement);

  EXPECT_EQ(owner.get(), second);
#if !defined(_WIN32)
  EXPECT_FALSE(fd_is_open(old_fd));
#endif
}

TEST(UniqueFileTest, ReleaseDoesNotCloseFile) {
  std::FILE *raw = make_tmp_file();
  ASSERT_NE(raw, nullptr);
  {
    mental1104::unique_file file(raw);
    EXPECT_EQ(file.release(), raw);
    EXPECT_EQ(file.get(), nullptr);
  }
  EXPECT_EQ(std::fclose(raw), 0);
}

TEST(UniqueFileTest, ResetNewFileClosesOldFileAndOwnsNewFile) {
  std::FILE *first = make_tmp_file();
  std::FILE *second = make_tmp_file();
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
#if !defined(_WIN32)
  const int old_fd = ::fileno(first);
#endif
  mental1104::unique_file file(first);

  file.reset(second);

  EXPECT_EQ(file.get(), second);
#if !defined(_WIN32)
  EXPECT_FALSE(fd_is_open(old_fd));
#endif
}

TEST(UniqueFileTest, ResetWithoutArgumentClosesAndInvalidates) {
  std::FILE *raw = make_tmp_file();
  ASSERT_NE(raw, nullptr);
#if !defined(_WIN32)
  const int fd = ::fileno(raw);
#endif
  mental1104::unique_file file(raw);

  file.reset();

  EXPECT_EQ(file.get(), nullptr);
#if !defined(_WIN32)
  EXPECT_FALSE(fd_is_open(fd));
#endif
}

TEST(UniqueFileTest, DestroyingNullFileIsSafe) {
  mental1104::unique_file file;
  file.reset();
  SUCCEED();
}

TEST(ScopeExitTest, RunsOnceOnNormalScopeExit) {
  int count = 0;
  {
    auto guard = mental1104::make_scope_exit([&count]() { ++count; });
    EXPECT_TRUE(guard.active());
  }
  EXPECT_EQ(count, 1);
}

TEST(ScopeExitTest, RunsOnEarlyReturn) {
  bool ran = false;
  const bool result = [&ran]() {
    auto guard = mental1104::make_scope_exit([&ran]() { ran = true; });
    EXPECT_TRUE(guard.active());
    return false;
  }();

  EXPECT_FALSE(result);
  EXPECT_TRUE(ran);
}

TEST(ScopeExitTest, RunsOnExceptionPath) {
  bool ran = false;
  try {
    auto guard = mental1104::make_scope_exit([&ran]() { ran = true; });
    EXPECT_TRUE(guard.active());
    throw std::runtime_error("fail");
  } catch (const std::runtime_error &) {
  }

  EXPECT_TRUE(ran);
}

TEST(ScopeExitTest, MoveConstructRunsCleanupOnlyOnce) {
  int count = 0;
  {
    auto first = mental1104::make_scope_exit([&count]() { ++count; });
    {
      auto second(std::move(first));
      EXPECT_TRUE(second.active());
    }
    EXPECT_EQ(count, 1);
  }
  EXPECT_EQ(count, 1);
}

TEST(ScopeExitTest, DismissCancelsCleanup) {
  int count = 0;
  {
    auto guard = mental1104::make_scope_exit([&count]() { ++count; });
    guard.dismiss();
    EXPECT_FALSE(guard.active());
  }
  EXPECT_EQ(count, 0);
}

TEST(ScopeExitTest, ReleaseAliasCancelsCleanup) {
  int count = 0;
  {
    auto guard = mental1104::make_scope_exit([&count]() { ++count; });
    guard.release();
  }
  EXPECT_EQ(count, 0);
}

TEST(ScopeExitTest, DestructorSwallowsCallbackException) {
  int count = 0;
  {
    auto guard = mental1104::make_scope_exit([&count]() {
      ++count;
      throw std::runtime_error("cleanup failed");
    });
  }
  EXPECT_EQ(count, 1);
}
