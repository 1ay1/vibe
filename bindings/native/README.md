# Native extension modules for libvibe

These are **true per-runtime native extensions**, not FFI bindings. Each one is
compiled native code that `#include`s the real [`vibe.h`](../../vibe.h) and links
[`libvibe.a`](../../libvibe.a) at build time, then plugs into its runtime's own
C extension ABI. There is no `dlopen`, no ctypes/koffi/fiddle marshalling layer,
and no separate `.so` loaded at runtime — the library is part of the module.

For the FFI bindings (25 languages, runtime `dlopen` of `libvibe.so`), see
[`../`](../).

## Status — all verified end-to-end

Each target parses [`../sample.vibe`](../sample.vibe), asserts identical values,
and prints `ALL OK (...)`.

| Runtime | Mechanism | Extension ABI | Loads as |
|---------|-----------|---------------|----------|
| **CPython** | C-API extension | `Python.h` / `PyModuleDef` | `import vibe` |
| **Node.js** | N-API addon | `node_api.h` / node-gyp | `require('.../vibe.node')` |
| **Ruby** | C-extension | `ruby.h` / `mkmf` | `require "vibe"` |
| **Java** | JNI | `jni.h` + `native` methods | `System.loadLibrary` |
| **Zig** | native module | `@cImport("vibe.h")` + `addObjectFile` | `@import("vibe.zig")` |
| **Rust** | `-sys` crate | `extern "C"` + `build.rs` static link | `use vibe_sys` |

## Run everything

```sh
./run_native.sh            # build + test all six
./run_native.sh python zig # a subset
```

Missing toolchains SKIP (not fail). Current box: **6 passed, 0 skipped, 0 failed**.

## Build individually

| Runtime | Build + test |
|---------|--------------|
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
