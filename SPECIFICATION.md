# VIBE Format Specification

**Version:** 1.0.0 &nbsp;·&nbsp; **Status:** Stable &nbsp;·&nbsp; **Date:** 2025-01-14  
**Canonical URL:** https://1ay1.github.io/vibe/specification.html  
**License:** MIT

> **VIBE** — *Values In Bracket Expression.* A configuration language with one
> radical rule: **every structured thing has a name.** No anonymous objects, no
> positional records, no indentation guessing. The structure you see is the
> structure you get.

---

## The First Law of VIBE

> **An array MUST NOT contain an object or another array.**

This single sentence is the heart of the language. Every other format lets you
write an *anonymous list of records*:

```json
"replicas": [ { "host": "a" }, { "host": "b" } ]
```

…and every such list is a latent bug: the entries have no stable identity, they
are addressed by a fragile array index, reordering silently rebinds every
reference, and merging two configs is undefined. VIBE forbids the pattern
outright. If a thing is worth structuring, it is worth naming:

```vibe
replicas {
  east { host db-east.internal  port 5432 }
  west { host db-west.internal  port 5432 }
}
```

Arrays in VIBE hold **scalars only** — a bag of numbers, strings, or booleans.
The moment you need structure, you need a key. This is not a missing feature; it
is *the point.* See [The Stability Paradox](docs/Stability_Paradox.md) for the full
argument.

---

## The VIBE Guarantees

