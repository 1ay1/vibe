#!/usr/bin/env bash
# Build + run every NATIVE extension module (not the FFI bindings — those live
# one level up and are driven by bindings/run_all.sh). Each target compiles a
# real per-runtime native extension against libvibe.a and asserts sample.vibe.
#
#   ./run_native.sh            # all
#   ./run_native.sh python zig # subset
#
# A target that PASSES prints "ALL OK (...)". A missing toolchain SKIPs (not a
# failure). A build/run error FAILs.
set -uo pipefail
cd "$(dirname "$0")"
ROOT=$(cd ../.. && pwd)

# libvibe.a is required to link every native module.
if [[ ! -f "$ROOT/libvibe.a" ]]; then
    echo "libvibe.a missing — building it..."
    make -C "$ROOT" >/dev/null 2>&1 || { echo "FATAL: cannot build libvibe.a"; exit 1; }
fi

ALL=(python node ruby java zig rust)
WANT=("$@")
[[ ${#WANT[@]} -eq 0 ]] && WANT=("${ALL[@]}")

have() { command -v "$1" >/dev/null 2>&1; }

pass=0 skip=0 fail=0
declare -A RESULT

run_one() {
    local name="$1"
    case "$name" in
      python)
        have python3 || { RESULT[$name]="SKIP (no python3)"; return 2; }
        ( cd python && python3 setup.py build_ext --inplace >/dev/null 2>&1 && python3 test.py ) ;;
      node)
        have node && have node-gyp || { RESULT[$name]="SKIP (no node/node-gyp)"; return 2; }
        ( cd node && node-gyp configure build >/dev/null 2>&1 && node test.mjs ) ;;
      ruby)
        have ruby || { RESULT[$name]="SKIP (no ruby)"; return 2; }
        ( cd ruby && ruby extconf.rb >/dev/null 2>&1 && make >/dev/null 2>&1 && ruby test.rb ) ;;
      java)
        have javac && have java || { RESULT[$name]="SKIP (no jdk)"; return 2; }
        ( cd java && ./build.sh 2>/dev/null ) ;;
      zig)
        have zig || { RESULT[$name]="SKIP (no zig)"; return 2; }
        ( cd zig && zig build run 2>/dev/null ) ;;
      rust)
        have cargo || { RESULT[$name]="SKIP (no cargo)"; return 2; }
        ( cd rust && cargo run --quiet --bin vibe-test 2>/dev/null ) ;;
      *)
        RESULT[$name]="SKIP (unknown)"; return 2 ;;
    esac
}

for name in "${WANT[@]}"; do
    printf '==> %-8s ' "$name"
    out=$(run_one "$name")
    rc=$?
    if [[ $rc -eq 2 ]]; then
        echo "${RESULT[$name]}"; ((skip++))
    elif [[ $rc -eq 0 && "$out" == *"ALL OK"* ]]; then
        echo "PASS  ${out##*ALL OK }"; RESULT[$name]="PASS"; ((pass++))
    else
        echo "FAIL"; [[ -n "$out" ]] && echo "$out" | sed 's/^/      /'
        RESULT[$name]="FAIL"; ((fail++))
    fi
done

echo
echo "native extensions: $pass passed, $skip skipped, $fail failed"
[[ $fail -eq 0 ]]
