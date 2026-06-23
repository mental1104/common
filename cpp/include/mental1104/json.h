#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include "mental1104/common/c_api_compat.h"
#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX17
#include <variant>
#else
#include <boost/variant2/variant.hpp>
#endif

COMMON_EXTERN_C_BEGIN
#if __has_include(<cJSON/cJSON.h>)
#include <cJSON/cJSON.h>
#else
#include "cJSON.h"
#endif
COMMON_EXTERN_C_END

#if __has_include(<rapidjson/document.h>)
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#else
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#endif

namespace mental1104 {

namespace json_detail {
#if M1104_HAS_CXX17
using std::get;
using std::get_if;
using std::holds_alternative;
using std::monostate;
template <typename... T>
using variant = std::variant<T...>;
#else
using boost::variant2::get;
using boost::variant2::get_if;
using boost::variant2::holds_alternative;
using boost::variant2::monostate;
template <typename... T>
using variant = boost::variant2::variant<T...>;
#endif
} // namespace json_detail

enum class JsonParser { CJSON, RapidJSON };

struct ParseResult;
ParseResult parse_json(const string_view &, JsonParser);
ParseResult parse_json(const char *, std::size_t, JsonParser);
#if M1104_HAS_CXX17
ParseResult parse_json(const std::string &, JsonParser);
#endif

class JsonValueView;
class JsonDoc;

namespace detail {
struct KeyBuf {
  char local[64]{};
  const char *cstr{nullptr};
  char *heap{nullptr};
  KeyBuf(const char *p, std::size_t n) {
    if (n < sizeof(local)) {
      std::memcpy(local, p, n);
      local[n] = '\0';
      cstr = local;
    } else {
      heap = new char[n + 1];
      std::memcpy(heap, p, n);
      heap[n] = '\0';
      cstr = heap;
    }
  }
  KeyBuf(string_view k) : KeyBuf(k.data(), k.size()) {}
  ~KeyBuf() { delete[] heap; }
};
} // namespace detail

class JsonDoc {
public:
  JsonDoc() = default;
  ~JsonDoc() { reset(); }

  JsonDoc(JsonDoc &&other) noexcept { move_from(std::move(other)); }
  JsonDoc &operator=(JsonDoc &&other) noexcept {
    if (this != &other) {
      reset();
      move_from(std::move(other));
    }
    return *this;
  }

  JsonDoc(const JsonDoc &) = delete;
  JsonDoc &operator=(const JsonDoc &) = delete;

  bool valid() const noexcept {
    return !json_detail::holds_alternative<json_detail::monostate>(impl_);
  }
  JsonParser backend() const noexcept { return backend_; }

  JsonValueView root() const;

private:
  using Impl =
      json_detail::variant<json_detail::monostate, cJSON *,
                           rapidjson::Document>;
  Impl impl_;
  JsonParser backend_{JsonParser::CJSON};

  void reset() noexcept {
    if (json_detail::holds_alternative<cJSON *>(impl_)) {
      if (auto *p = json_detail::get<cJSON *>(impl_))
        cJSON_Delete(p);
    }
    impl_.emplace<json_detail::monostate>();
    backend_ = JsonParser::CJSON;
  }
  void move_from(JsonDoc &&other) noexcept {
    impl_ = std::move(other.impl_);
    backend_ = other.backend_;
    other.impl_.emplace<json_detail::monostate>();
  }

  friend ParseResult parse_json(const string_view &, JsonParser);
  friend ParseResult parse_json(const char *, std::size_t, JsonParser);
#if M1104_HAS_CXX17
  friend ParseResult parse_json(const std::string &, JsonParser);
#endif

  friend class JsonValueView;
};

class JsonValueView {
public:
  JsonValueView() = default;

  static JsonValueView from_cjson(const cJSON *n) {
    return JsonValueView(Node{n});
  }
  static JsonValueView from_rapid(const rapidjson::Value *n) {
    return JsonValueView(Node{n});
  }

