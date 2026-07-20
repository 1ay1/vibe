/*
 * ============================================================================
 * libvibe — reference implementation of the VIBE config format
 * ============================================================================
 *
 * A single translation unit: lexer, recursive-descent-over-an-explicit-stack
 * parser, value model, path access, and a canonical emitter. The parse loop is
 * iterative (an explicit frame stack), so deeply nested input cannot overflow
 * the C call stack; the enforced resource limits keep untrusted input bounded.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "vibe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>

/* Initial capacity for objects/arrays; they grow geometrically. */
#define VIBE_INITIAL_CAPACITY 16

/* ============================================================================
 * Allocator hooks
 * ============================================================================
 * All heap traffic in libvibe routes through these, so a custom allocator set
 * via vibe_set_allocators() governs both allocation and freeing of the value
 * tree (which is why it must be installed before any value is created).
 */
static void* (*g_malloc)(size_t) = malloc;
static void* (*g_realloc)(void*, size_t) = realloc;
static void  (*g_free)(void*) = free;

void vibe_set_allocators(void* (*malloc_fn)(size_t),
                         void* (*realloc_fn)(void*, size_t),
                         void  (*free_fn)(void*)) {
    g_malloc  = malloc_fn  ? malloc_fn  : malloc;
    g_realloc = realloc_fn ? realloc_fn : realloc;
    g_free    = free_fn    ? free_fn    : free;
}

static void* vibe__malloc(size_t size) {
    if (size == 0) size = 1;
    return g_malloc(size);
}

static void* vibe__calloc(size_t n, size_t size) {
    if (n != 0 && size > (size_t)-1 / n) return NULL; /* overflow guard */
    size_t total = n * size;
    void* p = vibe__malloc(total);
    if (p) memset(p, 0, total ? total : 1);
    return p;
}

static void* vibe__realloc(void* ptr, size_t size) {
    if (size == 0) size = 1;
    return g_realloc(ptr, size);
}

static void vibe__free(void* ptr) {
    if (ptr) g_free(ptr);
}

static char* vibe__strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* out = (char*)vibe__malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static char* vibe__strndup(const char* s, size_t n) {
    char* out = (char*)vibe__malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Public thin wrappers so callers can free libvibe-owned buffers correctly. */
void* vibe_malloc(size_t size) { return vibe__malloc(size); }
void  vibe_free(void* ptr) { vibe__free(ptr); }

/* ============================================================================
 * Version, limits, and code/type names
 * ============================================================================ */

const char* vibe_version(void) { return VIBE_VERSION_STRING; }
int vibe_version_number(void) { return VIBE_VERSION_NUMBER; }
const char* vibe_format_version(void) { return VIBE_FORMAT_VERSION; }

VibeLimits vibe_default_limits(void) {
    VibeLimits l;
    l.max_document_size = 16u * 1024u * 1024u; /* 16 MiB  */
    l.max_depth         = 64;                  /* levels  */
    l.max_string_length = 1u * 1024u * 1024u;  /* 1 MiB   */
    l.max_key_length    = 1024;                /* 1 KiB   */
    l.max_array_elements = 65536;
    l.max_object_keys    = 65536;
    l.max_number_digits  = 1024;
    return l;
}

const char* vibe_error_code_string(VibeErrorCode code) {
    switch (code) {
        case VIBE_OK:                      return "ok";
        case VIBE_ERROR_ENCODING:          return "encoding-error";
        case VIBE_ERROR_ILLEGAL_CHARACTER: return "illegal-character";
        case VIBE_ERROR_UNEXPECTED_TOKEN:  return "unexpected-token";
        case VIBE_ERROR_UNCLOSED_OBJECT:   return "unclosed-object";
        case VIBE_ERROR_UNCLOSED_ARRAY:    return "unclosed-array";
        case VIBE_ERROR_UNTERMINATED_STRING: return "unterminated-string";
        case VIBE_ERROR_NESTED_CONTAINER:  return "nested-container";
        case VIBE_ERROR_INVALID_ESCAPE:    return "invalid-escape";
        case VIBE_ERROR_INVALID_NUMBER:    return "invalid-number";
        case VIBE_ERROR_LIMIT_EXCEEDED:    return "limit-exceeded";
        case VIBE_ERROR_IO:                return "io-error";
        case VIBE_ERROR_OUT_OF_MEMORY:     return "out-of-memory";
    }
    return "unknown-error";
}

const char* vibe_type_name(VibeType type) {
    switch (type) {
        case VIBE_TYPE_NULL:    return "null";
        case VIBE_TYPE_INTEGER: return "integer";
        case VIBE_TYPE_FLOAT:   return "float";
        case VIBE_TYPE_BOOLEAN: return "boolean";
        case VIBE_TYPE_STRING:  return "string";
        case VIBE_TYPE_ARRAY:   return "array";
        case VIBE_TYPE_OBJECT:  return "object";
    }
    return "unknown";
}

/* ============================================================================
 * Lexer / parser internal types
 * ============================================================================ */

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_BOOLEAN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_NEWLINE,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
    int column;
} Token;

typedef enum {
    STATE_ROOT,
    STATE_OBJECT,
    STATE_ARRAY
} ParseState;

typedef struct {
    ParseState state;
    VibeValue* container;
} StateFrame;

