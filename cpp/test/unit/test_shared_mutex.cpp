#include "mental1104/concurrency/lock/shared_mutex.h"
#include "mental1104/meta/compiler_support.h"

#include <gtest/gtest.h>
#include <mutex>
#include <type_traits>

#if M1104_HAS_CXX14 && M1104_HAS_INCLUDE(<shared_mutex>)
#include <shared_mutex>
#endif

TEST(SharedMutexTest, TypeMapping) {
#if M1104_HAS_CXX17 && M1104_HAS_INCLUDE(<shared_mutex>)
  EXPECT_TRUE((std::is_same<mental1104::detail::shared_mutex_t, std::shared_mutex>::value));
  EXPECT_TRUE((std::is_same<
               mental1104::detail::shared_lock_t<mental1104::detail::shared_mutex_t>,
               std::shared_lock<mental1104::detail::shared_mutex_t>>::value));
#elif M1104_HAS_CXX14 && M1104_HAS_INCLUDE(<shared_mutex>)
  EXPECT_TRUE((std::is_same<mental1104::detail::shared_mutex_t, std::shared_timed_mutex>::value));
  EXPECT_TRUE((std::is_same<
               mental1104::detail::shared_lock_t<mental1104::detail::shared_mutex_t>,
               std::shared_lock<mental1104::detail::shared_mutex_t>>::value));
#else
  EXPECT_TRUE((std::is_same<mental1104::detail::shared_mutex_t, std::mutex>::value));
  EXPECT_TRUE((std::is_same<
               mental1104::detail::shared_lock_t<mental1104::detail::shared_mutex_t>,
               std::unique_lock<mental1104::detail::shared_mutex_t>>::value));
#endif
}