  bool is_valid() const noexcept {
    return !json_detail::holds_alternative<json_detail::monostate>(node_);
  }
  bool is_object() const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_))
      return *p && cJSON_IsObject(*p);
    if (auto p = json_detail::get_if<const rapidjson::Value *>(&node_))
      return *p && (*p)->IsObject();
    return false;
  }
  bool is_array() const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_))
      return *p && cJSON_IsArray(*p);
    if (auto p = json_detail::get_if<const rapidjson::Value *>(&node_))
      return *p && (*p)->IsArray();
    return false;
  }
  bool is_string() const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_))
      return *p && cJSON_IsString(*p);
    if (auto p = json_detail::get_if<const rapidjson::Value *>(&node_))
      return *p && (*p)->IsString();
    return false;
  }
  bool is_bool() const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_))
      return *p && cJSON_IsBool(*p);
    if (auto p = json_detail::get_if<const rapidjson::Value *>(&node_))
      return *p && (*p)->IsBool();
    return false;
  }
  bool is_number() const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_))
      return *p && cJSON_IsNumber(*p);
    if (auto p = json_detail::get_if<const rapidjson::Value *>(&node_))
      return *p && (*p)->IsNumber();
    return false;
  }

  bool has(string_view key) const noexcept {
    if (!is_object())
      return false;
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      detail::KeyBuf kb(key);
      return cJSON_GetObjectItemCaseSensitive(*p, kb.cstr) != nullptr;
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      auto it = (*p2)->FindMember(rapidjson::StringRef(
          key.data(), static_cast<rapidjson::SizeType>(key.size())));
      return it != (*p2)->MemberEnd();
    }
    return false;
  }

  JsonValueView get(string_view key) const noexcept {
    if (!is_object())
      return {};
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      detail::KeyBuf kb(key);
      cJSON *item = cJSON_GetObjectItemCaseSensitive(*p, kb.cstr);
      return item ? JsonValueView::from_cjson(item) : JsonValueView{};
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      auto it = (*p2)->FindMember(rapidjson::StringRef(
          key.data(), static_cast<rapidjson::SizeType>(key.size())));
      return (it != (*p2)->MemberEnd()) ? JsonValueView::from_rapid(&it->value)
                                        : JsonValueView{};
    }
    return {};
  }

  std::size_t size() const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      return (*p && cJSON_IsArray(*p))
                 ? static_cast<std::size_t>(cJSON_GetArraySize(*p))
                 : 0;
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      return (*p2 && (*p2)->IsArray()) ? (*p2)->Size() : 0;
    }
    return 0;
  }

  JsonValueView at(std::size_t i) const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      if (*p && cJSON_IsArray(*p)) {
        cJSON *it = cJSON_GetArrayItem(*p, static_cast<int>(i));
        return it ? JsonValueView::from_cjson(it) : JsonValueView{};
      }
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      if (*p2 && (*p2)->IsArray() && i < (*p2)->Size()) {
        return JsonValueView::from_rapid(
            &(*p2)->GetArray()[static_cast<rapidjson::SizeType>(i)]);
      }
    }
    return {};
  }

  string_view as_string(string_view def = {}) const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      return (*p && cJSON_IsString(*p) && (*p)->valuestring)
                 ? string_view((*p)->valuestring)
                 : def;
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      return (*p2 && (*p2)->IsString())
                 ? string_view((*p2)->GetString(),
                               (*p2)->GetStringLength())
                 : def;
    }
    return def;
  }

  bool as_bool(bool def = false) const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      return (*p && cJSON_IsBool(*p)) ? cJSON_IsTrue(*p) : def;
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      return (*p2 && (*p2)->IsBool()) ? (*p2)->GetBool() : def;
    }
    return def;
  }

  long long as_i64(long long def = 0) const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      return (*p && cJSON_IsNumber(*p))
                 ? static_cast<long long>((*p)->valuedouble)
                 : def;
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      const rapidjson::Value *v = *p2;
      if (!v)
        return def;
      if (v->IsInt64())
        return v->GetInt64();
      if (v->IsUint64())
        return static_cast<long long>(v->GetUint64());
      if (v->IsInt())
        return v->GetInt();
      if (v->IsUint())
        return static_cast<long long>(v->GetUint());
      if (v->IsDouble())
        return static_cast<long long>(v->GetDouble());
    }
    return def;
  }

  double as_f64(double def = 0.0) const noexcept {
    if (auto p = json_detail::get_if<const cJSON *>(&node_)) {
      return (*p && cJSON_IsNumber(*p)) ? (*p)->valuedouble : def;
    } else if (auto p2 = json_detail::get_if<const rapidjson::Value *>(&node_)) {
      return (*p2 && (*p2)->IsNumber()) ? (*p2)->GetDouble() : def;
    }
    return def;
  }

  std::string as_string_copy(const std::string &def = {}) const {
    auto sv = as_string(def);
    return std::string(sv.data(), sv.size());
  }

private:
  using Node = json_detail::variant<json_detail::monostate, const cJSON *,
                                    const rapidjson::Value *>;
  explicit JsonValueView(Node n) : node_(n) {}
  Node node_;

  friend class JsonDoc;
};

struct ParseResult {
  bool ok{false};
  std::string error;
  std::size_t offset{0};
  JsonDoc doc;
  explicit operator bool() const noexcept { return ok; }
};

inline JsonValueView JsonDoc::root() const {
  if (!valid())
    return JsonValueView{};
  if (json_detail::holds_alternative<cJSON *>(impl_)) {
    return JsonValueView::from_cjson(json_detail::get<cJSON *>(impl_));
  } else {
    const rapidjson::Document &d = json_detail::get<rapidjson::Document>(impl_);
    return JsonValueView::from_rapid(static_cast<const rapidjson::Value *>(&d));
  }
}

inline ParseResult parse_json(const string_view &text, JsonParser parser) {
  ParseResult r;
  if (parser == JsonParser::CJSON) {
    cJSON *root =
        cJSON_ParseWithLength(text.data(), static_cast<int>(text.size()));
    if (!root) {
      const char *p = cJSON_GetErrorPtr();
      r.ok = false;
      r.error =
          p ? std::string("cJSON parse error near: ") + p : "cJSON parse error";
      r.offset = 0;
      return r;
    }
    r.doc.reset();
    r.doc.impl_.emplace<cJSON *>(root);
    r.doc.backend_ = JsonParser::CJSON;
    r.ok = true;
    return r;
  } else {
    rapidjson::Document d;
    rapidjson::ParseResult ok =
        d.Parse(text.data(), static_cast<rapidjson::SizeType>(text.size()));
    if (!ok) {
      r.ok = false;
      r.error = rapidjson::GetParseError_En(ok.Code());
      r.offset = ok.Offset();
      return r;
    }
    r.doc.reset();
    r.doc.impl_.emplace<rapidjson::Document>(std::move(d));
    r.doc.backend_ = JsonParser::RapidJSON;
    r.ok = true;
    return r;
  }
}
inline ParseResult parse_json(const char *data, std::size_t len,
                              JsonParser parser) {
  return parse_json(string_view(data, len), parser);
}
#if M1104_HAS_CXX17
inline ParseResult parse_json(const std::string &s, JsonParser parser) {
  return parse_json(string_view(s), parser);
}
#endif

} // namespace mental1104
