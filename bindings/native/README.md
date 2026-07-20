# Native extension modules for libvibe

These are **true per-runtime native extensions**, not FFI bindings. Each one is
compiled native code that `#include`s the real [`vibe.h`](../../vibe.h) and links
[`libvibe.a`](../../libvibe.a) at build time, then plugs into its runtime's own
C extension ABI. There is no `dlopen`, no ctypes/koffi/fiddle marshalling layer,
and no separate `.so` loaded at runtime — the library is part of the module.

For the FFI bindings (25 languages, runtime `dlopen` of `libvibe.so`), see
[`../`](../).

## VIBE as native syntax

These go beyond "call `parse("...")`": each one makes a VIBE document a
first-class construct in the host language, so VIBE reads as part of the
grammar. You write the document **inline** and get back live, typed, navigable
data.

| Runtime | Native-syntax form | Example |
|---------|--------------------|---------|
| **C++** | `_vibe` user-defined literal (a real operator) | `auto c = R"(port 8080)"_vibe; int p = c["port"];` |
| **Rust** | `vibe! { ... }` macro (compiles at the call site) | `let c = vibe!{r#"port 8080"#}; c.get_int("port")` |
| **Node** | `` vibe`...` `` tagged template (with `${}` interpolation) | `` const c = vibe`port ${p}`; c.port `` |
| **Python** | callable module **+** `# coding: vibe` source codec | `cfg = vibe("""port 8080"""); cfg.port` |
| **Ruby** | `VIBE(<<~V ... V)` heredoc DSL | `c = VIBE(<<~V)\nport 8080\nV\nc.port` |
| **Java** | text block `Vibe.of("""...""")` + fluent navigator | `Vibe.of("""port 8080""").get("port").asInt()` |

Interpolation (`${}` in Node) is serialized to VIBE and **escaped**, so nothing a
caller splices in can break out into structure — no injection. Malformed inline
documents raise the runtime's native error type at evaluation.

## Status — all verified end-to-end

Each target parses [`../sample.vibe`](../sample.vibe), asserts identical values,
and prints `ALL OK (...)`.

| Runtime | Mechanism | Extension ABI | Loads as |
|---------|-----------|---------------|----------|
| **C++** | header-only UDL | `#include "vibe.hpp"` (links libvibe.a) | `"..."_vibe` |
| **CPython** | C-API extension | `Python.h` / `PyModuleDef` | `import vibe` |
| **Node.js** | N-API addon | `node_api.h` / node-gyp | `require('.../vibe.node')` |
| **Ruby** | C-extension | `ruby.h` / `mkmf` | `require "vibe"` |
| **Java** | JNI | `jni.h` + `native` methods | `System.loadLibrary` |
| **Zig** | native module | `@cImport("vibe.h")` + `addObjectFile` | `@import("vibe.zig")` |
| **Rust** | `-sys` crate | `extern "C"` + `build.rs` static link | `use vibe_sys` |

## Run everything

```sh
./run_native.sh            # build + test all seven
./run_native.sh python zig # a subset
```

Missing toolchains SKIP (not fail). Current box: **7 passed, 0 skipped, 0 failed**.

## Build individually

| Runtime | Build + test |
|---------|--------------|
| C++     | `cd cpp && c++ -std=c++20 -I../../.. test.cpp ../../../libvibe.a -o test && ./test` |
| CPython | `cd python && python3 setup.py build_ext --inplace && python3 test.py` |
| Node    | `cd node && node-gyp configure build && node test.mjs` |
| Ruby    | `cd ruby && ruby extconf.rb && make && ruby test.rb` |
| Java    | `cd java && ./build.sh` |
| Zig     | `cd zig && zig build test` (or `zig build run`) |
| Rust    | `cd rust && cargo test` (or `cargo run --bin vibe-test`) |

## The C ABI they all wrap

The same canonical subset used by the FFI bindings (see [`../README.md`](../README.md)):
`vibe_version`, `vibe_parse(data, len, &err)`, `vibe_get_{string,int,float,bool}`,
`vibe_get_array` + `vibe_array_size`, `vibe_emit` (+ `vibe_free`), `vibe_value_free`,
`vibe_error_code_string`. `VibeError` is the only struct, always passed by pointer.

## FFI binding vs native extension — why both?

- **FFI binding** (`../`): zero compilation of C — the runtime loads `libvibe.so`
  dynamically and describes the ABI in the host language. Portable, quick, but the
  call goes through a generic FFI trampoline.
- **Native extension** (here): a runtime-specific C module compiled against that
  runtime's headers. Direct calls, native type marshalling, idiomatic objects
  (`vibe.Doc`, `Vibe::Doc`, a Rust `Doc` that frees on `Drop`), and it ships as
  part of the extension rather than depending on a separately-installed `.so`.
