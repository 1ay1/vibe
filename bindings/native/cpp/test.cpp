// test.cpp — proves VIBE is native C++ syntax via the `_vibe` literal.
//
// Build: c++ -std=c++20 -I../../.. test.cpp ../../../libvibe.a -o test
//
// SPDX-License-Identifier: MIT
#include "vibe.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace vibe::literals;

int main() {
    // VIBE written inline, as a language literal — not a parse() call.
    auto cfg = R"(
        name    my-service
        port    8080
        tls     true
        ratio   0.75
        origins [ https://a.example  https://b.example ]
        db {
            host    localhost
            port    5432
        }
    )"_vibe;

    // Implicit typed conversions.
    std::string name = cfg["name"];
    int         port = cfg["port"];
    bool        tls  = cfg["tls"].as_bool();
    double      ratio = cfg["ratio"];

    assert(name == "my-service");
    assert(port == 8080);
    assert(tls == true);
    assert(ratio == 0.75);

    // Nested access two ways: chained subscript and dotted path.
    assert(cfg["db"]["host"].str() == "localhost");
    assert((int)cfg["db.port"] == 5432);

    // Arrays: size, index, and range-for.
    assert(cfg["origins"].size() == 2);
    assert(cfg["origins"][0].str() == "https://a.example");
    std::size_t seen = 0;
    for (auto origin : cfg["origins"]) {
        assert(origin.str().rfind("https://", 0) == 0);
        ++seen;
    }
    assert(seen == 2);

    // Missing keys are falsy and yield fallbacks, never crash.
    assert(!cfg["nope"]);
    assert(cfg["nope"].as_int(-1) == -1);

    // Round-trip: emit canonical VIBE and re-parse it.
    std::string emitted = cfg.emit();
    auto again = vibe::parse(emitted);
    assert(again["port"].as_int() == 8080);

    // Malformed input throws vibe::parse_error.
    bool threw = false;
    try {
        auto bad = R"(name {)"_vibe;
        (void)bad;
    } catch (const vibe::parse_error& e) {
        threw = true;
        assert(e.line() >= 1);
    }
    assert(threw);

    std::printf("ALL OK (C++ _vibe literal, libvibe %s)\n", vibe::version().c_str());
    return 0;
}
