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
is *the point.* See [The Stability Paradox](Stability_Paradox.md) for the full
argument.

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

1. [Overview](#overview)
2. [Design Goals](#design-goals)
3. [Grammar](#grammar)
4. [Lexical Analysis](#lexical-analysis)
5. [Data Types](#data-types)
6. [String Literals](#string-literals)
7. [Comments](#comments)
8. [Arrays](#arrays)
9. [Objects](#objects)
10. [Whitespace and Formatting](#whitespace-and-formatting)
11. [Reserved Words](#reserved-words)
12. [Path Notation](#path-notation)
13. [Duplicate Keys](#duplicate-keys)
14. [Complete Examples](#complete-examples)
15. [Parsing Algorithm](#parsing-algorithm)
16. [Error Handling](#error-handling)
17. [File Format](#file-format)
18. [Conformance Test Suite](#conformance-test-suite)
19. [Implementation Guidelines](#implementation-guidelines)
20. [Security Considerations](#security-considerations)
21. [Performance Requirements](#performance-requirements)
22. [Validation and Schema](#validation-and-schema)
23. [Comparison to Other Formats](#comparison-to-other-formats)
24. [Migration Guide](#migration-guide)
25. [Versioning and Stability](#versioning-and-stability)
26. [Future Considerations](#future-considerations)
27. [References](#references)

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

### 4. Fast Parsing
The grammar enables single-pass, O(n) parsing using a simple state machine. No backtracking, unbounded lookahead, or complex grammar rules are required. This design choice ensures predictable performance characteristics and low memory overhead, making VIBE suitable for resource-constrained environments.

### 5. Type Safety
Clear, deterministic type inference rules ensure that values are consistently interpreted across all implementations. Numbers, booleans, and strings are unambiguously distinguished through syntax, eliminating the need for explicit type annotations while maintaining type safety.

### 6. Human-Friendly
The syntax is optimized for human reading and writing. Whitespace is non-significant (except as token separation), comments are allowed anywhere, and unquoted strings reduce visual clutter for simple values. Error messages should provide clear context and actionable guidance for resolution.

## Grammar

### Formal Grammar Definition (EBNF)

```
config        = { statement } ;
statement     = ( assignment | object | array | comment ) [ comment ] newline ;
assignment    = identifier ( scalar_value | object | array ) ;
object        = identifier "{" { statement } "}" ;
array         = identifier "[" [ value_list ] "]" ;
value_list    = scalar_value { ( whitespace | newline ) scalar_value } ;
scalar_value  = string | number | boolean | identifier ;
comment       = "#" { any_char - newline } ;
identifier    = ( letter | "_" ) { letter | digit | "_" | "-" } ;
string        = quoted_string | unquoted_string ;
quoted_string = '"' { char | escape_sequence } '"' ;
unquoted_string = ( letter | digit | "_" | "-" | "." | "/" | ":" ) 
                  { letter | digit | "_" | "-" | "." | "/" | ":" } ;
number        = [ "-" ] ( integer | float ) ;
integer       = digit { digit } ;
float         = digit { digit } "." digit { digit } ;
boolean       = "true" | "false" ;
escape_sequence = "\" ( '"' | "\" | "n" | "t" | "r" | "u" hex_digit hex_digit hex_digit hex_digit ) ;
```

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
- Key must be a valid identifier
- Value continues until end of line or special character
- Multiple words in value require quotes

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

## Data Types

VIBE supports 5 fundamental data types with automatic type inference based on syntax patterns:

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

**Range**: Implementations MUST support at least IEEE 754 double precision (64-bit floating point).

**Examples**:
```
pi 3.14159
negative -0.5
zero 0.0
small 0.001
precise 123.456789012345
```

**Rules**:
- Decimal point is REQUIRED (distinguishes from integers)
- At least one digit must appear on both sides of decimal point
- Scientific/exponential notation is NOT supported in VIBE 1.0
- Leading zeros after decimal are significant (0.001 ≠ 0.1)

**Special Values**: 
Implementations MAY support these unquoted string literals with special float semantics:
- `inf` or `Infinity` - positive infinity
- `-inf` or `-Infinity` - negative infinity  
- `nan` or `NaN` - not a number

**Invalid Syntax**:
```
no_decimal 42       # Missing decimal point (this is an integer)
scientific 1.23e10  # Scientific notation not supported
trailing_dot 42.    # Requires digits after decimal
leading_dot .42     # Requires digits before decimal
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

> **Design Philosophy**: This restriction is intentional and core to VIBE's design. Arrays of objects create instability through index-based references, ambiguous merging, and lack of inherent identity. VIBE forces you to use named objects, which provide stable path-based references and deterministic configuration merging. See [Stability_Paradox.md](docs/Stability_Paradox.md) for the full rationale.

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

- Files must be valid UTF-8
- Unicode characters can appear directly in quoted strings and comments only
- `\uXXXX` escape sequences support Basic Multilingual Plane
- No support for surrogate pairs or `\UXXXXXXXX` sequences
- **Identifiers and unquoted strings are strictly ASCII-only** for simplicity and performance

**Examples**:
```
greeting "Hello, world! 🌍"
path "C:\Users\Name\Documents"
escaped "She said \"Hello\""
unicode "Emoji: \u1F44D"
multiline "Line 1\nLine 2\nLine 3"

# ASCII identifiers with Unicode quoted values
configuration "aplicación"  # ASCII identifier, quoted Unicode value
settings "сервер"
config "データベース"
language english     # ASCII identifier and unquoted ASCII value
language "español"   # ASCII identifier, quoted Unicode value
```

### String Parsing Rules

1. **Unquoted String Parsing**:
   - Starts with first non-whitespace character  
   - Continues until whitespace, special character, or end of line
   - Cannot contain `# { } [ ]`
   - ASCII-only: `[a-zA-Z0-9_-./:]`

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
null null
if conditional
for loop_config
class object_type
vibe excellent  # meta!

# Unicode content must be in quoted strings
chinese_name "配置"
russian_name "сервер"
spanish_name "configuración"
```

This design choice prioritizes flexibility and eliminates the need to escape or quote common configuration key names. Because reserving words is just not the vibe.

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

### Error Categories

1. **Lexical Errors**: Invalid character sequences
2. **Syntax Errors**: Invalid grammar
3. **Semantic Errors**: Valid syntax but invalid meaning
4. **Type Errors**: Type inference failures

### Error Reporting

Parsers should provide detailed error information:

```
Error: Unexpected token '}' on line 15, column 8
  ssl {
      ^
Expected identifier or closing brace
```

### Common Errors and Messages

#### Unclosed Objects
```
Error: Unclosed object on line 12
  server {
         ^
Expected '}' before end of file
(Looks like this object needs some closure 😉)
```

#### Invalid Escape Sequence
```
Error: Invalid escape sequence '\x' on line 8, column 15
  path "C:\Users\name"
              ^
Valid escape sequences: \" \\ \n \t \r \uXXXX
(That backslash is trying to escape reality, but VIBE keeps it real!)
```

#### Duplicate Keys (if implementation rejects)
```
Error: Duplicate key 'port' on line 16
  port 9000
  ^
Previously defined on line 14
(Déjà vu! This key is having an identity crisis.)
```

#### Invalid Array Syntax
```
Error: Expected ']' on line 10, column 25
  servers [web1.com web2.com
                            ^
Arrays must be closed with ']'
(This array is feeling a bit... open-ended. Let's give it some closure!)
```

### Recovery Strategies

1. **Panic Mode**: Skip tokens until a synchronization point (e.g., newline, brace)
2. **Error Production**: Define grammar rules for common errors
3. **Insertion**: Insert missing tokens when obvious
4. **Deletion**: Skip unexpected tokens

## File Format

### File Extension
**Recommended**: `.vibe`

Alternative extensions: `.vb`, `.config`, `.conf`

### MIME Type
**Proposed**: `application/vibe`  
**Alternative**: `text/vibe`

### Character Encoding
**Required**: UTF-8 without BOM

**Byte Order Mark**: Not permitted. Files must not start with UTF-8 BOM (EF BB BF).

### Line Endings
**Supported**: 
- Unix (LF): `\n`
- Windows (CRLF): `\r\n`
- Legacy Mac (CR): `\r`

**Recommended**: Unix line endings for cross-platform compatibility.

### File Size Limits
**Recommended Limits**:
- Maximum file size: 10 MB
- Maximum line length: 1000 characters
- Maximum nesting depth: 64 levels
- Maximum identifier length: 255 characters
- Maximum string length: 1 MB

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

Every file in `invalid/` is paired with exactly one of these category tokens.
Parsers MUST reject the input; they SHOULD classify the error into the matching
category, and MUST NOT accept the document.

| Category token       | Triggered by                                             |
|----------------------|----------------------------------------------------------|
| `unclosed-object`    | `{` with no matching `}` before EOF                      |
| `unclosed-array`     | `[` with no matching `]` before EOF                      |
| `unterminated-string`| `"` with no closing `"` on the same line                 |
| `nested-container`   | an object or array appearing inside an array (First Law) |
| `unexpected-token`   | a structural token where a value or key was required     |
| `invalid-escape`     | a backslash escape not listed in [§String Literals](#string-literals) |
| `invalid-number`     | numeric-looking token outside the representable range    |
| `depth-exceeded`     | nesting deeper than the implementation limit             |

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

### Performance Targets

- **Parse Speed**: > 100 MB/s on modern hardware (3+ GHz CPU)
- **Memory Usage**: < 2x input file size
- **Latency**: < 1ms for files under 1KB
- **Scalability**: Linear time complexity O(n) with input size

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
3. **Performance Tests**: Benchmarks with various file sizes
4. **Fuzz Tests**: Random input testing for robustness
5. **Compatibility Tests**: Cross-implementation compatibility

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

### Input Validation

1. **File Size Limits**: Prevent memory exhaustion attacks
2. **Nesting Depth**: Prevent stack overflow with deep structures
3. **String Length**: Limit individual string sizes
4. **Identifier Length**: Limit key name lengths

### Recommended Security Limits

```
MAX_FILE_SIZE = 10 MB
MAX_NESTING_DEPTH = 64
MAX_STRING_LENGTH = 1 MB
MAX_IDENTIFIER_LENGTH = 255 characters
MAX_ARRAY_SIZE = 10,000 elements
MAX_OBJECT_KEYS = 10,000 keys
```

### Attack Vectors

#### Billion Laughs Attack
Prevent exponential memory growth through nested structures:
```
# Potential attack - deeply nested objects
level1 {
  level2 {
    level3 {
      # ... continues for many levels
    }
  }
}
```

**Mitigation**: Enforce maximum nesting depth

#### Memory Exhaustion
Large arrays or strings can consume excessive memory:
```
# Potential attack - huge array
huge_array [item1 item2 ... (millions of items)]
```

**Mitigation**: Enforce size limits and streaming parsing

#### Parser Complexity Attacks
Certain input patterns may cause quadratic parsing time:
```
# Potential attack - many duplicate keys
key value
key value
# ... repeated thousands of times
```

**Mitigation**: Implement linear-time duplicate detection

### Safe Parsing Practices

1. **Validate Input**: Check file size before parsing
2. **Resource Limits**: Set timeouts and memory limits
3. **Sanitize Output**: Validate parsed values before use
4. **Error Handling**: Don't expose internal parser state in errors
5. **Logging**: Log parsing attempts for security monitoring

## Performance Requirements

### Benchmarking Standards

#### Test Files
1. **Small**: 1KB configuration file
2. **Medium**: 100KB structured data
3. **Large**: 10MB complex configuration
4. **Deep**: Maximum nesting depth file
5. **Wide**: Many top-level keys

#### Performance Metrics
- **Parse Time**: Wall clock time to parse file
- **Memory Usage**: Peak memory consumption during parsing
- **Memory Efficiency**: Ratio of parsed structure size to file size
- **Throughput**: MB/s processing rate

#### Target Performance (on 3GHz CPU, 8GB RAM)

| File Size | Parse Time | Memory Usage | Throughput |
|-----------|------------|--------------|------------|
| 1KB | < 0.1ms | < 10KB | > 10 MB/s |
| 100KB | < 10ms | < 1MB | > 100 MB/s |
| 10MB | < 1s | < 50MB | > 100 MB/s |

### Optimization Strategies

1. **Single-Pass Parsing**: No backtracking or multiple passes
2. **String Interning**: Reuse common strings
3. **Memory Pooling**: Reduce allocation overhead
4. **SIMD Instructions**: Use vectorized operations for scanning
5. **Branch Prediction**: Optimize hot parsing paths

## Validation and Schema

While VIBE itself has no built-in schema validation, implementations may provide schema languages for validation.

### Proposed Schema Syntax

```vibe
# Example schema definition
schema ApplicationConfig {
  app {
    name: string
    version: string
    debug: boolean?  # Optional field
  }
  
  server {
    host: string
    port: integer(1..65535)  # Range constraint
    
    ssl?: {  # Optional object
      enabled: boolean
      cert: string
    }
  }
  
  features: [string]  # Array of strings
}
```

### Validation Rules

1. **Type Checking**: Ensure values match expected types
2. **Required Fields**: Validate presence of mandatory keys
3. **Range Constraints**: Check numeric values are within bounds
4. **Format Validation**: Validate strings match patterns (e.g., URLs, emails)
5. **Cross-Field Validation**: Validate relationships between fields

## Comparison to Other Formats

### Feature Comparison

| Feature | VIBE | JSON | YAML | TOML | XML |
|---------|------|------|------|------|-----|
| Human Readable | ✓ | ✗ | ✓ | ✓ | ✗ |
| Minimal Syntax | ✓ | ✗ | ✓ | ✓ | ✗ |
| Visual Hierarchy | ✓ | ✓ | ✗ | ✗ | ✓ |
| Fast Parsing | ✓ | ✓ | ✗ | ✓ | ✗ |
| No Indentation Rules | ✓ | ✓ | ✗ | ✓ | ✓ |
| Type Inference | ✓ | ✗ | ✓ | ✓ | ✗ |
| Comments | ✓ | ✗ | ✓ | ✓ | ✓ |
| Unambiguous | ✓ | ✓ | ✗ | ✓ | ✓ |
| Single Pass Parse | ✓ | ✓ | ✗ | ✓ | ✗ |

### When to Use VIBE

**Choose VIBE when**:
- Configuration files need to be human-readable and editable
- Fast parsing is important
- Visual structure clarity is valued
- Simple syntax is preferred
- Comments are needed

**Choose JSON when**:
- Web APIs and data interchange
- Strict typing is not needed
- Minimal parser complexity is required
- Maximum compatibility is needed

**Choose YAML when**:
- Complex data structures with references
- Multi-document files are needed
- Existing YAML ecosystem is required

**Choose TOML when**:
- Simple configuration files
- Strong typing is important
- INI-like format is preferred

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

Implementations should provide conversion utilities:

```bash
# Command-line converters
vibe-convert --from json --to vibe config.json config.vibe
vibe-convert --from yaml --to vibe config.yaml config.vibe
vibe-convert --from toml --to vibe config.toml config.vibe

# Validation
vibe-validate config.vibe
vibe-format config.vibe  # Pretty-print formatter
```

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
requirement, or may be assumed present by a conforming document or parser. Ideas
graduate from this list only when they ship in a numbered release with
conformance tests.

The future is looking bright for VIBE! We're constantly vibing with new ideas while keeping the core philosophy intact.

### Version 2.0 Potential Features

#### Multi-line Strings
```vibe
description """
This is a multi-line string
that spans several lines
and preserves formatting.
"""
```

#### Include Directives
```vibe
include "database.vibe"
include "logging.vibe"
```

#### Variable Substitution
```vibe
env development
database_host ${env}.db.example.com
```

#### Schema Validation
```vibe
schema ConfigSchema {
  server {
    host: string
    port: integer(1..65535)
  }
}
```

#### Binary Format
A binary representation for faster parsing and smaller size:
- Magic number: `VIBE` (0x56494245)
- Version byte
- Length-prefixed strings
- Type markers for values

### Experimental Features

#### Extended Types
- Date/time literals: `2025-01-14T10:30:00Z`
- Duration literals: `30s`, `5m`, `2h`
- Size literals: `10MB`, `1GB`

#### Conditional Blocks
```vibe
if environment == "production" {
  debug false
  log_level error
}
```

#### Templates
```vibe
template DatabaseConfig {
  host ${db_host}
  port ${db_port}
  name ${db_name}
}

primary_db: DatabaseConfig {
  db_host prod-db.example.com
  db_port 5432
  db_name app_prod
}
```

Templates would let you copy that vibe across multiple configurations!

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

**VIBE Format Specification v1.0**  
*Pass the vibe check* ✓

Remember: Configuration doesn't have to be complicated. Sometimes the best solution is the one that just feels right. 

Keep calm and VIBE on! 🌊

This specification is released under the MIT License (because sharing good vibes should be free).