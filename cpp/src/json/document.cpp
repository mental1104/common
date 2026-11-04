#include "mental1104/json/document.h"

#include <utility>

namespace mental1104 {
IJsonDocument::~IJsonDocument() = default;

JsonValueView IJsonDocument::root() const noexcept {
    return JsonValueView(this, this->do_root());
}

JsonDoc::JsonDoc() noexcept = default;
JsonDoc::JsonDoc(
    std::unique_ptr<IJsonDocument> document, JsonParser backend) noexcept
    : document_(std::move(document)), backend_(backend) {}
JsonDoc::~JsonDoc() = default;
JsonDoc::JsonDoc(JsonDoc &&other) noexcept = default;
JsonDoc &JsonDoc::operator=(JsonDoc &&other) noexcept = default;

bool JsonDoc::valid() const noexcept {
    return static_cast<bool>(this->document_);
}
JsonParser JsonDoc::backend() const noexcept { return this->backend_; }
JsonValueView JsonDoc::root() const noexcept {
    return this->document_ ? this->document_->root() : JsonValueView{};
}
}  // namespace mental1104