/* Forward declarations */
static void set_error(VibeParser* parser, VibeErrorCode code, const char* fmt, ...);
static bool is_identifier_start(int c);
static bool is_identifier_char(int c);
static bool is_unquoted_string_char(int c);
static bool is_unquoted_start_char(int c);
static bool is_valid_number(const char* str);
static Token next_token(VibeParser* parser);
static void token_free(Token* token);
static VibeValue* parse_value_from_token(Token* token);

/* ============================================================================
 * Parser lifecycle
 * ============================================================================ */

VibeParser* vibe_parser_new(void) {
    VibeParser* parser = (VibeParser*)vibe__calloc(1, sizeof(VibeParser));
    if (!parser) return NULL;
    parser->line = 1;
    parser->column = 1;
    parser->limits = vibe_default_limits();
    return parser;
}

void vibe_parser_free(VibeParser* parser) {
    if (!parser) return;
    vibe__free(parser->error.message);
    vibe__free(parser);
}

void vibe_parser_set_limits(VibeParser* parser, const VibeLimits* limits) {
    if (!parser) return;
    parser->limits = limits ? *limits : vibe_default_limits();
}

/* ============================================================================
 * Error handling
 * ============================================================================ */

static void set_error(VibeParser* parser, VibeErrorCode code, const char* fmt, ...) {
    if (parser->error.has_error) return; /* keep the first error */

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    parser->error.has_error = true;
    parser->error.code = code;
    vibe__free(parser->error.message);
    parser->error.message = vibe__strdup(buffer);
    parser->error.line = parser->line;
    parser->error.column = parser->column;
}

VibeError vibe_get_last_error(VibeParser* parser) {
    if (!parser) {
        VibeError e;
        memset(&e, 0, sizeof(e));
        return e;
    }
    return parser->error;
}

void vibe_error_free(VibeError* error) {
    if (error && error->message) {
        vibe__free(error->message);
        error->message = NULL;
    }
}

/* ============================================================================
 * Character classification (all take int; callers pass unsigned-char values)
 * ============================================================================ */

static bool is_identifier_start(int c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_identifier_char(int c) {
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

/* Printable ASCII 0x21..0x7E, excluding the structural characters. */
static bool is_unquoted_string_char(int c) {
    unsigned char uc = (unsigned char)c;
    if (uc <= 0x20 || uc > 0x7E) return false;
    if (uc == '{' || uc == '}' || uc == '[' || uc == ']' || uc == '#') return false;
    return true;
}

/* The characters that may BEGIN an unquoted token (mirrors next_token). */
static bool is_unquoted_start_char(int c) {
    return is_identifier_start(c) || isdigit((unsigned char)c) ||
           c == '-' || c == '/' || c == '.' || c == '~';
}

static bool is_valid_number(const char* str) {
    if (!str || *str == '\0') return false;
    const char* p = str;
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return false;

    bool has_dot = false;
    bool has_digit_after_dot = false;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            if (has_dot) has_digit_after_dot = true;
            p++;
        } else if (*p == '.') {
            if (has_dot) return false;
            has_dot = true;
            p++;
        } else {
            return false;
        }
    }
    if (has_dot && !has_digit_after_dot) return false;
    return true;
}

/* ============================================================================
 * Lexer
 * ============================================================================ */

static void skip_whitespace(VibeParser* parser) {
    while (parser->pos < parser->length) {
        char c = parser->input[parser->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            parser->pos++;
            parser->column++;
        } else {
            break;
        }
    }
}

static void skip_comment(VibeParser* parser) {
    if (parser->pos < parser->length && parser->input[parser->pos] == '#') {
        while (parser->pos < parser->length && parser->input[parser->pos] != '\n') {
            parser->pos++;
        }
    }
}

/* Read a quoted string into a dynamically grown buffer. Enforces the decoded
 * string-length limit rather than a fixed cap. Returns a heap string the caller
 * owns, or NULL on error (with parser error set). */
static char* parse_quoted_string(VibeParser* parser) {
    parser->pos++; /* opening quote */
    parser->column++;

    size_t cap = 64;
    size_t len = 0;
    char* buf = (char*)vibe__malloc(cap);
    if (!buf) {
        set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    while (parser->pos < parser->length) {
        char c = parser->input[parser->pos];
        char out;

        if (c == '"') {
            parser->pos++;
            parser->column++;
            buf[len] = '\0';
            return buf;
        } else if (c == '\\' && parser->pos + 1 < parser->length) {
            parser->pos++;
            parser->column++;
            char next = parser->input[parser->pos];
            switch (next) {
                case '"':  out = '"';  break;
                case '\\': out = '\\'; break;
                case 'n':  out = '\n'; break;
                case 't':  out = '\t'; break;
                case 'r':  out = '\r'; break;
                default:
                    set_error(parser, VIBE_ERROR_INVALID_ESCAPE,
                              "Invalid escape sequence '\\%c'", next);
                    vibe__free(buf);
                    return NULL;
            }
            parser->pos++;
            parser->column++;
        } else if (c == '\n') {
            set_error(parser, VIBE_ERROR_UNTERMINATED_STRING,
                      "Unterminated string (newline before closing quote)");
            vibe__free(buf);
            return NULL;
        } else {
            out = c;
            parser->pos++;
            parser->column++;
        }

        if (len > parser->limits.max_string_length) {
            set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                      "String exceeds the maximum length of %zu bytes",
                      parser->limits.max_string_length);
            vibe__free(buf);
            return NULL;
        }
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            char* nbuf = (char*)vibe__realloc(buf, ncap);
            if (!nbuf) {
                set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory");
                vibe__free(buf);
                return NULL;
            }
            buf = nbuf;
            cap = ncap;
        }
        buf[len++] = out;
    }

    set_error(parser, VIBE_ERROR_UNTERMINATED_STRING,
              "Unterminated string (end of input before closing quote)");
    vibe__free(buf);
    return NULL;
}

