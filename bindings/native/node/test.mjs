// Smoke test for the NATIVE Node N-API addon (require the compiled .node).
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const here = dirname(fileURLToPath(import.meta.url));
const vibe = require(join(here, "build", "Release", "vibe.node"));

const sample = readFileSync(join(here, "..", "..", "sample.vibe"), "utf8");

const assert = (c, m) => { if (!c) { console.error("FAIL:", m); process.exit(1); } };

assert(vibe.version() === "1.1.0", "version");

const doc = vibe.parse(sample);
assert(vibe.getString(doc, "name") === "libvibe", "name");
assert(vibe.getInt(doc, "answer") === 42, "answer");
assert(Math.abs(vibe.getFloat(doc, "pi") - 3.14159) < 1e-9, "pi");
assert(vibe.getBool(doc, "enabled") === true, "enabled");
assert(vibe.getString(doc, "server.host") === "localhost", "host");
assert(vibe.getInt(doc, "server.port") === 8080, "port");
assert(vibe.arraySize(doc, "ports") === 3, "ports");
assert(vibe.emit(doc).includes("libvibe"), "emit");

let threw = false;
try { vibe.parse("name {"); } catch { threw = true; }
assert(threw, "expected throw on malformed input");

vibe.free(doc);
console.log("ALL OK (node native / N-API)");
