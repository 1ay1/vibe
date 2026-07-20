#!/usr/bin/env bash
# Build the WebAssembly module from the reference C source with Emscripten.
# Produces vibe.wasm + vibe.mjs (ES module loader) that run in Node, Deno,
# Bun, and the browser. Requires `emcc` on PATH (pacman: emscripten; the
# binary lives in /usr/lib/emscripten on Arch).
set -eu
cd "$(dirname "$0")"
ROOT="$(cd ../.. && pwd)"

emcc "$ROOT/vibe.c" -O2 -I"$ROOT" -o vibe.mjs \
  -sEXPORTED_FUNCTIONS=_vibe_version,_vibe_parse,_vibe_get_string,_vibe_get_int,_vibe_get_float,_vibe_get_bool,_vibe_get_array,_vibe_array_size,_vibe_emit,_vibe_free,_vibe_value_free,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPU8 \
  -sMODULARIZE -sEXPORT_ES6 -sENVIRONMENT=node,web -sALLOW_MEMORY_GROWTH

echo "built vibe.wasm + vibe.mjs"
