# vibe-native

Native Node.js N-API addon for [libvibe](https://github.com/1ay1/vibe) — the
VIBE config language as a tagged template literal. The single-header C library
is compiled from source at install time (no runtime `.so` dependency).

```js
import { vibe } from "vibe-native";

const port = 8080;
const cfg = vibe`
  port ${port}
  name "web"
`;

console.log(cfg.port); // 8080
console.log(cfg.name); // "web"
```

`${}` interpolation is injection-safe (values are encoded, not string-spliced),
and you get back a frozen, typed JS object.

## Install

```sh
npm install vibe-native
```

Requires a C toolchain + `node-gyp` (builds the addon on install).

## Build from a checkout

```sh
node-gyp configure build
node test.mjs
```

MIT © 1ay1 — https://github.com/1ay1/vibe
