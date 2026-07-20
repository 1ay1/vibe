# libvibe bindings — VIBE in (almost) every language

VIBE's reference parser is a small C library, **libvibe**, with a stable C ABI.
That means *any* language with a foreign-function interface can read and write
VIBE without re-implementing the parser. This directory has a real, runnable
binding for each — **25 languages**, 24 of them verified end-to-end on the build
machine by actually executing them against `libvibe.so`.

Every binding parses the same [`sample.vibe`](sample.vibe) and asserts the same
values, so the bindings cross-check each other. Run the whole matrix:

```console
$ ./run_all.sh
================= libvibe bindings =================
  PASS  python
  PASS  ruby
  PASS  luajit
  ... (one line per language) ...
  PASS  wasm
  SKIP  tcl     (no Tcl cffi package)
----------------------------------------------------
  24 passed, 1 skipped, 0 failed
====================================================
```

`run_all.sh <lang> ...` runs a subset; missing toolchains are **skipped**, never
failed.

## Status

| Language | Mechanism | Status |
|----------|-----------|:------:|
| C | it *is* the library (`#include "vibe.h"`) | ✅ |
| C++ | `extern "C"` header, RAII wrapper | ✅ |
| Rust | raw `extern "C"` (no crates) | ✅ |
| Go | cgo | ✅ |
| Zig | `@cImport("vibe.h")` | ✅ |
| Swift | C module map | ✅ |
| D | `extern(C)` | ✅ |
| Nim | `importc` | ✅ |
| Crystal | `lib` binding | ✅ |
| Java | Foreign Function & Memory API (JEP 454) | ✅ |
| Kotlin | JVM FFM via `invokeWithArguments` | ✅ |
| C# / .NET | P/Invoke (`DllImport`) | ✅ |
| Python | `ctypes` (stdlib) | ✅ |
| Ruby | `fiddle` (stdlib) | ✅ |
| Lua | LuaJIT `ffi` | ✅ |
| Perl | `FFI::Platypus` | ✅ |
| PHP | `FFI` extension | ✅ |
| Node.js | `koffi` | ✅ |
| JavaScript (browser/Deno/Bun) | WebAssembly (Emscripten) | ✅ |
| Julia | `ccall` (stdlib) | ✅ |
| Haskell | `foreign import ccall` | ✅ |
| OCaml | `ctypes` + `ctypes-foreign` | ✅ |
| Racket | `ffi/unsafe` (stdlib) | ✅ |
| Guile | `(system foreign)` (stdlib) | ✅ |
| CHICKEN Scheme | `foreign-lambda` | ✅ |
| Tcl | `cffi` | 📄 source only¹ |

¹ The Tcl `cffi` package isn't in any distro repo or the AUR; the binding is
written to its documented API but wasn't executed here.

## The C ABI every binding targets

All bindings use the same pointer-and-primitive subset of `vibe.h` (no
struct-by-value returns, so it maps cleanly onto every FFI):

```c
const char* vibe_version(void);
/* Stateless parse. Returns NULL on failure; out_err may itself be NULL. */
VibeValue*  vibe_parse(const char* data, size_t len, VibeError* out_err);

const char* vibe_get_string(VibeValue*, const char* path);  /* dotted path */
int64_t     vibe_get_int   (VibeValue*, const char* path);
double      vibe_get_float (VibeValue*, const char* path);
bool        vibe_get_bool  (VibeValue*, const char* path);
VibeArray*  vibe_get_array (VibeValue*, const char* path);
size_t      vibe_array_size(const VibeArray*);

char*       vibe_emit(const VibeValue*);   /* canonical VIBE text; vibe_free it */
void        vibe_free(void*);
void        vibe_value_free(VibeValue*);
void        vibe_error_free(VibeError*);
const char* vibe_error_code_string(int code);
```

`VibeError` is the only struct — passed **by pointer** to `vibe_parse`. On
LP64 it is 24 bytes:

```c
struct VibeError {
    bool        has_error;  /* offset 0  (+3 pad) */
    int         code;       /* offset 4           */
    const char* message;    /* offset 8           */
    int         line;       /* offset 16          */
    int         column;     /* offset 20          */
};
```

Two things worth knowing, both consequences of VIBE's design:

- **Paths are dotted** (`server.host`). Arrays hold *scalars only* (the First
  Law), so there is no `ports.0` path syntax — get the array with
  `vibe_get_array` and read its length with `vibe_array_size`.
- Parsing **fails closed**: a bad document returns `NULL` with a stable
  `code` (e.g. `unclosed-object`), never a partial tree.

## Building the library

From the repo root:

```console
$ make                 # builds libvibe.so / .dylib + the vibe CLI
```

Then point a binding at it (each binding also has a header comment with its
exact command):

```console
$ VIBE_LIB=$PWD/libvibe.so python3 bindings/python/vibe.py
$ LD_LIBRARY_PATH=$PWD  rustc bindings/rust/vibe.rs -L $PWD -o /tmp/v && /tmp/v
```

`VIBE_LIB` overrides the library path; `VIBE_SAMPLE` overrides the test
document. Compiled bindings that link `-lvibe` use `LD_LIBRARY_PATH` (or an
rpath) to find it at runtime.

## Adding your language

1. Load `libvibe` (dlopen/link) and declare the ~12 functions above.
2. Mirror `VibeError` as a 24-byte struct, pass it by pointer to `vibe_parse`.
3. Copy the assertions from any existing binding — they all check the same
   `sample.vibe`.
4. Add one line to `run_all.sh`.

If your language speaks C, it can speak VIBE.
