#!/usr/bin/env python3
"""Smoke test: native CPython extension + VIBE-as-native-Python-syntax."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import _vibe          # low-level compiled extension
import vibe           # native-syntax layer (vibe callable, View, codec)

SAMPLE = os.path.join(HERE, "..", "..", "sample.vibe")
with open(SAMPLE, "rb") as f:
    data = f.read()

# ---- low-level native extension still works ---------------------------
assert _vibe.version() == "1.2.0", _vibe.version()
doc = _vibe.parse(data)
assert doc.get_string("name") == "libvibe"
assert doc.get_int("answer") == 42
assert doc.array_size("ports") == 3
assert "libvibe" in doc.emit()
assert doc.to_dict()["server"]["port"] == 8080

# ---- VIBE as native Python syntax: the `vibe(...)` callable -----------
cfg = vibe('''
    name    my-service
    port    8080
    tls     true
    ratio   0.75
    origins [ https://a.example  https://b.example ]
    db {
        host  localhost
        port  5432
    }
''')

# Attribute access.
assert cfg.name == "my-service"
assert cfg.port == 8080
assert cfg.tls is True
assert cfg.ratio == 0.75
# Nested — dots and brackets interchangeably.
assert cfg.db.host == "localhost"
assert cfg["db"]["port"] == 5432
# Arrays are native lists.
assert isinstance(cfg.origins, list)
assert len(cfg.origins) == 2
assert cfg.origins[0] == "https://a.example"
assert all(o.startswith("https://") for o in cfg.origins)
# Missing keys behave like Python.
try:
    cfg.nope
    raise SystemExit("expected AttributeError")
except AttributeError:
    pass
assert cfg.get("nope", 42) == 42

# Malformed input raises the native exception type.
try:
    vibe("name {")
    raise SystemExit("expected VibeError on malformed input")
except vibe.VibeError:
    pass

# ---- the `# coding: vibe` source codec --------------------------------
# Decode a module that uses `vibe'''...'''` literal syntax and run it.
src = (
    "# coding: vibe\n"
    "import vibe\n"
    "CONFIG = vibe'''\n"
    "    service  api\n"
    "    workers  4\n"
    "    tags     [ a b c ]\n"
    "'''\n"
)
decoded = src.encode("vibe").decode("vibe")   # exercise the codec round-trip
ns = {}
exec(compile(decoded, "<coding-vibe>", "exec"), ns)
CONFIG = ns["CONFIG"]
assert CONFIG.service == "api"
assert CONFIG.workers == 4
assert CONFIG.tags == ["a", "b", "c"]

print("ALL OK (python native / C-API, vibe() + `# coding: vibe`)")
