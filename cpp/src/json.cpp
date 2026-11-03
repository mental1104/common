#include "mental1104/json.h"

namespace mental1104 {

namespace {
/// 为 cJSON 查询临时提供以 NUL 结尾的键，长键使用独占堆缓冲。
struct KeyBuf {
  char local[64]{};
  const char *cstr{nullptr};
  char *heap{nullptr};
  /// 复制只读键字节；p 必须指向至少 n 个有效字节。
  /// @param p 不接管所有权的键数据。
  /// @param n 键的字节数。
  KeyBuf(const char *p, std::size_t n) {
    if (n < sizeof(this->local)) {
      std::memcpy(this->local, p, n);
      this->local[n] = '\0';
      this->cstr = this->local;
    } else {
      this->heap = new char[n + 1];
      std::memcpy(this->heap, p, n);
      this->heap[n] = '\0';
      this->cstr = this->heap;
    }
  }
  /// 从只读键视图建立临时缓冲。
  /// @param k 当前查询期间有效的键视图。
  KeyBuf(string_view k) : KeyBuf(k.data(), k.size()) {}
  /// 释放仅由当前对象持有的堆缓冲。
  ~KeyBuf() { delete[] this->heap; }
};
} // namespace

/// 释放由当前文档持有的后端资源。
JsonDoc::~JsonDoc() { this->reset(); }

/// 接管另一个文档的后端资源。
/// @param other 移动后置为空状态的源文档。
JsonDoc::JsonDoc(JsonDoc &&other) noexcept {
  this->move_from(std::move(other));
}

/// 释放旧资源后接管源文档。
/// @param other 移动后置为空状态的源文档。
/// @return 当前文档引用。
JsonDoc &JsonDoc::operator=(JsonDoc &&other) noexcept {
  if (this != &other) {
    this->reset();
    this->move_from(std::move(other));
  }
  return *this;
}

/// 查询是否持有后端文档。
/// @return 非空文档返回 true。
bool JsonDoc::valid() const noexcept {
  return !json_detail::holds_alternative<json_detail::monostate>(this->impl_);
}

/// 释放后端资源并恢复为空文档状态。
void JsonDoc::reset() noexcept {
  if (json_detail::holds_alternative<cJSON *>(this->impl_)) {
    if (auto *p = json_detail::get<cJSON *>(this->impl_))
      cJSON_Delete(p);
  }
  this->impl_.emplace<json_detail::monostate>();
  this->backend_ = JsonParser::CJSON;
}

/// 接管源文档表示，不保留源对象所有权。
/// @param other 移动后清空的源文档。
void JsonDoc::move_from(JsonDoc &&other) noexcept {
  this->impl_ = std::move(other.impl_);
  this->backend_ = other.backend_;
  other.impl_.emplace<json_detail::monostate>();
}

/// 建立不拥有资源的 cJSON 节点视图。
/// @param n 可为空的借用节点，所属文档必须在视图使用期间存活。
/// @return 借用节点的视图。
JsonValueView JsonValueView::from_cjson(const cJSON *n) {
  return JsonValueView(Node{n});
}

/// 建立不拥有资源的 RapidJSON 节点视图。
/// @param n 可为空的借用节点，所属文档必须在视图使用期间存活。
/// @return 借用节点的视图。
JsonValueView JsonValueView::from_rapid(const rapidjson::Value *n) {
  return JsonValueView(Node{n});
}

/// 查询是否保存后端节点表示。
/// @return 不是 monostate 时返回 true，沿用现有表示语义。
bool JsonValueView::is_valid() const noexcept {
  return !json_detail::holds_alternative<json_detail::monostate>(this->node_);
}

/// 查询节点是否为 object。
/// @return 非空且类型匹配时为 true。
bool JsonValueView::is_object() const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_))
    return *p && cJSON_IsObject(*p);
  if (auto p = json_detail::get_if<const rapidjson::Value *>(&this->node_))
    return *p && (*p)->IsObject();
  return false;
}

/// 查询节点是否为 array。
/// @return 非空且类型匹配时为 true。
bool JsonValueView::is_array() const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_))
    return *p && cJSON_IsArray(*p);
  if (auto p = json_detail::get_if<const rapidjson::Value *>(&this->node_))
    return *p && (*p)->IsArray();
  return false;
}

/// 查询节点是否为 string。
/// @return 非空且类型匹配时为 true。
bool JsonValueView::is_string() const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_))
    return *p && cJSON_IsString(*p);
  if (auto p = json_detail::get_if<const rapidjson::Value *>(&this->node_))
    return *p && (*p)->IsString();
  return false;
}

/// 查询对象是否包含区分大小写的键。
/// @param key 只读键，不保存视图。
/// @return 非对象或键缺失时为 false。
bool JsonValueView::has(string_view key) const noexcept {
  if (!this->is_object())
    return false;
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    KeyBuf kb(key);
    return cJSON_GetObjectItemCaseSensitive(*p, kb.cstr) != nullptr;
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    auto it = (*p2)->FindMember(rapidjson::StringRef(
        key.data(), static_cast<rapidjson::SizeType>(key.size())));
    return it != (*p2)->MemberEnd();
  }
  return false;
}

/// 查询对象成员，不转移后端资源。
/// @param key 只读键，不保存视图。
/// @return 成员视图；类型不匹配或缺失时为空视图。
JsonValueView JsonValueView::get(string_view key) const noexcept {
  if (!this->is_object())
    return {};
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    KeyBuf kb(key);
    cJSON *item = cJSON_GetObjectItemCaseSensitive(*p, kb.cstr);
    return item ? JsonValueView::from_cjson(item) : JsonValueView{};
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    auto it = (*p2)->FindMember(rapidjson::StringRef(
        key.data(), static_cast<rapidjson::SizeType>(key.size())));
    return (it != (*p2)->MemberEnd()) ? JsonValueView::from_rapid(&it->value)
                                      : JsonValueView{};
  }
  return {};
}

