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
    fn vibe_get(v: *mut VibeValue, path: *const c_char) -> *mut VibeValue;
    fn vibe_get_string(v: *mut VibeValue, path: *const c_char) -> *const c_char;
    fn vibe_get_int(v: *mut VibeValue, path: *const c_char) -> i64;
    fn vibe_get_float(v: *mut VibeValue, path: *const c_char) -> c_double;
    fn vibe_get_bool(v: *mut VibeValue, path: *const c_char) -> bool;
    fn vibe_get_array(v: *mut VibeValue, path: *const c_char) -> *mut VibeArray;
    fn vibe_array_size(arr: *const VibeArray) -> usize;
    fn vibe_array_get(arr: *mut VibeArray, index: usize) -> *mut VibeValue;
    fn vibe_value_type(v: *const VibeValue) -> c_int;
    fn vibe_is_integer(v: *const VibeValue) -> bool;
    fn vibe_emit(v: *mut VibeValue) -> *mut c_char;
    fn vibe_free(p: *mut c_void);
    fn vibe_value_free(v: *mut VibeValue);
    fn vibe_error_code_string(code: c_int) -> *const c_char;
}

/// VibeType discriminants (must match the C enum order in vibe.h).
const T_NULL: c_int = 0;
const T_INTEGER: c_int = 1;
const T_FLOAT: c_int = 2;
const T_BOOLEAN: c_int = 3;
const T_STRING: c_int = 4;
const T_ARRAY: c_int = 5;
const T_OBJECT: c_int = 6;

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

    /// Borrow the document root as a navigable [`Value`]. Lets you write
    /// `doc.root()["server"]["port"].as_int()` with array indexing too.
    pub fn root(&self) -> Value<'_> {
        Value { ptr: self.root, _doc: std::marker::PhantomData }
    }
}

/// A borrowed view of a node inside a [`Doc`]. Bound to the document's
/// lifetime, so it can't dangle. Absent nodes are represented by a null
/// pointer and read back as the type's fallback — navigation never panics.
#[derive(Clone, Copy)]
pub struct Value<'a> {
    ptr: *mut VibeValue,
    _doc: std::marker::PhantomData<&'a Doc>,
}

impl<'a> Value<'a> {
    /// True if this node is present (not a missing key / out-of-range index).
    pub fn exists(&self) -> bool {
        !self.ptr.is_null()
    }

    fn ty(&self) -> c_int {
        if self.ptr.is_null() { T_NULL } else { unsafe { vibe_value_type(self.ptr) } }
    }
    pub fn is_null(&self) -> bool { self.ptr.is_null() || self.ty() == T_NULL }
    pub fn is_int(&self) -> bool { !self.ptr.is_null() && unsafe { vibe_is_integer(self.ptr) } }
    pub fn is_float(&self) -> bool { self.ty() == T_FLOAT }
    pub fn is_bool(&self) -> bool { self.ty() == T_BOOLEAN }
    pub fn is_string(&self) -> bool { self.ty() == T_STRING }
    pub fn is_array(&self) -> bool { self.ty() == T_ARRAY }
    pub fn is_object(&self) -> bool { self.ty() == T_OBJECT }

    /// Look up a key (or dotted path). Returns an absent Value if missing.
    pub fn get(&self, key: &str) -> Value<'a> {
        if self.ptr.is_null() {
            return Value { ptr: std::ptr::null_mut(), _doc: std::marker::PhantomData };
        }
        let c = CString::new(key).unwrap();
        let p = unsafe { vibe_get(self.ptr, c.as_ptr()) };
        Value { ptr: p, _doc: std::marker::PhantomData }
    }

    /// Array length (0 for non-arrays / absent).
    pub fn len(&self) -> usize {
        if self.ptr.is_null() { return 0; }
        let arr = unsafe { vibe_get_array(self.ptr, std::ptr::null()) };
        if arr.is_null() { return 0; }
        unsafe { vibe_array_size(arr) }
    }
    pub fn is_empty(&self) -> bool { self.len() == 0 }

    /// Index into an array. Absent Value if out of range / not an array.
    pub fn at(&self, i: usize) -> Value<'a> {
        let none = Value { ptr: std::ptr::null_mut(), _doc: std::marker::PhantomData };
        if self.ptr.is_null() { return none; }
        let arr = unsafe { vibe_get_array(self.ptr, std::ptr::null()) };
        if arr.is_null() { return none; }
        let p = unsafe { vibe_array_get(arr, i) };
        Value { ptr: p, _doc: std::marker::PhantomData }
    }

    /// Iterate array elements.
    pub fn iter(&self) -> impl Iterator<Item = Value<'a>> + '_ {
        let n = self.len();
        (0..n).map(move |i| self.at(i))
    }

    // Typed readers, each with a fallback via the *_or variants.
    pub fn as_str(&self) -> Option<&'a str> {
        if self.ty() != T_STRING { return None; }
        let s = unsafe { vibe_get_string(self.ptr, std::ptr::null()) };
        if s.is_null() { return None; }
        unsafe { CStr::from_ptr(s).to_str().ok() }
    }
    pub fn as_string(&self) -> String { self.as_str().unwrap_or_default().to_owned() }
    pub fn as_int(&self) -> i64 {
        if !self.is_int() { return 0; }
        unsafe { vibe_get_int(self.ptr, std::ptr::null()) }
    }
    pub fn as_float(&self) -> f64 {
        match self.ty() {
            T_FLOAT | T_INTEGER => unsafe { vibe_get_float(self.ptr, std::ptr::null()) },
            _ => 0.0,
        }
    }
    pub fn as_bool(&self) -> bool {
        if self.ty() != T_BOOLEAN { return false; }
        unsafe { vibe_get_bool(self.ptr, std::ptr::null()) }
    }
}

