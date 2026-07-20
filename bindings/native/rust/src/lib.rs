//! vibe-sys — native Rust bindings for libvibe.
//!
//! The `extern "C"` block declares the canonical libvibe ABI; build.rs
//! statically links libvibe.a so these calls dispatch to compiled native
//! code (no dlopen, no runtime FFI marshalling). A safe `Doc` wrapper on top
//! handles ownership and frees the document on drop.

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

#[allow(non_camel_case_types)]
type VibeValue = c_void;
#[allow(non_camel_case_types)]
type VibeArray = c_void;

extern "C" {
    fn vibe_version() -> *const c_char;
    fn vibe_parse(data: *const c_char, len: usize, out_err: *mut VibeError) -> *mut VibeValue;
    fn vibe_get_string(v: *mut VibeValue, path: *const c_char) -> *const c_char;
    fn vibe_get_int(v: *mut VibeValue, path: *const c_char) -> i64;
    fn vibe_get_float(v: *mut VibeValue, path: *const c_char) -> c_double;
    fn vibe_get_bool(v: *mut VibeValue, path: *const c_char) -> bool;
    fn vibe_get_array(v: *mut VibeValue, path: *const c_char) -> *mut VibeArray;
    fn vibe_array_size(arr: *const VibeArray) -> usize;
    fn vibe_emit(v: *mut VibeValue) -> *mut c_char;
    fn vibe_free(p: *mut c_void);
    fn vibe_value_free(v: *mut VibeValue);
    fn vibe_error_code_string(code: c_int) -> *const c_char;
}

/// libvibe version string, e.g. "1.1.0".
pub fn version() -> String {
    unsafe { CStr::from_ptr(vibe_version()).to_string_lossy().into_owned() }
}

/// A parsed VIBE document; frees itself on drop.
pub struct Doc {
    root: *mut VibeValue,
}

impl Doc {
    pub fn parse(text: &str) -> Result<Doc, String> {
        let mut err = VibeError {
            has_error: false,
            code: 0,
            message: std::ptr::null(),
            line: 0,
            column: 0,
        };
        let root = unsafe { vibe_parse(text.as_ptr() as *const c_char, text.len(), &mut err) };
        if root.is_null() {
            let code = unsafe {
                CStr::from_ptr(vibe_error_code_string(err.code))
                    .to_string_lossy()
                    .into_owned()
            };
            return Err(format!("{} (line {}, col {})", code, err.line, err.column));
        }
        Ok(Doc { root })
    }

    pub fn get_string(&self, path: &str) -> Option<String> {
        let c = CString::new(path).unwrap();
        let s = unsafe { vibe_get_string(self.root, c.as_ptr()) };
        if s.is_null() {
            None
        } else {
            Some(unsafe { CStr::from_ptr(s).to_string_lossy().into_owned() })
        }
    }

    pub fn get_int(&self, path: &str) -> i64 {
        let c = CString::new(path).unwrap();
        unsafe { vibe_get_int(self.root, c.as_ptr()) }
    }

    pub fn get_float(&self, path: &str) -> f64 {
        let c = CString::new(path).unwrap();
        unsafe { vibe_get_float(self.root, c.as_ptr()) }
    }

    pub fn get_bool(&self, path: &str) -> bool {
        let c = CString::new(path).unwrap();
        unsafe { vibe_get_bool(self.root, c.as_ptr()) }
    }

    pub fn array_size(&self, path: &str) -> usize {
        let c = CString::new(path).unwrap();
        let arr = unsafe { vibe_get_array(self.root, c.as_ptr()) };
        if arr.is_null() {
            0
        } else {
            unsafe { vibe_array_size(arr) }
        }
    }

    pub fn emit(&self) -> Option<String> {
        let s = unsafe { vibe_emit(self.root) };
        if s.is_null() {
            return None;
        }
        let out = unsafe { CStr::from_ptr(s).to_string_lossy().into_owned() };
        unsafe { vibe_free(s as *mut c_void) };
        Some(out)
    }
}

impl Drop for Doc {
    fn drop(&mut self) {
        unsafe { vibe_value_free(self.root) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = include_str!("../../../sample.vibe");

    #[test]
    fn round_trips_sample() {
        assert_eq!(version(), "1.1.0");
        let doc = Doc::parse(SAMPLE).unwrap();
        assert_eq!(doc.get_string("name").as_deref(), Some("libvibe"));
        assert_eq!(doc.get_int("answer"), 42);
        assert!((doc.get_float("pi") - 3.14159).abs() < 1e-9);
        assert!(doc.get_bool("enabled"));
        assert_eq!(doc.get_string("server.host").as_deref(), Some("localhost"));
        assert_eq!(doc.get_int("server.port"), 8080);
        assert_eq!(doc.array_size("ports"), 3);
        assert!(doc.emit().unwrap().contains("libvibe"));
        assert!(Doc::parse("name {").is_err());
    }
}
