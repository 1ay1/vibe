//! Link the static libvibe.a from the repo root at build time.
//!
//! This is what makes vibe-sys a real *-sys crate rather than a runtime FFI
//! binding: cargo statically links the native archive into the Rust binary.

use std::path::PathBuf;

fn main() {
    // build.rs runs in the crate dir; repo root is three levels up.
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../..")
        .canonicalize()
        .expect("locate repo root");

    println!("cargo:rustc-link-search=native={}", root.display());
    println!("cargo:rustc-link-lib=static=vibe");
    println!("cargo:rerun-if-changed={}", root.join("libvibe.a").display());
}
