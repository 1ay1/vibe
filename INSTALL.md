# Installation Guide

How to build, install, and use **libvibe** — the reference C library for the
VIBE configuration format — and the `vibe` command-line tool.

## Quick Start

```bash
git clone https://github.com/1ay1/vibe.git
cd vibe
make
```

`make` builds:

- `libvibe.a` — static library
- `libvibe.so.1.2.0` (with SONAME `libvibe.so.1`; `.dylib` on macOS) — shared library
- `vibe` — the command-line tool
- `vibe_example` — the example program

Run `make help` to list every target.

## Install System-Wide

```bash
sudo make install                 # PREFIX=/usr/local by default
# or choose a prefix:
sudo make install PREFIX=/usr
```

This installs:

| Path | What |
|------|------|
| `$PREFIX/include/vibe.h`             | the public header |
| `$PREFIX/lib/libvibe.a`              | static library |
| `$PREFIX/lib/libvibe.so*`            | shared library + SONAME symlinks |
| `$PREFIX/bin/vibe`                   | the CLI |
| `$PREFIX/lib/pkgconfig/vibe.pc`      | pkg-config metadata |

On Linux you may need `sudo ldconfig` afterward so the dynamic linker picks up
the new shared library. Remove everything with `sudo make uninstall`.

## Use It in Your Project

### Option 1 — link the installed library (recommended)

```bash
cc -std=c11 app.c $(pkg-config --cflags --libs vibe) -o app
```

`#include <vibe.h>` in your source. pkg-config supplies the include path and
`-lvibe`.

### Option 2 — vendor the two files

The entire parser and emitter is two files with no dependencies beyond the C
standard library. Copy them in and compile:

```bash
cp vibe.h vibe.c /path/to/your/project/
cc -std=c11 app.c vibe.c -o app       # #include "vibe.h"
```

> You do **not** need to define `_POSIX_C_SOURCE`; `vibe.c` sets what it needs
> internally.

### CMake

Vendored:

```cmake
add_library(vibe STATIC vibe.c)
target_include_directories(vibe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(myapp PRIVATE vibe)
```

Installed (via pkg-config):

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIBE REQUIRED vibe)
target_include_directories(myapp PRIVATE ${VIBE_INCLUDE_DIRS})
target_link_libraries(myapp PRIVATE ${VIBE_LIBRARIES})
```

### Meson

```meson
# vendored
vibe_lib = static_library('vibe', 'vibe.c')
vibe_dep = declare_dependency(link_with: vibe_lib, include_directories: '.')

# or installed
vibe_dep = dependency('vibe')
```

## The `vibe` CLI

```
vibe check <file>          validate; prints file:line:col: error [category]: message
vibe fmt   <file> [-w]     reformat to canonical VIBE (stdout, or -w in place)
vibe get   <file> <path>   print the scalar at a dotted path
vibe version               print library + format versions
```

```bash
vibe check config.vibe
vibe get   config.vibe server.port
vibe fmt   config.vibe -w
```

## Requirements

- A **C11** compiler (GCC 4.9+, Clang 3.1+, or MSVC 2015+).
- `make` and `ar` to build the libraries.
- No third-party dependencies. The interactive TUI (`make parser_tool`) is the
  one exception — it needs `ncurses`.

## Platform Notes

### Linux (Debian/Ubuntu)

```bash
sudo apt-get update && sudo apt-get install -y build-essential
git clone https://github.com/1ay1/vibe.git && cd vibe
make && make test-all
sudo make install && sudo ldconfig
```

### macOS

```bash
xcode-select --install         # provides clang + make
git clone https://github.com/1ay1/vibe.git && cd vibe
make && make test-all
sudo make install              # builds libvibe.1.2.0.dylib
```

### Windows

Use **MSYS2/MinGW** or **WSL** and follow the Linux instructions, or add
`vibe.c`/`vibe.h` directly to a Visual Studio C project (vendored build).

## Verify

```bash
make test-all        # example parse + 25 unit tests + conformance suite
vibe version         # if installed
```

Consume the installed shared library end-to-end:

```bash
printf 'port 8080\n' > /tmp/t.vibe
vibe get /tmp/t.vibe port      # -> 8080
```

## Troubleshooting

**`cannot find -lvibe` / library not found at runtime.** Run `sudo ldconfig`
(Linux), or point the linker/loader at your prefix:

```bash
cc app.c -I$PREFIX/include -L$PREFIX/lib -lvibe -o app
LD_LIBRARY_PATH=$PREFIX/lib ./app
```

Prefer `pkg-config --cflags --libs vibe`, which handles this for you (set
`PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig` for a non-standard prefix).

**Static linking** (no runtime dependency): link `libvibe.a` explicitly, e.g.
`cc app.c $PREFIX/lib/libvibe.a -o app`.

## Next Steps

- [API.md](docs/API.md) — the complete C API reference
- [README.md](README.md) — overview and quick examples
- [SPECIFICATION.md](SPECIFICATION.md) — the language, normatively
- [tests/conformance/](tests/conformance) — the language-neutral test suite

---

Keep calm and VIBE on! 🌊
