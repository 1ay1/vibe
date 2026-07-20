// libvibe — Rust binding via raw FFI (no external crates).
//
// Build:  rustc vibe.rs -L <dir-with-libvibe.so> -o vibe_rs
// Run:    LD_LIBRARY_PATH=<dir> ./vibe_rs
//
// A production crate would wrap this in a safe module + build.rs; this single
// file is the whole FFI surface, kept dependency-free so it builds offline.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_double, c_int, c_void};

#[repr(C)]
struct VibeError {
    has_error: bool,
    code: c_int,
    message: *const c_char,
    line: c_int,
    column: c_int,
}

#[link(name = "vibe")]
extern "C" {
    fn vibe_version() -> *const c_char;
    fn vibe_parse(data: *const c_char, len: usize, err: *mut VibeError) -> *mut c_void;
    fn vibe_get_string(v: *mut c_void, path: *const c_char) -> *const c_char;
    fn vibe_get_int(v: *mut c_void, path: *const c_char) -> i64;
    fn vibe_get_float(v: *mut c_void, path: *const c_char) -> c_double;
    fn vibe_get_bool(v: *mut c_void, path: *const c_char) -> bool;
    fn vibe_get_array(v: *mut c_void, path: *const c_char) -> *mut c_void;
    fn vibe_array_size(a: *mut c_void) -> usize;
    fn vibe_emit(v: *mut c_void) -> *mut c_char;
    fn vibe_free(p: *mut c_void);
    fn vibe_value_free(v: *mut c_void);
    fn vibe_error_code_string(code: c_int) -> *const c_char;
}

unsafe fn cstr(p: *const c_char) -> Option<String> {
    if p.is_null() {
        None
    } else {
        Some(CStr::from_ptr(p).to_string_lossy().into_owned())
    }
}

struct Doc {
    ptr: *mut c_void,
}

impl Doc {
    fn get_string(&self, path: &str) -> Option<String> {
        let c = CString::new(path).unwrap();
        unsafe { cstr(vibe_get_string(self.ptr, c.as_ptr())) }
    }
    fn get_int(&self, path: &str) -> i64 {
        let c = CString::new(path).unwrap();
        unsafe { vibe_get_int(self.ptr, c.as_ptr()) }
    }
    fn get_float(&self, path: &str) -> f64 {
        let c = CString::new(path).unwrap();
        unsafe { vibe_get_float(self.ptr, c.as_ptr()) }
    }
    fn get_bool(&self, path: &str) -> bool {
        let c = CString::new(path).unwrap();
        unsafe { vibe_get_bool(self.ptr, c.as_ptr()) }
    }
    fn array_size(&self, path: &str) -> usize {
        let c = CString::new(path).unwrap();
        unsafe {
            let a = vibe_get_array(self.ptr, c.as_ptr());
            if a.is_null() { 0 } else { vibe_array_size(a) }
        }
    }
    fn emit(&self) -> String {
        unsafe {
            let raw = vibe_emit(self.ptr);
            if raw.is_null() { return String::new(); }
            let s = cstr(raw).unwrap_or_default();
            vibe_free(raw as *mut c_void);
            s
        }
    }
}

impl Drop for Doc {
    fn drop(&mut self) {
        unsafe { vibe_value_free(self.ptr) }
    }
}

fn parse(data: &[u8]) -> Result<Doc, String> {
    let mut err = VibeError { has_error: false, code: 0, message: std::ptr::null(), line: 0, column: 0 };
    let ptr = unsafe { vibe_parse(data.as_ptr() as *const c_char, data.len(), &mut err) };
    if ptr.is_null() {
        let code = unsafe { cstr(vibe_error_code_string(err.code)).unwrap_or_default() };
        return Err(format!("{} at {}:{}", code, err.line, err.column));
    }
    Ok(Doc { ptr })
}

fn version() -> String {
    unsafe { cstr(vibe_version()).unwrap_or_default() }
}

fn main() {
    let path = std::env::var("VIBE_SAMPLE").unwrap_or_else(|_| "../sample.vibe".into());
    let data = std::fs::read(&path).expect("read sample.vibe");
    let doc = parse(&data).expect("parse");

    let mut ok = true;
    macro_rules! check {
        ($name:expr, $got:expr, $want:expr) => {{
            let got = $got;
            let pass = got == $want;
            if !pass { ok = false; }
            println!("  [{}] {} = {:?}", if pass { "ok " } else { "BAD" }, $name, got);
        }};
    }
    check!("version", version(), "1.2.0");
    check!("name", doc.get_string("name"), Some("libvibe".to_string()));
    check!("answer", doc.get_int("answer"), 42);
    check!("pi", (doc.get_float("pi") * 100000.0).round() / 100000.0, 3.14159);
    check!("enabled", doc.get_bool("enabled"), true);
    check!("server.host", doc.get_string("server.host"), Some("localhost".to_string()));
    check!("server.port", doc.get_int("server.port"), 8080);
    check!("len(ports)", doc.array_size("ports"), 3usize);
    if doc.emit().contains("libvibe") {
        println!("  [ok ] emit() round-trips");
    } else {
        ok = false;
        println!("  [BAD] emit() did not round-trip");
    }
    match parse(b"name {") {
        Err(_) => println!("  [ok ] rejects malformed input"),
        Ok(_) => { ok = false; println!("  [BAD] malformed input did not raise"); }
    }
    println!("{}", if ok { "ALL OK (rust)" } else { "FAILED (rust)" });
    std::process::exit(if ok { 0 } else { 1 });
}
