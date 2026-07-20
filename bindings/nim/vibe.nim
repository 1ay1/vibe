# libvibe — Nim binding (importc against the shared library).
#
#   nim c -r --passL:"-L/path -lvibe" vibe.nim

import std/[os, math, strutils]

{.passl: "-lvibe".}

type
  VibeError = object
    has_error: bool
    code: cint
    message: cstring
    line: cint
    column: cint

proc vibe_version(): cstring {.importc, cdecl.}
proc vibe_parse(data: cstring, length: csize_t, err: ptr VibeError): pointer {.importc, cdecl.}
proc vibe_get_string(v: pointer, path: cstring): cstring {.importc, cdecl.}
proc vibe_get_int(v: pointer, path: cstring): int64 {.importc, cdecl.}
proc vibe_get_float(v: pointer, path: cstring): cdouble {.importc, cdecl.}
proc vibe_get_bool(v: pointer, path: cstring): bool {.importc, cdecl.}
proc vibe_get_array(v: pointer, path: cstring): pointer {.importc, cdecl.}
proc vibe_array_size(a: pointer): csize_t {.importc, cdecl.}
proc vibe_emit(v: pointer): cstring {.importc, cdecl.}
proc vibe_free(p: pointer) {.importc, cdecl.}
proc vibe_value_free(v: pointer) {.importc, cdecl.}
proc vibe_error_code_string(code: cint): cstring {.importc, cdecl.}

proc getStr(v: pointer, path: string): string =
  let p = vibe_get_string(v, path.cstring)
  if p.isNil: "" else: $p

when isMainModule:
  let sample = getEnv("VIBE_SAMPLE", "../sample.vibe")
  let data = readFile(sample)
  var err: VibeError
  let v = vibe_parse(data.cstring, data.len.csize_t, addr err)
  if v.isNil:
    echo "FAILED (nim): parse error"
    quit(1)

  var ok = true
  proc check(name: string, got, want: auto) =
    if got != want: ok = false
    echo "  [", (if got == want: "ok " else: "BAD"), "] ", name, " = ", got

  check("version", $vibe_version(), "1.2.0")
  check("name", getStr(v, "name"), "libvibe")
  check("answer", vibe_get_int(v, "answer"), 42'i64)
  check("pi", round(vibe_get_float(v, "pi") * 100000) / 100000, 3.14159)
  check("enabled", vibe_get_bool(v, "enabled"), true)
  check("server.host", getStr(v, "server.host"), "localhost")
  check("server.port", vibe_get_int(v, "server.port"), 8080'i64)
  let arr = vibe_get_array(v, "ports")
  check("len(ports)", (if arr.isNil: 0 else: vibe_array_size(arr).int), 3)

  let raw = vibe_emit(v)
  if not raw.isNil and ($raw).contains("libvibe"):
    echo "  [ok ] emit() round-trips"
    vibe_free(cast[pointer](raw))
  else:
    ok = false
    echo "  [BAD] emit() did not round-trip"

  var e2: VibeError
  if vibe_parse("name {".cstring, 6.csize_t, addr e2).isNil:
    echo "  [ok ] rejects malformed input"
  else:
    ok = false
    echo "  [BAD] malformed input did not raise"

  vibe_value_free(v)
  echo (if ok: "ALL OK (nim)" else: "FAILED (nim)")
  quit(if ok: 0 else: 1)
