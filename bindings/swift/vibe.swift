// libvibe — Swift binding via C interop. `module.modulemap` exposes vibe.h as
// the CVibe module, so Swift calls the C ABI with no glue.
//
//   swiftc -I bindings/swift vibe.swift -L <dir> -lvibe -o vibe_swift
//   LD_LIBRARY_PATH=<dir> ./vibe_swift

import CVibe
import Foundation

func cstr(_ p: UnsafePointer<CChar>?) -> String {
    p == nil ? "" : String(cString: p!)
}

let sample = ProcessInfo.processInfo.environment["VIBE_SAMPLE"] ?? "../sample.vibe"
let data = FileManager.default.contents(atPath: sample) ?? Data()

var err = VibeError()
let v: UnsafeMutablePointer<VibeValue>? = data.withUnsafeBytes { raw in
    vibe_parse(raw.bindMemory(to: CChar.self).baseAddress, data.count, &err)
}
if v == nil {
    print("FAILED (swift): parse error")
    exit(1)
}

var ok = true
func check<T: Equatable>(_ name: String, _ got: T, _ want: T) {
    let pass = got == want
    if !pass { ok = false }
    print("  [\(pass ? "ok " : "BAD")] \(name) = \(got)")
}

check("version", cstr(vibe_version()), "1.2.0")
check("name", cstr(vibe_get_string(v, "name")), "libvibe")
check("answer", vibe_get_int(v, "answer"), 42)
check("pi", (vibe_get_float(v, "pi") * 100000).rounded() / 100000, 3.14159)
check("enabled", vibe_get_bool(v, "enabled"), true)
check("server.host", cstr(vibe_get_string(v, "server.host")), "localhost")
check("server.port", vibe_get_int(v, "server.port"), 8080)
let arr = vibe_get_array(v, "ports")
check("len(ports)", arr != nil ? vibe_array_size(arr) : 0, 3)

let raw = vibe_emit(v)
let emitted = raw != nil ? String(cString: raw!) : ""
if emitted.contains("libvibe") {
    print("  [ok ] emit() round-trips")
} else {
    ok = false
    print("  [BAD] emit() did not round-trip")
}
if raw != nil { vibe_free(raw) }

var err2 = VibeError()
let bad = "name {".withCString { vibe_parse($0, 6, &err2) }
if bad == nil {
    print("  [ok ] rejects malformed input")
} else {
    ok = false
    print("  [BAD] malformed input did not raise")
}

vibe_value_free(v)
print(ok ? "ALL OK (swift)" : "FAILED (swift)")
exit(ok ? 0 : 1)
