#!/usr/bin/env python3
"""Smoke test for the NATIVE CPython C-API extension (import vibe)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vibe  # the compiled .so, not the ctypes bindings/python/vibe.py

HERE = os.path.dirname(os.path.abspath(__file__))
SAMPLE = os.path.join(HERE, "..", "..", "sample.vibe")

with open(SAMPLE, "rb") as f:
    data = f.read()

assert vibe.version() == "1.1.0", vibe.version()

doc = vibe.parse(data)
assert doc.get_string("name") == "libvibe"
assert doc.get_int("answer") == 42
assert abs(doc.get_float("pi") - 3.14159) < 1e-9
assert doc.get_bool("enabled") is True
assert doc.get_string("server.host") == "localhost"
assert doc.get_int("server.port") == 8080
assert doc.array_size("ports") == 3

# emit round-trips something non-empty
assert "libvibe" in doc.emit()

# malformed input raises the native exception type
try:
    vibe.parse("name {")
    raise SystemExit("expected VibeError on malformed input")
except vibe.VibeError:
    pass

print("ALL OK (python native / C-API)")
