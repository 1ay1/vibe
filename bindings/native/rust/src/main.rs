//! Runnable smoke test for the native vibe-sys crate: `cargo run --bin vibe-test`.

use vibe_sys::vibe;

fn main() {
    // Self-contained sample (crate ships no files outside its own dir).
    let sample = concat!(
        "name libvibe\n",
        "answer 42\n",
        "pi 3.14159\n",
        "enabled true\n",
        "ports [8080, 8081, 8082]\n",
        "server {\n",
        "  host localhost\n",
        "  port 8080\n",
        "}\n",
    );

    assert_eq!(vibe_sys::version(), "1.2.0");

    let doc = vibe_sys::Doc::parse(sample).expect("parse sample.vibe");
    assert_eq!(doc.get_string("name").as_deref(), Some("libvibe"));
    assert_eq!(doc.get_int("answer"), 42);
    assert!((doc.get_float("pi") - 3.14159).abs() < 1e-9);
    assert!(doc.get_bool("enabled"));
    assert_eq!(doc.get_string("server.host").as_deref(), Some("localhost"));
    assert_eq!(doc.get_int("server.port"), 8080);
    assert_eq!(doc.array_size("ports"), 3);
    assert!(doc.emit().unwrap().contains("libvibe"));
    assert!(vibe_sys::Doc::parse("name {").is_err());

    // VIBE as native Rust syntax via the vibe! macro.
    let cfg = vibe! {r#"
        name  my-service
        port  8080
        tls   true
    "#};
    assert_eq!(cfg.get_string("name").as_deref(), Some("my-service"));
    assert_eq!(cfg.get_int("port"), 8080);
    assert!(cfg.root().get("tls").as_bool());

    println!("ALL OK (rust native / -sys crate, vibe! macro)");
}
