// walkthrough: done
#ifndef MENTAL1104_CONCURRENCY_LOCK_SHARED_MUTEX_H
#define MENTAL1104_CONCURRENCY_LOCK_SHARED_MUTEX_H

#include <mutex>

#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX14 && M1104_HAS_INCLUDE(<shared_mutex>)
#include <shared_mutex>
#endif

namespace mental1104 {
namespace detail {
// C++17: std::shared_mutex (shared read, exclusive write).
// C++14: std::shared_timed_mutex (like shared_mutex + timed locks).
// fallback: std::mutex (no shared read; shared_lock_t maps to unique_lock).
#if M1104_HAS_CXX17 && M1104_HAS_INCLUDE(<shared_mutex>)
using shared_mutex_t = std::shared_mutex;
template <typename Mutex> using shared_lock_t = std::shared_lock<Mutex>;
#elif M1104_HAS_CXX14 && M1104_HAS_INCLUDE(<shared_mutex>)
using shared_mutex_t = std::shared_timed_mutex;
template <typename Mutex> using shared_lock_t = std::shared_lock<Mutex>;
#else
using shared_mutex_t = std::mutex;
template <typename Mutex> using shared_lock_t = std::unique_lock<Mutex>;
#endif
} // namespace detail
} // namespace mental1104

#endif // MENTAL1104_CONCURRENCY_LOCK_SHARED_MUTEX_H
