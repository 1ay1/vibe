// Smoke test for the NATIVE Node N-API addon + the `vibe` tagged template.
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { createRequire } from "node:module";
import vibe, { version, parse } from "./vibe.mjs";

const require = createRequire(import.meta.url);
const here = dirname(fileURLToPath(import.meta.url));
const native = require(join(here, "build", "Release", "vibe.node"));

const sample = readFileSync(join(here, "..", "..", "sample.vibe"), "utf8");
const assert = (c, m) => { if (!c) { console.error("FAIL:", m); process.exit(1); } };

// ---- low-level native addon still works -------------------------------
assert(version() === "1.2.0", "version");
const doc = native.parse(sample);
assert(native.getString(doc, "name") === "libvibe", "name");
assert(native.getInt(doc, "answer") === 42, "answer");
assert(native.arraySize(doc, "ports") === 3, "ports");
native.free(doc);

// ---- VIBE as native JS syntax: the tagged template --------------------
const port = 8080;
const origins = ["https://a.example", "https://b.example"];
const cfg = vibe`
    name    my-service
    port    ${port}
    tls     true
    ratio   0.75
    origins ${origins}
    db {
        host  localhost
        port  5432
    }
`;

// Native property access — the result is a plain JS object.
assert(cfg.name === "my-service", "template name");
assert(cfg.port === 8080, "template interpolated int");
assert(cfg.tls === true, "template bool");
assert(cfg.ratio === 0.75, "template float");
assert(cfg.db.host === "localhost", "nested object");
assert(cfg.db.port === 5432, "nested int");

// Arrays are real JS Arrays.
assert(Array.isArray(cfg.origins), "origins is Array");
assert(cfg.origins.length === 2, "origins length");
assert(cfg.origins[0] === "https://a.example", "origins[0]");
assert(cfg.origins.every((o) => o.startsWith("https://")), "origins iterate");

// Result is deeply frozen (immutable config).
let frozen = false;
try { "use strict"; cfg.port = 1; } catch { frozen = true; }
assert(Object.isFrozen(cfg), "cfg is frozen");

// Interpolation is injection-safe: a string with structure stays a string.
const evil = 'x\n port 9999';
const c2 = vibe`name ${evil}`;
assert(c2.name === "x\n port 9999", "interpolation is escaped, not structural");
assert(c2.port === undefined, "no injected key");

// vibe(stringVar) also parses runtime documents.
const c3 = parse("k 1\n");
assert(c3.k === 1, "parse() runtime");

// Malformed input throws at evaluation.
let threw = false;
try { vibe`name {`; } catch { threw = true; }
assert(threw, "malformed template throws");

console.log("ALL OK (node native / N-API, vibe`` tagged template)");