static Token next_token(VibeParser* parser) {
    Token token;
    memset(&token, 0, sizeof(token));

    skip_whitespace(parser);
    skip_comment(parser);
    skip_whitespace(parser);

    if (parser->pos >= parser->length) {
        token.type = TOKEN_EOF;
        return token;
    }

    char c = parser->input[parser->pos];
    token.line = parser->line;
    token.column = parser->column;

    if (c == '\n') {
        token.type = TOKEN_NEWLINE;
        parser->pos++;
        parser->line++;
        parser->column = 1;
        return token;
    }
    if (c == '{') { token.type = TOKEN_LEFT_BRACE;    parser->pos++; parser->column++; return token; }
    if (c == '}') { token.type = TOKEN_RIGHT_BRACE;   parser->pos++; parser->column++; return token; }
    if (c == '[') { token.type = TOKEN_LEFT_BRACKET;  parser->pos++; parser->column++; return token; }
    if (c == ']') { token.type = TOKEN_RIGHT_BRACKET; parser->pos++; parser->column++; return token; }

    if (c == '"') {
        token.type = TOKEN_STRING;
        token.value = parse_quoted_string(parser);
        if (!token.value) token.type = TOKEN_ERROR;
        return token;
    }

    if (is_unquoted_start_char(c)) {
        size_t start = parser->pos;

        /* A leading '-' immediately followed by a digit is part of a number. */
        if (c == '-' && parser->pos + 1 < parser->length &&
            isdigit((unsigned char)parser->input[parser->pos + 1])) {
            parser->pos++;
            parser->column++;
        }

        while (parser->pos < parser->length &&
               is_unquoted_string_char(parser->input[parser->pos])) {
            parser->pos++;
            parser->column++;
        }

        token.value = vibe__strndup(parser->input + start, parser->pos - start);
        if (!token.value) {
            set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory");
            token.type = TOKEN_ERROR;
            return token;
        }

        if (strcmp(token.value, "true") == 0 || strcmp(token.value, "false") == 0) {
            token.type = TOKEN_BOOLEAN;
        } else if (is_valid_number(token.value)) {
            if (strlen(token.value) > parser->limits.max_number_digits) {
                set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                          "Numeric token exceeds the maximum of %zu characters",
                          parser->limits.max_number_digits);
                token_free(&token);
                token.type = TOKEN_ERROR;
                return token;
            }
            token.type = TOKEN_NUMBER;
        } else if (is_identifier_start(token.value[0])) {
            bool ident = true;
            for (const char* p = token.value; *p; p++) {
                if (!is_identifier_char(*p)) { ident = false; break; }
            }
            token.type = ident ? TOKEN_IDENTIFIER : TOKEN_STRING;
        } else {
            token.type = TOKEN_STRING;
        }
        return token;
    }

    set_error(parser, VIBE_ERROR_ILLEGAL_CHARACTER, "Unexpected character '%c' (0x%02X)",
              (c >= 0x20 && c < 0x7F) ? c : '?', (unsigned char)c);
    token.type = TOKEN_ERROR;
    return token;
}

static void token_free(Token* token) {
    if (token && token->value) {
        vibe__free(token->value);
        token->value = NULL;
    }
}

/* ============================================================================
 * Value constructors
 * ============================================================================ */

static VibeValue* value_alloc(VibeType type) {
    VibeValue* v = (VibeValue*)vibe__calloc(1, sizeof(VibeValue));
    if (v) v->type = type;
    return v;
}

VibeValue* vibe_value_new_null(void) { return value_alloc(VIBE_TYPE_NULL); }

VibeValue* vibe_value_new_integer(int64_t value) {
    VibeValue* v = value_alloc(VIBE_TYPE_INTEGER);
    if (v) v->as_integer = value;
    return v;
}

VibeValue* vibe_value_new_float(double value) {
    VibeValue* v = value_alloc(VIBE_TYPE_FLOAT);
    if (v) v->as_float = value;
    return v;
}

VibeValue* vibe_value_new_boolean(bool value) {
    VibeValue* v = value_alloc(VIBE_TYPE_BOOLEAN);
    if (v) v->as_boolean = value;
    return v;
}

VibeValue* vibe_value_new_string_len(const char* value, size_t length) {
    VibeValue* v = value_alloc(VIBE_TYPE_STRING);
    if (!v) return NULL;
    v->as_string = vibe__strndup(value ? value : "", value ? length : 0);
    if (!v->as_string) { vibe__free(v); return NULL; }
    return v;
}

VibeValue* vibe_value_new_string(const char* value) {
    return vibe_value_new_string_len(value, value ? strlen(value) : 0);
}

