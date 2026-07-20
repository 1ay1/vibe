# VIBE Conformance Test Suite

This is the language-neutral, normative test suite referenced by
[SPECIFICATION.md → Conformance Test Suite](../../SPECIFICATION.md#conformance-test-suite).
Any VIBE parser, in any language, can run these fixtures to prove it agrees with
the specification byte-for-byte.

## Layout

```
valid/
  <name>.vibe   a conforming document
  <name>.json   its expected value tree (tagged-JSON interchange encoding)
invalid/
  <name>.vibe   a non-conforming document
  <name>.txt    the required error category (a single token)
```

## Interchange encoding

Every scalar is encoded as `{"type": T, "value": V}` where `V` is **always a
string** so no precision is lost across languages:

| VIBE type | tag         | value example           |
|-----------|-------------|-------------------------|
| integer   | `"integer"` | `"9223372036854775807"` |
| float     | `"float"`   | `"3.14"`                |
| boolean   | `"boolean"` | `"true"`                |
| string    | `"string"`  | `"hello"`               |

Objects → JSON objects, arrays → JSON arrays. Structural comparison ignores
object key ordering.

## Error categories (invalid/)

`unclosed-object`, `unclosed-array`, `unterminated-string`, `nested-container`,
`unexpected-token`, `invalid-escape`, `invalid-number`, `depth-exceeded`.

## Running

A harness parses each `.vibe` with the implementation under test, re-encodes the
result into the interchange encoding, and compares it to the sibling `.json`
(for `valid/`) or asserts rejection with the matching category (for `invalid/`).

An implementation may claim **VIBE 1.0 conformance** only if it passes 100% of
`valid/` and rejects 100% of `invalid/`.
