/*
 * ============================================================================
 * libvibe — the reference parser and emitter for the VIBE config format
 * ============================================================================
 *
 * VIBE is a config format for humans: nested objects, flat scalar arrays, and
 * exactly four scalar types (integer, float, boolean, string). It refuses the
 * features that turn config into a programming language — no anchors, no
 * includes, no templates, no variable substitution, no implicit coercion.
 *
 * This single header is the entire public API. Pair it with vibe.c (or link
 * against libvibe). No global init, no build-time configuration required.
 *
 *   VibeParser *p   = vibe_parser_new();
 *   VibeValue  *cfg = vibe_parse_file(p, "config.vibe");
 *   if (!cfg) { VibeError e = vibe_get_last_error(p); ... }
 *   const char *host = vibe_get_string(cfg, "server.host");
 *   vibe_value_free(cfg);
 *   vibe_parser_free(p);
 *
 * Design guarantees:
 *   - No global mutable state beyond the optional allocator hooks; every parse
 *     is independent and re-entrant across threads (one VibeParser per thread).
 *   - Fails closed: malformed or oversized input is rejected with a stable
 *     error code, never silently truncated or coerced.
 *   - Round-trips: vibe_emit() serialises a value tree back to canonical VIBE
 *     text that re-parses to an equal tree.
 *
 * License: see LICENSE. SPDX-License-Identifier: MIT
 */

#ifndef VIBE_H
#define VIBE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VERSION
 * ============================================================================
 * VIBE_VERSION_* is the version of THIS LIBRARY (libvibe). VIBE_FORMAT_VERSION
 * is the version of the language it implements. They advance independently.
 */
#define VIBE_VERSION_MAJOR 1
#define VIBE_VERSION_MINOR 1
#define VIBE_VERSION_PATCH 0
#define VIBE_VERSION_STRING "1.1.0"
#define VIBE_VERSION_NUMBER \
    (VIBE_VERSION_MAJOR * 10000 + VIBE_VERSION_MINOR * 100 + VIBE_VERSION_PATCH)

/* The VIBE language level this build parses and emits. */
#define VIBE_FORMAT_VERSION "1.0"

/* ============================================================================
 * SYMBOL VISIBILITY
 * ============================================================================
 * Define VIBE_BUILD_SHARED when compiling the shared library, and
 * VIBE_USE_SHARED when consuming it as a DLL on Windows. Static use needs
 * neither.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(VIBE_BUILD_SHARED)
#    define VIBE_API __declspec(dllexport)
#  elif defined(VIBE_USE_SHARED)
#    define VIBE_API __declspec(dllimport)
#  else
#    define VIBE_API
#  endif
#elif defined(VIBE_BUILD_SHARED) && (defined(__GNUC__) || defined(__clang__))
#  define VIBE_API __attribute__((visibility("default")))
#else
#  define VIBE_API
#endif

/* ============================================================================
 * VALUE TYPES
 * ============================================================================ */

/** The type tag carried by every VibeValue. */
typedef enum {
    VIBE_TYPE_NULL = 0,   /* absence of value (never produced by the parser) */
    VIBE_TYPE_INTEGER,    /* 64-bit signed integer */
    VIBE_TYPE_FLOAT,      /* IEEE-754 double */
    VIBE_TYPE_BOOLEAN,    /* true / false */
    VIBE_TYPE_STRING,     /* UTF-8 text (heap owned) */
    VIBE_TYPE_ARRAY,      /* ordered list of scalars (never objects/arrays) */
    VIBE_TYPE_OBJECT      /* ordered set of key -> value entries */
} VibeType;

typedef struct VibeValue VibeValue;
typedef struct VibeObject VibeObject;
typedef struct VibeArray VibeArray;

/** One key/value pair inside an object. Keys are heap-owned UTF-8. */
typedef struct {
    char* key;
    VibeValue* value;
} VibeObjectEntry;

/** An object: an insertion-ordered collection of key/value pairs. */
struct VibeObject {
    VibeObjectEntry* entries;
    size_t count;
    size_t capacity;
};

/** An array: an insertion-ordered list of scalar values. */
struct VibeArray {
    VibeValue** values;
    size_t count;
    size_t capacity;
};

/**
 * The universal tagged-union value. Inspect .type, then read the matching
 * union member. A value owns everything it points to; vibe_value_free()
 * releases the whole subtree.
 */
struct VibeValue {
    VibeType type;
    union {
        int64_t as_integer;
        double as_float;
        bool as_boolean;
        char* as_string;
        VibeArray* as_array;
        VibeObject* as_object;
    };
};

/* ============================================================================
 * ERRORS
 * ============================================================================ */

/**
 * Stable, machine-comparable error categories. The first eleven map 1:1 to the
 * error codes defined normatively in SPECIFICATION.md; the last two cover
 * non-parse failures (I/O, allocation). Use vibe_error_code_string() to get the
 * canonical spelling ("unclosed-object", ...).
 */
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
    VIBE_ERROR_IO,                  /* "io-error"    (non-parse) */
    VIBE_ERROR_OUT_OF_MEMORY        /* "out-of-memory" (non-parse) */
} VibeErrorCode;