VibeValue* vibe_value_new_array(void) {
    VibeValue* v = value_alloc(VIBE_TYPE_ARRAY);
    if (!v) return NULL;
    v->as_array = (VibeArray*)vibe__calloc(1, sizeof(VibeArray));
    if (!v->as_array) { vibe__free(v); return NULL; }
    v->as_array->capacity = VIBE_INITIAL_CAPACITY;
    v->as_array->values = (VibeValue**)vibe__calloc(VIBE_INITIAL_CAPACITY, sizeof(VibeValue*));
    if (!v->as_array->values) { vibe__free(v->as_array); vibe__free(v); return NULL; }
    return v;
}

VibeValue* vibe_value_new_object(void) {
    VibeValue* v = value_alloc(VIBE_TYPE_OBJECT);
    if (!v) return NULL;
    v->as_object = (VibeObject*)vibe__calloc(1, sizeof(VibeObject));
    if (!v->as_object) { vibe__free(v); return NULL; }
    v->as_object->capacity = VIBE_INITIAL_CAPACITY;
    v->as_object->entries = (VibeObjectEntry*)vibe__calloc(VIBE_INITIAL_CAPACITY, sizeof(VibeObjectEntry));
    if (!v->as_object->entries) { vibe__free(v->as_object); vibe__free(v); return NULL; }
    return v;
}

VibeValue* vibe_value_clone(const VibeValue* value) {
    if (!value) return NULL;
    switch (value->type) {
        case VIBE_TYPE_NULL:    return vibe_value_new_null();
        case VIBE_TYPE_INTEGER: return vibe_value_new_integer(value->as_integer);
        case VIBE_TYPE_FLOAT:   return vibe_value_new_float(value->as_float);
        case VIBE_TYPE_BOOLEAN: return vibe_value_new_boolean(value->as_boolean);
        case VIBE_TYPE_STRING:  return vibe_value_new_string(value->as_string);
        case VIBE_TYPE_ARRAY: {
            VibeValue* arr = vibe_value_new_array();
            if (!arr) return NULL;
            for (size_t i = 0; i < value->as_array->count; i++) {
                VibeValue* el = vibe_value_clone(value->as_array->values[i]);
                if (!el) { vibe_value_free(arr); return NULL; }
                vibe_array_push(arr->as_array, el);
            }
            return arr;
        }
        case VIBE_TYPE_OBJECT: {
            VibeValue* obj = vibe_value_new_object();
            if (!obj) return NULL;
            for (size_t i = 0; i < value->as_object->count; i++) {
                VibeValue* cv = vibe_value_clone(value->as_object->entries[i].value);
                if (!cv) { vibe_value_free(obj); return NULL; }
                vibe_object_set(obj->as_object, value->as_object->entries[i].key, cv);
            }
            return obj;
        }
    }
    return NULL;
}

static VibeValue* parse_value_from_token(Token* token) {
    if (!token) return NULL;
    switch (token->type) {
        case TOKEN_BOOLEAN:
            return vibe_value_new_boolean(strcmp(token->value, "true") == 0);
        case TOKEN_NUMBER: {
            char* endptr;
            errno = 0;
            if (strchr(token->value, '.')) {
                double val = strtod(token->value, &endptr);
                if (endptr == token->value || *endptr != '\0' || errno == ERANGE) return NULL;
                return vibe_value_new_float(val);
            } else {
                long long val = strtoll(token->value, &endptr, 10);
                if (endptr == token->value || *endptr != '\0' || errno == ERANGE) return NULL;
                return vibe_value_new_integer((int64_t)val);
            }
        }
        case TOKEN_STRING:
        case TOKEN_IDENTIFIER:
            return vibe_value_new_string(token->value);
        default:
            return NULL;
    }
}

/* ============================================================================
 * Object / array operations
 * ============================================================================ */

void vibe_object_set(VibeObject* obj, const char* key, VibeValue* value) {
    if (!obj || !key || !value) { vibe_value_free(value); return; }

    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            vibe_value_free(obj->entries[i].value);
            obj->entries[i].value = value; /* last-wins */
            return;
        }
    }

    if (obj->count >= obj->capacity) {
        size_t ncap = obj->capacity ? obj->capacity * 2 : VIBE_INITIAL_CAPACITY;
        VibeObjectEntry* ne = (VibeObjectEntry*)vibe__realloc(obj->entries, ncap * sizeof(VibeObjectEntry));
        if (!ne) { vibe_value_free(value); return; }
        obj->entries = ne;
        obj->capacity = ncap;
    }

    char* kdup = vibe__strdup(key);
    if (!kdup) { vibe_value_free(value); return; }
    obj->entries[obj->count].key = kdup;
    obj->entries[obj->count].value = value;
    obj->count++;
}

VibeValue* vibe_object_get(VibeObject* obj, const char* key) {
    if (!obj || !key) return NULL;
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) return obj->entries[i].value;
    }
    return NULL;
}

size_t vibe_object_size(const VibeObject* obj) { return obj ? obj->count : 0; }

void vibe_array_push(VibeArray* arr, VibeValue* value) {
    if (!arr || !value) { vibe_value_free(value); return; }

    if (arr->count >= arr->capacity) {
        size_t ncap = arr->capacity ? arr->capacity * 2 : VIBE_INITIAL_CAPACITY;
        VibeValue** nv = (VibeValue**)vibe__realloc(arr->values, ncap * sizeof(VibeValue*));
        if (!nv) { vibe_value_free(value); return; }
        arr->values = nv;
        arr->capacity = ncap;
    }
    arr->values[arr->count++] = value;
}

