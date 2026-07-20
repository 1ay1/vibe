"""
libvibe — Python binding (ctypes, standard library only).

    import vibe
    doc = vibe.parse(open("config.vibe", "rb").read())
    host = doc.get_string("server.host")

No third-party packages: this loads the shared library that `make` builds
(libvibe.so / .dylib / .dll) and talks to its stable C ABI directly.
"""

import ctypes
import os
import sys

# --- locate and load the shared library ------------------------------------

def _default_lib():
    env = os.environ.get("VIBE_LIB")
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    for cand in ("libvibe.so", "libvibe.dylib", "libvibe.so.1"):
        p = os.path.join(here, "..", "..", cand)
        if os.path.exists(p):
            return p
    return "libvibe.so"  # let the loader search the system paths


_lib = ctypes.CDLL(_default_lib())


class _CError(ctypes.Structure):
    # Mirrors `VibeError` in vibe.h. ctypes inserts the padding for us.
    _fields_ = [
        ("has_error", ctypes.c_bool),
        ("code", ctypes.c_int),
        ("message", ctypes.c_char_p),
        ("line", ctypes.c_int),
        ("column", ctypes.c_int),
    ]


def _decl(name, restype, argtypes):
    fn = getattr(_lib, name)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


_P = ctypes.c_void_p  # opaque VibeValue*/VibeArray*
_s = ctypes.c_char_p

_vibe_version = _decl("vibe_version", _s, [])
_vibe_parse = _decl("vibe_parse", _P, [_s, ctypes.c_size_t, ctypes.POINTER(_CError)])
_vibe_get_string = _decl("vibe_get_string", _s, [_P, _s])
_vibe_get_int = _decl("vibe_get_int", ctypes.c_int64, [_P, _s])
_vibe_get_float = _decl("vibe_get_float", ctypes.c_double, [_P, _s])
_vibe_get_bool = _decl("vibe_get_bool", ctypes.c_bool, [_P, _s])
_vibe_get_array = _decl("vibe_get_array", _P, [_P, _s])
_vibe_array_size = _decl("vibe_array_size", ctypes.c_size_t, [_P])
_vibe_emit = _decl("vibe_emit", _P, [_P])
_vibe_free = _decl("vibe_free", None, [_P])
_vibe_value_free = _decl("vibe_value_free", None, [_P])
_vibe_error_free = _decl("vibe_error_free", None, [ctypes.POINTER(_CError)])
_vibe_error_code_string = _decl("vibe_error_code_string", _s, [ctypes.c_int])


class VibeError(Exception):
    """A parse failure, with the spec's stable error code plus line/column."""

    def __init__(self, code, message, line, column):
        self.code = code
        self.line = line
        self.column = column
        super().__init__(f"{message} (code {code!r} at {line}:{column})")


def version():
    """Runtime library version, e.g. '1.1.0'."""
    return _vibe_version().decode()


class Doc:
    """A parsed VIBE document. Scalars are read by dotted path."""

    def __init__(self, ptr):
        self._ptr = ptr

    def get_string(self, path):
        v = _vibe_get_string(self._ptr, path.encode())
        return v.decode() if v is not None else None

    def get_int(self, path):
        return _vibe_get_int(self._ptr, path.encode())

    def get_float(self, path):
        return _vibe_get_float(self._ptr, path.encode())

    def get_bool(self, path):
        return _vibe_get_bool(self._ptr, path.encode())

    def array_size(self, path):
        arr = _vibe_get_array(self._ptr, path.encode())
        return _vibe_array_size(arr) if arr else 0

    def emit(self):
        """Serialise back to canonical VIBE text (idempotent)."""
        raw = _vibe_emit(self._ptr)
        if not raw:
            return ""
        try:
            return ctypes.cast(raw, ctypes.c_char_p).value.decode()
        finally:
            _vibe_free(raw)

    def close(self):
        if self._ptr:
            _vibe_value_free(self._ptr)
            self._ptr = None

    __del__ = close
    def __enter__(self):
        return self
    def __exit__(self, *a):
        self.close()


def parse(data):
    """Parse VIBE text (str or bytes). Raises VibeError on malformed input."""
    if isinstance(data, str):
        data = data.encode()
    err = _CError()
    ptr = _vibe_parse(data, len(data), ctypes.byref(err))
    if not ptr:
        code = _vibe_error_code_string(err.code).decode()
        msg = err.message.decode() if err.message else "parse error"
        line, col = err.line, err.column
        _vibe_error_free(ctypes.byref(err))
        raise VibeError(code, msg, line, col)
    return Doc(ptr)


# --- self-test (run: python vibe.py) ---------------------------------------

def _selftest():
    sample = os.environ.get(
        "VIBE_SAMPLE",
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sample.vibe"),
    )
    with open(sample, "rb") as f:
        doc = parse(f.read())
    checks = [
        ("version", version(), "1.1.0"),
        ("name", doc.get_string("name"), "libvibe"),
        ("answer", doc.get_int("answer"), 42),
        ("pi", round(doc.get_float("pi"), 5), 3.14159),
        ("enabled", doc.get_bool("enabled"), True),
        ("server.host", doc.get_string("server.host"), "localhost"),
        ("server.port", doc.get_int("server.port"), 8080),
        ("len(ports)", doc.array_size("ports"), 3),
    ]
    ok = True
    for name, got, want in checks:
        flag = "ok " if got == want else "BAD"
        if got != want:
            ok = False
        print(f"  [{flag}] {name} = {got!r}")
    emitted = doc.emit()
    if "libvibe" not in emitted:
        ok = False
        print("  [BAD] emit() did not round-trip")
    else:
        print("  [ok ] emit() round-trips")
    # error path
    try:
        parse("name {")
        ok = False
        print("  [BAD] malformed input did not raise")
    except VibeError as e:
        print(f"  [ok ] rejects malformed input: {e.code}")
    print("ALL OK (python)" if ok else "FAILED (python)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(_selftest())
