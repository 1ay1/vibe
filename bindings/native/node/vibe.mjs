// vibe.mjs — VIBE as native JavaScript syntax.
//
// The `vibe` tagged template literal makes a VIBE document a first-class JS
// expression. You write the document inline — real template-literal syntax,
// with `${}` interpolation — and get back a plain, frozen JS object:
//
//     import { vibe } from "./vibe.mjs";
//
//     const port = 8080;
//     const cfg = vibe`
//         name    my-service
//         port    ${port}
//         tls     true
//         origins [ https://a.example  https://b.example ]
//         db {
//             host  localhost
//             port  5432
//         }
//     `;
//
//     cfg.name          // "my-service"   — native property access
//     cfg.port          // 8080           — a real JS number
//     cfg.db.host       // "localhost"    — nested, no getters
//     cfg.origins[0]    // "https://a.example"
//     for (const o of cfg.origins) ...   // it's a real Array
//
// Interpolated values are serialized to VIBE before splicing, so `${port}`,
// `${true}`, `${"a string"}`, and `${["x","y"]}` all Just Work and are safely
// quoted/escaped — no injection. A malformed document throws at evaluation.
//
// This is a TRUE native addon underneath: it #includes vibe.h and links
// libvibe.a (see vibe_addon.c). No FFI marshalling.
//
// SPDX-License-Identifier: MIT
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const here = dirname(fileURLToPath(import.meta.url));
const native = require(join(here, "build", "Release", "vibe.node"));

export const version = () => native.version();

// Serialize an interpolated JS value into a VIBE fragment that lexes as one
// value. Strings are always quoted+escaped so nothing a user interpolates can
// break out into structure (no injection). Arrays become `[ a b c ]`.
function toVibe(x) {
  if (x === null || x === undefined) return '""';
  const t = typeof x;
  if (t === "number") return Number.isFinite(x) ? String(x) : '""';
  if (t === "boolean") return x ? "true" : "false";
  if (t === "bigint") return x.toString();
  if (Array.isArray(x)) return "[ " + x.map(toVibe).join(" ") + " ]";
  // Everything else: emit as a quoted, escaped VIBE string.
  const s = String(x)
    .replace(/\\/g, "\\\\")
    .replace(/"/g, '\\"')
    .replace(/\n/g, "\\n")
    .replace(/\t/g, "\\t")
    .replace(/\r/g, "\\r");
  return `"${s}"`;
}

// Parse VIBE source into a frozen native JS object.
export function parse(source) {
  const handle = native.parse(source); // throws on malformed input
  try {
    return deepFreeze(native.toObject(handle));
  } finally {
    native.free(handle);
  }
}

function deepFreeze(o) {
  if (o && typeof o === "object") {
    for (const k of Object.keys(o)) deepFreeze(o[k]);
    Object.freeze(o);
  }
  return o;
}

// The tagged template: `vibe`...`` is now valid, native VIBE-in-JS.
export function vibe(strings, ...values) {
  let src = strings[0];
  for (let i = 0; i < values.length; i++) {
    src += toVibe(values[i]) + strings[i + 1];
  }
  return parse(src);
}

// Also allow `vibe(stringVariable)` for documents built/loaded at runtime.
const tag = new Proxy(vibe, {
  apply(target, thisArg, args) {
    if (args.length === 1 && typeof args[0] === "string") return parse(args[0]);
    return Reflect.apply(target, thisArg, args);
  },
});

export default tag;
