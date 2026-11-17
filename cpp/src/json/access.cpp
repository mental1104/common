/// @file
/// 统一只读访问层：管理文档拥有者、节点借用、类型化读取与默认值语义。
/// 原生句柄只经所属文档的虚函数解释，文档及视图的所有权边界在此集中维护。
#include "mental1104/json/access.h"

#include <cmath>
#include <limits>
#include <utility>

#include "detail/backend.h"

namespace mental1104 {
namespace json_detail {
/// 检查有符号整数范围和小数部分。
/// @param value 后端保存的 double，不恢复解析前丢失的精度。
/// @return 可安全转换的整数或空结果。
JsonOptional<long long> signed_integer(double value) {
    const double bound =
        std::ldexp(1.0, std::numeric_limits<long long>::digits);
    // 正上界不能用 double(LLONG_MAX)：它可能舍入到不可转换的 2^63。
    if (!std::isfinite(value) || std::trunc(value) != value || value < -bound ||
        value >= bound) {
        return {};
    }
    return static_cast<long long>(value);
}
/// 检查无符号整数的符号、范围和小数部分。
/// @param value 后端保存的 double，不恢复解析前丢失的精度。
/// @return 可安全转换的非负整数或空结果。
JsonOptional<unsigned long long> unsigned_integer(double value) {
    const double bound =
        std::ldexp(1.0, std::numeric_limits<unsigned long long>::digits);
    // 必须在转换前检查非负性和右开上界，避免浮点转整数触发未定义行为。
    if (!std::isfinite(value) || std::trunc(value) != value || value < 0 ||
        value >= bound) {
        return {};
    }
    return static_cast<unsigned long long>(value);
}

}  // namespace json_detail

/// 通过虚析构完成多态删除；派生文档在此前释放原生 DOM，所有借用节点随之失效。
IJsonDocument::~IJsonDocument() = default;

/// 把原生根句柄与所属文档绑定，避免调用方独立组合来自不同后端的指针。
/// @return 根节点的只读借用视图；空根表示 Missing，结果不延长文档生命周期。
JsonValueView IJsonDocument::root() const noexcept {
    return JsonValueView(this, this->do_root());
}

/// 创建空的独占拥有者，不分配原生 DOM；valid() 为 false，root() 返回 Missing。
JsonDoc::JsonDoc() noexcept = default;
/// 接管一个地址固定的多态文档；后续移动 JsonDoc 只移动拥有指针。
/// @param document 转入的独占所有权；nullptr 保持空状态，不复制 DOM。
/// @param backend 仅供诊断的后端标识，不参与节点读取分派。
JsonDoc::JsonDoc(
    std::unique_ptr<IJsonDocument> document, JsonParser backend) noexcept
    : document_(std::move(document)), backend_(backend) {}
/// 释放当前拥有的原生文档；此前取得的视图和借用字符串从此失效。
JsonDoc::~JsonDoc() = default;
/// 把拥有指针移入新对象，原生文档地址与已有借用视图保持不变。
/// @param other 交出所有权的源对象；移动后不再拥有文档，诊断标识无需清空。
JsonDoc::JsonDoc(JsonDoc &&other) noexcept = default;
/// 按 unique_ptr 的移动赋值语义释放旧文档，再接管源文档。
/// @param other 交出所有权的源对象；指向被替换旧文档的视图失效。
/// @return 当前拥有者的引用，不复制 DOM 或延长旧视图的生命周期。
JsonDoc &JsonDoc::operator=(JsonDoc &&other) noexcept = default;

/// 检查独占拥有指针是否非空；合法的 JSON null 也对应一个实际文档。
/// @return 有后端文档时为 true；默认构造或移出所有权后为 false。
bool JsonDoc::valid() const noexcept {
    return static_cast<bool>(this->document_);
}
/// 读取构造时记录的诊断标识，不以枚举替代文档虚函数分派。
/// @return 后端标识；空对象也保留该字段，调用方应另用 valid()
/// 判断是否拥有文档。
JsonParser JsonDoc::backend() const noexcept { return this->backend_; }
/// 仅在拥有文档时创建借用根视图，空拥有者不进入后端。
/// @return 文档根视图；空拥有者返回 Missing，结果必须在文档存活期间使用。
JsonValueView JsonDoc::root() const noexcept {
    return this->document_ ? this->document_->root() : JsonValueView{};
}

/// 记录所属文档与原生节点的配对，不取得或共享任何资源所有权。
/// @param owner 能解释 node 的只读文档，必须在视图使用期间存活。
/// @param node 属于 owner 的借用句柄；nullptr 表示 Missing，不代表 JSON null。
JsonValueView::JsonValueView(
    const IJsonDocument *owner, const void *node) noexcept
    : owner_(owner), node_(node) {}
/// 空视图直接返回 Missing，实际节点的类型由所属后端解释。
/// @return 统一逻辑类型；有效的 JSON null 返回 Null。
JsonType JsonValueView::type() const noexcept {
    return this->is_valid() ? this->owner_->do_type(this->node_)
                            : JsonType::Missing;
}
/// 检查所属文档和节点句柄是否都存在，不追踪文档生命周期。
/// @return 表示中含实际节点时为 true；该结果不能用于检测已悬空的视图。
bool JsonValueView::is_valid() const noexcept {
    return this->owner_ && this->node_;
}
/// 区分实际的 JSON null 与查找失败产生的 Missing。
/// @return 节点类型为 Null 时为 true；缺失节点为 false。
bool JsonValueView::is_null() const noexcept {
    return this->type() == JsonType::Null;
}
/// 按统一类型契约识别对象，不读取对象成员。
/// @return 对象为 true；Missing、null 与其他节点为 false。
bool JsonValueView::is_object() const noexcept {
    return this->type() == JsonType::Object;
}
/// 按统一类型契约识别数组，空数组仍属于有效数组。
/// @return 数组为 true；Missing、null 与其他节点为 false。
bool JsonValueView::is_array() const noexcept {
    return this->type() == JsonType::Array;
}
/// 仅识别实际字符串节点，不把数值或布尔隐式转换成字符串。
/// @return 字符串为 true；空字符串也为 true，缺失节点为 false。
bool JsonValueView::is_string() const noexcept {
    return this->type() == JsonType::String;
}
/// 仅识别 JSON 布尔类型，不根据数值或字符串内容猜测真假。
/// @return 布尔节点为 true；保存 false 的节点也为 true。
bool JsonValueView::is_bool() const noexcept {
    return this->type() == JsonType::Bool;
}
/// 识别后端保存的数值节点，具体整数范围在后续取值时检查。
/// @return 数值节点为 true；不承诺该值可转换为任意目标整数类型。
bool JsonValueView::is_number() const noexcept {
    return this->type() == JsonType::Number;
}
/// 向所属后端查询成员，并将结果继续绑定同一文档；空视图不会进入后端。
/// @param key 区分大小写的只读键，本次查找后不保存键存储。
/// @return 成员的借用视图；非对象或缺失返回 Missing，成员为 null 时仍有效。
JsonValueView JsonValueView::get(const string_view &key) const {
    return this->is_valid() ? JsonValueView(this->owner_,
                                  this->owner_->do_get(this->node_, key))
                            : JsonValueView{};
}
/// 只遍历借用视图，遇到缺失即停止，目标类型转换由调用方在最后执行。
/// @param path 从外到内的对象键序列；不含数组索引，空序列表示当前视图。
/// @return 最终节点的借用视图；任一步缺失或中间节点非对象时返回 Missing。
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
/// 复用成员查找的存在语义，不把 null、false 或空字符串误判为缺失。
/// @param key 区分大小写的只读键，不保存引用。
/// @return 找到实际成员时为 true；非对象或未找到时为 false。
bool JsonValueView::has(const string_view &key) const {
    return this->get(key).is_valid();
}
/// 读取数组元素数量，空视图不向后端传递空句柄。
/// @return 数组长度；非数组和 Missing 均为 0，可另用 is_array() 区分。
std::size_t JsonValueView::size() const noexcept {
    return this->is_valid() ? this->owner_->do_size(this->node_) : 0;
}
/// 借用数组元素并保留所属文档，不复制元素或转移其所有权。
/// @param index 从 0 开始的下标，范围检查由后端在收窄索引之前完成。
/// @return 元素借用视图；非数组、越界或 Missing 返回空视图。
JsonValueView JsonValueView::at(std::size_t index) const noexcept {
    return this->is_valid() ? JsonValueView(this->owner_,
                                  this->owner_->do_at(this->node_, index))
                            : JsonValueView{};
}
/// 为数组范围建立首位置；非数组的长度为零，因此会形成空范围。
/// @return 借用的起始游标，不复制原生元素。
JsonValueView::ArrayIterator JsonValueView::begin() const noexcept {
    return ArrayIterator(*this, 0);
}
/// 以数组长度标识末尾，非数组与起始位置重合。
/// @return 不可解引用的借用结束游标。
JsonValueView::ArrayIterator JsonValueView::end() const noexcept {
    return ArrayIterator(*this, this->size());
}
/// 保存所属数组与当前索引，不访问后端或分配资源。
/// @param array 借用的数组视图，必须在后续遍历期间保持文档存活。
/// @param index 元素位置，数组长度表示结束位置。
JsonValueView::ArrayIterator::ArrayIterator(
    JsonValueView array, std::size_t index) noexcept
    : array_(array), index_(index) {}
/// 按当前索引查询元素，不取得元素所有权。
/// @return 元素的借用视图；调用方须保证当前位置不是末尾。
JsonValueView JsonValueView::ArrayIterator::operator*() const noexcept {
    return this->array_.at(this->index_);
}
/// 仅推进位置，不提前读取后端元素。
/// @return 推进后的当前游标引用。
JsonValueView::ArrayIterator &
JsonValueView::ArrayIterator::operator++() noexcept {
    ++this->index_;
    return *this;
}
/// 只比较文档、数组句柄和索引，不解引用任何借用对象。
/// @param other 同一范围的另一游标。
/// @return 所属文档、数组及位置都相同时为 true。
bool JsonValueView::ArrayIterator::operator==(
    const ArrayIterator &other) const noexcept {
    return this->array_.owner_ == other.array_.owner_ &&
           this->array_.node_ == other.array_.node_ &&
           this->index_ == other.index_;
}
/// 复用相等比较供 range-for 判断是否结束。
/// @param other 待比较的游标。
/// @return 不在同一数组同一位置时为 true。
bool JsonValueView::ArrayIterator::operator!=(
    const ArrayIterator &other) const noexcept {
    return !(*this == other);
}
/// 以可选结果保留字符串读取的成功状态，空字符串不会变成失败。
/// @return 字符串或空结果；C++17 起借用文档，旧标准的 string_view 别名会复制。
JsonOptional<string_view> JsonValueView::read_string() const {
    return this->is_valid() ? this->owner_->do_string(this->node_)
                            : JsonOptional<string_view>{};
}
/// 在缺失时短路，实际布尔读取交给所属后端，不做跨类型转换。
/// @return 包含 true/false 的结果；缺失或类型不符时为空。
JsonOptional<bool> JsonValueView::read_bool() const {
    return this->is_valid() ? this->owner_->do_bool(this->node_)
                            : JsonOptional<bool>{};
}
/// 严格读取后端当前数值的有符号整数表示，不恢复解析时丢失的精度。
/// @return 整数或空结果；缺失、类型错误、小数及越界均失败。
JsonOptional<long long> JsonValueView::read_i64() const {
    return this->is_valid() ? this->owner_->do_i64(this->node_)
                            : JsonOptional<long long>{};
}
/// 严格读取后端当前数值的非负整数表示，不允许负数绕回无符号范围。
/// @return 无符号整数或空结果；负数、小数、越界和类型错误均失败。
JsonOptional<unsigned long long> JsonValueView::read_u64() const {
    return this->is_valid() ? this->owner_->do_u64(this->node_)
                            : JsonOptional<unsigned long long>{};
}
/// 向后端请求数值的 double 表示，保留读取失败与数值 0 的区别。
/// @return 数值或空结果；整数转 double 可能发生正常舍入，有限性由目标转换检查。
JsonOptional<double> JsonValueView::read_f64() const {
    return this->is_valid() ? this->owner_->do_f64(this->node_)
                            : JsonOptional<double>{};
}
/// 在读取失败时返回调用方默认值，沿用各标准下 string_view 的借用或复制语义。
/// @param def 缺失或类型不符时使用的默认值；被借用的默认值存储也须保持存活。
/// @return 节点字符串或默认值；C++17 起结果不延长底层存储生命周期。
string_view JsonValueView::as_string(const string_view &def) const {
    const auto value = this->read_string();
    return value ? *value : def;
}
/// 把字符串读取结果转换成独立拥有的文本，调用方无需继续持有源文档。
/// @param def 缺失或类型不符时复制的默认字符串，不保存其引用。
/// @return 独立字符串副本；分配失败仍可抛异常。
std::string JsonValueView::as_string_copy(const std::string &def) const {
    const auto value = this->get_optional<std::string>();
    return value ? *value : def;
}
/// 读取布尔并显式应用默认值，JSON false 本身仍是成功结果。
/// @param def 缺失或类型不符时返回的默认值。
/// @return 实际布尔值或 def，不把数字和字符串当作布尔。
bool JsonValueView::as_bool(bool def) const {
    const auto value = this->read_bool();
    return value ? *value : def;
}
/// 先保留后端原生整数精度，失败后才尝试兼容旧接口的小数截断。
/// @param def 类型不符、非有限值或整数越界时返回的默认值。
/// @return 安全范围内向零截断的整数或 def；严格读取应使用 get_optional。
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
/// 读取数值的 double 表示，再在缺失或类型错误时使用默认值。
/// @param def 非数值或缺失时的默认值，不用于修正已解析数值。
/// @return 后端保存的数值或 def；需要有限值检查时使用 get_optional。
double JsonValueView::as_f64(double def) const {
    const auto value = this->read_f64();
    return value ? *value : def;
}
}  // namespace mental1104
