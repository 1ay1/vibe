"""Build the native CPython C-API extension for libvibe.

Self-contained: the library is compiled from the vendored single-header
`vendor/vibe.h` (via `vendor/vibe_impl.c`), so an sdist/wheel builds anywhere
without a prebuilt libvibe.a. Metadata lives in pyproject.toml.

    python3 setup.py build_ext --inplace   # local dev
    pip install .                          # from a checkout
    python3 -m build                       # sdist + wheel for PyPI
"""
import os
from setuptools import setup, Extension

HERE = os.path.dirname(os.path.abspath(__file__))
VENDOR = os.path.join(HERE, "vendor")

vibe = Extension(
    "_vibe",
    sources=["vibemodule.c", os.path.join("vendor", "vibe_impl.c")],
    include_dirs=[VENDOR],
)

setup(ext_modules=[vibe])
