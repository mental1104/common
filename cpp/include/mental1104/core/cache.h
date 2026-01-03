/*
 * @Date: 2025-01-29 21:02:07
 * @Author: mental1104 mental1104@gmail.com
 * @LastEditors: mental1104 mental1104@gmail.com
 * @LastEditTime: 2025-01-31 14:40:59
 */
#ifndef __MENTAL1104_CORE_CACHE
#define __MENTAL1104_CORE_CACHE

#include <functional>
#include <iostream>
#include <limits> // std::numeric_limits
#include <list>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility> // std::forward

namespace mental1104 {
namespace detail {
template <typename T> struct decay {
  using type = typename std::decay<T>::type;
};

template <typename T> using decay_t = typename decay<T>::type;

inline void hash_combine(std::size_t &seed, std::size_t value) {
  seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
}

template <typename T> inline void hash_combine(std::size_t &seed, const T &v) {
  hash_combine(seed, std::hash<decay_t<T>>{}(v));
}

template <typename Tuple, std::size_t Index> struct tuple_hash_helper {
  static void apply(std::size_t &seed, const Tuple &t) {
    tuple_hash_helper<Tuple, Index - 1>::apply(seed, t);
    hash_combine(seed, std::get<Index - 1>(t));
  }
};

template <typename Tuple> struct tuple_hash_helper<Tuple, 0> {
  static void apply(std::size_t &, const Tuple &) {}
};

template <typename Tuple> inline std::size_t hash_tuple(const Tuple &t) {
  std::size_t seed = 0;
  tuple_hash_helper<Tuple, std::tuple_size<Tuple>::value>::apply(seed, t);
  return seed;
}
} // namespace detail
} // namespace mental1104

// Hash function for tuple arguments
namespace std {
template <typename... Args> struct hash<std::tuple<Args...>> {
  size_t operator()(const std::tuple<Args...> &t) const {
    return ::mental1104::detail::hash_tuple(t);
  }
};
} // namespace std

namespace mental1104 {

template <typename Ret, typename... Args> class LRUCache {
private:
  using KeyType = std::tuple<Args...>;
  using ListIt = typename std::list<std::pair<KeyType, Ret>>::iterator;

  size_t capacity;
  std::unordered_map<KeyType, ListIt> cache;
  std::list<std::pair<KeyType, Ret>> lru_list;
  std::function<Ret(Args...)> func;

public:
  explicit LRUCache(size_t cap, std::function<Ret(Args...)> f)
      : capacity(cap), func(f) {}

  Ret operator()(Args... args) {
    KeyType key = std::make_tuple(args...);

    auto it = cache.find(key);
    if (it != cache.end()) {
      lru_list.splice(lru_list.begin(), lru_list, it->second);
      return it->second->second;
    }

    Ret result = func(args...);

    lru_list.emplace_front(key, result);
    cache[key] = lru_list.begin();

    if (cache.size() > capacity) {
      cache.erase(lru_list.back().first);
      lru_list.pop_back();
    }

    return result;
  }

  // 调试/格式化辅助（避免直接暴露内部容器类型）
  const std::list<std::pair<KeyType, Ret>> &debug_lru_entries() const {
    return lru_list;
  }
};

// 使用完美转发来推导类型，支持 lambda 表达式
template <typename Ret, typename... Args, typename F>
LRUCache<Ret, Args...> make_lru_cache(size_t capacity, F &&func) {
  return LRUCache<Ret, Args...>(capacity, std::forward<F>(func));
}

// 新增的无限容量缓存装饰器
template <typename Ret, typename... Args, typename F>
LRUCache<Ret, Args...> make_cache(F &&func) {
  const size_t unlimited_capacity = std::numeric_limits<size_t>::max();
  return make_lru_cache<Ret, Args...>(unlimited_capacity, func);
}

template <typename Ret, typename... Args> class LFUCache {
private:
  using KeyType = std::tuple<Args...>;
  using ListIt = typename std::list<KeyType>::iterator;

  size_t capacity;
  std::function<Ret(Args...)> func;

  struct CacheEntry {
    Ret value;
    int frequency;
    ListIt listIt;
  };

  std::unordered_map<KeyType, CacheEntry> cache;
  std::unordered_map<int, std::list<KeyType>> freqList;
  int minFreq = 0; // 当前最小访问频率

public:
  explicit LFUCache(size_t cap, std::function<Ret(Args...)> f)
      : capacity(cap), func(f) {}

  Ret operator()(Args... args) {
    KeyType key = std::make_tuple(args...);

    auto it = cache.find(key);
    if (it != cache.end()) {
      CacheEntry &entry = it->second;
      int oldFreq = entry.frequency;
      freqList[oldFreq].erase(entry.listIt);

      if (freqList[oldFreq].empty() && oldFreq == minFreq) {
        freqList.erase(oldFreq);
        minFreq++;
      }

      int newFreq = oldFreq + 1;
      freqList[newFreq].push_front(key);
      entry.frequency = newFreq;
      entry.listIt = freqList[newFreq].begin();

      return entry.value;
    }

    Ret result = func(args...);

    if (cache.size() >= capacity) {
      KeyType lfuKey = freqList[minFreq].back();
      freqList[minFreq].pop_back();
      if (freqList[minFreq].empty()) {
        freqList.erase(minFreq);
      }
      cache.erase(lfuKey);
    }

    minFreq = 1;
    freqList[1].push_front(key);
    cache[key] = {result, 1, freqList[1].begin()};

    return result;
  }

  const std::unordered_map<KeyType, CacheEntry> &debug_lfu_entries() const {
    return cache;
  }
};

// 使用完美转发来推导类型，支持 lambda 表达式
template <typename Ret, typename... Args, typename F>
LFUCache<Ret, Args...> make_lfu_cache(size_t capacity, F &&func) {
  return LFUCache<Ret, Args...>(capacity, std::forward<F>(func));
}

} // namespace mental1104

#endif
