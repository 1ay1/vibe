# vibe-sys

Native Rust bindings for [libvibe](https://github.com/1ay1/vibe) — the VIBE
config language. A real `-sys` crate: `build.rs` compiles the single-header C
library from vendored source and links it statically. No prebuilt `libvibe.a`,
no runtime `.so`.

```rust
use vibe_sys::{vibe, Doc};

// Parse at runtime...
let doc = Doc::parse("port 8080\nname \"web\"").unwrap();
assert_eq!(doc.get_int("port"), 8080);

// ...or as native syntax via the vibe! macro.
let cfg = vibe! {r#"
    name my-service
    port 8080
    tls  true
"#};
assert_eq!(cfg.get_string("name").as_deref(), Some("my-service"));
```

## Install

```sh
cargo add vibe-sys
```

## Build from a checkout

```sh
cargo run --bin vibe-test   # smoke test
cargo test
```

MIT © 1ay1 — https://github.com/1ay1/vibe
