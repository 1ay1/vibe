"""vibe — VIBE as native Python syntax.

Two ways to embed VIBE directly in Python, both backed by the native C
extension (``_vibe``, which #includes vibe.h and links libvibe.a):

1. The ``vibe`` callable — parse an inline document and get an object whose
   keys are attributes AND items::

       import vibe
       cfg = vibe('''
           name    my-service
           port    8080
           tls     true
           origins [ https://a.example  https://b.example ]
           db {
               host  localhost
               port  5432
           }
       ''')
       cfg.name          # "my-service"
       cfg.port          # 8080
       cfg.db.host       # "localhost"   (nested)
       cfg["db"]["port"] # 5432          (item access too)
       cfg.origins[0]    # "https://a.example"
       for o in cfg.origins: ...

2. The ``# coding: vibe`` source codec — makes ``vibe`` a real literal prefix
   so a whole module can carry VIBE blocks the interpreter decodes for you::

       # coding: vibe
       import vibe
       CONFIG = vibe'''
           name  my-service
           port  8080
       '''

   (register the codec once with ``vibe.install_codec()`` — see below.)

SPDX-License-Identifier: MIT
"""
from __future__ import annotations

import codecs
import encodings
import io
import re
import tokenize as _tok

import _vibe

VibeError = _vibe.VibeError

__all__ = ["vibe", "parse", "version", "VibeError", "View", "install_codec"]


def version() -> str:
    return _vibe.version()


class View:
    """Attribute- and item-accessible read-only view over a parsed VIBE value.

    Wraps the plain dict/list/scalars produced by the native extension so you
    can navigate with dots (``cfg.db.host``) or brackets (``cfg["db"]["host"]``)
    interchangeably. Missing keys raise the usual KeyError/AttributeError.
    """

    __slots__ = ("_d",)

    def __init__(self, data):
        object.__setattr__(self, "_d", data)

    @staticmethod
    def _wrap(v):
        if isinstance(v, dict):
            return View(v)
        if isinstance(v, list):
            return [View._wrap(x) for x in v]
        return v

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return View._wrap(d[name])
        raise AttributeError(name)

    def __getitem__(self, key):
        return View._wrap(object.__getattribute__(self, "_d")[key])

    def __contains__(self, key):
        return key in object.__getattribute__(self, "_d")

    def __iter__(self):
        return iter(object.__getattribute__(self, "_d"))

    def keys(self):
        return object.__getattribute__(self, "_d").keys()

    def get(self, key, default=None):
        d = object.__getattribute__(self, "_d")
        return View._wrap(d[key]) if key in d else default

    def to_dict(self):
        return object.__getattribute__(self, "_d")

    def __len__(self):
        return len(object.__getattribute__(self, "_d"))

    def __repr__(self):
        return f"View({object.__getattribute__(self, '_d')!r})"

    def __eq__(self, other):
        if isinstance(other, View):
            other = other.to_dict()
        return object.__getattribute__(self, "_d") == other


def parse(source):
    """Parse VIBE text (str or bytes) into a navigable View.

    Raises vibe.VibeError on malformed input.
    """
    if isinstance(source, bytes):
        source = source.decode("utf-8")
    doc = _vibe.parse(source)
    return View._wrap(doc.to_dict())


# `vibe(...)` is the native-feeling entry point.
vibe = parse


# --------------------------------------------------------------------------
# Source codec: `# coding: vibe` turns `vibe'''...'''` / `vibe"""..."""` into
# `vibe(r'''...''')` so VIBE literals are a real part of the module's grammar.
# --------------------------------------------------------------------------
_utf8 = encodings.search_function("utf8")

# Match a `vibe` prefix immediately followed by a (possibly triple) quote.
_LITERAL = re.compile(
    r"""\bvibe(?P<q>'''|\"\"\"|'|")""",
)


def _rewrite(text: str) -> str:
    """Rewrite `vibe<quote>...<quote>` literals into `vibe(r<quote>...<quote>)`.

    Uses a small state machine so quotes inside the VIBE body don't confuse it:
    once we see an opening `vibe'''`, we scan to the matching closing quote.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        m = _LITERAL.search(text, i)
        if not m:
            out.append(text[i:])
            break
        out.append(text[i:m.start()])
        q = m.group("q")
        body_start = m.end()
        end = text.find(q, body_start)
        if end == -1:
            # Unterminated — leave the rest untouched; Python will error clearly.
            out.append(text[m.start():])
            break
        body = text[body_start:end]
        # Wrap as a raw string call so backslashes/quotes pass through verbatim.
        # Guard against a raw-string ending in a backslash (illegal in Python).
        if body.endswith("\\"):
            body += " "
        out.append(f"vibe(r{q}{body}{q})")
        i = end + len(q)
    return "".join(out)


def _decode(input_bytes, errors="strict"):
    text, consumed = _utf8.decode(input_bytes, errors)
    return _rewrite(text), consumed


class _IncrementalDecoder(codecs.BufferedIncrementalDecoder):
    def _buffer_decode(self, input, errors, final):
        if final:
            return _decode(input, errors)
        return "", 0


def _search(name):
    if name.replace("-", "_") != "vibe":
        return None
    return codecs.CodecInfo(
        name="vibe",
        encode=_utf8.encode,
        decode=_decode,
        incrementaldecoder=_IncrementalDecoder,
        streamreader=_utf8.streamreader,
        streamwriter=_utf8.streamwriter,
    )


_installed = False


def install_codec():
    """Register the `# coding: vibe` source codec (idempotent)."""
    global _installed
    if not _installed:
        codecs.register(_search)
        _installed = True


# Auto-register on import so `# coding: vibe` works without a manual call.
install_codec()


# Make the *module itself* callable, so `import vibe; vibe("...")` works and the
# VIBE literal reads as native syntax (rather than requiring `vibe.vibe(...)`).
import sys as _sys


class _CallableModule(_sys.modules[__name__].__class__):
    def __call__(self, source):
        return parse(source)


_sys.modules[__name__].__class__ = _CallableModule