/// 查询数组长度。
/// @return 数组元素数量；非数组返回 0。
std::size_t JsonValueView::size() const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    return (*p && cJSON_IsArray(*p))
               ? static_cast<std::size_t>(cJSON_GetArraySize(*p))
               : 0;
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    return (*p2 && (*p2)->IsArray()) ? (*p2)->Size() : 0;
  }
  return 0;
}

/// 查询数组元素，不转移资源。
/// @param i 从 0 开始的元素索引。
/// @return 元素视图；非数组或越界时为空视图。
JsonValueView JsonValueView::at(std::size_t i) const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    if (*p && cJSON_IsArray(*p)) {
      cJSON *it = cJSON_GetArrayItem(*p, static_cast<int>(i));
      return it ? JsonValueView::from_cjson(it) : JsonValueView{};
    }
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    if (*p2 && (*p2)->IsArray() && i < (*p2)->Size()) {
      return JsonValueView::from_rapid(
          &(*p2)->GetArray()[static_cast<rapidjson::SizeType>(i)]);
    }
  }
  return {};
}

/// 读取字符串，沿用 string_view 在当前标准下的借用或复制语义。
/// @param def 类型不匹配时返回的默认值。
/// @return 字符串；借用结果要求文档或默认值的存储保持存活。
string_view JsonValueView::as_string(string_view def) const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    return (*p && cJSON_IsString(*p) && (*p)->valuestring)
               ? string_view((*p)->valuestring)
               : def;
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    return (*p2 && (*p2)->IsString())
               ? string_view((*p2)->GetString(), (*p2)->GetStringLength())
               : def;
  }
  return def;
}

/// 读取布尔值。
/// @param def 类型不匹配时的默认值。
/// @return 节点值或默认值。
bool JsonValueView::as_bool(bool def) const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    return (*p && cJSON_IsBool(*p)) ? cJSON_IsTrue(*p) : def;
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    return (*p2 && (*p2)->IsBool()) ? (*p2)->GetBool() : def;
  }
  return def;
}

/// 按后端既有数值转换规则读取整数。
/// @param def 类型不匹配时的默认值。
/// @return 转换值或默认值。
long long JsonValueView::as_i64(long long def) const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    return (*p && cJSON_IsNumber(*p))
               ? static_cast<long long>((*p)->valuedouble)
               : def;
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
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

/// 读取浮点数。
/// @param def 类型不匹配时的默认值。
/// @return 数值或默认值。
double JsonValueView::as_f64(double def) const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_)) {
    return (*p && cJSON_IsNumber(*p)) ? (*p)->valuedouble : def;
  } else if (auto p2 =
                 json_detail::get_if<const rapidjson::Value *>(&this->node_)) {
    return (*p2 && (*p2)->IsNumber()) ? (*p2)->GetDouble() : def;
  }
  return def;
}

/// 复制节点字符串，不返回借用视图。
/// @param def 类型不匹配时复制的默认值。
/// @return 独立字符串副本。
std::string JsonValueView::as_string_copy(const std::string &def) const {
  auto sv = this->as_string(def);
  return std::string(sv.data(), sv.size());
}

/// 借用当前文档的根节点。
/// @return 根视图；文档为空时返回空视图。
JsonValueView JsonDoc::root() const {
  if (!this->valid())
    return JsonValueView{};
  if (json_detail::holds_alternative<cJSON *>(this->impl_)) {
    return JsonValueView::from_cjson(json_detail::get<cJSON *>(this->impl_));
  } else {
    const rapidjson::Document &d =
        json_detail::get<rapidjson::Document>(this->impl_);
    return JsonValueView::from_rapid(static_cast<const rapidjson::Value *>(&d));
  }
}

/// 使用指定后端解析 JSON，并把文档所有权放入结果。
/// @param text 只读输入，本函数不保存调用方视图。
/// @param parser 选择 cJSON 或 RapidJSON。
/// @return 成功文档，或包含后端错误和位置的失败结果。
ParseResult parse_json(const string_view &text, JsonParser parser) {
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

/// 按显式字节长度解析 JSON。
/// @param data 指向至少 len 字节的只读数据。
/// @param len 输入字节数。
/// @param parser 选择解析后端。
/// @return 解析结果及独占文档。
ParseResult parse_json(const char *data, std::size_t len, JsonParser parser) {
  return parse_json(string_view(data, len), parser);
}

#if M1104_HAS_CXX17

/// 解析只读字符串。
/// @param s JSON 字符串，不保存其引用。
/// @param parser 选择解析后端。
/// @return 解析结果及独占文档。
ParseResult parse_json(const std::string &s, JsonParser parser) {
  return parse_json(string_view(s), parser);
}

#endif

/// 查询节点是否为 bool。
/// @return 非空且类型匹配时为 true。
bool JsonValueView::is_bool() const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_))
    return *p && cJSON_IsBool(*p);
  if (auto p = json_detail::get_if<const rapidjson::Value *>(&this->node_))
    return *p && (*p)->IsBool();
  return false;
}

/// 查询节点是否为 number。
/// @return 非空且类型匹配时为 true。
bool JsonValueView::is_number() const noexcept {
  if (auto p = json_detail::get_if<const cJSON *>(&this->node_))
    return *p && cJSON_IsNumber(*p);
  if (auto p = json_detail::get_if<const rapidjson::Value *>(&this->node_))
    return *p && (*p)->IsNumber();
  return false;
}
} // namespace mental1104
