//! Compile libvibe from the vendored single-header source and link it
//! statically into the crate.
//!
//! This is what makes vibe-sys a self-contained, publishable `-sys` crate: the
//! C library is built from `vendor/vibe_impl.c` (which instantiates the
//! single-header `vendor/vibe.h`) — no prebuilt libvibe.a is required.

fn main() {
    cc::Build::new()
        .file("vendor/vibe_impl.c")
        .include("vendor")
        .opt_level(2)
        .warnings(false)
        .compile("vibe");

    println!("cargo:rerun-if-changed=vendor/vibe_impl.c");
    println!("cargo:rerun-if-changed=vendor/vibe.h");
}
