#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "export/json.hpp"

namespace py = pybind11;

namespace {

py::object rapidjson_value_to_py(const rapidjson::Value& v) {
    using rapidjson::SizeType;
    if (v.IsObject()) {
        py::dict d;
        for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
            const auto& name = it->name;
            d[py::str(name.GetString(), static_cast<py::ssize_t>(name.GetStringLength()))] =
                rapidjson_value_to_py(it->value);
        }
        return d;
    }
    if (v.IsArray()) {
        py::list lst;
        for (SizeType i = 0; i < v.Size(); ++i) {
            lst.append(rapidjson_value_to_py(v[i]));
        }
        return lst;
    }
    if (v.IsString()) return py::str(v.GetString(), static_cast<py::ssize_t>(v.GetStringLength()));
    if (v.IsBool()) return py::bool_(v.GetBool());
    if (v.IsInt64()) return py::int_(v.GetInt64());
    if (v.IsUint64()) return py::int_(v.GetUint64());
    if (v.IsDouble()) return py::float_(v.GetDouble());
    if (v.IsInt()) return py::int_(v.GetInt());
    if (v.IsUint()) return py::int_(v.GetUint());
    return py::none();
}

}  // namespace

PYBIND11_MODULE(mental1104_export_layer_pybind, m) {
    m.doc() = "Pybind11 bindings for mental1104_export_layer JSON";
    m.def(
        "parse_json",
        [](const std::string &payload) {
            auto r = mental1104_export_layer::parse_json(payload, mental1104::JsonParser::CJSON);
            return py::make_tuple(r.ok, r.error, r.offset);
        },
        R"pbdoc(Parse JSON with the C++ backend; returns (ok, error, offset).)pbdoc");

    m.def(
        "parse_json_value",
        [](const std::string& payload) {
            rapidjson::Document d;
            rapidjson::ParseResult ok = d.Parse(payload.data(), static_cast<rapidjson::SizeType>(payload.size()));
            if (!ok) {
                return py::make_tuple(false,
                                      py::none(),
                                      std::string(rapidjson::GetParseError_En(ok.Code())),
                                      static_cast<std::size_t>(ok.Offset()));
            }
            py::object obj = rapidjson_value_to_py(d);
            return py::make_tuple(true, obj, std::string{}, static_cast<std::size_t>(ok.Offset()));
        },
        R"pbdoc(Parse JSON with RapidJSON and return a Python object plus (ok, error, offset).)pbdoc");
}
