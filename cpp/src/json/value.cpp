#include <cmath>

#include "mental1104/json/document.h"
#include "support.h"

namespace mental1104 {
JsonValueView::JsonValueView(
    const IJsonDocument *owner, const void *node) noexcept
    : owner_(owner), node_(node) {}
JsonType JsonValueView::type() const noexcept {
    return this->is_valid() ? this->owner_->do_type(this->node_)
                            : JsonType::Missing;
}
bool JsonValueView::is_valid() const noexcept {
    return this->owner_ && this->node_;
}
bool JsonValueView::is_null() const noexcept {
    return this->type() == JsonType::Null;
}
bool JsonValueView::is_object() const noexcept {
    return this->type() == JsonType::Object;
}
bool JsonValueView::is_array() const noexcept {
    return this->type() == JsonType::Array;
}
bool JsonValueView::is_string() const noexcept {
    return this->type() == JsonType::String;
}
bool JsonValueView::is_bool() const noexcept {
    return this->type() == JsonType::Bool;
}
bool JsonValueView::is_number() const noexcept {
    return this->type() == JsonType::Number;
}
JsonValueView JsonValueView::get(const string_view &key) const {
    return this->is_valid() ? JsonValueView(this->owner_,
                                  this->owner_->do_get(this->node_, key))
                            : JsonValueView{};
}
JsonValueView JsonValueView::get(
    std::initializer_list<string_view> path) const {
    auto current = *this;
    for (const auto &key : path) {
        current = current.get(key);
        // 缺失会沿路径传播，不能把中间节点当成目标类型提前转换。
        if (!current.is_valid()) {
            return {};
        }
    }
    return current;
}
bool JsonValueView::has(const string_view &key) const {
    return this->get(key).is_valid();
}
std::size_t JsonValueView::size() const noexcept {
    return this->is_valid() ? this->owner_->do_size(this->node_) : 0;
}
JsonValueView JsonValueView::at(std::size_t index) const noexcept {
    return this->is_valid() ? JsonValueView(this->owner_,
                                  this->owner_->do_at(this->node_, index))
                            : JsonValueView{};
}
JsonOptional<string_view> JsonValueView::read_string() const {
    return this->is_valid() ? this->owner_->do_string(this->node_)
                            : JsonOptional<string_view>{};
}
JsonOptional<bool> JsonValueView::read_bool() const {
    return this->is_valid() ? this->owner_->do_bool(this->node_)
                            : JsonOptional<bool>{};
}
JsonOptional<long long> JsonValueView::read_i64() const {
    return this->is_valid() ? this->owner_->do_i64(this->node_)
                            : JsonOptional<long long>{};
}
JsonOptional<unsigned long long> JsonValueView::read_u64() const {
    return this->is_valid() ? this->owner_->do_u64(this->node_)
                            : JsonOptional<unsigned long long>{};
}
JsonOptional<double> JsonValueView::read_f64() const {
    return this->is_valid() ? this->owner_->do_f64(this->node_)
                            : JsonOptional<double>{};
}
string_view JsonValueView::as_string(const string_view &def) const {
    const auto value = this->read_string();
    return value ? *value : def;
}
std::string JsonValueView::as_string_copy(const std::string &def) const {
    const auto value = this->as_optional<std::string>();
    return value ? *value : def;
}
bool JsonValueView::as_bool(bool def) const {
    const auto value = this->read_bool();
    return value ? *value : def;
}
long long JsonValueView::as_i64(long long def) const {
    const auto integer = this->read_i64();
    if (integer) {
        return *integer;
    }
    const auto number = this->read_f64();
    // 仅为旧默认值接口保留小数截断，严格 optional 读取不走这条路径。
    const auto truncated =
        number ? json_detail::signed_integer(std::trunc(*number))
               : JsonOptional<long long>{};
    return truncated ? *truncated : def;
}
double JsonValueView::as_f64(double def) const {
    const auto value = this->read_f64();
    return value ? *value : def;
}
}  // namespace mental1104
