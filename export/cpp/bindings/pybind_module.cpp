#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "export/json.hpp"

namespace py = pybind11;

PYBIND11_MODULE(export_pybind, m) {
    m.doc() = "Pybind11 bindings for export_layer JSON";
    m.def(
        "parse_json",
        [](const std::string &payload) {
            auto r = export_layer::parse_json(payload, mental1104::JsonParser::CJSON);
            return py::make_tuple(r.ok, r.error, r.offset);
        },
        R"pbdoc(Parse JSON with the C++ backend; returns (ok, error, offset).)pbdoc");
}
