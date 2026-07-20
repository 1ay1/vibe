// libvibe — Node.js binding via koffi (a maintained FFI for Node).
//
//   npm install koffi
//   VIBE_LIB=/path/libvibe.so node vibe.mjs
//
// For the browser / Deno / Bun, see ../wasm for a WebAssembly build that needs
// no native module at all.

import koffi from "koffi";
import { readFileSync } from "node:fs";
import process from "node:process";

const libPath = process.env.VIBE_LIB || "../../libvibe.so";
const lib = koffi.load(libPath);

const vibe_version = lib.func("const char *vibe_version()");
const vibe_parse = lib.func("void *vibe_parse(const char *data, size_t len, void *err)");
const vibe_get_string = lib.func("const char *vibe_get_string(void *v, const char *path)");
const vibe_get_int = lib.func("int64_t vibe_get_int(void *v, const char *path)");
const vibe_get_float = lib.func("double vibe_get_float(void *v, const char *path)");
const vibe_get_bool = lib.func("bool vibe_get_bool(void *v, const char *path)");
const vibe_get_array = lib.func("void *vibe_get_array(void *v, const char *path)");
const vibe_array_size = lib.func("size_t vibe_array_size(void *a)");
const vibe_emit = lib.func("void *vibe_emit(void *v)");
const vibe_free = lib.func("void vibe_free(void *p)");
const vibe_value_free = lib.func("void vibe_value_free(void *v)");

const sample = process.env.VIBE_SAMPLE || "../sample.vibe";
const data = readFileSync(sample);

const v = vibe_parse(data, data.length, null);
if (!v) {
  console.log("FAILED (node): parse error");
  process.exit(1);
}

let ok = true;
const check = (name, got, want) => {
  const pass = got === want;
  if (!pass) ok = false;
  console.log(`  [${pass ? "ok " : "BAD"}] ${name} = ${JSON.stringify(got)}`);
};

check("version", vibe_version(), "1.2.0");
check("name", vibe_get_string(v, "name"), "libvibe");
check("answer", Number(vibe_get_int(v, "answer")), 42);
check("pi", Math.round(vibe_get_float(v, "pi") * 100000) / 100000, 3.14159);
check("enabled", vibe_get_bool(v, "enabled"), true);
check("server.host", vibe_get_string(v, "server.host"), "localhost");
check("server.port", Number(vibe_get_int(v, "server.port")), 8080);
const arr = vibe_get_array(v, "ports");
check("len(ports)", arr ? Number(vibe_array_size(arr)) : 0, 3);

const raw = vibe_emit(v);
const emitted = raw ? koffi.decode(raw, "char", -1) : "";
if (emitted.includes("libvibe")) console.log("  [ok ] emit() round-trips");
else { ok = false; console.log("  [BAD] emit() did not round-trip"); }
if (raw) vibe_free(raw);

const bad = vibe_parse(Buffer.from("name {"), 6, null);
if (!bad) console.log("  [ok ] rejects malformed input");
else { ok = false; console.log("  [BAD] malformed input did not raise"); }

vibe_value_free(v);
console.log(ok ? "ALL OK (node)" : "FAILED (node)");
process.exit(ok ? 0 : 1);
