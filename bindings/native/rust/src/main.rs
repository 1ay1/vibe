//! Runnable smoke test for the native vibe-sys crate: `cargo run --bin vibe-test`.

fn main() {
    let sample = include_str!("../../../sample.vibe");

    assert_eq!(vibe_sys::version(), "1.1.0");

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

    println!("ALL OK (rust native / -sys crate)");
}
