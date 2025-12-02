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
  bool first_entry = true;
  for (const auto &[key, entry] : cache.debug_lfu_entries()) {
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
