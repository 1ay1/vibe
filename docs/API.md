# libvibe API Reference

The complete C API for **libvibe**, the reference implementation of the
[VIBE configuration format](../SPECIFICATION.md). Everything lives in a single
header, `vibe.h`, and links against `libvibe` (or the vendored `vibe.c`).

```c
#include <vibe.h>          /* installed  */
/* or */
#include "vibe.h"          /* vendored   */
```

The library has **no dependencies** beyond the C standard library, keeps **no
global mutable state** (aside from the optional allocator hooks), and every
parse is independent, so one `VibeParser` per thread is fully re-entrant.

## Table of Contents

- [Conventions](#conventions)
- [Versioning](#versioning)
- [Types](#types)
- [Parser Lifecycle](#parser-lifecycle)
- [Parsing](#parsing)
- [Error Handling](#error-handling)
- [Resource Limits](#resource-limits)
- [Allocator Hooks](#allocator-hooks)
- [Value Constructors](#value-constructors)
- [Path Access](#path-access)
- [Container Operations](#container-operations)
- [Emitting](#emitting)
- [Miscellaneous](#miscellaneous)
- [Command-Line Tool](#command-line-tool)
- [Examples](#examples)

---

## Conventions

- **Ownership.** A successful parse returns a heap-allocated root `VibeValue`
  (always an `OBJECT`). You own it; free it with `vibe_value_free()`, which
  recursively frees the whole subtree. `vibe_object_set()` and
  `vibe_array_push()` **take ownership** of the value you hand them.
- **Buffers returned to you** (e.g. from `vibe_emit()`) must be freed with
  `vibe_free()`, so the right allocator is used.
- **Failure.** Parsing functions return `NULL` on failure; query the parser with
  `vibe_get_last_error()` (or pass a `VibeError*` to the stateless
  `vibe_parse()`).
- **Const-ness.** Read-only helpers accept `const` pointers where the legacy
  signature allowed it; the tree itself is a plain tagged union you may inspect
  directly (`value->type`, `value->as_integer`, …).

---

## Versioning

`VIBE_VERSION_*` describes **this library**; `VIBE_FORMAT_VERSION` describes the
**language** it implements. They advance independently.

```c
#define VIBE_VERSION_MAJOR 1
#define VIBE_VERSION_MINOR 1
#define VIBE_VERSION_PATCH 0
#define VIBE_VERSION_STRING "1.1.0"
#define VIBE_VERSION_NUMBER  /* MAJOR*10000 + MINOR*100 + PATCH */
#define VIBE_FORMAT_VERSION "1.0"

const char *vibe_version(void);         /* "1.1.0"           */
int         vibe_version_number(void);  /* 10100             */
const char *vibe_format_version(void);  /* "1.0"             */
```

Use the compile-time macros for `#if` guards and the runtime functions to verify
the library you actually linked against.

---

## Types

### `VibeType`

```c
typedef enum {
    VIBE_TYPE_NULL = 0,  /* absence of value (never produced by the parser) */
    VIBE_TYPE_INTEGER,   /* 64-bit signed */
    VIBE_TYPE_FLOAT,     /* IEEE-754 double */
    VIBE_TYPE_BOOLEAN,   /* true / false */
    VIBE_TYPE_STRING,    /* UTF-8, heap owned */
    VIBE_TYPE_ARRAY,     /* ordered list of scalars */
    VIBE_TYPE_OBJECT     /* ordered key -> value entries */
} VibeType;

const char *vibe_type_name(VibeType type); /* "object", "integer", ... */
```

### `VibeValue`

A tagged union. Inspect `.type`, then read the matching member:

```c
struct VibeValue {
    VibeType type;
    union {
        int64_t     as_integer;
        double      as_float;
        bool        as_boolean;
        char       *as_string;
        VibeArray  *as_array;
        VibeObject *as_object;
    };
};
```

### `VibeObject` / `VibeArray`

```c
struct VibeObject { VibeObjectEntry *entries; size_t count; size_t capacity; };
struct VibeArray  { VibeValue      **values;  size_t count; size_t capacity; };
typedef struct { char *key; VibeValue *value; } VibeObjectEntry;
```

Both preserve insertion order. Objects are last-wins on duplicate keys (matching
the parser). Per the **First Law of VIBE**, arrays only ever hold scalars.

### `VibeErrorCode` / `VibeError`

```c
typedef enum {
    VIBE_OK = 0,
    VIBE_ERROR_ENCODING,            /* "encoding-error"       */
    VIBE_ERROR_ILLEGAL_CHARACTER,   /* "illegal-character"    */
    VIBE_ERROR_UNEXPECTED_TOKEN,    /* "unexpected-token"     */
    VIBE_ERROR_UNCLOSED_OBJECT,     /* "unclosed-object"      */
    VIBE_ERROR_UNCLOSED_ARRAY,      /* "unclosed-array"       */
    VIBE_ERROR_UNTERMINATED_STRING, /* "unterminated-string"  */
    VIBE_ERROR_NESTED_CONTAINER,    /* "nested-container"     */
    VIBE_ERROR_INVALID_ESCAPE,      /* "invalid-escape"       */
    VIBE_ERROR_INVALID_NUMBER,      /* "invalid-number"       */
    VIBE_ERROR_LIMIT_EXCEEDED,      /* "limit-exceeded"       */
    VIBE_ERROR_IO,                  /* "io-error"      (non-parse) */
    VIBE_ERROR_OUT_OF_MEMORY        /* "out-of-memory" (non-parse) */
} VibeErrorCode;

typedef struct {
    bool          has_error;
    VibeErrorCode code;
    char         *message;   /* human-readable; owned by the parser */
    int           line;      /* 1-based */
    int           column;    /* 1-based */
} VibeError;

const char *vibe_error_code_string(VibeErrorCode code); /* canonical hyphenated name */
```

The first eleven codes match the [specification's error
model](../SPECIFICATION.md#error-model) 1:1.

### `VibeLimits`

See [Resource Limits](#resource-limits).

---

## Parser Lifecycle

```c
VibeParser *vibe_parser_new(void);
void        vibe_parser_free(VibeParser *parser);
void        vibe_parser_set_limits(VibeParser *parser, const VibeLimits *limits);
```

Create once, reuse across many parses. `vibe_parser_free()` is NULL-safe and
also releases the last error message.

```c
VibeParser *p = vibe_parser_new();
VibeValue  *a = vibe_parse_file(p, "a.vibe");
VibeValue  *b = vibe_parse_file(p, "b.vibe");   /* reuse */
vibe_value_free(a); vibe_value_free(b);
vibe_parser_free(p);
```

---

## Parsing

```c
VibeValue *vibe_parse_string(VibeParser *p, const char *input);
VibeValue *vibe_parse_buffer(VibeParser *p, const char *data, size_t length);
VibeValue *vibe_parse_file(VibeParser *p, const char *filename);
VibeValue *vibe_parse(const char *data, size_t length, VibeError *out_error);
```

| Function | Use it when |
|----------|-------------|
| `vibe_parse_string` | You have a NUL-terminated string (embedded NUL truncates). |
| `vibe_parse_buffer` | You have `(ptr, len)` and the data may contain NUL bytes. |
| `vibe_parse_file`   | You want the library to read the file for you. |
| `vibe_parse`        | One-shot; no parser object. Fills `out_error` (if non-NULL) with an **owned** copy you free via `vibe_error_free()`. |

All return the root `OBJECT` on success or `NULL` on failure. The input is not
retained after the call returns. Empty, whitespace-only, and comment-only
documents parse to a valid **empty object**.

```c
VibeError err;
VibeValue *cfg = vibe_parse(buf, len, &err);
if (!cfg) {
    fprintf(stderr, "%d:%d [%s] %s\n",
            err.line, err.column, vibe_error_code_string(err.code), err.message);
    vibe_error_free(&err);
    return 1;
}
```

---

## Error Handling

```c
VibeError vibe_get_last_error(VibeParser *parser); /* snapshot; message aliases parser */
void      vibe_error_free(VibeError *error);       /* only for vibe_parse()'s copy */
```

`vibe_get_last_error()` returns a shallow copy whose `message` points into the
parser's own buffer — **do not** free it, and treat it as valid only until the
next parse. The `VibeError` filled by `vibe_parse()` owns its message and **must**
be released with `vibe_error_free()`.

| `VibeErrorCode` | `vibe_error_code_string()` | Meaning |
|-----------------|----------------------------|---------|
| `VIBE_ERROR_ENCODING`            | `encoding-error`        | e.g. a leading UTF-8 BOM |
| `VIBE_ERROR_ILLEGAL_CHARACTER`   | `illegal-character`     | byte not allowed at that position |
| `VIBE_ERROR_UNEXPECTED_TOKEN`    | `unexpected-token`      | token where another was required (incl. empty key) |
| `VIBE_ERROR_UNCLOSED_OBJECT`     | `unclosed-object`       | missing `}` |
| `VIBE_ERROR_UNCLOSED_ARRAY`      | `unclosed-array`        | missing `]` |
| `VIBE_ERROR_UNTERMINATED_STRING` | `unterminated-string`   | quote never closed |
| `VIBE_ERROR_NESTED_CONTAINER`    | `nested-container`      | object/array inside an array |
| `VIBE_ERROR_INVALID_ESCAPE`      | `invalid-escape`        | unknown `\x` in a quoted string |
| `VIBE_ERROR_INVALID_NUMBER`      | `invalid-number`        | out-of-range or malformed number |
| `VIBE_ERROR_LIMIT_EXCEEDED`      | `limit-exceeded`        | a `VibeLimits` bound was hit |
| `VIBE_ERROR_IO`                  | `io-error`              | file could not be read |
| `VIBE_ERROR_OUT_OF_MEMORY`       | `out-of-memory`         | allocation failed |

---

## Resource Limits

libvibe bounds every parse so untrusted input cannot exhaust memory or the
stack. Defaults are the **required minimums** from the spec; raise them when you
trust the source. Exceeding any bound yields `VIBE_ERROR_LIMIT_EXCEEDED`.

```c
typedef struct {
    size_t max_document_size;   /* default 16 MiB */
    size_t max_depth;           /* default 64     */
    size_t max_string_length;   /* default 1 MiB  */
    size_t max_key_length;      /* default 1 KiB  */
    size_t max_array_elements;  /* default 65536  */
    size_t max_object_keys;     /* default 65536  */
    size_t max_number_digits;   /* default 1024   */
} VibeLimits;

VibeLimits vibe_default_limits(void);
void       vibe_parser_set_limits(VibeParser *p, const VibeLimits *limits); /* NULL = defaults */
```

```c
VibeLimits limits = vibe_default_limits();
limits.max_document_size = 256 * 1024;   /* stricter: 256 KiB cap */
vibe_parser_set_limits(p, &limits);
```

---

## Allocator Hooks

Optional. Install a custom allocator **once at startup**, before any other
libvibe call, and do not change it while values are live (it frees them too).
Passing `NULL` for a slot resets it to the C standard library.

```c
void  vibe_set_allocators(void *(*malloc_fn)(size_t),
                          void *(*realloc_fn)(void *, size_t),
                          void  (*free_fn)(void *));
void *vibe_malloc(size_t size);
void  vibe_free(void *ptr);   /* free any buffer libvibe returned to you */
```

---

## Value Constructors

Each returns a new heap value (or `NULL` on allocation failure). String
constructors copy their input. Build trees, then `vibe_emit()` them.

```c
VibeValue *vibe_value_new_null(void);
VibeValue *vibe_value_new_integer(int64_t value);
VibeValue *vibe_value_new_float(double value);
VibeValue *vibe_value_new_boolean(bool value);
VibeValue *vibe_value_new_string(const char *value);
VibeValue *vibe_value_new_string_len(const char *value, size_t length); /* NUL-safe copy */
VibeValue *vibe_value_new_array(void);
VibeValue *vibe_value_new_object(void);
VibeValue *vibe_value_clone(const VibeValue *value);                    /* deep copy */
```

---

## Path Access

Dotted paths walk nested objects (`"server.tls.port"`). The plain readers return
a zero/NULL sentinel when the path is missing or the wrong type; the `*_or`
variants take the fallback explicitly.

```c
VibeValue  *vibe_get(VibeValue *root, const char *path);
const char *vibe_get_string(VibeValue *value, const char *path);
int64_t     vibe_get_int(VibeValue *value, const char *path);
double      vibe_get_float(VibeValue *value, const char *path);
bool        vibe_get_bool(VibeValue *value, const char *path);
VibeArray  *vibe_get_array(VibeValue *value, const char *path);
VibeObject *vibe_get_object(VibeValue *value, const char *path);

const char *vibe_get_string_or(VibeValue *value, const char *path, const char *fallback);
int64_t     vibe_get_int_or(VibeValue *value, const char *path, int64_t fallback);
double      vibe_get_float_or(VibeValue *value, const char *path, double fallback);
bool        vibe_get_bool_or(VibeValue *value, const char *path, bool fallback);
```

Passing `NULL` as the path returns/reads the value itself — handy for typing a
value you already hold: `vibe_get_int(elem, NULL)`.

```c
const char *host = vibe_get_string_or(cfg, "server.host", "127.0.0.1");
int64_t     port = vibe_get_int_or(cfg, "server.port", 8080);
```

---

## Container Operations

```c
void       vibe_object_set(VibeObject *obj, const char *key, VibeValue *value); /* takes ownership */
VibeValue *vibe_object_get(VibeObject *obj, const char *key);                   /* borrowed */
size_t     vibe_object_size(const VibeObject *obj);

void       vibe_array_push(VibeArray *arr, VibeValue *value);                   /* takes ownership */
VibeValue *vibe_array_get(VibeArray *arr, size_t index);                        /* borrowed */
size_t     vibe_array_size(const VibeArray *arr);
```

`vibe_object_set()` replaces an existing key (freeing the old value). On
allocation failure the passed value is freed rather than leaked.

```c
VibeArray *ports = vibe_get_array(cfg, "server.ports");
for (size_t i = 0; i < vibe_array_size(ports); i++)
    printf("port %lld\n", (long long)vibe_get_int(vibe_array_get(ports, i), NULL));
```

---

## Emitting

Serialise a value tree back to **canonical VIBE text**. The result re-parses to
an equal tree, and `vibe fmt` is idempotent.

```c
char *vibe_emit(const VibeValue *value);                 /* free with vibe_free() */
bool  vibe_emit_file(const VibeValue *value, const char *path);
```

```c
char *text = vibe_emit(cfg);
fputs(text, stdout);
vibe_free(text);
```

Notes: keys/strings are emitted bare when they re-lex unambiguously, quoted (with
escapes) otherwise; floats use plain decimal (VIBE has no exponent syntax), so
values of extreme magnitude are outside the round-trip guarantee.

---

## Miscellaneous

```c
const char *vibe_type_name(VibeType type);            /* "object", ... */
void        vibe_value_free(VibeValue *value);        /* recursive; NULL-safe */
void        vibe_value_print(VibeValue *value, int indent); /* debug dump (not VIBE) */
```

---

## Command-Line Tool

Installed as `vibe`:

```
vibe check <file>          validate; on failure prints  file:line:col: error [category]: message
vibe fmt   <file> [-w]     reformat to canonical VIBE (stdout, or -w to rewrite in place)
vibe get   <file> <path>   print the scalar at a dotted path
vibe version               print library + format versions
```

Exit codes: `0` success, `1` parse/lookup error, `2` usage error, `3` I/O error.

---

## Examples

### Read a config

```c
#include <vibe.h>
#include <stdio.h>

int main(void) {
    VibeParser *p = vibe_parser_new();
    VibeValue  *cfg = vibe_parse_file(p, "config.vibe");
    if (!cfg) {
        VibeError e = vibe_get_last_error(p);
        fprintf(stderr, "%d:%d [%s] %s\n",
                e.line, e.column, vibe_error_code_string(e.code), e.message);
        vibe_parser_free(p);
        return 1;
    }

    printf("host = %s\n", vibe_get_string_or(cfg, "server.host", "localhost"));
    printf("port = %lld\n", (long long)vibe_get_int_or(cfg, "server.port", 8080));

    vibe_value_free(cfg);
    vibe_parser_free(p);
    return 0;
}
```

Build (installed): `cc app.c $(pkg-config --cflags --libs vibe) -o app`
Build (vendored): `cc app.c vibe.c -o app`

### Build a tree and emit it

```c
VibeValue *root = vibe_value_new_object();
vibe_object_set(root->as_object, "name", vibe_value_new_string("My App"));
vibe_object_set(root->as_object, "port", vibe_value_new_integer(8080));

VibeValue *tags = vibe_value_new_array();
vibe_array_push(tags->as_array, vibe_value_new_string("web"));
vibe_array_push(tags->as_array, vibe_value_new_string("api"));
vibe_object_set(root->as_object, "tags", tags);

char *text = vibe_emit(root);   /* name "My App"\nport 8080\ntags [ ... ]\n */
fputs(text, stdout);
vibe_free(text);
vibe_value_free(root);
```

### Validate untrusted input strictly

```c
VibeParser *p = vibe_parser_new();
VibeLimits limits = vibe_default_limits();
limits.max_document_size = 64 * 1024;   /* 64 KiB hard cap */
limits.max_depth = 16;
vibe_parser_set_limits(p, &limits);

VibeValue *v = vibe_parse_buffer(p, untrusted, untrusted_len);
if (!v) { /* fails closed with a stable VibeErrorCode */ }
```

---

For the language itself, see the [SPECIFICATION](../SPECIFICATION.md) and the
[conformance suite](../tests/conformance). Keep calm and VIBE on. 🌊
