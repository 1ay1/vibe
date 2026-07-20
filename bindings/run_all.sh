#!/usr/bin/env bash
# Build and run every libvibe language binding against the shared library,
# then print a pass/skip/fail table. A binding "passes" when it prints
# "ALL OK (<lang>)" and exits 0. Toolchains that aren't installed are SKIPPED,
# never failed — so this is safe to run anywhere.
#
#   ./run_all.sh            # run all
#   ./run_all.sh python go  # run a subset

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Build libvibe if needed.
if [ ! -e "$ROOT/libvibe.so" ]; then
  echo "Building libvibe ..."
  make -C "$ROOT" >/dev/null
fi

export VIBE_LIB="$ROOT/libvibe.so"
export VIBE_SAMPLE="$HERE/sample.vibe"
export LD_LIBRARY_PATH="$ROOT${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export CRYSTAL_LIBRARY_PATH="$ROOT"
export GUILE_AUTO_COMPILE=0

PASS=0; SKIP=0; FAIL=0
declare -a RESULTS

have() { command -v "$1" >/dev/null 2>&1; }

# run <label> <toolchain-binary> <shell-command>
run() {
  local label="$1" tool="$2" cmd="$3"
  if ! have "$tool"; then
    RESULTS+=("SKIP  $label  (no $tool)"); SKIP=$((SKIP+1)); return
  fi
  local out
  if out="$(bash -c "$cmd" 2>&1)" && printf '%s' "$out" | grep -q "ALL OK ("; then
    RESULTS+=("PASS  $label"); PASS=$((PASS+1))
  else
    RESULTS+=("FAIL  $label"); FAIL=$((FAIL+1))
    printf '%s\n' "$out" | sed 's/^/        /' | tail -6
  fi
}

SEL_ALL=("$@")
sel() { # sel <label> -> should we run it?
  [ "${#SEL_ALL[@]}" -eq 0 ] && return 0
  for a in "${SEL_ALL[@]}"; do [ "$a" = "$1" ] && return 0; done
  return 1
}

cd "$HERE"

sel python  && run "python "  python3 "cd python && python3 vibe.py"
sel ruby    && run "ruby   "  ruby    "cd ruby && ruby vibe.rb"
sel luajit  && run "luajit "  luajit  "cd lua && luajit vibe.lua"
sel julia   && run "julia  "  julia   "cd julia && julia vibe.jl"
sel perl    && run "perl   "  perl    "cd perl && perl vibe.pl"
sel php     && run "php    "  php     "cd php && php -d ffi.enable=1 -d extension=ffi vibe.php"
sel node    && run "node   "  node    "cd node && node vibe.mjs"
sel racket  && run "racket "  racket  "cd racket && racket vibe.rkt"
sel guile   && run "guile  "  guile   "cd guile && guile vibe.scm"

sel rust    && run "rust   "  rustc   "cd rust && rustc --edition 2021 vibe.rs -L '$ROOT' -o '$TMP/vibe_rs' && '$TMP/vibe_rs'"
sel cpp     && run "cpp    "  g++     "cd cpp && g++ -std=c++17 -I'$ROOT' vibe.cpp -L'$ROOT' -lvibe -o '$TMP/vibe_cpp' && '$TMP/vibe_cpp'"
sel go      && run "go     "  go      "cd go && go run vibe.go"
sel nim     && run "nim    "  nim     "cd nim && nim c -r --hints:off --passL:'-L$ROOT' --passL:'-Wl,-rpath,$ROOT' -o:'$TMP/vibe_nim' vibe.nim"
sel d       && run "d      "  ldc2    "cd d && ldc2 vibe.d -L-L'$ROOT' -L-lvibe -of='$TMP/vibe_d' && '$TMP/vibe_d'"
sel crystal && run "crystal"  crystal "cd crystal && crystal run vibe.cr"
sel zig     && run "zig    "  zig     "cd zig && zig run vibe.zig -I'$ROOT' -L'$ROOT' -lvibe -lc"
sel swift   && run "swift  "  swiftc  "cd swift && swiftc -I . vibe.swift -L'$ROOT' -lvibe -o '$TMP/vibe_swift' && '$TMP/vibe_swift'"
sel ocaml   && run "ocaml  "  ocamlfind "cd ocaml && ocamlfind ocamlopt -package ctypes,ctypes.foreign -linkpkg vibe.ml -o '$TMP/vibe_ml' 2>/dev/null && '$TMP/vibe_ml'"
sel haskell && run "haskell"  ghc     "cd haskell && ghc -v0 -O0 vibe.hs -L'$ROOT' -lvibe -o '$TMP/vibe_hs' && '$TMP/vibe_hs'"
sel chicken && run "chicken"  chicken-csc "cd chicken && chicken-csc -C -I'$ROOT' -L '-L$ROOT -lvibe' vibe.scm -o '$TMP/vibe_chicken' && '$TMP/vibe_chicken'"

sel java    && run "java   "  java    "cd java && java --enable-native-access=ALL-UNNAMED Vibe.java"
sel csharp  && run "csharp "  dotnet  "cd csharp && dotnet run vibe.cs 2>/dev/null"
sel kotlin  && run "kotlin "  kotlinc "cd kotlin && kotlinc VibeKt.kt -include-runtime -d '$TMP/vibe.jar' 2>/dev/null && java --enable-native-access=ALL-UNNAMED -jar '$TMP/vibe.jar'"

# WASM: build once with emscripten if the artifact is missing.
if sel wasm; then
  if [ ! -e "$HERE/wasm/vibe.wasm" ] && have emcc; then
    (cd "$HERE/wasm" && make >/dev/null 2>&1 || bash ./build.sh >/dev/null 2>&1) || true
  fi
  if [ -e "$HERE/wasm/vibe.mjs" ]; then
    run "wasm   " node "cd wasm && node test.mjs"
  else
    RESULTS+=("SKIP  wasm    (no emcc / not built)"); SKIP=$((SKIP+1))
  fi
fi

if sel tcl; then
  if have tclsh && echo 'if {[catch {package require cffi}]} {exit 1}' | tclsh >/dev/null 2>&1; then
    run "tcl    " tclsh "cd tcl && tclsh vibe.tcl"
  else
    RESULTS+=("SKIP  tcl     (no Tcl cffi package)"); SKIP=$((SKIP+1))
  fi
fi

echo
echo "================= libvibe bindings ================="
for r in "${RESULTS[@]}"; do echo "  $r"; done
echo "----------------------------------------------------"
printf "  %d passed, %d skipped, %d failed\n" "$PASS" "$SKIP" "$FAIL"
echo "===================================================="
[ "$FAIL" -eq 0 ]
