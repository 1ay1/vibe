// vibe.hpp — VIBE as native C++ syntax.
//
// This makes VIBE a first-class C++ literal. You write a document *inline* and
// the `_vibe` user-defined literal turns it into a live, typed value at the
// point of use — no parse("...") call, no separate loader step. VIBE becomes
// part of the language:
//
//     #include "vibe.hpp"
//     using namespace vibe::literals;
//
//     auto cfg = R"(
//         name    my-service
//         port    8080
//         tls     true
//         origins [ https://a.example  https://b.example ]
//     )"_vibe;
//
//     std::string name = cfg["name"];          // implicit conversion
//     int         port = cfg["port"];          // typed on assignment
//     bool        tls  = cfg["tls"];
//     for (auto v : cfg["origins"]) use(v.str());
//
// A malformed literal throws vibe::parse_error at runtime (the earliest a
// runtime library can react). Everything is a thin RAII view over libvibe;
// the underlying VibeValue tree is reference-counted so `node["a"]["b"]`
// stays valid as long as any handle to the document lives.
//
// Header-only. Requires C++17. Link against libvibe (or #define
// VIBE_IMPLEMENTATION in exactly one .cpp before including vibe.h).
//
// SPDX-License-Identifier: MIT
#ifndef VIBE_HPP
#define VIBE_HPP

#include "vibe.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace vibe {

// Thrown when a `_vibe` literal (or vibe::parse) is handed malformed input.
class parse_error : public std::runtime_error {
public:
    parse_error(std::string msg, int line, int column, VibeErrorCode code)
        : std::runtime_error(std::move(msg)), line_(line), column_(column), code_(code) {}
    int line() const noexcept { return line_; }
    int column() const noexcept { return column_; }
    VibeErrorCode code() const noexcept { return code_; }
private:
    int line_, column_;
    VibeErrorCode code_;
};

// A read-only view onto a node in a VIBE document. Cheap to copy (it shares
// ownership of the document via a shared_ptr, so child views outlive the
// expression that produced them). Never owns an individual node — only the
// document root frees the tree.
class node {
public:
    node() = default;
    node(std::shared_ptr<VibeValue> doc, VibeValue* v) : doc_(std::move(doc)), v_(v) {}

    bool valid() const noexcept { return v_ != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    VibeType type() const noexcept { return vibe_value_type(v_); }
    bool is_null()    const noexcept { return vibe_is_null(v_); }
    bool is_int()     const noexcept { return vibe_is_integer(v_); }
    bool is_float()   const noexcept { return type() == VIBE_TYPE_FLOAT; }
    bool is_bool()    const noexcept { return type() == VIBE_TYPE_BOOLEAN; }
    bool is_string()  const noexcept { return type() == VIBE_TYPE_STRING; }
    bool is_array()   const noexcept { return type() == VIBE_TYPE_ARRAY; }
    bool is_object()  const noexcept { return type() == VIBE_TYPE_OBJECT; }

    // Object / path lookup. `cfg["a"]["b"]` and `cfg["a.b"]` both work; a dot
    // in the key is treated as a path separator, matching libvibe semantics.
    node operator[](const char* path) const {
        return node(doc_, v_ ? vibe_get(v_, path) : nullptr);
    }
    node operator[](const std::string& path) const { return (*this)[path.c_str()]; }

    // Array indexing. Both int and size_t are accepted so a literal `[0]`
    // isn't ambiguous with the const char* key overload.
    node operator[](std::size_t i) const {
        VibeArray* a = (v_ && type() == VIBE_TYPE_ARRAY) ? v_->as_array : nullptr;
        return node(doc_, a ? vibe_array_get(a, i) : nullptr);
    }
    node operator[](int i) const { return (*this)[static_cast<std::size_t>(i)]; }

    std::size_t size() const noexcept {
        if (!v_) return 0;
        if (type() == VIBE_TYPE_ARRAY)  return vibe_array_size(v_->as_array);
        if (type() == VIBE_TYPE_OBJECT) return vibe_object_size(v_->as_object);
        return 0;
    }

    // Typed readers. Each takes an optional fallback used when the node is
    // absent or of the wrong type.
    std::string str(const std::string& fb = {}) const {
        const char* s = (v_ && type() == VIBE_TYPE_STRING) ? v_->as_string : nullptr;
        return s ? std::string(s) : fb;
    }
    long long as_int(long long fb = 0) const {
        return vibe_is_integer(v_) ? (long long)v_->as_integer : fb;
    }
    double as_float(double fb = 0.0) const {
        if (type() == VIBE_TYPE_FLOAT)   return v_->as_float;
        if (type() == VIBE_TYPE_INTEGER) return (double)v_->as_integer;
        return fb;
    }
    bool as_bool(bool fb = false) const {
        return type() == VIBE_TYPE_BOOLEAN ? v_->as_boolean : fb;
    }

    // Implicit conversions so `int port = cfg["port"];` just works.
    // (Note: no implicit `operator bool` — use as_bool() explicitly to read a
    //  boolean value; `explicit operator bool` above only tests node validity.)
    operator std::string() const { return str(); }
    operator long long()   const { return as_int(); }
    operator int()         const { return (int)as_int(); }
    operator double()      const { return as_float(); }

    // Range-for over arrays and objects.
    class iterator {
    public:
        iterator(const node* parent, std::size_t i) : p_(parent), i_(i) {}
        node operator*() const { return (*p_)[i_]; }
        iterator& operator++() { ++i_; return *this; }
        bool operator!=(const iterator& o) const { return i_ != o.i_; }
    private:
        const node* p_;
        std::size_t i_;
    };
    iterator begin() const { return iterator(this, 0); }
    iterator end()   const { return iterator(this, size()); }

    // Re-emit canonical VIBE from this node down.
    std::string emit() const {
        char* s = v_ ? vibe_emit(v_) : nullptr;
        std::string out = s ? s : "";
        if (s) vibe_free(s);
        return out;
    }

    VibeValue* raw() const noexcept { return v_; }

private:
    std::shared_ptr<VibeValue> doc_;
    VibeValue* v_ = nullptr;
};

// A whole parsed document: a node that owns the underlying tree.
class document : public node {
public:
    explicit document(std::shared_ptr<VibeValue> root)
        : node(root, root.get()) {}
};

// Parse VIBE text into a document, throwing vibe::parse_error on failure.
inline document parse(std::string_view text) {
    VibeError err;
    VibeValue* root = vibe_parse(text.data(), text.size(), &err);
    if (!root) {
        std::string msg = err.message ? err.message : "parse error";
        int line = err.line, column = err.column;
        VibeErrorCode code = err.code;
        vibe_error_free(&err);
        throw parse_error(std::move(msg), line, column, code);
    }
    auto owner = std::shared_ptr<VibeValue>(root, [](VibeValue* v) { vibe_value_free(v); });
    return document(owner);
}

inline std::string version() { return vibe_version(); }

// The literal that makes VIBE part of the C++ grammar.
namespace literals {
inline document operator""_vibe(const char* s, std::size_t n) {
    return parse(std::string_view(s, n));
}
} // namespace literals

} // namespace vibe

#endif // VIBE_HPP