VibeValue* vibe_array_get(VibeArray* arr, size_t index) {
    if (!arr || index >= arr->count) return NULL;
    return arr->values[index];
}

size_t vibe_array_size(const VibeArray* arr) { return arr ? arr->count : 0; }

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void vibe_value_free(VibeValue* value) {
    if (!value) return;
    switch (value->type) {
        case VIBE_TYPE_STRING:
            vibe__free(value->as_string);
            break;
        case VIBE_TYPE_ARRAY:
            if (value->as_array) {
                for (size_t i = 0; i < value->as_array->count; i++)
                    vibe_value_free(value->as_array->values[i]);
                vibe__free(value->as_array->values);
                vibe__free(value->as_array);
            }
            break;
        case VIBE_TYPE_OBJECT:
            if (value->as_object) {
                for (size_t i = 0; i < value->as_object->count; i++) {
                    vibe__free(value->as_object->entries[i].key);
                    vibe_value_free(value->as_object->entries[i].value);
                }
                vibe__free(value->as_object->entries);
                vibe__free(value->as_object);
            }
            break;
        default:
            break;
    }
    vibe__free(value);
}

/* ============================================================================
 * Core parse loop
 * ============================================================================ */

VibeValue* vibe_parse_buffer(VibeParser* parser, const char* data, size_t length) {
    if (!parser || !data) return NULL;

    /* Fresh parse: clear any prior error. */
    vibe__free(parser->error.message);
    parser->error.message = NULL;
    parser->error.has_error = false;
    parser->error.code = VIBE_OK;

    parser->input = data;
    parser->length = length;
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;

    if (length > parser->limits.max_document_size) {
        set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                  "Document exceeds the maximum size of %zu bytes",
                  parser->limits.max_document_size);
        return NULL;
    }

    /* Reject a leading UTF-8 byte order mark (U+FEFF). */
    if (length >= 3 && (unsigned char)data[0] == 0xEF &&
        (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) {
        set_error(parser, VIBE_ERROR_ENCODING,
                  "Byte order mark (U+FEFF) is not permitted; encode documents as plain UTF-8");
        return NULL;
    }

    VibeValue* root = vibe_value_new_object();
    if (!root) {
        set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    size_t nframes = parser->limits.max_depth + 2;
    StateFrame* frames = (StateFrame*)vibe__calloc(nframes, sizeof(StateFrame));
    if (!frames) {
        set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory");
        vibe_value_free(root);
        return NULL;
    }
    int depth = 0;
    frames[0].state = STATE_ROOT;
    frames[0].container = root;

    char* current_key = NULL;

#define VIBE_FAIL()                       \
    do {                                  \
        vibe__free(current_key);          \
        vibe__free(frames);               \
        vibe_value_free(root);            \
        return NULL;                      \
    } while (0)

    for (;;) {
        Token token = next_token(parser);

        if (token.type == TOKEN_ERROR || parser->error.has_error) {
            token_free(&token);
            VIBE_FAIL();
        }

        if (token.type == TOKEN_EOF) {
            token_free(&token);
            if (depth > 0) {
                StateFrame* open = &frames[depth];
                if (open->state == STATE_ARRAY)
                    set_error(parser, VIBE_ERROR_UNCLOSED_ARRAY,
                              "Unexpected end of input: unclosed array (missing ']')");
                else
                    set_error(parser, VIBE_ERROR_UNCLOSED_OBJECT,
                              "Unexpected end of input: unclosed object (missing '}')");
                VIBE_FAIL();
            }
            break;
        }

        if (token.type == TOKEN_NEWLINE) { token_free(&token); continue; }

        StateFrame* frame = &frames[depth];

        if (frame->state == STATE_ROOT || frame->state == STATE_OBJECT) {
            /* Keys may be bare identifiers or quoted strings. */
            if (token.type == TOKEN_IDENTIFIER || token.type == TOKEN_STRING) {
                current_key = vibe__strdup(token.value);
                token_free(&token);
                if (!current_key) { set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory"); VIBE_FAIL(); }

                if (current_key[0] == '\0') {
                    set_error(parser, VIBE_ERROR_UNEXPECTED_TOKEN, "Empty key is not allowed");
                    VIBE_FAIL();
                }
                if (strlen(current_key) > parser->limits.max_key_length) {
                    set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                              "Key exceeds the maximum length of %zu bytes",
                              parser->limits.max_key_length);
                    VIBE_FAIL();
                }

                VibeObject* obj = frame->container->as_object;
                if (obj->count >= parser->limits.max_object_keys &&
                    !vibe_object_get(obj, current_key)) {
                    set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                              "Object exceeds the maximum of %zu keys",
                              parser->limits.max_object_keys);
                    VIBE_FAIL();
                }

                Token next = next_token(parser);
                if (parser->error.has_error) { token_free(&next); VIBE_FAIL(); }

                if (next.type == TOKEN_LEFT_BRACE) {
                    token_free(&next);
                    VibeValue* child = vibe_value_new_object();
                    if (!child) { set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory"); VIBE_FAIL(); }
                    vibe_object_set(obj, current_key, child);
                    depth++;
                    if ((size_t)depth > parser->limits.max_depth) {
                        set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                                  "Nesting depth exceeds the maximum of %zu levels",
                                  parser->limits.max_depth);
                        VIBE_FAIL();
                    }
                    frames[depth].state = STATE_OBJECT;
                    frames[depth].container = child;
                } else if (next.type == TOKEN_LEFT_BRACKET) {
                    token_free(&next);
                    VibeValue* child = vibe_value_new_array();
                    if (!child) { set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory"); VIBE_FAIL(); }
                    vibe_object_set(obj, current_key, child);
                    depth++;
                    if ((size_t)depth > parser->limits.max_depth) {
                        set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                                  "Nesting depth exceeds the maximum of %zu levels",
                                  parser->limits.max_depth);
                        VIBE_FAIL();
                    }
                    frames[depth].state = STATE_ARRAY;
                    frames[depth].container = child;
                } else {
                    VibeValue* val = parse_value_from_token(&next);
                    if (!val) {
                        set_error(parser, VIBE_ERROR_INVALID_NUMBER,
                                  "Invalid value for key '%s' (number out of range or malformed token)",
                                  current_key);
                        token_free(&next);
                        VIBE_FAIL();
                    }
                    vibe_object_set(obj, current_key, val);
                    token_free(&next);
                }
                vibe__free(current_key);
                current_key = NULL;
            } else if (token.type == TOKEN_RIGHT_BRACE) {
                token_free(&token);
                if (depth > 0) {
                    depth--;
                } else {
                    set_error(parser, VIBE_ERROR_UNEXPECTED_TOKEN,
                              "Unexpected '}' with no matching '{'");
                    VIBE_FAIL();
                }
            } else {
                set_error(parser, VIBE_ERROR_UNEXPECTED_TOKEN,
                          "Unexpected token where a key was expected");
                token_free(&token);
                VIBE_FAIL();
            }
        } else { /* STATE_ARRAY */
            if (token.type == TOKEN_RIGHT_BRACKET) {
                token_free(&token);
                if (depth > 0) depth--;
            } else if (token.type == TOKEN_LEFT_BRACE) {
                set_error(parser, VIBE_ERROR_NESTED_CONTAINER,
                          "Objects cannot be placed inside arrays (the First Law of VIBE)");
                token_free(&token);
                VIBE_FAIL();
            } else if (token.type == TOKEN_LEFT_BRACKET) {
                set_error(parser, VIBE_ERROR_NESTED_CONTAINER,
                          "Arrays cannot be nested inside other arrays (the First Law of VIBE)");
                token_free(&token);
                VIBE_FAIL();
            } else {
                VibeArray* arr = frame->container->as_array;
                if (arr->count >= parser->limits.max_array_elements) {
                    set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                              "Array exceeds the maximum of %zu elements",
                              parser->limits.max_array_elements);
                    token_free(&token);
                    VIBE_FAIL();
                }
                VibeValue* val = parse_value_from_token(&token);
                if (!val) {
                    set_error(parser, VIBE_ERROR_INVALID_NUMBER,
                              "Invalid array element (number out of range or malformed token)");
                    token_free(&token);
                    VIBE_FAIL();
                }
                vibe_array_push(arr, val);
                token_free(&token);
            }
        }
    }

