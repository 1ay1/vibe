// libvibe — WebAssembly binding test. The SAME vibe.wasm runs in Node, Deno,
// Bun, and the browser with no native module. This harness runs it in Node.
//
//   node test.mjs

import createModule from "./vibe.mjs";
import { readFileSync } from "node:fs";
import process from "node:process";

const M = await createModule();

const version = M.cwrap("vibe_version", "number", []);
const getString = M.cwrap("vibe_get_string", "number", ["number", "string"]);
const getInt = M.cwrap("vibe_get_int", "number", ["number", "string"]);
const getFloat = M.cwrap("vibe_get_float", "number", ["number", "string"]);
const getBool = M.cwrap("vibe_get_bool", "boolean", ["number", "string"]);
const getArray = M.cwrap("vibe_get_array", "number", ["number", "string"]);
const arraySize = M.cwrap("vibe_array_size", "number", ["number"]);
const emit = M.cwrap("vibe_emit", "number", ["number"]);
const valueFree = M.cwrap("vibe_value_free", null, ["number"]);
const freeMem = M.cwrap("vibe_free", null, ["number"]);

function parse(bytes) {
  const ptr = M._malloc(bytes.length);
  M.HEAPU8.set(bytes, ptr);
  const v = M.ccall("vibe_parse", "number",
    ["number", "number", "number"], [ptr, bytes.length, 0]);
  M._free(ptr);
  return v;
}

const sample = process.env.VIBE_SAMPLE || "../sample.vibe";
const v = parse(readFileSync(sample));
if (!v) { console.log("FAILED (wasm): parse error"); process.exit(1); }

let ok = true;
const check = (name, got, want) => {
  const pass = got === want;
  if (!pass) ok = false;
  console.log(`  [${pass ? "ok " : "BAD"}] ${name} = ${JSON.stringify(got)}`);
};

check("version", M.UTF8ToString(version()), "1.1.0");
check("name", M.UTF8ToString(getString(v, "name")), "libvibe");
check("answer", Number(getInt(v, "answer")), 42);
check("pi", Math.round(getFloat(v, "pi") * 100000) / 100000, 3.14159);
check("enabled", getBool(v, "enabled"), true);
check("server.host", M.UTF8ToString(getString(v, "server.host")), "localhost");
check("server.port", Number(getInt(v, "server.port")), 8080);
const arr = getArray(v, "ports");
check("len(ports)", arr ? Number(arraySize(arr)) : 0, 3);

const raw = emit(v);
const emitted = raw ? M.UTF8ToString(raw) : "";
if (emitted.includes("libvibe")) console.log("  [ok ] emit() round-trips");
else { ok = false; console.log("  [BAD] emit() did not round-trip"); }
if (raw) freeMem(raw);

const bad = parse(Buffer.from("name {"));
if (!bad) console.log("  [ok ] rejects malformed input");
else { ok = false; console.log("  [BAD] malformed input did not raise"); }

valueFree(v);
console.log(ok ? "ALL OK (wasm)" : "FAILED (wasm)");
process.exit(ok ? 0 : 1);
