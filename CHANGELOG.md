# Changelog

All notable changes to the VIBE project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.0] — library API safety

### Changed (source-compatible; no ABI break)
- 🔁 **`vibe_object_set()` and `vibe_array_push()` now return `bool`** instead of
  `void`: `true` when the value is stored, `false` on allocation failure, a NULL
  argument, or a First-Law violation (pushing a container). In every failure
  case the passed value is still freed, so ownership is always consumed and a
  rejected mutation never leaks. Existing callers that ignore the result keep
  compiling and behaving exactly as before — this is why it is a minor bump.
- 📦 SONAME payload is now `libvibe.so.1.2.0` (SOVERSION unchanged at `1`).

### Fixed
- 🐛 **Latent use-after-free in the parser.** When growing an object's entry
  table failed under memory pressure mid-parse, `vibe_object_set()` freed the
  just-created child container but the parse loop still recorded it as the
  active container and kept writing into freed memory. The new `bool` return is
  now checked at every internal call site (parser and `vibe_value_clone`), so
  an allocation failure aborts cleanly with `out-of-memory` instead.

### libvibe — the reference implementation is now a real C library
- 📦 **Static + shared library.** `make` builds `libvibe.a` and a versioned
  shared library (`libvibe.so.1.2.0`, SONAME `libvibe.so.1`; `.dylib` on macOS);
  `make install` lays down the header, both libraries, a `vibe.pc` pkg-config
  file, and the CLI. Consume it with `cc app.c $(pkg-config --cflags --libs vibe)`.
- 🛠️ **`vibe` command-line tool.** `vibe check` (validate + structured error),
  `vibe fmt [-w]` (canonical reformat), `vibe get <path>` (read a scalar),
  `vibe version`.
- ✍️ **Emitter / serialiser.** `vibe_emit()` / `vibe_emit_file()` render a value
  tree back to canonical VIBE text that re-parses to an equal tree; `fmt∘fmt`
  is idempotent.
- 🧵 **Length-aware & stateless parsing.** `vibe_parse_buffer(p, data, len)`
  (NUL-safe) and one-shot `vibe_parse(data, len, &err)` — no parser object.
- 🎛️ **Configurable, enforced resource limits** (`VibeLimits`,
  `vibe_parser_set_limits()`), defaulting to the spec's required minimums.
- 🧯 **Stable error codes on the API**: `VibeError.code` (`VibeErrorCode`) plus
  `vibe_error_code_string()`, matching the spec's vocabulary 1:1.
- 🔌 Pluggable allocator (`vibe_set_allocators()`), version queries
  (`vibe_version()`, `VIBE_VERSION_*`), deep copy (`vibe_value_clone()`),
  `*_or` defaulted getters, `vibe_type_name()`, and container-size helpers.
- 🏷️ ABI hygiene: `extern "C"`, a `VIBE_API` visibility macro, and
  `-fvisibility=hidden` so the shared object exports **only** the public
  `vibe_*` symbols.
- 🧪 Committed conformance runner (`tests/conformance/run.c`, `make conformance`)
  that re-encodes each parsed tree into the interchange encoding and diffs it
  against the fixture, and matches invalid fixtures by error category.
- 🐛 Implementation hardening: `ctype` calls pass `unsigned char` (no UB on
  bytes ≥ 0x80); quoted strings use a growable buffer instead of a fixed 4 KiB
  stack buffer; allocation-failure paths no longer leak or crash; a leading
  UTF-8 BOM reports `encoding-error` specifically. Clean under
  `-Wall -Wextra -Werror -pedantic` and ASan/UBSan.

### Added
- 🔐 **Quoted keys.** Keys that are not valid identifiers (URL paths, header
  names, IP/CIDR, non-ASCII) can now be written as quoted strings, e.g.
  `"/api/login" { ... }`. Resolves a long-standing contradiction where the
  spec's own examples used quoted keys the parser rejected.
- 📐 **Character Encoding & Document Model** spec section: mandatory UTF-8,
  BOM rejection, forbidden code points (NUL, raw control chars), line-terminator
  rules (LF/CRLF/lone-CR), and byte-exact key/value comparison (no Unicode
  normalization). Empty/whitespace/comment-only documents defined as a valid
  empty root object.
- 🚦 **Normative Resource Limits**: a single MUST-enforce table (document size,
  nesting depth, string/key length, elements/keys per container, number length)
  with required minimums, replacing three inconsistent “recommended” copies.
- 🧯 **Structured error model**: a closed set of stable error codes
  (`encoding-error`, `illegal-character`, `unexpected-token`, `unclosed-object`,
  `unclosed-array`, `unterminated-string`, `nested-container`, `invalid-escape`,
  `invalid-number`, `limit-exceeded`) with required 1-based line/column + byte
  offset and a fail-closed guarantee. Tooling matches on codes, not messages.