#undef VIBE_FAIL
    vibe__free(current_key);
    vibe__free(frames);
    return root;
}

VibeValue* vibe_parse_string(VibeParser* parser, const char* input) {
    if (!parser || !input) return NULL;
    return vibe_parse_buffer(parser, input, strlen(input));
}

VibeValue* vibe_parse_file(VibeParser* parser, const char* filename) {
    if (!parser || !filename) return NULL;

    vibe__free(parser->error.message);
    parser->error.message = NULL;
    parser->error.has_error = false;
    parser->error.code = VIBE_OK;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        set_error(parser, VIBE_ERROR_IO, "Cannot open file '%s'", filename);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(parser, VIBE_ERROR_IO, "Cannot seek in '%s' (not a regular file?)", filename);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        set_error(parser, VIBE_ERROR_IO, "Cannot determine size of '%s'", filename);
        return NULL;
    }
    if ((size_t)size > parser->limits.max_document_size) {
        fclose(file);
        set_error(parser, VIBE_ERROR_LIMIT_EXCEEDED,
                  "File '%s' exceeds the maximum size of %zu bytes",
                  filename, parser->limits.max_document_size);
        return NULL;
    }
    rewind(file);

    char* buffer = (char*)vibe__malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        set_error(parser, VIBE_ERROR_OUT_OF_MEMORY, "Out of memory reading '%s'", filename);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, file);
    int read_ok = (bytes_read == (size_t)size) || feof(file);
    fclose(file);
    if (!read_ok) {
        vibe__free(buffer);
        set_error(parser, VIBE_ERROR_IO, "Failed to read '%s' completely", filename);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    VibeValue* result = vibe_parse_buffer(parser, buffer, bytes_read);
    vibe__free(buffer);
    return result;
}

VibeValue* vibe_parse(const char* data, size_t length, VibeError* out_error) {
    VibeParser parser;
    memset(&parser, 0, sizeof(parser));
    parser.line = 1;
    parser.column = 1;
    parser.limits = vibe_default_limits();

    VibeValue* root = vibe_parse_buffer(&parser, data, length);
    if (!root && out_error) {
        *out_error = parser.error;      /* transfer ownership of the message */
        parser.error.message = NULL;
    } else {
        vibe__free(parser.error.message);
    }
    return root;
}

