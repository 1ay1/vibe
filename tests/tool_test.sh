#!/usr/bin/env bash
# Smoke test for vibe-tool: exercises every subcommand and asserts output +
# exit codes. Run from the repo root (make tool-test) or directly.
set -uo pipefail
cd "$(dirname "$0")/.."

TOOL=./vibe-tool
[[ -x "$TOOL" ]] || { echo "building vibe-tool..."; make "$TOOL" >/dev/null 2>&1 || { echo "FATAL: build failed"; exit 1; }; }

pass=0 fail=0
ok()   { pass=$((pass+1)); }
bad()  { fail=$((fail+1)); echo "  FAIL: $1"; }
check(){ if [[ "$2" == *"$3"* ]]; then ok; else bad "$1 (got: $2)"; fi; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/a.vibe" <<'EOF'
name    svc
port    8080
tls     true
tags    [ x y z ]
db { host localhost  port 5432 }
EOF
cat > "$TMP/b.vibe" <<'EOF'
name    svc
port    9090
tls     true
tags    [ x y ]
db { host remote  port 5432 }
region  us
EOF

# version
check "version" "$($TOOL version)" "vibe-tool"

# json pretty + compact
check "json"          "$($TOOL json "$TMP/a.vibe")"           '"port": 8080'
check "json-compact"  "$($TOOL json "$TMP/a.vibe" --compact)" '"port":8080'

# json round-trip (VIBE -> JSON -> VIBE)
rt=$($TOOL json "$TMP/a.vibe" --compact | $TOOL json - --from-json | $TOOL json - --compact 2>/dev/null || true)
# re-parse the emitted VIBE back to JSON to compare structurally
check "json-roundtrip" "$rt" '"port":8080'

# tree
check "tree"     "$($TOOL tree --no-color "$TMP/a.vibe")" "db {2}"
check "tree-arr" "$($TOOL tree --no-color "$TMP/a.vibe")" "[0] = x"

# stats
check "stats-depth"  "$($TOOL stats --no-color "$TMP/a.vibe")" "nesting depth : 2"
check "stats-types"  "$($TOOL stats --no-color "$TMP/a.vibe")" "value types:"

# select exact + wildcard
check "select-exact" "$($TOOL select --no-color "$TMP/a.vibe" db.port)" "db.port = 5432"
check "select-wild"  "$($TOOL select --no-color "$TMP/a.vibe" 'db.*')"  "db.host = localhost"
check "select-arr"   "$($TOOL select --no-color "$TMP/a.vibe" 'tags.*')" "tags[2] = z"

# diff: exit 4 when different, 0 when equal
$TOOL diff --no-color "$TMP/a.vibe" "$TMP/b.vibe" >/dev/null; [[ $? -eq 4 ]] && ok || bad "diff-exit-different"
$TOOL diff --no-color "$TMP/a.vibe" "$TMP/a.vibe" >/dev/null; [[ $? -eq 0 ]] && ok || bad "diff-exit-equal"
check "diff-change" "$($TOOL diff --no-color "$TMP/a.vibe" "$TMP/b.vibe")" "region"

# stdin support
check "stdin" "$(echo 'k 1' | $TOOL json - --compact)" '"k":1'

# malformed input is rejected (exit 1)
echo 'bad {' | $TOOL tree - >/dev/null 2>&1; [[ $? -eq 1 ]] && ok || bad "reject-malformed"

echo
if [[ $fail -eq 0 ]]; then
    echo "vibe-tool: $pass passed, 0 failed  ✓"
    exit 0
else
    echo "vibe-tool: $pass passed, $fail FAILED"
    exit 1
fi