VIBE is defined by what it **promises** and what it **refuses** — not by a feature
list. Every guarantee below is enforced by the grammar and verified by the
[conformance suite](#conformance-test-suite).

1. **One Parse.** The same bytes produce the same value tree in *every*
   conforming parser, forever. VIBE has no ambiguous documents — none. If you can
   show two conforming parsers that disagree on any input, that is a spec bug and
   we will fix it.
2. **Named Entities.** Every structured value has a name. There are no anonymous
   records, no positional identity, no `[0]`-addressed objects. Reordering never
   silently rebinds a reference.
3. **No Surprises.** A value is exactly what it looks like. There is no implicit
   coercion that turns your data into something else behind your back (see
   *No Footguns* below).
4. **Frozen Grammar.** The syntax and semantics are locked for all of VIBE 1.x. A
   document you write today parses identically under every future 1.x parser
   (see [Versioning and Stability](#versioning-and-stability)).
5. **Conformance-Tested.** Compliance is not an honor system. A shared,
   language-neutral test suite decides whether a parser is conforming.

### No Footguns

VIBE deliberately does not have the traps that make hand-written config
dangerous in other formats:

- **No “Norway problem.”** `country no` is the string `"no"`, never the boolean
  `false`. Only the exact tokens `true` and `false` are booleans.
- **No accidental octals or number magic.** `007` is the integer `7`; `2.1.0` is
  the string `"2.1.0"`; `10:30` is the string `"10:30"`, not 630 seconds.
- **No significant whitespace.** Indentation is decoration. There are no
  tab-versus-space wars and no way to break a document by re-indenting it.
- **No trailing-comma errors.** VIBE has no value separators to get wrong —
  newlines and spaces separate values.
- **No silent truncation.** An integer that overflows the parser's range is
  *rejected*, not quietly clamped.

### What VIBE Refuses (and Why)

A format is defined by its refusals. VIBE will **never** add these, because each
one re-introduces the ambiguity or instability the language exists to prevent:

| Refused                         | Why |
|---------------------------------|-----|
| Objects/arrays inside arrays    | Anonymous records have no stable identity (the First Law). |
| Anchors, references, aliases    | Non-local action; a value's meaning stops being readable in place (YAML's `&`/`*`). |
| Implicit type coercion          | The Norway problem and its whole family. A token means one thing. |
| Templates, conditionals, `if`   | Config is data, not a program. Logic belongs in your code, not your config file. |
| Significant indentation         | Whitespace-as-structure is the single largest source of config bugs. |
| Multiple ways to write the same thing | One canonical form keeps diffs, merges, and tooling sane. |

> **VIBE is for configuration that humans write by hand — not data that machines
> exchange.** For APIs and wire formats, use JSON. We mean it. VIBE wins by being
> the best possible *config* language, not by trying to be everything.

---

## Conformance and Notation

The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**,
**SHOULD**, **SHOULD NOT**, **RECOMMENDED**, **MAY**, and **OPTIONAL** in this
document are to be interpreted as described in [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119)
and [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174) when, and only when, they
appear in all capitals.

A **conforming VIBE document** is a UTF-8 byte stream that satisfies the grammar
in [§Grammar](#grammar) and every normative **MUST** in this specification.

A **conforming VIBE parser** is a program that accepts every conforming document
and produces the value tree this specification defines for it, and rejects every
non-conforming document with an error. Behavior on non-conforming input is
defined in [§Error Handling](#error-handling); a conforming parser MUST NOT
silently accept malformed input.

Sections marked **(Non-normative)** — notably [§Future Considerations](#future-considerations) —
are informative and impose no requirements.

---

## Table of Contents

1. [The First Law of VIBE](#the-first-law-of-vibe)
2. [The VIBE Guarantees](#the-vibe-guarantees)
3. [Overview](#overview)
4. [Design Goals](#design-goals)
5. [Grammar](#grammar)
6. [Lexical Analysis](#lexical-analysis)
7. [Character Encoding and Document Model](#character-encoding-and-document-model)
8. [Data Types](#data-types)
9. [String Literals](#string-literals)
10. [Comments](#comments)
11. [Arrays](#arrays)
12. [Objects](#objects)
13. [Whitespace and Formatting](#whitespace-and-formatting)
14. [Reserved Words](#reserved-words)
15. [Path Notation](#path-notation)
16. [Duplicate Keys](#duplicate-keys)
17. [Complete Examples](#complete-examples)
18. [Parsing Algorithm](#parsing-algorithm)
19. [Error Handling](#error-handling)
20. [File Format](#file-format)
21. [Conformance Test Suite](#conformance-test-suite)
22. [Implementation Guidelines](#implementation-guidelines)
23. [Security Considerations](#security-considerations)
24. [Performance Requirements](#performance-requirements)
25. [Validation and Schema](#validation-and-schema)
26. [Comparison to Other Formats](#comparison-to-other-formats)
27. [Migration Guide](#migration-guide)
28. [Versioning and Stability](#versioning-and-stability)
29. [Future Considerations](#future-considerations)
30. [References](#references)

## Overview

VIBE (Values In Bracket Expression) is a hierarchical configuration file format designed to optimize both human readability and machine parsing performance. It combines the visual clarity of bracket-delimited structures with minimal syntax overhead, making it ideal for configuration files, data serialization, and human-editable structured data.

VIBE addresses common pain points in existing configuration formats: it eliminates YAML's indentation sensitivity, removes JSON's syntactic overhead (trailing commas, excessive quotes), and provides clearer visual hierarchy than INI files while maintaining simplicity.

### Core Principles

- **Readable**: Structure is immediately apparent through visual inspection, with clear delimiters and minimal noise
- **Writable**: Simple syntax rules make hand-editing natural and error-resistant
- **Parseable**: Deterministic LL(1) grammar enables single-pass parsing with O(n) complexity
- **Unambiguous**: Each data structure has exactly one canonical representation
- **Efficient**: Minimal memory overhead during parsing and low implementation complexity

## Design Goals

### 1. Visual Hierarchy
The structure of data should be instantly recognizable through visual inspection alone. Nested objects use braces `{}` that create clear visual boundaries, while arrays use brackets `[]` for unambiguous collection identification. This bracket-based syntax provides immediate structural context without requiring counting indentation levels or tracking implicit scope.

### 2. Minimal Syntax
VIBE uses exactly 6 core token types (identifiers, strings, numbers, booleans, braces, brackets), making it one of the simplest structured formats to learn and implement. The absence of mandatory commas, semicolons, or colons reduces syntactic noise and simplifies both human editing and parser implementation.

### 3. Unambiguous Grammar
Each data structure has exactly one canonical representation in VIBE. This eliminates formatting debates, ensures consistent machine generation, and enables reliable diffing and version control. The deterministic grammar means there are no edge cases or surprising interpretations.

### 4. Predictable Parsing
The grammar admits single-pass, O(n) parsing with a simple state machine — no
backtracking, no unbounded lookahead, no ambiguous productions. The point is not
raw speed (config files are tiny); it is that parsing is **predictable**: the
same input always produces the same tree with no surprising interpretations, and
any competent parser is trivially fast as a side effect.

### 5. Type Safety
Clear, deterministic type inference rules ensure that values are consistently interpreted across all implementations. Numbers, booleans, and strings are unambiguously distinguished through syntax, eliminating the need for explicit type annotations while maintaining type safety.

### 6. Human-Friendly
The syntax is optimized for human reading and writing. Whitespace is non-significant (except as token separation), comments are allowed anywhere, and unquoted strings reduce visual clutter for simple values. Error messages should provide clear context and actionable guidance for resolution.

## Grammar

### Formal Grammar Definition (EBNF)

```
config        = { statement } ;
statement     = ( assignment | object | array | comment ) [ comment ] newline ;
assignment    = key ( scalar_value | object | array ) ;
object        = key "{" { statement } "}" ;
array         = key "[" [ value_list ] "]" ;
key           = identifier | quoted_string ;
value_list    = scalar_value { ( whitespace | newline ) scalar_value } ;
scalar_value  = string | number | boolean | identifier ;
comment       = "#" { any_char - newline } ;
identifier    = ( letter | "_" ) { letter | digit | "_" | "-" } ;
string        = quoted_string | unquoted_string ;
quoted_string = '"' { string_char | escape_sequence } '"' ;
string_char   = any_char - '"' - "\" - control_char ;
unquoted_string = unquoted_char { unquoted_char } ;
unquoted_char = ( 0x21 ... 0x7E ) - "{" - "}" - "[" - "]" - "#" - '"' ;
number        = [ "-" ] ( integer | float ) ;
integer       = digit { digit } ;
float         = digit { digit } "." digit { digit } ;
boolean       = "true" | "false" ;
escape_sequence = "\" ( '"' | "\" | "n" | "t" | "r" ) ;
(* "\u" hex hex hex hex is reserved for VIBE 1.1; see String Literals *)
```

> **Note on `key`.** A key is normally a bare `identifier`. A `quoted_string` key
> is permitted so that keys which are not valid identifiers — URL paths
> (`"/api/login"`), header names (`"content-type"`), IP/CIDR (`"10.0.0.0/8"`) —
> can still be named. The key's value is the *decoded* string content (after
> escape processing), compared byte-exactly. `unquoted_char` intentionally admits
> the full printable-ASCII range minus the five structural characters and the
> quote, so values like `s3cr3t!@%` are unquoted strings; anything outside that
> range (spaces, non-ASCII, control characters) requires quoting.

### Tokens

VIBE recognizes exactly 5 token types:

1. **IDENTIFIER** - `[a-zA-Z_][a-zA-Z0-9_-]*`
2. **STRING** - Quoted or unquoted text
3. **LEFT_BRACE** - `{`
4. **RIGHT_BRACE** - `}`
5. **LEFT_BRACKET** - `[`
6. **RIGHT_BRACKET** - `]`

### Syntax Rules

#### Assignment
```
key value
```
- The key is a bare identifier, or a quoted string when it is not a valid
  identifier (e.g. `"/api/login"`, `"content-type"`).
- Value continues until end of line or a structural character.
- Multiple words in a value require quotes.

#### Object Declaration
```
name {
  statements...
}
```
- Opens a new scope with nested statements
- Can contain assignments, objects, or arrays
- Must be closed with matching `}`

#### Array Declaration
```
name [value1 value2 value3]
```
or multi-line:
```
name [
  value1
  value2
  value3
]
```
- Space or newline separated items
- No commas or other separators
- Must be closed with matching `]`

## Lexical Analysis

### Tokenization Process

The lexer processes the input character by character, maintaining state to determine token boundaries:

1. **Whitespace Handling**: Spaces, tabs, and newlines are used as separators but are not significant tokens
2. **Comment Recognition**: `#` starts a comment that continues to end of line
3. **String Recognition**: Quoted strings begin with `"` and end with matching `"`
4. **Identifier Recognition**: Alphanumeric characters, underscores, and hyphens
5. **Operator Recognition**: Braces and brackets are single-character tokens

### Character Classes

```
ALPHA     = [a-zA-Z]
DIGIT     = [0-9]
ALNUM     = ALPHA | DIGIT
IDENT_START = ALPHA | "_"
IDENT_CHAR  = ALNUM | "_" | "-"
UNQUOTED_CHAR = [0x21-0x7E] - [{}[]#] - WHITESPACE
WHITESPACE = " " | "\t" | "\r"
NEWLINE   = "\n"
```

### Token Examples

```
# Valid identifiers
server
database_host
ssl-enabled
_private
config2

# Valid unquoted strings
localhost
192.168.1.1
/usr/local/bin
example.com:8080

# Invalid identifiers (would be treated as strings)
2servers    # starts with digit
server.host # contains dot (would be unquoted string)
配置        # Unicode not allowed in identifiers
```

## Character Encoding and Document Model

This section defines, byte-for-byte, what a VIBE document *is*. These rules exist
to close the gaps that cause **parser differentials** — the situation where two
parsers disagree about the same bytes, which is both a correctness bug and a
security vulnerability (see [Security Considerations](#security-considerations)).

### Encoding

1. A VIBE document MUST be a stream of bytes encoded in **UTF-8**.
2. A parser MUST reject any input that is not well-formed UTF-8 with an
   `encoding-error`. Overlong encodings, unpaired surrogate code points
   (U+D800–U+DFFF), and truncated sequences are all ill-formed and MUST be
   rejected. A parser MUST NOT attempt to auto-detect UTF-16, Latin-1, or any
   other encoding.
3. **No byte order mark.** A leading U+FEFF (BOM) MUST be rejected with an
   `encoding-error`, and producers MUST NOT emit one. (A BOM is meaningless in
   UTF-8 and is a common source of “invisible first key” bugs.)

### Forbidden Code Points

1. **U+0000 (NUL) MUST NOT appear anywhere** in a document, including inside
   quoted strings. A parser that receives length-delimited input MUST reject an
   embedded NUL with an `illegal-character` error. *(Implementations built on
   NUL-terminated C strings inherently truncate at the first NUL; such an
   implementation MUST document this and SHOULD provide a length-aware entry
   point to remain safe against NUL-smuggling.)*
2. Raw C0 control characters (U+0001–U+001F) and U+007F **MUST NOT appear
   literally inside quoted strings**, with the sole exception of the horizontal
   tab (U+0009). To include other control characters, use an escape such as
   `\n`, `\r`, or `\t`. This keeps a copy-pasted config from carrying invisible,
   diff-hostile bytes.
3. Unicode noncharacters (U+FFFE, U+FFFF, and the like) are permitted inside
   quoted strings but SHOULD be avoided.

### Line Terminators

1. The statement separator is the line feed **LF (U+000A)**.
2. **CRLF (U+000D U+000A) is accepted**: the carriage return is treated as
   insignificant whitespace and the LF terminates the line. This makes VIBE
   robust to Windows editors and `git` autocrlf.
3. A **lone CR** (classic-Mac line ending) is **not** a line terminator; it is
   insignificant whitespace. Producers targeting maximum portability SHOULD use
   LF. A parser MUST report line numbers by counting LF terminators only, so that
   error positions are stable across platforms.

### Whitespace

Outside quoted strings, the space (U+0020), horizontal tab (U+0009), and carriage
return (U+000D) are **insignificant** — they separate tokens but carry no meaning.
Indentation is never significant. There is no other whitespace: a parser MUST NOT
treat U+00A0 (no-break space), U+2028/U+2029, or other Unicode spaces as token
separators; they are ordinary characters and, outside quoted strings, an
`illegal-character` error.

### The Document Model

1. A document is a sequence of statements that populate an implicit **root
   object**. The parse result is always an object (never a bare scalar or array).
2. An **empty document** — zero bytes, or a document containing only whitespace,
   newlines, and comments — is **valid** and yields an **empty root object**.
   This is the canonical “nothing configured” state; a parser MUST NOT error on it.
3. Keys and string contents are compared and stored **byte-exactly**. A parser
   MUST NOT apply Unicode normalization (NFC/NFD), case folding, or trimming to
   keys or values. `"caf\u00e9"` (composed) and `"cafe\u0301"` (decomposed) are
   *different* keys. Since identifiers are ASCII-only (below), this only affects
   quoted keys and string values.

## Data Types

VIBE supports 5 fundamental data types with automatic type inference based on
syntax patterns. Type inference is **total and deterministic**: every token maps
to exactly one type, and the mapping is identical in every conforming parser.

### Type Inference Algorithm

Type determination follows a strict precedence order to ensure unambiguous interpretation:

```
function inferType(value):
    stripped = strip_whitespace(value)
    
    # Check in order of specificity
    if stripped matches /^-?\d+$/
        return INTEGER
    else if stripped matches /^-?\d+\.\d+$/
        return FLOAT
    else if stripped == "true" or stripped == "false"
        return BOOLEAN
    else if starts_with(stripped, '"') and ends_with(stripped, '"')
        return STRING (quoted, with escape sequence processing)
    else
        return STRING (unquoted, literal value)
```

**Key Points:**
- Type inference is deterministic and order-independent
- No ambiguity between types - each syntax pattern maps to exactly one type
- Implementations must follow this exact algorithm for compatibility

### Integer Type

**Definition**: A sequence of digits optionally preceded by a minus sign, representing whole numbers.

**Syntax Pattern**: `-?[0-9]+`

**Range**: Implementations MUST support at least 64-bit signed integers (−2^63 to 2^63−1). A value that does not fit the implementation's integer type MUST be rejected with an `invalid-number` error — a conforming parser MUST NOT silently clamp, wrap, or truncate an out-of-range integer, because doing so corrupts configuration without warning.

**Examples**:
```
count 42
negative -17
zero 0
large 9223372036854775807
min_value -9223372036854775808
```

**Rules**:
- Leading zeros are allowed but discouraged (e.g., `007` is valid but should be written as `7`)
- No thousands separators (commas, underscores, or spaces)
- Plus sign prefix is not supported (use unsigned form)
- Scientific notation is not supported for integers

**Invalid Syntax**:
```
with_comma 1,000    # Commas not allowed
with_space 1 000    # Spaces not allowed
with_plus +42       # Plus sign not supported
hex 0xFF            # Only decimal notation supported
```

### Float Type

**Definition**: A sequence of digits with exactly one decimal point, representing real numbers.

**Syntax Pattern**: `-?[0-9]+\.[0-9]+`

**Range**: A float MUST be represented as at least IEEE 754 double precision
(binary64). A syntactically valid float whose magnitude is too large to represent
(it would round to infinity) MUST be rejected with an `invalid-number` error —
just like an out-of-range integer, a parser MUST NOT silently substitute infinity.
Values that merely lose precision in the low-order digits (the normal case for
binary64) are accepted and rounded to the nearest representable double, which is
deterministic per IEEE 754.

**Examples**:
```
pi 3.14159
negative -0.5
zero 0.0
small 0.001
precise 123.456789012345
```

**Rules**:
- Decimal point is REQUIRED (distinguishes from integers).
- At least one digit MUST appear on both sides of the decimal point.
- Exactly one decimal point; `1.2.3` is a **string**, not a float.
- Leading zeros after the decimal are significant (`0.001` ≠ `0.1`).
- Negative zero (`-0.0`) is preserved as the IEEE 754 value −0.0.

**No special float values, no exponents — by design.** These tokens are **strings**,
never floats, because silently minting a non-finite number from a bare word is
exactly the [Norway-problem](#the-vibe-guarantees) class of bug:

| Token       | VIBE type | Note |
|-------------|-----------|------|
| `NaN`, `nan`, `Infinity`, `inf` | **string** | Not-a-number and infinities are never inferred. |
| `1.5e10`, `6.022e23` | **string** | Scientific/exponential notation is not a VIBE number. |
| `0x1p4`, `1_000.0` | **string** | No hex floats, no digit separators. |

If an application genuinely needs infinity or scientific notation, it stores the
string and interprets it itself — the format refuses to guess.

**Invalid Syntax** (rejected, not reinterpreted):
```
trailing_dot 42.    # requires digits after the decimal
leading_dot .42     # requires digits before the decimal
```

### Boolean Type

**Definition**: Logical true/false values represented by the exact literals `true` or `false`.

**Syntax**: Lowercase keywords only: `true` | `false`

**Case Sensitivity**: VIBE is case-sensitive for boolean literals. `True`, `TRUE`, `False`, `FALSE`, or any other capitalization variants are treated as unquoted strings, not booleans.

**Examples**:
```
enabled true
debug false
production true
maintenance_mode false
```

**Common Mistakes**:
```
enabled True        # This is a STRING "True", not boolean
debug FALSE         # This is a STRING "FALSE", not boolean  
active yes          # This is a STRING "yes", not boolean
disabled no         # This is a STRING "no", not boolean
```

**Design Rationale**: Using only lowercase `true` and `false` eliminates ambiguity and aligns with most programming languages (JavaScript, Python, JSON). Alternative boolean representations (yes/no, on/off, 1/0) can be used as strings and interpreted by application logic if needed.

### String Type

Strings represent textual data and come in two forms: **quoted** and **unquoted**.

#### Unquoted Strings

Unquoted strings provide a cleaner syntax for simple identifiers and values without spaces or special characters.

**Syntax**: Any sequence of non-whitespace, non-special characters

**Valid Characters**: Letters, digits, and the characters: `_`, `-`, `.`, `/`, `:`, `@`

**Termination**: Unquoted strings end at:
- Whitespace (space, tab, newline)
- Structural characters (`{`, `}`, `[`, `]`)
- Comment start (`#`)

**Restrictions**:
- MUST NOT match the pattern of a number (integer or float)
- MUST NOT be exactly `true` or `false` (reserved for booleans)
- MUST NOT contain spaces or special characters that could create ambiguity
- Unicode characters beyond ASCII require quoted strings

**Examples**:
```
hostname server1.example.com
path /usr/local/bin
email admin@example.com
version 2.1.0-beta
identifier user_id_123
url_path /api/v1/users
```

**When to Quote**: Use quoted strings when:
- Value contains spaces
- Value matches a number pattern but should be a string
- Value is `true` or `false` but should be a string
- Value contains special characters (`#`, `{`, `}`, `[`, `]`)
- Value contains Unicode characters beyond ASCII
protocol https
url https://api.example.com/v1
email user@example.com
port_name 8080  # String, not number, because of underscore
password s3cr3t!@#$%^&*()
connection_string postgres://user:pass@host:5432/db

# Unicode values must be quoted
language "中文"
protocol "сервер"
port "データベース"

# Structural characters must be quoted when ambiguous
config_with_braces "{\"key\": \"value\"}"
array_syntax "[1, 2, 3]"
comment_text "This contains # a hash"
```

#### Quoted Strings

**Delimiters**: Double quotes `"`

**Required When**:
- Value contains whitespace
- Value contains special characters: `# { } [ ]`
- Value has leading/trailing whitespace
- Value contains Unicode (non-ASCII) characters
- Value would otherwise be ambiguous

**Examples**:
```
message "Hello, world!"
path_with_spaces "/Program Files/App"
special "value with # comment character"
empty ""
just_spaces "   "
unicode_string "你好 世界"  # Unicode requires quotes
mixed_special "配置 # with comment char"
```

### Array Type

**Definition**: Ordered collection of scalar values (integers, floats, booleans, strings)

**Syntax**: Values separated by whitespace, enclosed in `[]`

**Restrictions**:
- Arrays can **ONLY** contain scalar values: integers, floats, booleans, and strings
- **Objects are NOT allowed inside arrays** - The parser will reject this with an error
- **Nested arrays are NOT allowed** - Arrays cannot contain other arrays
- All values are parsed independently as primitives
- Use named objects instead of object arrays for complex data

> **Design Philosophy**: This restriction is intentional and core to VIBE's design. Arrays of objects create instability through index-based references, ambiguous merging, and lack of inherent identity. VIBE forces you to use named objects, which provide stable path-based references and deterministic configuration merging. See [The Stability Paradox](docs/Stability_Paradox.md) for the full rationale.

**Examples**:
```
numbers [1 2 3]
strings [hello world foo]
mixed [42 "hello" true 3.14]
hosts [server1.com server2.com server3.com]
flags [enabled debug verbose]
```

**Invalid** (objects in arrays not allowed):
```
# ❌ This is NOT valid VIBE syntax - parser will reject this
servers [
  {
    host server1.com
    port 8080
  }
]

# ❌ Nested arrays also not allowed
matrix [
  [1 2 3]
  [4 5 6]
]
```

**Error messages you'll see**:
- `"Objects cannot be placed inside arrays. Use named objects instead."`
- `"Arrays cannot be nested inside other arrays."`

**Use this pattern instead**:
```
# Use named objects for complex data
servers {
  primary {
    host server1.com
    port 8080
  }
  secondary {
    host server2.com
    port 8081
  }
}
```

### Object Type

**Definition**: Unordered collection of key-value pairs

**Syntax**: Statements enclosed in `{}`

**Examples**:
```
config {
  host localhost
  port 8080
}
```

## String Literals

### Escape Sequences

Quoted strings support the following escape sequences. A conforming **VIBE 1.0**
parser MUST recognize all five core escapes:

| Escape | Character | Unicode | Description | Since |
|--------|-----------|---------|-------------|-------|
| `\"` | `"` | U+0022 | Double quote | 1.0 |
| `\\` | `\` | U+005C | Backslash | 1.0 |
| `\n` | | U+000A | Newline (LF) | 1.0 |
| `\r` | | U+000D | Carriage return (CR) | 1.0 |
| `\t` | | U+0009 | Horizontal tab | 1.0 |
| `\uXXXX` | | U+XXXX | Unicode codepoint (4 hex digits) | 1.1 |

Any backslash **not** followed by one of the escapes above (for its version) is
an `invalid-escape` error. In particular, a strict VIBE 1.0 parser — including
the reference implementation — MUST reject `\uXXXX` with `invalid-escape`;
`\uXXXX` support is introduced in VIBE 1.1 and is OPTIONAL in 1.0. Note that a
Unicode character embedded directly in a quoted UTF-8 string needs no escape at
all — `\uXXXX` exists only for encoding codepoints that are awkward to type.

### Unicode Support

- A document MUST be well-formed UTF-8 (see [Character Encoding and Document
  Model](#character-encoding-and-document-model)).
- Non-ASCII characters MAY appear directly, unescaped, inside **quoted strings**
  and **comments** — and this is the RECOMMENDED way to write them (`"café"`, not
  an escape).
- **Identifiers and unquoted strings are ASCII-only** (bytes 0x21–0x7E). To use a
  key or value containing non-ASCII text, quote it. A non-ASCII byte where an
  identifier is expected is an `illegal-character` error.
- The `\uXXXX` escape (four hex digits, Basic Multilingual Plane) is a **VIBE 1.1**
  feature and is OPTIONAL; a strict 1.0 parser rejects it with `invalid-escape`.
  There is intentionally **no** `\U`-style 8-digit escape and no surrogate-pair
  escaping: characters outside the BMP (emoji, etc.) are written **directly** as
  UTF-8 inside a quoted string, which needs no escape at all.

**Examples**:
```
greeting "Hello, world! 🌍"     # astral character written directly — the normal way
thumbs_up "👍 nice"             # no escape needed for emoji
escaped "She said \"Hello\""
multiline "Line 1\nLine 2\nLine 3"
windows_path "C:\\Users\\Name"  # backslashes must be escaped

# ASCII identifiers with Unicode quoted values
configuration "aplicación"       # ASCII key, quoted Unicode value
settings "сервер"
language english               # ASCII key and unquoted ASCII value
language "español"             # ASCII key, quoted Unicode value
```

### String Parsing Rules

1. **Unquoted String Parsing**:
   - Starts with first non-whitespace character.
   - Continues until whitespace, a structural character, or end of line.
   - Cannot contain `" # { } [ ]`.
   - ASCII printable only (0x21–0x7E); e.g. `s3cr3t!@%`, `/usr/local`, `10.0.0.1`,
     `https://x.example` are all valid unquoted strings. Non-ASCII or
     whitespace-containing values must be quoted.

2. **Quoted String Parsing**:
   - Starts with `"`
   - Continues until matching `"` (not escaped)
   - Must be closed on same logical line
   - Processes escape sequences

3. **Empty Strings**:
   - Only possible with quoted strings: `""`
   - Unquoted empty values are not allowed

## Comments

### Syntax

Comments start with `#` and continue to the end of the line.

```
# This is a full line comment
key value  # This is an inline comment
```

### Rules

1. **Placement**: Comments can appear anywhere except inside quoted strings
2. **Preservation**: Comments are not preserved in the parsed data structure
3. **Escaping**: `#` inside quoted strings is literal, not a comment start
4. **Unicode**: Comments can contain any valid UTF-8 characters

### Examples

```
# Application configuration file
# Author: John Doe
# Date: 2025-01-14

app {
  name "My App"  # Application display name
  version 1.0    # Semantic version
  # debug true   # Commented out debug flag
}

database {
  # Database connection settings
  host localhost      # Development database
  port 5432          # PostgreSQL default port
  name "app_db"      # Database name with # character
}
```

### Comment Best Practices

1. Use comments to explain non-obvious configuration choices
2. Include units for numeric values when relevant  
3. Document valid value ranges or formats
4. Explain the purpose of complex nested structures
5. Feel free to add encouraging comments - good vibes are contagious!

```
timeout 30  # seconds - just enough time to grab a coffee ☕
retries 3   # third time's the charm!
```

## Arrays

### Syntax Variants

#### Inline Arrays
```
ports [8080 8081 8082]
hosts [server1 server2 server3]
```

#### Multi-line Arrays
```
servers [
  prod.example.com
  staging.example.com
  dev.example.com
]
```

#### Mixed Format (Not Recommended)
```
mixed [item1 item2
  item3 item4]
```

### Array Content Rules

1. **Type Mixing**: Arrays can contain values of different types
2. **Nesting**: Arrays cannot directly contain other arrays or objects
3. **Empty Arrays**: `[]` represents an empty array
4. **Whitespace**: Items separated by any amount of whitespace
5. **Comments**: Comments can appear between array items

### Examples

```
# Homogeneous arrays
numbers [1 2 3 4 5]
strings [apple orange banana]
booleans [true false true]

# Heterogeneous arrays
mixed [42 "hello" true 3.14]

# Multi-line with comments
servers [
  # Production servers
  prod1.example.com
  prod2.example.com
  
  # Staging server
  staging.example.com
]

# Empty array
empty_list []
```

### Array Access

How arrays are accessed is an **API concern of the host implementation**, not part
of the document format. Implementations typically expose element access by
integer index (e.g. a `vibe_array_get(arr, 0)` call). Whether *path strings* also
support an `[index]` suffix (e.g. `"servers[0]"`) is OPTIONAL and
implementation-defined; the reference C implementation resolves dotted keys only
and does not interpret `[index]` inside a path string. Portable code SHOULD fetch
the array by path and then index it through the array API.

## Objects

### Syntax

Objects are defined using braces and can contain any number of statements:

```
object_name {
  key1 value1
  key2 value2
  nested_object {
    nested_key nested_value
  }
}
```

### Nesting Rules

1. **Unlimited Depth**: Objects can be nested to arbitrary depth (subject to implementation limits)
2. **Mixed Content**: Objects can contain assignments, nested objects, and arrays
3. **Empty Objects**: `{}` represents an empty object
4. **Scope**: Each object creates a new scope for key names

### Examples

```
# Simple object
server {
  host localhost
  port 8080
}

# Nested objects
application {
  name "My App"
  
  database {
    host db.example.com
    port 5432
    
    connection_pool {
      min_size 5
      max_size 20
      timeout 30.0
    }
  }
  
  cache {
    type redis
    host cache.example.com
    
    settings {
      max_memory 1gb
      eviction_policy lru
    }
  }
}

# Object with arrays
web_server {
  hosts [
    web1.example.com
    web2.example.com
  ]
  
  ssl {
    enabled true
    protocols [TLSv1.2 TLSv1.3]
  }
}
```

### Key Names

A key is either a **bare identifier** or a **quoted string**:

1. **Identifier keys** match `[A-Za-z_][A-Za-z0-9_-]*` and are the common case:
   `host`, `max_connections`, `ssl-enabled`.
2. **Quoted keys** are used when the key is not a valid identifier — it contains
   slashes, dots, colons, spaces, or non-ASCII text. The key is the *decoded*
   string content:
   ```
   rate_limits {
     "/api/auth/login" { rpm 5 }
     "/api/orders"     { rpm 30 }
     "content-type"    application/json
   }
   ```
3. Keys are compared **byte-exactly** with no normalization or case folding
   (see [Character Encoding and Document Model](#character-encoding-and-document-model)).
   `host` and `"host"` denote the **same** key.
4. A key MUST NOT be empty: `""` is not a valid key (`unexpected-token`).
5. Key length is bounded by the [resource limits](#resource-limits-normative).

### Object Access

Objects are typically accessed using dot notation:
- `server.host` → `"localhost"`
- `application.database.port` → `5432`
- `web_server.ssl.protocols` → the array `[TLSv1.2 TLSv1.3]` (index it via the array API)

## Whitespace and Formatting

### Whitespace Rules

1. **Insignificant Indentation**: Indentation is purely visual and not syntactically meaningful
2. **Flexible Spacing**: Any amount of whitespace can separate tokens
3. **Line Breaks**: Statements are separated by newlines
4. **Visual Alignment**: Encouraged for readability but not required

### Recommended Style

```
# Good: Consistent 2-space indentation
server {
  host localhost
  port 8080
  
  ssl {
    enabled true
    cert_path /etc/ssl/cert.pem
  }
}

# Also valid: No indentation
server {
host localhost
port 8080
ssl {
enabled true
cert_path /etc/ssl/cert.pem
}
}

# Also valid: Tab indentation
server {
	host localhost
	port 8080
}
```

### Formatting Guidelines

1. **Indentation**: Use 2 or 4 spaces consistently
2. **Blank Lines**: Use blank lines to separate logical sections
3. **Alignment**: Align values for related keys when it improves readability
4. **Array Formatting**: Use multi-line format for arrays with more than 3-4 items

## Reserved Words

**None.** VIBE has no reserved words. Any valid identifier can be used as a key name. We don't believe in gatekeeping - all words are welcome in the VIBE family.

```
# All of these are valid key names
true true
false false
if conditional
for loop_config
class object_type
vibe excellent  # meta!

# Keys that are not valid identifiers just need quoting
"content-type" application/json
"/api/login" endpoint
"192.168.0.0/16" internal

# Unicode content must be in quoted strings
chinese_name "配置"
russian_name "сервер"
spanish_name "configuración"
```

VIBE has no `null`: absence is expressed by omitting the key, which keeps the
value model total (every present key has a concrete typed value). This design
choice eliminates the need to escape or quote common configuration key names.
Because reserving words is just not the vibe.

## Path Notation

For programmatic access to nested values, implementations MUST support dot
notation over object keys. An optional `[index]` suffix for indexing into arrays
from within a path string is a common convenience but is **implementation-defined**
— it is not required for conformance, and the reference C implementation does not
provide it (fetch the array by dotted path, then index it through the array API).

### Syntax

```
object.property
object.nested_object.property
object.array            # resolves to the array value; index via the array API
object.array[index]     # OPTIONAL sugar, implementation-defined
```

### Examples

Given this VIBE configuration:
```
app {
  name "My Application"
  version 1.0
  
  database {
    hosts [db1.example.com db2.example.com]
    port 5432
  }
  
  features [auth api cache]
}
```

Path access:
- `app.name` → `"My Application"`
- `app.version` → `1.0`
- `app.database.port` → `5432`
- `app.database.hosts` → the array `[db1.example.com db2.example.com]`
- `app.features` → the array `[auth api cache]`

(To reach an individual element such as `db1.example.com`, resolve the array by
path and then index it through the implementation's array API.)

### Implementation Notes

1. **Case Sensitivity**: Paths are case-sensitive
2. **Array Bounds**: Out-of-bounds array access should return null/undefined
3. **Missing Keys**: Access to non-existent keys should return null/undefined
4. **Type Safety**: Implementations may provide typed access methods

## Duplicate Keys

### Behavior

When the same key appears more than once **in the same scope**, the **last
assignment wins** — later values replace earlier ones. This is the single
normative rule for VIBE 1.0: a conforming parser MUST resolve a duplicate key to
its final assignment, and the resulting value tree contains that key exactly
once.

```
port 8080
host localhost
port 9000  # the resolved value of `port` is 9000
```

One deterministic rule (rather than a menu of options) is what keeps documents
portable across implementations: the same bytes always produce the same tree.

> **Rationale.** “Error on duplicate” and “merge into an array” were both
> considered and rejected. Erroring punishes the common, harmless case of
> overriding a default; array-merging silently changes a value's *type* based on
> how many times a key appears, which is exactly the kind of positional fragility
> the [First Law](#the-first-law-of-vibe) exists to eliminate. Last-wins is the
> only rule that is both forgiving and unambiguous. A **linting** layer MAY warn
> on duplicates, but the parse result is defined.

### Scope Rules

Duplicate keys are only considered within the same scope:

```
server {
  port 8080  # Different scope
}

client {
  port 3000  # Different scope - this is allowed
}
```

## Complete Examples

### Web Application Configuration

```
# Web Application Configuration
# Environment: Production

application {
  name "E-commerce API"
  version 2.1.4
  environment production
  debug false
  
  # Server configuration
  server {
    host 0.0.0.0
    port 8080
    
    ssl {
      enabled true
      cert_file /etc/ssl/certs/api.crt
      key_file /etc/ssl/private/api.key
      protocols [TLSv1.2 TLSv1.3]
    }
    
    timeouts {
      read 30
      write 30
      idle 120
      shutdown 10
    }
  }
  
  # Database configuration
  database {
    primary {
      driver postgresql
      host db-primary.internal
      port 5432
      database ecommerce_prod
      username api_user
      password_file /etc/secrets/db_password
      
      pool {
        min_connections 10
        max_connections 50
        idle_timeout 300
        max_lifetime 3600
      }
    }
    
    replicas {
      east { host db-replica-east.internal  port 5432 }
      west { host db-replica-west.internal  port 5432 }
      analytics { host db-replica-analytics.internal  port 5432 }
    }
    
    migrations {
      auto_migrate false
      directory /app/migrations
    }
  }
  
  # Cache configuration
  cache {
    type redis
    
    primary {
      host cache-primary.internal
      port 6379
      database 0
      max_connections 20
    }
    
    cluster {
      node1 { host cache1.internal  port 6379 }
      node2 { host cache2.internal  port 6379 }
      node3 { host cache3.internal  port 6379 }
    }
    
    settings {
      default_ttl 3600
      max_memory 2gb
      eviction_policy allkeys-lru
    }
  }
  
  # Logging configuration
  logging {
    level info
    format json
    
    outputs [
      stdout
      /var/log/app/application.log
      /var/log/app/errors.log
    ]
    
    loggers {
      database {
        level debug
        output /var/log/app/database.log
      }
      
      security {
        level warn
        output /var/log/app/security.log
      }
    }
  }
  
  # Feature flags
  features {
    payment_v2 true
    recommendation_engine true
    beta_checkout false
    advanced_search true
  }
  
  # External services
  services {
    payment_gateway {
      url https://api.payments.example.com
      api_key_file /etc/secrets/payment_api_key
      timeout 10
      retry_attempts 3
    }
    
    email {
      provider sendgrid
      api_key_file /etc/secrets/sendgrid_api_key
      from_address "noreply@mystore.com"
      
      templates {
        welcome_email template_123
        order_confirmation template_456
        password_reset template_789
      }
    }
    
    analytics {
      provider google_analytics
      tracking_id "GA-XXXXXXXX-X"
      
      events [
        page_view
        purchase
        signup
        cart_abandonment
      ]
    }
  }
  
  # Security settings
  security {
    cors {
      enabled true
      allowed_origins [
        https://mystore.com
        https://admin.mystore.com
      ]
      allowed_methods [GET POST PUT DELETE OPTIONS]
      max_age 86400
    }
    
    rate_limiting {
      enabled true
      requests_per_minute 60
      burst_size 10
      
      endpoints {
        "/api/auth/login" {
          requests_per_minute 5
          burst_size 2
        }
        
        "/api/orders" {
          requests_per_minute 30
          burst_size 5
        }
      }
    }
    
    jwt {
      secret_file /etc/secrets/jwt_secret
      expiry 3600
      refresh_expiry 604800
      issuer "mystore-api"
    }
  }
  
  # Monitoring and health checks
  monitoring {
    health_check {
      enabled true
      path /health
      interval 30
      timeout 5
    }
    
    metrics {
      enabled true
      provider prometheus
      path /metrics
      
      custom_metrics [
        order_processing_time
        cart_conversion_rate
        api_response_time
      ]
    }
    
    alerts {
      error_rate_threshold 5.0
      response_time_threshold 500
      
      notifications {
        email admin@mystore.com
        slack "#alerts"
        pagerduty true
      }
    }
  }
}
```

### Development Environment Override

```
# Development overrides
application {
  environment development
  debug true
  
  server {
    port 3000
    
    ssl {
      enabled false
    }
  }
  
  database {
    primary {
      host localhost
      database ecommerce_dev
      username dev_user
      password "dev_password_123"
    }
    
    replicas {}  # No replicas in development
  }
  
  cache {
    type memory  # In-memory cache for development
  }
  
  logging {
    level debug
    format text
    outputs [stdout]
  }
  
  features {
    payment_v2 true
    beta_checkout true  # Enable beta features in dev
  }
}
```

## Parsing Algorithm

### State Machine

The VIBE parser is implemented as a state machine with the following states:

```
States:
- ROOT: Expecting top-level statements
- OBJECT: Inside an object, expecting statements
- ARRAY: Inside an array, expecting values
- VALUE: Reading a value
- COMMENT: Reading a comment
```

### Parsing Process

```
1. Initialize parser state to ROOT
2. Read next token
3. Based on current state and token type:
   - ROOT state:
     - IDENTIFIER: Read as key, transition to VALUE
     - IDENTIFIER + '{': Create object, push OBJECT state
     - IDENTIFIER + '[': Create array, push ARRAY state
     - '#': Transition to COMMENT state
     - EOF: End parsing
   
   - OBJECT state:
     - IDENTIFIER: Read as key, transition to VALUE
     - IDENTIFIER + '{': Create nested object, push OBJECT state
     - IDENTIFIER + '[': Create array, push ARRAY state
     - '}': Pop state, return to previous
     - '#': Transition to COMMENT state
   
   - ARRAY state:
     - STRING/NUMBER/BOOLEAN/IDENTIFIER: Add to array
     - ']': Pop state, return to previous
     - '#': Transition to COMMENT state
   
   - VALUE state:
     - STRING/NUMBER/BOOLEAN/IDENTIFIER: Set value, return to previous state
   
   - COMMENT state:
     - NEWLINE: Return to previous state
     - Any other: Continue reading comment
```

### Parser Implementation Example (Pseudocode)

```c
typedef struct {
    TokenType type;
    char* value;
    int line;
    int column;
} Token;

typedef struct {
    ParseState state;
    int depth;
    VibeValue* current_object;
    char* current_key;
} Parser;

VibeValue* parse_vibe(char* input) {
    Parser parser = {0};
    parser.state = ROOT;
    parser.current_object = create_object();
    
    Lexer lexer = init_lexer(input);
    Token token;
    
    while ((token = next_token(&lexer)).type != EOF_TOKEN) {
        switch (parser.state) {
            case ROOT:
            case OBJECT:
                if (token.type == IDENTIFIER) {
                    parser.current_key = token.value;
                    Token next = peek_token(&lexer);
                    
                    if (next.type == LEFT_BRACE) {
                        consume_token(&lexer); // consume '{'
                        push_object(&parser, token.value);
                    } else if (next.type == LEFT_BRACKET) {
                        consume_token(&lexer); // consume '['
                        push_array(&parser, token.value);
                    } else {
                        parser.state = VALUE;
                    }
                } else if (token.type == RIGHT_BRACE) {
                    pop_state(&parser);
                }
                break;
                
            case ARRAY:
                if (token.type == RIGHT_BRACKET) {
                    pop_state(&parser);
                } else {
                    add_array_value(&parser, parse_value(token));
                }
                break;
                
            case VALUE:
                set_object_value(&parser, parser.current_key, parse_value(token));
                parser.state = (parser.depth > 0) ? OBJECT : ROOT;
                break;
        }
    }
    
    return parser.current_object;
}
```

## Error Handling

Errors are part of the contract, not an afterthought. Production tooling (editors,
CI, config validators) needs errors that are **stable, located, and machine-
readable**. This section defines that contract.

### Fail-Closed Guarantee

A conforming parser is **all-or-nothing**: it either returns the complete value
tree for a fully valid document, or it reports an error and returns **no tree**.
A parser MUST NOT return a partial or “best-effort” result, and MUST NOT continue
as if a malformed construct were absent. There is no error recovery, no panic-mode
resynchronization, and no partial parse — silently salvaging broken input is how
configuration bugs reach production.

### Error Codes

Every rejection MUST carry one of the following **stable error codes**. These are
the same tokens used by the [Conformance Test Suite](#conformance-test-suite), so
a parser's behavior is directly testable. The set is closed for VIBE 1.x; new
codes may only be *added* in a minor version.

| Code | Meaning |
|------|---------|
| `encoding-error` | Input is not well-formed UTF-8, or begins with a BOM. |
| `illegal-character` | A byte not permitted in this context (NUL, raw control char, stray `+`, non-ASCII identifier byte). |
| `unexpected-token` | A structural token where a key or value was required (incl. stray `}` / `]`). |
| `unclosed-object` | `{` with no matching `}` before end of input. |
| `unclosed-array` | `[` with no matching `]` before end of input. |
| `unterminated-string` | `"` with no closing `"` on the same logical line. |
| `nested-container` | An object or array inside an array (violates the First Law). |
| `invalid-escape` | A backslash escape not defined for this VIBE version. |
| `invalid-number` | A numeric token out of range or otherwise malformed. |
| `limit-exceeded` | A [resource limit](#resource-limits-normative) was exceeded. |

### Error Position

An error report MUST include the position of the offending construct:

1. **Line** — 1-based, counting LF terminators only (stable across LF/CRLF).
2. **Column** — 1-based, counted in Unicode scalar values (not bytes) from the
   start of the line.
3. **Byte offset** — 0-based offset into the document, RECOMMENDED, for tools that
   index by byte.

An error SHOULD also carry a human-readable message. Messages are for humans and
are **not** part of conformance — only the code and position are. This lets
implementations phrase messages freely (and yes, keep the good vibes) without
breaking tools that match on codes.

### Example Report

A structured error, and a matching human rendering:

```
{ "code": "unclosed-object", "line": 12, "column": 10, "byte": 214 }
```
```
error[unclosed-object]: object opened here was never closed
  --> config.vibe:12:10
   |
12 |   server {
   |          ^ expected '}' before end of file
```

EDITORS AND CI SHOULD match on `code`; never scrape the message text.

## File Format

### File Extension
**Recommended**: `.vibe`

Alternative extensions: `.vb`, `.config`, `.conf`

### MIME Type
**Proposed**: `application/vibe`  
**Alternative**: `text/vibe`

### Character Encoding

**Required**: UTF-8, no BOM. See [Character Encoding and Document
Model](#character-encoding-and-document-model) for the full normative rules
(UTF-8 validation, forbidden code points, NUL handling). A parser MUST reject a
leading BOM and any ill-formed UTF-8.

### Line Endings

- **LF (`\n`)** — the canonical statement terminator. **RECOMMENDED** for all
  documents.
- **CRLF (`\r\n`)** — accepted; the CR is insignificant whitespace.
- **Lone CR (`\r`)** — **not** a line terminator (it is insignificant
  whitespace). Classic-Mac line endings therefore merge statements onto one
  logical line; convert such files to LF. Line numbers are counted by LF only,
  so error positions are identical on every platform.

### Resource Limits (Normative)

Unbounded input is a denial-of-service vector. A conforming parser **MUST**
enforce a finite limit on each dimension below and **MUST** reject a document
that exceeds one with a `limit-exceeded` error (a parser MUST fail closed, never
truncate silently). The values below are **REQUIRED minimums**: a conforming
parser MUST accept documents up to at least these limits, and MAY expose higher
limits as explicit, opt-in configuration.

| Dimension | REQUIRED minimum a parser must accept | Purpose |
|-----------|----------------------------------------|---------|
| Document size | 16 MiB | Bounds total memory. |
| Nesting depth (objects/arrays) | 64 levels | Prevents stack exhaustion / “billion laughs”. |
| Quoted-string length (decoded) | 1 MiB | Bounds a single allocation. |
| Identifier / key length | 1 KiB | Bounds key storage. |
| Array elements per array | 65 536 | Bounds a single container. |
| Keys per object | 65 536 | Bounds a single container. |
| Digits in a numeric token | 64 | Bounds number parsing work. |

Rationale: fixed, published limits are what make VIBE **safe to parse from
untrusted input**. Because every conforming parser accepts at least these
minimums, a document within them is portable; because every parser also *caps*
its input, no document can exhaust memory or the stack. Limits MUST be checked
before the corresponding allocation, not after.

## Conformance Test Suite

A specification is only as real as the tests that pin it down. VIBE ships a
language-neutral conformance suite so that **any** parser — in any language — can
prove it agrees with this document, byte for byte. This is the mechanism by
which independent VIBE implementations stay interoperable.

### Structure

The suite lives under `tests/conformance/` and is split into two trees:

```
tests/conformance/
  valid/
    <name>.vibe      # a conforming document
    <name>.json      # its expected value tree (see encoding below)
  invalid/
    <name>.vibe      # a non-conforming document
    <name>.txt       # the REQUIRED error category (one token, see below)
```

Every file in `valid/` MUST parse successfully and produce **exactly** the value
tree described by its sibling `.json`. Every file in `invalid/` MUST be
rejected; a conforming parser reports an error whose category matches the
sibling `.txt`.

### The Interchange Encoding

Because target languages disagree about numbers and types, expected values are
encoded in a small tagged-JSON dialect. Every VIBE scalar becomes a JSON object
`{"type": T, "value": V}` where `V` is **always a JSON string** (so no precision
is lost across languages):

| VIBE type | `type` tag | `value` example            |
|-----------|------------|----------------------------|
| integer   | `"integer"`| `"9223372036854775807"`    |
| float     | `"float"`  | `"3.14159"`                |
| boolean   | `"boolean"`| `"true"`                   |
| string    | `"string"` | `"hello\nworld"`           |

Objects map to plain JSON objects; arrays map to plain JSON arrays. For example,
this document:

```vibe
server {
  host localhost
  port 8080
}
ports [80 443]
```

has exactly this expected tree:

```json
{
  "server": {
    "host": { "type": "string",  "value": "localhost" },
    "port": { "type": "integer", "value": "8080" }
  },
  "ports": [
    { "type": "integer", "value": "80" },
    { "type": "integer", "value": "443" }
  ]
}
```

A test harness parses the `.vibe` file with the implementation under test,
re-encodes the result into this dialect, and compares it structurally to the
expected `.json`. Structural comparison ignores object key ordering.

### Error Categories

Every file in `invalid/` is paired with exactly one **error code** from the
canonical set defined in [Error Handling → Error Codes](#error-codes). Parsers
MUST reject the input and MUST NOT accept the document; they SHOULD classify the
failure with the matching code. The categories currently exercised by the suite:

| Category token       | Triggered by                                             |
|----------------------|----------------------------------------------------------|
| `encoding-error`     | ill-formed UTF-8, or a leading BOM                       |
| `illegal-character`  | a byte not permitted here (NUL, raw control, stray `+`)  |
| `unexpected-token`   | a structural token where a key or value was required; empty key |
| `unclosed-object`    | `{` with no matching `}` before EOF                      |
| `unclosed-array`     | `[` with no matching `]` before EOF                      |
| `unterminated-string`| `"` with no closing `"` on the same line                 |
| `nested-container`   | an object or array appearing inside an array (First Law) |
| `invalid-escape`     | a backslash escape not listed in [§String Literals](#string-literals) |
| `invalid-number`     | numeric token out of range or malformed                  |
| `limit-exceeded`     | a [resource limit](#resource-limits-normative) was exceeded |

### The Minimum Bar

An implementation MAY claim **VIBE 1.0 conformance** if and only if it passes
100% of the `valid/` cases and rejects 100% of the `invalid/` cases. Partial
passes MUST NOT be advertised as “VIBE-compatible.” Publishing a conformance
badge that links to the suite run is RECOMMENDED.

## Implementation Guidelines

### Memory Management

1. **In-Memory Model**: A VIBE document is a bounded configuration file, not a
   data stream. Implementations MAY read the whole document into memory before
   parsing; the reference implementation does. Streaming/incremental parsing is
   OPTIONAL and only worthwhile for unusually large inputs.
2. **Memory Efficiency**: The parsed structure SHOULD not exceed ~2× the input size.
3. **Deterministic Cleanup**: Implement complete cleanup of nested structures
   (recursive free / RAII / GC) with no leaks on either success or error paths.
4. **String Interning**: Consider interning common string values.

### Performance Notes

Parsing speed is **not** a design goal of VIBE and is not a reason to choose it —
no configuration file is large enough for parse time to matter. The single-pass,
backtrack-free grammar happens to parse in linear time, but VIBE competes on
*predictability*, not throughput. Implementations SHOULD keep memory proportional
to input size (roughly ≤ 2×) and MUST NOT sacrifice correctness or clear error
reporting for speed. See [Performance Requirements](#performance-requirements)
for the (deliberately minimal) normative rules.

### API Design Principles

#### Parser Interface
```c
// C API example
typedef struct VibeParser VibeParser;

VibeParser* vibe_parser_new();
void vibe_parser_free(VibeParser* parser);
VibeValue* vibe_parse_string(VibeParser* parser, const char* input);
VibeValue* vibe_parse_file(VibeParser* parser, const char* filename);
VibeError vibe_get_last_error(VibeParser* parser);
```

#### Value Access Interface
```c
// Type-safe accessors
const char* vibe_get_string(VibeValue* value, const char* path);
int64_t vibe_get_int(VibeValue* value, const char* path);
double vibe_get_float(VibeValue* value, const char* path);
bool vibe_get_bool(VibeValue* value, const char* path);
VibeArray* vibe_get_array(VibeValue* value, const char* path);
VibeObject* vibe_get_object(VibeValue* value, const char* path);
```

### Thread Safety

Implementations should specify thread safety guarantees:

1. **Parser Objects**: Not thread-safe (one parser per thread)
2. **Parsed Values**: Immutable and thread-safe for reading
3. **Concurrent Parsing**: Multiple parsers can operate concurrently

### Testing Requirements

Implementations should include:

1. **Unit Tests**: Individual component testing
2. **Integration Tests**: End-to-end parsing tests
3. **Conformance Tests**: The [conformance suite](#conformance-test-suite) — REQUIRED to claim conformance
4. **Fuzz Tests**: Random input testing for robustness (a parser MUST NOT crash on any input)
5. **Round-trip Tests**: parse → serialize → parse yields an identical tree

### Test Suite Examples

```
# Basic parsing tests
test_simple_assignment()
test_nested_objects()
test_arrays()
test_comments()
test_string_escaping()

# Error handling tests
test_syntax_errors()
test_unclosed_braces()
test_invalid_escape_sequences()

# Edge cases
test_empty_file()
test_large_files()
test_deep_nesting()
test_unicode_content()

# Performance tests
benchmark_parsing_speed()
benchmark_memory_usage()
profile_large_file_parsing()
```

## Security Considerations

VIBE is designed so that **parsing a config file from an untrusted source is
safe by construction.** This section states the guarantees, the threats they
neutralize, and the small number of obligations left to the implementation.

### Threat Model

The assumed adversary controls the *bytes of the document* (e.g. a config
uploaded by a tenant, checked in by a third party, or fetched over the network)
but not the parser binary. The goals are: no code execution, no resource
exhaustion, no information disclosure, and — critically — **no parser
differential.**

### Guarantees Provided by the Format

1. **No code execution, ever.** VIBE has no includes, no references, no variable
   substitution, no templates, and no external-entity resolution (all
   [explicitly rejected](#future-considerations)). Parsing a document touches
   *nothing* outside the input bytes — no filesystem, no network, no environment.
   This eliminates the entire class of attacks that plague YAML (`!!python/object`)
   and XML (XXE / billion laughs via external entities).
2. **No parser differential.** Because the grammar is unambiguous, duplicate keys
   are defined (last-wins), numbers never silently coerce, and encoding is
   strict, two conforming parsers **cannot** be made to disagree about a
   document. This closes the vulnerability class where a validator and a consumer
   interpret the same config differently (the config-level analogue of HTTP
   request smuggling). Determinism is a *security property*, not just a nicety.
3. **Bounded work.** Parsing is O(n) time and O(depth) stack, and every input
   dimension is capped by a mandatory [resource limit](#resource-limits-normative).
   There is no construct whose cost is superlinear in its byte length.

### Attacks and Why They Fail

| Attack | Why VIBE is immune |
|--------|--------------------|
| **Billion laughs / entity expansion** | No references or entities exist to expand. |
| **Deep-nesting stack overflow** | Nesting depth is capped (`limit-exceeded`); check is before recursion/allocation. |
| **Memory exhaustion** (huge string/array/doc) | Document size, string length, and element counts are all capped. |
| **NUL / encoding smuggling** | NUL is forbidden and ill-formed UTF-8 is rejected, so a validator and a consumer see the same bytes. |
| **Duplicate-key confusion** | Last-wins is normative; every parser resolves duplicates identically. |
| **Type-coercion trickery** (`no`→false) | No implicit coercion; a token has exactly one type. |

### Implementation Obligations

A parser is safe against untrusted input **only if** it also:

1. **Enforces the resource limits** in
   [Resource Limits](#resource-limits-normative), checking each *before* the
   corresponding allocation. This is the one place an implementation can
   reintroduce a DoS if it is careless.
2. **Fails closed.** On any error, return no tree; never a partial result (see
   [Error Handling](#error-handling)).
3. **Does not leak internals in errors.** Error messages SHOULD report position
   and code, not memory addresses or internal parser state.
4. **Uses a length-aware entry point** where possible, so an embedded NUL is
   rejected rather than silently truncating the document. Implementations layered
   on NUL-terminated C strings MUST document the truncation caveat.

> **De-facto duplicate detection cost.** A naive last-wins implementation that
> re-scans an object's keys on each insert is O(n²) in the number of keys — itself
> a mild complexity-attack surface. Implementations SHOULD use a hash index for
> large objects; the mandatory keys-per-object cap bounds the worst case either
> way.


## Performance Requirements

VIBE sets **no throughput requirement.** Config files are small and parsed once at
startup; a format that traded away clarity or correctness for megabytes-per-second
would be optimizing the wrong thing. The only hard requirements are:

1. **Linear time.** Parsing MUST be O(n) in input size — single-pass, no
   backtracking. This falls out of the grammar automatically.
2. **Bounded memory.** Peak memory SHOULD stay within ~2× the input size.
3. **Correctness first.** An implementation MUST NOT skip validation, error
   reporting, or the [conformance suite](#conformance-test-suite) in the name of
   speed.

Optimization (string interning, memory pooling, vectorized scanning) is entirely
OPTIONAL and is a quality-of-implementation concern, not a property of the format.

## Validation and Schema

VIBE has **no** built-in schema language, and [never will](#future-considerations):
validation is a separate layer that consumes the parsed value tree, not part of
the grammar. This keeps the format frozen and lets validation evolve
independently. Tooling built *on top of* VIBE MAY offer schemas; such a schema
dialect is not defined by, and imposes no requirement on, this specification.

Typical responsibilities of an external validation layer:

1. **Type Checking**: Ensure values match expected types.
2. **Required Fields**: Validate presence of mandatory keys.
3. **Range Constraints**: Check numeric values are within bounds.
4. **Format Validation**: Validate strings match patterns (URLs, emails, …).
5. **Cross-Field Validation**: Validate relationships between fields.

## Comparison to Other Formats

VIBE does not try to win a feature checklist — it makes a different **bet**. The
other formats optimize for expressiveness or interchange; VIBE optimizes for a
human editing a config file at 2am without getting surprised.

### The one-line summary of each

- **JSON** — great for machines, hostile to humans (no comments, quotes and commas
  everywhere). VIBE concedes data-interchange to JSON entirely.
- **YAML** — readable until it bites: significant whitespace, implicit typing (the
  Norway problem), anchors and aliases that act at a distance. VIBE keeps YAML's
  readability and removes every one of those traps.
- **TOML** — excellent and closest in spirit; but arrays-of-tables (`[[x]]`) bring
  back anonymous records, and deeply nested tables get awkward. VIBE forbids the
  anonymous-record pattern outright.
- **XML** — verbose; not a config language anyone reaches for by choice.

### The bet, stated plainly

> Every other format lets you write an anonymous list of records and lets a bare
> word silently become a boolean. VIBE forbids both. You give up two “features”
> and get, in return, a format with **no ambiguous documents and no positional
> identity.** If that trade sounds good, VIBE is for you. If you need references,
> multi-document streams, or templating, it is honestly *not* — use YAML or a
> config-as-code tool, and we'll cheer you on.

### When to Use VIBE

**Choose VIBE when** you are hand-writing configuration and you want it to be
obvious, un-ambiguous, and diff-friendly — with comments and without whitespace
traps.

**Do not choose VIBE when** you need data interchange (use **JSON**), references /
multi-document streams (use **YAML**), or logic and templating in the config
itself (use a real programming language or a tool like Jsonnet/CUE). Picking the
right tool is the good vibe.

## Migration Guide

### From JSON

JSON structures map directly to VIBE:

**JSON**:
```json
{
  "server": {
    "host": "localhost",
    "port": 8080,
    "ssl": {
      "enabled": true
    }
  },
  "features": ["auth", "api"]
}
```

**VIBE**:
```vibe
server {
  host localhost
  port 8080
  ssl {
    enabled true
  }
}
features [auth api]
```

### From YAML

YAML structures translate with some changes:

**YAML**:
```yaml
server:
  host: localhost
  port: 8080
  ssl:
    enabled: true
features:
  - auth
  - api
```

**VIBE**:
```vibe
server {
  host localhost
  port 8080
  ssl {
    enabled true
  }
}
features [auth api]
```

### From TOML

TOML sections become VIBE objects:

**TOML**:
```toml
[server]
host = "localhost"
port = 8080

[server.ssl]
enabled = true

features = ["auth", "api"]
```

**VIBE**:
```vibe
server {
  host localhost
  port 8080
  ssl {
    enabled true
  }
}
features [auth api]
```

### Migration Tools

Conversion and validation are **tooling that lives outside the language.** A
conforming ecosystem MAY provide utilities such as:

```bash
# Illustrative CLI (not part of the spec, not shipped by default)
vibe convert --from json config.json > config.vibe
vibe validate config.vibe
vibe fmt config.vibe          # canonical pretty-printer
```

These are conveniences, not requirements, and are intentionally kept out of the
format itself — see [Explicitly Rejected](#future-considerations).

## Versioning and Stability

VIBE follows [Semantic Versioning](https://semver.org/) at the level of the
*language*, not any single implementation.

- **PATCH** (`1.0.x`) — wording clarifications and test-suite additions that do not
  change which documents are conforming.
- **MINOR** (`1.x.0`) — **backward-compatible** language additions: any document
  valid under `1.0` remains valid, with the same value tree, under every later
  `1.x`. New OPTIONAL syntax (e.g. the `\uXXXX` escape) enters here.
- **MAJOR** (`x.0.0`) — reserved for a change that could alter the meaning of an
  existing valid document. None is planned.

### The Stability Promise

The grammar in [§Grammar](#grammar), the type-inference rules, the First Law, and
the last-wins duplicate rule are **frozen** for all of VIBE 1.x. A document you
write today will parse to the same tree under any conforming 1.x parser, forever.
This promise — not any feature — is what makes VIBE safe to build on.

Features under [§Future Considerations](#future-considerations) are explicitly
**not** part of any stability guarantee until they land in a numbered release with
accompanying conformance tests. Do not rely on them.

## Future Considerations *(Non-normative)*

This section is **informative**. Nothing here is part of VIBE 1.0, imposes any
requirement, or may be assumed present by a conforming document or parser.

VIBE's roadmap is deliberately tiny. A configuration format earns trust by the
features it **refuses**, and most “obvious” additions are exactly the ones that
turned other formats into footgun museums. So this section is split in two: a
short list of additions that *might* land (because they don't break any
[Guarantee](#the-vibe-guarantees)), and a firm list of things we will **not** add.

### Candidate Additions (compatible with the Guarantees)

These are backward-compatible and preserve One Parse and No Surprises. They may
appear in a future 1.x, each gated behind a conformance-test addition:

- **`\uXXXX` string escapes** — already reserved; targeted for 1.1.
- **Multi-line strings** — a triple-quoted form with fully explicit, whitespace-
  insensitive semantics (no indentation-stripping magic). Only if the exact byte
  content is unambiguous.

That's the entire list. If a proposed feature is not here, assume the answer is
no.

### Explicitly Rejected (and why)

These are **non-goals by design.** They are not “not yet” — they are *never*,
because each one re-introduces the ambiguity, non-locality, or instability that
VIBE exists to eliminate. Requests to add them will be closed with a link here.

| Rejected feature | Why it will never be added |
|------------------|-----------------------------|
| **Variable substitution** (`${env}`) | Non-local: a value's meaning now depends on another line. Breaks *read-it-in-place*. Do interpolation in your program. |
| **Include directives** (`include "x.vibe"`) | A document would no longer mean one thing on its own; parsing becomes filesystem- and order-dependent. Compose configs in code. |
| **Conditionals / `if` blocks** | Config is data, not a program. The moment config has control flow, it needs a debugger. Logic lives in your application. |
| **Templates / inheritance** | Same as conditionals: turns a data file into a mini-language with its own evaluation order and surprises. |
| **Anchors / references / aliases** | YAML's `&`/`*` are the textbook example of action-at-a-distance. A value must be readable where it sits. |
| **Built-in schema validation** | Validation is a *separate* layer that consumes the parsed tree; baking it into the grammar couples two concerns and freezes a schema dialect forever. Ship schemas as tooling, not syntax. |
| **Date/duration/size literals** (`30s`, `10MB`) | Magic types are the Norway problem wearing a hat: is `10MB` a string or a number-with-unit? Keep it a string and let the application interpret it. |
| **A binary format** | VIBE is for humans hand-editing text. A binary form solves a speed problem VIBE explicitly does not have. |

> If you find yourself wanting one of these, you have most likely outgrown a
> *configuration format* and want a configuration *language* (CUE, Jsonnet,
> Dhall) or plain code. That is a good problem to have — reach for the right
> tool. VIBE stays small on purpose.

## References

### Standards and Specifications
- [JSON Specification (RFC 7159)](https://tools.ietf.org/html/rfc7159)
- [YAML Specification](https://yaml.org/spec/)
- [TOML Specification](https://toml.io/en/v1.0.0)
- [XML Specification](https://www.w3.org/TR/xml/)

### Related Work
- [JSON5](https://json5.org/) - JSON for humans
- [HJSON](https://hjson.github.io/) - Human JSON
- [CSON](https://github.com/bevry/cson) - CoffeeScript Object Notation
- [SDLang](https://sdlang.org/) - Simple Declarative Language

### Implementation References
- [Parsing Techniques](https://dickgrune.com/Books/PTAPG_2nd_Edition/) - Grune & Jacobs
- [Crafting Interpreters](https://craftinginterpreters.com/) - Robert Nystrom
- [ANTLR](https://www.antlr.org/) - Parser generator

---

**VIBE Format Specification v1.0.0**  
*Pass the vibe check* ✓

Remember: Configuration doesn't have to be complicated. Sometimes the best solution is the one that just feels right. 

Keep calm and VIBE on! 🌊

This specification is released under the MIT License (because sharing good vibes should be free).