/* ============================================================================
 * Path access
 * ============================================================================ */

VibeValue* vibe_get(VibeValue* root, const char* path) {
    if (!root || !path) return NULL;

    VibeValue* current = root;
    char* copy = vibe__strdup(path);
    if (!copy) return NULL;
    char* saveptr = NULL;
    char* tok = strtok_r(copy, ".", &saveptr);
    while (tok && current) {
        if (current->type == VIBE_TYPE_OBJECT)
            current = vibe_object_get(current->as_object, tok);
        else {
            vibe__free(copy);
            return NULL;
        }
        tok = strtok_r(NULL, ".", &saveptr);
    }
    vibe__free(copy);
    return current;
}

const char* vibe_get_string(VibeValue* value, const char* path) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_STRING) ? v->as_string : NULL;
}
int64_t vibe_get_int(VibeValue* value, const char* path) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_INTEGER) ? v->as_integer : 0;
}
double vibe_get_float(VibeValue* value, const char* path) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_FLOAT) ? v->as_float : 0.0;
}
bool vibe_get_bool(VibeValue* value, const char* path) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_BOOLEAN) ? v->as_boolean : false;
}
VibeArray* vibe_get_array(VibeValue* value, const char* path) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_ARRAY) ? v->as_array : NULL;
}
VibeObject* vibe_get_object(VibeValue* value, const char* path) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_OBJECT) ? v->as_object : NULL;
}

const char* vibe_get_string_or(VibeValue* value, const char* path, const char* fallback) {
    const char* s = vibe_get_string(value, path);
    return s ? s : fallback;
}
int64_t vibe_get_int_or(VibeValue* value, const char* path, int64_t fallback) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_INTEGER) ? v->as_integer : fallback;
}
double vibe_get_float_or(VibeValue* value, const char* path, double fallback) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_FLOAT) ? v->as_float : fallback;
}
bool vibe_get_bool_or(VibeValue* value, const char* path, bool fallback) {
    VibeValue* v = path ? vibe_get(value, path) : value;
    return (v && v->type == VIBE_TYPE_BOOLEAN) ? v->as_boolean : fallback;
}

/* ============================================================================
 * Emitter — serialise a value tree back to canonical VIBE text
 * ============================================================================ */

typedef struct {
    char* data;
    size_t len;
    size_t cap;
    bool oom;
} StrBuf;

static void sb_reserve(StrBuf* sb, size_t extra) {
    if (sb->oom) return;
    if (sb->len + extra + 1 > sb->cap) {
        size_t ncap = sb->cap ? sb->cap : 128;
        while (sb->len + extra + 1 > ncap) ncap *= 2;
        char* nd = (char*)vibe__realloc(sb->data, ncap);
        if (!nd) { sb->oom = true; return; }
        sb->data = nd;
        sb->cap = ncap;
    }
}
static void sb_putc(StrBuf* sb, char c) {
    sb_reserve(sb, 1);
    if (sb->oom) return;
    sb->data[sb->len++] = c;
}
static void sb_putn(StrBuf* sb, const char* s, size_t n) {
    sb_reserve(sb, n);
    if (sb->oom) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
}
static void sb_puts(StrBuf* sb, const char* s) { sb_putn(sb, s, strlen(s)); }
static void sb_indent(StrBuf* sb, int n) { for (int i = 0; i < n; i++) sb_putn(sb, "  ", 2); }

static void sb_put_quoted(StrBuf* sb, const char* s) {
    sb_putc(sb, '"');
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '"':  sb_putn(sb, "\\\"", 2); break;
            case '\\': sb_putn(sb, "\\\\", 2); break;
            case '\n': sb_putn(sb, "\\n", 2);  break;
            case '\t': sb_putn(sb, "\\t", 2);  break;
            case '\r': sb_putn(sb, "\\r", 2);  break;
            default:   sb_putc(sb, *p);        break;
        }
    }
    sb_putc(sb, '"');
}

/* A string round-trips bare only if it re-lexes to the same STRING token. */
static bool string_is_bare_safe(const char* s) {
    if (!s || s[0] == '\0') return false;
    if (!is_unquoted_start_char((unsigned char)s[0])) return false;
    for (const char* p = s; *p; p++) {
        if (!is_unquoted_string_char((unsigned char)*p)) return false;
        if (*p == '"') return false;
    }
    if (is_valid_number(s)) return false;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0) return false;
    return true;
}

static bool key_is_bare_safe(const char* s) {
    if (!s || s[0] == '\0') return false;
    if (!is_identifier_start((unsigned char)s[0])) return false;
    for (const char* p = s; *p; p++)
        if (!is_identifier_char((unsigned char)*p)) return false;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0) return false;
    return true;
}

static void sb_put_double(StrBuf* sb, double d) {
    char buf[512];
    if (!isfinite(d)) {
        /* VIBE has no inf/nan literal; emit a valid (string) token. */
        sb_put_quoted(sb, isnan(d) ? "nan" : (d < 0 ? "-inf" : "inf"));
        return;
    }
    int dec;
    for (dec = 0; dec <= 17; dec++) {
        snprintf(buf, sizeof(buf), "%.*f", dec, d);
        if (strtod(buf, NULL) == d) break;
    }
    if (dec > 17) snprintf(buf, sizeof(buf), "%.17g", d); /* extreme magnitude */
    sb_puts(sb, buf);
    if (!strpbrk(buf, ".eE")) sb_puts(sb, ".0"); /* keep it lexing as a float */
}