/**
 * Details of the most recent failure. Check has_error first; when true, code is
 * the stable category, message is a human-readable explanation, and line/column
 * are 1-based positions into the source. The message is owned by the parser and
 * remains valid until the next parse or vibe_parser_free().
 */
typedef struct {
    bool has_error;
    VibeErrorCode code;
    char* message;
    int line;
    int column;
} VibeError;

/* Canonical hyphenated spelling of an error code, e.g. "unclosed-object". */
VIBE_API const char* vibe_error_code_string(VibeErrorCode code);

/* ============================================================================
 * RESOURCE LIMITS
 * ============================================================================
 * Bounds enforced while parsing so that untrusted input cannot exhaust memory
 * or the stack. Defaults are the REQUIRED minimums from SPECIFICATION.md; every
 * conforming parser accepts at least this much, so a document within these
 * bounds is portable. Raise them via vibe_parser_set_limits() when you trust
 * the source. Exceeding any bound yields VIBE_ERROR_LIMIT_EXCEEDED.
 */
typedef struct {
    size_t max_document_size;   /* total input bytes            (default 16 MiB) */
    size_t max_depth;           /* object/array nesting levels  (default 64)     */
    size_t max_string_length;   /* decoded quoted-string bytes  (default 1 MiB)  */
    size_t max_key_length;      /* key bytes                    (default 1 KiB)  */
    size_t max_array_elements;  /* elements in one array        (default 65536)  */
    size_t max_object_keys;     /* keys in one object           (default 65536)  */
    size_t max_number_digits;   /* chars in one numeric token   (default 1024)   */
} VibeLimits;

/** The default (spec-minimum) limits. */
VIBE_API VibeLimits vibe_default_limits(void);

/* ============================================================================
 * ALLOCATOR HOOKS
 * ============================================================================
 * Optional. Install a custom allocator ONCE at startup, before any other libvibe
 * call, and do not change it while values are live (it is used to both allocate
 * and free them). Passing NULL for any function resets that slot to the C
 * standard library. Not thread-safe to call concurrently with other libvibe
 * work; treat it as one-time configuration.
 */
VIBE_API void vibe_set_allocators(void* (*malloc_fn)(size_t),
                                  void* (*realloc_fn)(void*, size_t),
                                  void  (*free_fn)(void*));

/** Allocate/free through the installed hooks. Use vibe_free() on any buffer
 *  libvibe hands back to you (e.g. from vibe_emit()). */
VIBE_API void* vibe_malloc(size_t size);
VIBE_API void  vibe_free(void* ptr);

/* ============================================================================
 * VERSION QUERIES
 * ============================================================================ */

/** Runtime library version string, e.g. "1.1.0". */
VIBE_API const char* vibe_version(void);
/** Runtime library version as MAJOR*10000 + MINOR*100 + PATCH. */
VIBE_API int vibe_version_number(void);
/** The VIBE language level this build implements, e.g. "1.0". */
VIBE_API const char* vibe_format_version(void);

/* ============================================================================
 * PARSER STATE
 * ============================================================================
 * A reusable parse context: current position plus the last error. Create with
 * vibe_parser_new(), reuse across many parses, free with vibe_parser_free().
 * The position fields are exposed for tooling/visualisation; treat them as
 * read-only during a parse.
 */
typedef struct {
    const char* input;   /* current source (not owned)     */
    size_t pos;          /* byte offset into input         */
    size_t length;       /* length of input in bytes       */
    int line;            /* current line   (1-based)       */
    int column;          /* current column (1-based)       */
    VibeError error;     /* details of the last failure    */
    VibeLimits limits;   /* bounds enforced while parsing  */
} VibeParser;

/* ============================================================================
 * PARSER LIFECYCLE
 * ============================================================================ */

/** Create a parser with default limits. Returns NULL on allocation failure. */
VIBE_API VibeParser* vibe_parser_new(void);

/** Free a parser and its error buffer. NULL-safe. */
VIBE_API void vibe_parser_free(VibeParser* parser);

/** Replace the parser's resource limits. NULL restores the defaults. */
VIBE_API void vibe_parser_set_limits(VibeParser* parser, const VibeLimits* limits);

/* ============================================================================
 * PARSING
 * ============================================================================
 * All return a heap-allocated root VibeValue (of type OBJECT) on success, or
 * NULL on failure — call vibe_get_last_error() for details. Free the result
 * with vibe_value_free(). The input is not retained after the call returns.
 */

/** Parse a NUL-terminated string. Embedded NUL bytes truncate the input;
 *  use vibe_parse_buffer() for data that may contain NUL. */
VIBE_API VibeValue* vibe_parse_string(VibeParser* parser, const char* input);

/** Parse exactly `length` bytes (NUL-safe, length-aware). */
VIBE_API VibeValue* vibe_parse_buffer(VibeParser* parser, const char* data, size_t length);

/** Read a file fully and parse it. */
VIBE_API VibeValue* vibe_parse_file(VibeParser* parser, const char* filename);

