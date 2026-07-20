# Changelog

All notable changes to the VIBE project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