static void emit_value(StrBuf* sb, const VibeValue* v, int indent);

static void emit_scalar(StrBuf* sb, const VibeValue* v) {
    char numbuf[32];
    switch (v->type) {
        case VIBE_TYPE_NULL:    sb_put_quoted(sb, ""); break;
        case VIBE_TYPE_BOOLEAN: sb_puts(sb, v->as_boolean ? "true" : "false"); break;
        case VIBE_TYPE_INTEGER:
            snprintf(numbuf, sizeof(numbuf), "%lld", (long long)v->as_integer);
            sb_puts(sb, numbuf);
            break;
        case VIBE_TYPE_FLOAT: sb_put_double(sb, v->as_float); break;
        case VIBE_TYPE_STRING:
            if (string_is_bare_safe(v->as_string)) sb_puts(sb, v->as_string);
            else sb_put_quoted(sb, v->as_string);
            break;
        default: break;
    }
}

static void emit_key(StrBuf* sb, const char* key) {
    if (key_is_bare_safe(key)) sb_puts(sb, key);
    else sb_put_quoted(sb, key);
}

static void emit_value(StrBuf* sb, const VibeValue* v, int indent) {
    if (v->type == VIBE_TYPE_OBJECT) {
        sb_puts(sb, "{\n");
        for (size_t i = 0; i < v->as_object->count; i++) {
            sb_indent(sb, indent + 1);
            emit_key(sb, v->as_object->entries[i].key);
            sb_putc(sb, ' ');
            emit_value(sb, v->as_object->entries[i].value, indent + 1);
            sb_putc(sb, '\n');
        }
        sb_indent(sb, indent);
        sb_putc(sb, '}');
    } else if (v->type == VIBE_TYPE_ARRAY) {
        if (v->as_array->count == 0) { sb_puts(sb, "[]"); return; }
        sb_puts(sb, "[\n");
        for (size_t i = 0; i < v->as_array->count; i++) {
            sb_indent(sb, indent + 1);
            emit_value(sb, v->as_array->values[i], indent + 1);
            sb_putc(sb, '\n');
        }
        sb_indent(sb, indent);
        sb_putc(sb, ']');
    } else {
        emit_scalar(sb, v);
    }
}

char* vibe_emit(const VibeValue* value) {
    if (!value) return NULL;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));

    if (value->type == VIBE_TYPE_OBJECT) {
        /* Root object: emit entries at the document level, no wrapping braces. */
        for (size_t i = 0; i < value->as_object->count; i++) {
            emit_key(&sb, value->as_object->entries[i].key);
            sb_putc(&sb, ' ');
            emit_value(&sb, value->as_object->entries[i].value, 0);
            sb_putc(&sb, '\n');
        }
    } else {
        emit_value(&sb, value, 0);
        sb_putc(&sb, '\n');
    }

    if (sb.oom) { vibe__free(sb.data); return NULL; }
    if (!sb.data) { /* empty document */
        sb.data = (char*)vibe__malloc(1);
        if (!sb.data) return NULL;
        sb.len = 0;
    }
    sb.data[sb.len] = '\0';
    return sb.data;
}

bool vibe_emit_file(const VibeValue* value, const char* path) {
    if (!value || !path) return false;
    char* text = vibe_emit(value);
    if (!text) return false;
    FILE* f = fopen(path, "wb");
    if (!f) { vibe__free(text); return false; }
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, f);
    int ok = (w == n);
    if (fclose(f) != 0) ok = 0;
    vibe__free(text);
    return ok != 0;
}

/* ============================================================================
 * Debug printer (not canonical VIBE)
 * ============================================================================ */

void vibe_value_print(VibeValue* value, int indent) {
    if (!value) return;
    const char* istr = "  ";
    switch (value->type) {
        case VIBE_TYPE_INTEGER: printf("%lld", (long long)value->as_integer); break;
        case VIBE_TYPE_FLOAT:   printf("%g", value->as_float); break;
        case VIBE_TYPE_BOOLEAN: printf("%s", value->as_boolean ? "true" : "false"); break;
        case VIBE_TYPE_STRING:  printf("\"%s\"", value->as_string); break;
        case VIBE_TYPE_ARRAY:
            printf("[\n");
            for (size_t i = 0; i < value->as_array->count; i++) {
                for (int j = 0; j < indent + 1; j++) printf("%s", istr);
                vibe_value_print(value->as_array->values[i], indent + 1);
                printf("\n");
            }
            for (int j = 0; j < indent; j++) printf("%s", istr);
            printf("]");
            break;
        case VIBE_TYPE_OBJECT:
            printf("{\n");
            for (size_t i = 0; i < value->as_object->count; i++) {
                for (int j = 0; j < indent + 1; j++) printf("%s", istr);
                printf("%s: ", value->as_object->entries[i].key);
                vibe_value_print(value->as_object->entries[i].value, indent + 1);
                printf("\n");
            }
            for (int j = 0; j < indent; j++) printf("%s", istr);
            printf("}");
            break;
        default:
            printf("null");
            break;
    }
}
