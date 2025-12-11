#ifndef MENTAL1104_LOG_ADAPTERS_CACHE_PRINTER_H
#define MENTAL1104_LOG_ADAPTERS_CACHE_PRINTER_H

#include "mental1104/core/cache.h"
#include "mental1104/log.h"

namespace mental1104 {
namespace log_detail {

template <typename Ret, typename... Args>
struct is_supported<LRUCache<Ret, Args...>> : std::true_type {};
template <typename Ret, typename... Args>
struct is_supported<LFUCache<Ret, Args...>> : std::true_type {};

template <typename Ret, typename... Args>
std::string format_value(const LRUCache<Ret, Args...> &cache) {
  std::ostringstream os;
  os << "{\n";
  bool first = true;
  for (const auto &pair : cache.debug_lru_entries()) {
    if (!first)
      os << ",\n";
    first = false;
    os << "    \"";
    bool key_first = true;
    std::apply(
        [&](const auto &...key_elements) {
          ((key_first ? (key_first = false, os << key_elements)
                      : (os << "_" << key_elements)),
           ...);
        },
        pair.first);
    os << "\": \"" << pair.second << "\"";
  }
  os << "\n}\n";
  return os.str();
}

template <typename Ret, typename... Args>
std::string format_value(const LFUCache<Ret, Args...> &cache) {
  std::ostringstream os;
  os << "{\n";
  const auto &entries = cache.debug_lfu_entries();

  // 将输出顺序固定为：频率降序，其次按 key 升序，避免 unordered_map
  // 在不同平台/编译器上的迭代顺序差异导致日志不稳定。
  using EntriesType = std::decay_t<decltype(entries)>;
  using EntryRef = std::reference_wrapper<
      const typename EntriesType::value_type>; // value_type = pair<key, entry>
  std::vector<EntryRef> items;
  items.reserve(entries.size());
  for (const auto &kv : entries) {
    items.emplace_back(kv);
  }

  std::sort(items.begin(), items.end(), [](const auto &lhs_ref, const auto &rhs_ref) {
    const auto &lhs = lhs_ref.get();
    const auto &rhs = rhs_ref.get();
    if (lhs.second.frequency != rhs.second.frequency) {
      return lhs.second.frequency > rhs.second.frequency; // 频率高的优先
    }
    return lhs.first < rhs.first; // 同频率按 key 递增
  });

  bool first_entry = true;
  for (const auto &kv_ref : items) {
    const auto &key = kv_ref.get().first;
    const auto &entry = kv_ref.get().second;

    if (!first_entry) {
      os << ",\n";
    }
    first_entry = false;

    os << "    \"";
    bool key_first = true;
    std::apply(
        [&](const auto &...key_elements) {
          ((key_first ? (key_first = false, os << key_elements)
                      : (os << "_" << key_elements)),
           ...);
        },
        key);

    os << "\": {\n"
       << "        \"freq\": " << entry.frequency << ",\n"
       << "        \"value\": " << entry.value << "\n"
       << "    }";
  }
  os << "\n}\n";
  return os.str();
}

} // namespace log_detail
} // namespace mental1104

#endif // MENTAL1104_LOG_ADAPTERS_CACHE_PRINTER_H
