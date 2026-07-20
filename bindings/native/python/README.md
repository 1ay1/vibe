# vibe-lang (native Python extension)

VIBE as native Python syntax, backed by a compiled-in C extension (the
single-header [libvibe](https://github.com/1ay1/vibe) is built from source — no
runtime `.so` dependency).

```python
import vibe

cfg = vibe("""
port 8080
name "web"
""")
print(cfg.port)   # 8080
print(cfg.name)   # web
```

Or use the source codec:

```python
# coding: vibe
port 8080
```

## Install

```sh
pip install vibe-lang
```

## Build from a checkout

```sh
python3 setup.py build_ext --inplace
python3 test.py
```

MIT © 1ay1 — https://github.com/1ay1/vibe