- 🛡️ **Real security threat model**: untrusted-input contract, the
  parser-differential threat class, and a why-each-attack-fails table
  (billion laughs, deep nesting, memory exhaustion, NUL/encoding smuggling,
  duplicate-key confusion, type coercion).
- 🧪 Six new conformance fixtures: `quoted_keys`, `empty_document`,
  `no_number_coercion` (valid); `empty_key`, `float_overflow`, `bom_prefix`
  (invalid). Suite is 18/18 green.

### Fixed
- 🔢 **Float overflow** (a valid-syntax float too large for binary64) is now
  rejected with `invalid-number` instead of becoming infinity, matching the
  integer-overflow rule.
- 🚫 **Empty keys** (`"" value`) are now rejected.
- 📝 Removed spec footguns/contradictions: `NaN`/`Infinity`/`1.5e10` are strings
  (never floats); the broken `\u1F44D` example; the surrogate-pair contradiction;
  the `null null` reserved-word example; the wrong unquoted-string charset; and
  the “legacy Mac CR is a line terminator” claim.

### Changed
- 📖 Grammar updated with a `key = identifier | quoted_string` production and a
  printable-ASCII `unquoted_char` set that matches the implementation.

## [Conformance milestone]

### Added
- 🧪 Language-neutral **conformance test suite** (`tests/conformance/`) with a
  tagged-JSON interchange encoding and fixed error-category vocabulary; the
  reference parser now passes 100% (7 valid + 7 invalid fixtures).
- 📜 Conformance-grade specification: RFC 2119 language, "The First Law of VIBE",
  a Conformance Test Suite section, and a Versioning & Stability promise.

### Fixed
- 🛑 **Unclosed objects/arrays are now rejected** instead of silently accepted as
  a partial parse (the parser no longer accepts malformed input).
- 🔢 **Out-of-range integers and floats are rejected** (`invalid-number`) instead
  of being silently clamped to `INT64_MAX`/infinity.
- 🚫 Stray `}` at the top level and unexpected tokens where a key is expected now
  raise an error rather than being silently dropped.
- 💧 Fixed a memory leak in `set_error` that orphaned the previous error message
  when a parser was reused across multiple failing parses.
- 🗂️ `vibe_parse_file` now guards against `ftell` returning -1 (non-regular files)
  before allocating.

### Changed
- 📖 Spec honesty pass: `\uXXXX` documented as an OPTIONAL v1.1 escape (a strict
  1.0 parser rejects it), `[index]` path suffix marked implementation-defined,
  the false "streaming parser" guideline removed, and the duplicate-key rule
  reduced to a single normative behavior (last-wins).

## [1.0.0] - 2025-01-15

### Added
- 🎉 Initial release of VIBE format specification v1.0
- ✨ Complete C parser implementation with single-pass O(n) parsing
- 📚 Comprehensive format specification (SPECIFICATION.md)
- 🔧 Full API for parsing, value access, and memory management
- 📖 Detailed README with examples and documentation
- 🧪 Example configuration files (simple, web server, database)
- 🤝 Contributing guidelines (CONTRIBUTING.md)
- 📄 MIT License
- 🚀 GitHub Actions CI/CD pipeline
- 🔍 Memory leak detection with Valgrind
- 📊 Code coverage reporting

### Features
- Support for all basic data types:
  - 64-bit integers
  - Double-precision floats
  - Booleans (true/false)
  - UTF-8 strings (quoted and unquoted)
  - Arrays (inline and multi-line)
  - Nested objects
- Comments with `#`
- String escape sequences (`\"`, `\\`, `\n`, `\t`, `\r`, `\uXXXX`)
- Path-style value access with dot notation
- Detailed error reporting with line/column information
- No reserved words - maximum flexibility
- Whitespace-insensitive (no significant indentation)

### Implementation Details
- Maximum nesting depth: 64 levels
- Initial capacity: 16 elements (dynamic growth)
- POSIX-compliant (strdup usage)
- C11 standard required
- Zero external dependencies

### Examples
- Simple configuration
- Complex web server setup
- Database cluster configuration
- Error test cases

### Documentation
- API reference in vibe.h
- Usage examples in examples/
- Complete specification document
- Contributing guidelines
- Installation instructions

### Testing
- Automated CI on Ubuntu and macOS
- GCC and Clang compiler support
- Valgrind memory leak detection
- Static analysis with extra warnings
- Code coverage reporting

## [Unreleased]

### Planned
- Python bindings
- Rust implementation
- JavaScript/Node.js implementation
- Go implementation
- VS Code syntax highlighting extension
- Conversion tools (JSON↔VIBE, YAML↔VIBE, TOML↔VIBE)
- Schema validation support
- Comprehensive benchmark suite
- Windows support testing

---

**Legend:**
- 🎉 Major release
- ✨ New feature
- 🐛 Bug fix
- 📚 Documentation
- 🔧 Tooling
- 🚀 Performance
- ⚠️ Breaking change
- 🔒 Security fix

[1.0.0]: https://github.com/1ay1/vibe/releases/tag/v1.0.0
