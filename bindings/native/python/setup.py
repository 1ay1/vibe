"""Build the native CPython C-API extension for libvibe.

    cd bindings/native/python
    python3 setup.py build_ext --inplace

Then:  python3 test.py
"""
import os
from setuptools import setup, Extension

# Repo root holds vibe.h and libvibe.a (three levels up from this file).
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

vibe = Extension(
    "vibe",
    sources=["vibemodule.c"],
    include_dirs=[ROOT],
    extra_objects=[os.path.join(ROOT, "libvibe.a")],
)

setup(
    name="vibe",
    version="1.1.0",
    description="Native CPython C-API bindings for libvibe",
    ext_modules=[vibe],
)
