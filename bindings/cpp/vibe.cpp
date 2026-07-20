// libvibe — C++ binding. The C API is already C++-friendly (extern "C" in
// vibe.h), so a thin RAII wrapper is all you need.
//
//   g++ -std=c++17 -I../.. vibe.cpp -L../.. -lvibe -o vibe_cpp
//   LD_LIBRARY_PATH=../.. ./vibe_cpp

#include "vibe.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace vibe {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Doc {
public:
    explicit Doc(VibeValue* p) : ptr_(p) {}
    ~Doc() { vibe_value_free(ptr_); }
    Doc(const Doc&) = delete;
    Doc& operator=(const Doc&) = delete;

    std::string get_string(const char* path) const {
        const char* s = vibe_get_string(ptr_, path);
        return s ? std::string(s) : std::string();
    }
    int64_t get_int(const char* path) const { return vibe_get_int(ptr_, path); }
    double get_float(const char* path) const { return vibe_get_float(ptr_, path); }
    bool get_bool(const char* path) const { return vibe_get_bool(ptr_, path); }
    size_t array_size(const char* path) const {
        VibeArray* a = vibe_get_array(ptr_, path);
        return a ? vibe_array_size(a) : 0;
    }
    std::string emit() const {
        char* raw = vibe_emit(ptr_);
        if (!raw) return {};
        std::string out(raw);
        vibe_free(raw);
        return out;
    }

private:
    VibeValue* ptr_;
};

inline std::string version() { return vibe_version(); }

inline Doc parse(const std::string& data) {
    VibeError err;
    VibeValue* p = vibe_parse(data.data(), data.size(), &err);
    if (!p) {
        std::string code = vibe_error_code_string(err.code);
        std::ostringstream m;
        m << code << " at " << err.line << ":" << err.column;
        Error e(m.str());
        vibe_error_free(&err);
        throw e;
    }
    return Doc(p);
}

}  // namespace vibe

int main() {
    const char* env = std::getenv("VIBE_SAMPLE");
    std::string path = env ? env : "../sample.vibe";
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string data = ss.str();

    vibe::Doc doc = vibe::parse(data);
    bool ok = true;
    auto check = [&](const char* name, const auto& got, const auto& want) {
        bool pass = (got == want);
        if (!pass) ok = false;
        std::ostringstream v; v << got;
        std::printf("  [%s] %s = %s\n", pass ? "ok " : "BAD", name, v.str().c_str());
    };
    check("version", vibe::version(), std::string("1.2.0"));
    check("name", doc.get_string("name"), std::string("libvibe"));
    check("answer", doc.get_int("answer"), (int64_t)42);
    check("pi", std::round(doc.get_float("pi") * 100000) / 100000, 3.14159);
    check("enabled", (int)doc.get_bool("enabled"), 1);
    check("server.host", doc.get_string("server.host"), std::string("localhost"));
    check("server.port", doc.get_int("server.port"), (int64_t)8080);
    check("len(ports)", doc.array_size("ports"), (size_t)3);
    if (doc.emit().find("libvibe") != std::string::npos) {
        std::printf("  [ok ] emit() round-trips\n");
    } else {
        ok = false;
        std::printf("  [BAD] emit() did not round-trip\n");
    }
    try {
        vibe::parse("name {");
        ok = false;
        std::printf("  [BAD] malformed input did not raise\n");
    } catch (const vibe::Error&) {
        std::printf("  [ok ] rejects malformed input\n");
    }
    std::printf("%s\n", ok ? "ALL OK (cpp)" : "FAILED (cpp)");
    return ok ? 0 : 1;
}