/** Stateless one-shot parse: no parser object required. On failure returns NULL
 *  and, if out_error is non-NULL, fills it with an owned copy of the error that
 *  the caller frees with vibe_error_free(). */
VIBE_API VibeValue* vibe_parse(const char* data, size_t length, VibeError* out_error);

/* ============================================================================
 * ERROR RETRIEVAL
 * ============================================================================ */

/** Snapshot the parser's last error. The returned message aliases the parser's
 *  buffer; do not free it and do not call vibe_error_free() on this copy. */
VIBE_API VibeError vibe_get_last_error(VibeParser* parser);

/** Free an owned error (only the one produced by vibe_parse()). NULL-safe. */
VIBE_API void vibe_error_free(VibeError* error);

/* ============================================================================
 * VALUE CONSTRUCTORS
 * ============================================================================
 * Each returns a new heap value, or NULL on allocation failure. String
 * constructors copy their input. Free the whole tree with vibe_value_free().
 */
VIBE_API VibeValue* vibe_value_new_null(void);
VIBE_API VibeValue* vibe_value_new_integer(int64_t value);
VIBE_API VibeValue* vibe_value_new_float(double value);
VIBE_API VibeValue* vibe_value_new_boolean(bool value);
VIBE_API VibeValue* vibe_value_new_string(const char* value);
VIBE_API VibeValue* vibe_value_new_string_len(const char* value, size_t length);
VIBE_API VibeValue* vibe_value_new_array(void);
VIBE_API VibeValue* vibe_value_new_object(void);

/** Deep-copy a value and everything it owns. NULL on failure. */
VIBE_API VibeValue* vibe_value_clone(const VibeValue* value);

/* ============================================================================
 * PATH ACCESS  (dotted paths, e.g. "server.tls.port")
 * ============================================================================
 * Convenience readers that walk objects by dotted key. The plain readers return
 * a zero/NULL sentinel when the path is missing or the wrong type; the *_or
 * variants let you supply the fallback explicitly.
 */
VIBE_API VibeValue* vibe_get(VibeValue* root, const char* path);
VIBE_API const char* vibe_get_string(VibeValue* value, const char* path);
VIBE_API int64_t vibe_get_int(VibeValue* value, const char* path);
VIBE_API double vibe_get_float(VibeValue* value, const char* path);
VIBE_API bool vibe_get_bool(VibeValue* value, const char* path);
VIBE_API VibeArray* vibe_get_array(VibeValue* value, const char* path);
VIBE_API VibeObject* vibe_get_object(VibeValue* value, const char* path);

VIBE_API const char* vibe_get_string_or(VibeValue* value, const char* path, const char* fallback);
VIBE_API int64_t vibe_get_int_or(VibeValue* value, const char* path, int64_t fallback);
VIBE_API double vibe_get_float_or(VibeValue* value, const char* path, double fallback);
VIBE_API bool vibe_get_bool_or(VibeValue* value, const char* path, bool fallback);

/* ============================================================================
 * CONTAINER MANIPULATION
 * ============================================================================ */

/** Insert or replace `key` in `object`, taking ownership of `value`. Replacing
 *  an existing key frees the old value (last-wins, matching the parser). */
VIBE_API void vibe_object_set(VibeObject* object, const char* key, VibeValue* value);

/** Look up a key. Returns the stored value (not a copy), or NULL if absent. */
VIBE_API VibeValue* vibe_object_get(VibeObject* object, const char* key);

/** Number of entries in an object (0 if NULL). */
VIBE_API size_t vibe_object_size(const VibeObject* object);

/** Append `value` to `array`, taking ownership. */
VIBE_API void vibe_array_push(VibeArray* array, VibeValue* value);

/** Element at `index` (not a copy), or NULL if out of range. */
VIBE_API VibeValue* vibe_array_get(VibeArray* array, size_t index);

/** Number of elements in an array (0 if NULL). */
VIBE_API size_t vibe_array_size(const VibeArray* array);

/* ============================================================================
 * EMITTING  (serialise a value tree back to canonical VIBE text)
 * ============================================================================ */

/** Serialise a value to canonical VIBE text. The root should be an OBJECT (its
 *  entries are emitted at the document level); any value type is accepted. The
 *  result is heap-allocated — free it with vibe_free(). NULL on failure. */
VIBE_API char* vibe_emit(const VibeValue* value);

/** Emit to a file (atomic write is not guaranteed). Returns true on success. */
VIBE_API bool vibe_emit_file(const VibeValue* value, const char* path);

/* ============================================================================
 * MISC
 * ============================================================================ */

/** Human-readable name of a type, e.g. "object". Never NULL. */
VIBE_API const char* vibe_type_name(VibeType type);

/** Recursively free a value and everything it owns. NULL-safe. */
VIBE_API void vibe_value_free(VibeValue* value);

/** Pretty-print a value to stdout (debug aid; not canonical VIBE). */
VIBE_API void vibe_value_print(VibeValue* value, int indent);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* VIBE_H */
