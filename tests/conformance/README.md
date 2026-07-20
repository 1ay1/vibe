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
canonical/
  <name>.vibe   a (possibly non-canonical) conforming document
  <name>.canon  the exact bytes a conforming emitter MUST produce
invalid/
  <name>.vibe   a non-conforming document
  <name>.txt    the required error category (a single token)
```

The `canonical/` tree pins the [Canonical Form](../../SPECIFICATION.md#canonical-form-normative):
for each fixture the harness parses `<name>.vibe`, emits it, and requires the
output to equal `<name>.canon` **byte for byte**; it then re-parses the emitted
text (must yield an equal tree) and emits again (must be identical). This is how
emitter determinism, round-trip, and idempotence are machine-checked.

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

The canonical error-code set (see SPECIFICATION.md → Error Handling → Error Codes):

`encoding-error`, `illegal-character`, `unexpected-token`, `unclosed-object`,
`unclosed-array`, `unterminated-string`, `nested-container`, `invalid-escape`,
`invalid-number`, `limit-exceeded`.

## Running

A harness parses each `.vibe` with the implementation under test, re-encodes the
result into the interchange encoding, and compares it to the sibling `.json`
(for `valid/`) or asserts rejection with the matching category (for `invalid/`).

An implementation may claim **VIBE 1.0 conformance** only if it passes 100% of
`valid/` and rejects 100% of `invalid/`.