/// `doc.root()["key"]` and `arr.at(0)` navigation. Index by &str returns a
/// borrowed child; because [`Value`] is `Copy` and pointer-sized we keep the
/// safe, chainable `.get()` API rather than a lifetime-fragile `Index` impl.
impl<'a> Value<'a> {
    /// Chainable subscript alias for [`Value::get`].
    pub fn key(&self, k: &str) -> Value<'a> { self.get(k) }
}

impl Drop for Doc {
    fn drop(&mut self) {
        unsafe { vibe_value_free(self.root) };
    }
}

/// `vibe! { ... }` — VIBE as native Rust syntax.
///
/// Write a VIBE document inline; the macro captures it verbatim (via a raw
/// string so newlines and indentation are preserved exactly) and parses it
/// into a [`Doc`] when the expression evaluates. A malformed document panics
/// with the parser's diagnostic — the earliest a runtime library can react to
/// bad input embedded in source.
///
/// ```ignore
/// use vibe::vibe;
/// let cfg = vibe! {r#"
///     name  my-service
///     port  8080
///     tls   true
/// "#};
/// assert_eq!(cfg.get_int("port"), 8080);
/// ```
///
/// Passing a non-literal expression (e.g. a `String` built at runtime) also
/// works: `vibe!(my_string)`.
#[macro_export]
macro_rules! vibe {
    ($text:literal) => {
        $crate::Doc::parse($text).expect("vibe! literal failed to parse")
    };
    ($text:expr) => {
        $crate::Doc::parse(&$text).expect("vibe! input failed to parse")
    };
    // Brace form: `vibe! { r#"..."# }` reads most like native block syntax.
    ({ $text:literal }) => {
        $crate::Doc::parse($text).expect("vibe! literal failed to parse")
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = concat!(
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

    #[test]
    fn round_trips_sample() {
        assert_eq!(version(), "1.2.0");
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

    #[test]
    fn vibe_macro_is_native_syntax() {
        // VIBE written inline as native Rust syntax.
        let cfg = vibe! {r#"
            name    my-service
            port    8080
            tls     true
            ratio   0.75
            origins [ https://a.example  https://b.example ]
            db {
                host  localhost
                port  5432
            }
        "#};

        assert_eq!(cfg.get_string("name").as_deref(), Some("my-service"));
        assert_eq!(cfg.get_int("port"), 8080);
        assert!(cfg.get_bool("tls"));

        // Navigate via the Value API: keys, dotted paths, array indexing, iter.
        let root = cfg.root();
        assert_eq!(root.get("db").get("host").as_str(), Some("localhost"));
        assert_eq!(root.get("db").get("port").as_int(), 5432);
        assert_eq!(root.get("ratio").as_float(), 0.75);

        let origins = root.get("origins");
        assert!(origins.is_array());
        assert_eq!(origins.len(), 2);
        assert_eq!(origins.at(0).as_str(), Some("https://a.example"));
        let count = origins.iter().filter(|v| v.as_string().starts_with("https://")).count();
        assert_eq!(count, 2);

        // Absent keys never panic.
        assert!(!root.get("nope").exists());
        assert_eq!(root.get("nope").as_int(), 0);
    }

    #[test]
    #[should_panic(expected = "failed to parse")]
    fn vibe_macro_panics_on_bad_input() {
        let _ = vibe!("name {");
    }
}
