/*
 * VIBE conformance harness.
 *
 * For each fixture under <base>/valid, parse the .vibe with libvibe, re-encode
 * the resulting tree into the tagged-JSON interchange encoding, and compare it
 * (order- and content-sensitive, whitespace-insensitive) against the sibling
 * .json. For each fixture under <base>/invalid, assert the .vibe is rejected
 * with the error category named in the sibling .txt.
 *
 *   run [<base-dir>]        default base: tests/conformance
 *
 * Exit code 0 iff every fixture conforms.
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
#include <dirent.h>

/* ---- tiny dynamic string ------------------------------------------------- */
typedef struct { char* s; size_t n, cap; } Buf;
static void buf_put(Buf* b, const char* p, size_t n) {
    if (b->n + n + 1 > b->cap) {
        b->cap = (b->cap ? b->cap : 128);
        while (b->n + n + 1 > b->cap) b->cap *= 2;
        b->s = (char*)realloc(b->s, b->cap);
    }
    memcpy(b->s + b->n, p, n);
    b->n += n;
    b->s[b->n] = '\0';
}
static void buf_puts(Buf* b, const char* p) { buf_put(b, p, strlen(p)); }
static void buf_putc(Buf* b, char c) { buf_put(b, &c, 1); }

/* Append `s` as a canonical JSON string literal (used for both sides). */
static void buf_json_string(Buf* b, const char* s, size_t len) {
    buf_putc(b, '"');
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  buf_puts(b, "\\\""); break;
            case '\\': buf_puts(b, "\\\\"); break;
            case '\n': buf_puts(b, "\\n");  break;
            case '\t': buf_puts(b, "\\t");  break;
            case '\r': buf_puts(b, "\\r");  break;
            case '\b': buf_puts(b, "\\b");  break;
            case '\f': buf_puts(b, "\\f");  break;
            default:
                if (c < 0x20) { char u[8]; snprintf(u, sizeof u, "\\u%04x", c); buf_puts(b, u); }
                else buf_putc(b, (char)c);
        }
    }
    buf_putc(b, '"');
}

/* ---- read a whole file --------------------------------------------------- */
static char* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char* buf = (char*)malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[r] = '\0';
    if (out_len) *out_len = r;
    return buf;
}

/* ---- canonicalise an arbitrary JSON document (subset: object/array/string)
 *      into a compact, order-preserving form ------------------------------- */
static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}
static const char* canon_json(const char* p, Buf* out, int* ok);

static const char* canon_string(const char* p, Buf* out, int* ok) {
    /* p points at opening quote; decode escapes, re-emit canonically. */
    Buf raw = {0};
    p++; /* opening quote */
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"':  buf_putc(&raw, '"');  break;
                case '\\': buf_putc(&raw, '\\'); break;
                case '/':  buf_putc(&raw, '/');  break;
                case 'b':  buf_putc(&raw, '\b'); break;
                case 'f':  buf_putc(&raw, '\f'); break;
                case 'n':  buf_putc(&raw, '\n'); break;
                case 't':  buf_putc(&raw, '\t'); break;
                case 'r':  buf_putc(&raw, '\r'); break;
                case 'u': {
                    unsigned code = 0;
                    for (int i = 0; i < 4 && p[1]; i++) {
                        char h = *++p; code <<= 4;
                        if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
                    }
                    if (code < 0x80) buf_putc(&raw, (char)code);
                    else if (code < 0x800) {
                        char u[2] = { (char)(0xC0 | (code >> 6)), (char)(0x80 | (code & 0x3F)) };
                        buf_put(&raw, u, 2);
                    } else {
                        char u[3] = { (char)(0xE0 | (code >> 12)),
                                      (char)(0x80 | ((code >> 6) & 0x3F)),
                                      (char)(0x80 | (code & 0x3F)) };
                        buf_put(&raw, u, 3);
                    }
                    break;
                }
                default: buf_putc(&raw, *p); break;
            }
            if (*p) p++;
        } else {
            buf_putc(&raw, *p++);
        }
    }
    if (*p == '"') p++; else *ok = 0;
    buf_json_string(out, raw.s ? raw.s : "", raw.n);
    free(raw.s);
    return p;
}

static const char* canon_json(const char* p, Buf* out, int* ok) {
    p = skip_ws(p);
    if (*p == '{') {
        buf_putc(out, '{');
        p = skip_ws(p + 1);
        int first = 1;
        while (*p && *p != '}') {
            if (!first) buf_putc(out, ',');
            first = 0;
            p = skip_ws(p);
            p = canon_string(p, out, ok);   /* key */
            buf_putc(out, ':');
            p = skip_ws(p);
            if (*p == ':') p = skip_ws(p + 1);
            p = canon_json(p, out, ok);     /* value */
            p = skip_ws(p);
            if (*p == ',') p = skip_ws(p + 1);
        }
        if (*p == '}') p++; else *ok = 0;
        buf_putc(out, '}');
    } else if (*p == '[') {
        buf_putc(out, '[');
        p = skip_ws(p + 1);
        int first = 1;
        while (*p && *p != ']') {
            if (!first) buf_putc(out, ',');
            first = 0;
            p = canon_json(p, out, ok);
            p = skip_ws(p);
            if (*p == ',') p = skip_ws(p + 1);
        }
        if (*p == ']') p++; else *ok = 0;
        buf_putc(out, ']');
    } else if (*p == '"') {
        p = canon_string(p, out, ok);
    } else {
        *ok = 0; /* interchange encoding contains only objects/arrays/strings */
    }
    return p;
}

/* ---- encode a VibeValue tree into the interchange encoding, then canon --- */
static void encode_scalar(Buf* out, const char* type, const char* value, size_t vlen) {
    buf_puts(out, "{\"type\":");
    buf_json_string(out, type, strlen(type));
    buf_puts(out, ",\"value\":");
    buf_json_string(out, value, vlen);
    buf_putc(out, '}');
}

static void encode_value(Buf* out, const VibeValue* v) {
    char num[64];
    switch (v->type) {
        case VIBE_TYPE_OBJECT:
            buf_putc(out, '{');
            for (size_t i = 0; i < v->as_object->count; i++) {
                if (i) buf_putc(out, ',');
                buf_json_string(out, v->as_object->entries[i].key,
                                strlen(v->as_object->entries[i].key));
                buf_putc(out, ':');
                encode_value(out, v->as_object->entries[i].value);
            }
            buf_putc(out, '}');
            break;
        case VIBE_TYPE_ARRAY:
            buf_putc(out, '[');
            for (size_t i = 0; i < v->as_array->count; i++) {
                if (i) buf_putc(out, ',');
                encode_value(out, v->as_array->values[i]);
            }
            buf_putc(out, ']');
            break;
        case VIBE_TYPE_INTEGER:
            snprintf(num, sizeof num, "%lld", (long long)v->as_integer);
            encode_scalar(out, "integer", num, strlen(num));
            break;
        case VIBE_TYPE_FLOAT:
            snprintf(num, sizeof num, "%.17g", v->as_float);
            /* shortest round-trip form */
            for (int prec = 1; prec <= 17; prec++) {
                char t[64];
                snprintf(t, sizeof t, "%.*g", prec, v->as_float);
                if (strtod(t, NULL) == v->as_float) { snprintf(num, sizeof num, "%s", t); break; }
            }
            encode_scalar(out, "float", num, strlen(num));
            break;
        case VIBE_TYPE_BOOLEAN:
            encode_scalar(out, "boolean", v->as_boolean ? "true" : "false",
                          v->as_boolean ? 4 : 5);
            break;
        case VIBE_TYPE_STRING:
            encode_scalar(out, "string", v->as_string, strlen(v->as_string));
            break;
        default:
            encode_scalar(out, "null", "", 0);
            break;
    }
}

/* ---- fixture drivers ----------------------------------------------------- */
static int g_pass = 0, g_fail = 0;

static void check_valid(const char* dir, const char* name) {
    char vibe_path[1024], json_path[1024];
    snprintf(vibe_path, sizeof vibe_path, "%s/valid/%s.vibe", dir, name);
    snprintf(json_path, sizeof json_path, "%s/valid/%s.json", dir, name);

    size_t vlen = 0;
    char* src = read_file(vibe_path, &vlen);
    char* expect_raw = read_file(json_path, NULL);
    if (!src || !expect_raw) {
        printf("  FAIL  valid/%s  (missing fixture)\n", name);
        g_fail++; free(src); free(expect_raw); return;
    }

    VibeError err;
    memset(&err, 0, sizeof err);
    VibeValue* root = vibe_parse(src, vlen, &err);
    if (!root) {
        printf("  FAIL  valid/%s  (rejected: [%s] %s)\n", name,
               vibe_error_code_string(err.code), err.message ? err.message : "");
        vibe_error_free(&err);
        g_fail++; free(src); free(expect_raw); return;
    }

    Buf got = {0}, want = {0};
    encode_value(&got, root);           /* already compact/canonical */
    int ok = 1;
    canon_json(expect_raw, &want, &ok);

    if (ok && got.s && want.s && strcmp(got.s, want.s) == 0) {
        printf("  ok    valid/%s\n", name);
        g_pass++;
    } else {
        printf("  FAIL  valid/%s  (tree mismatch)\n", name);
        printf("        expected: %s\n", want.s ? want.s : "(unparseable)");
        printf("        actual:   %s\n", got.s ? got.s : "");
        g_fail++;
    }
    free(got.s); free(want.s);
    vibe_value_free(root);
    free(src); free(expect_raw);
}

static void check_invalid(const char* dir, const char* name) {
    char vibe_path[1024], txt_path[1024];
    snprintf(vibe_path, sizeof vibe_path, "%s/invalid/%s.vibe", dir, name);
    snprintf(txt_path, sizeof txt_path, "%s/invalid/%s.txt", dir, name);

    size_t vlen = 0;
    char* src = read_file(vibe_path, &vlen);
    char* want = read_file(txt_path, NULL);
    if (!src || !want) {
        printf("  FAIL  invalid/%s  (missing fixture)\n", name);
        g_fail++; free(src); free(want); return;
    }
    /* trim trailing whitespace/newline from expected category */
    size_t wl = strlen(want);
    while (wl && (want[wl-1] == '\n' || want[wl-1] == '\r' || want[wl-1] == ' ' || want[wl-1] == '\t'))
        want[--wl] = '\0';

    VibeError err;
    memset(&err, 0, sizeof err);
    VibeValue* root = vibe_parse(src, vlen, &err);
    if (root) {
        printf("  FAIL  invalid/%s  (accepted, expected [%s])\n", name, want);
        vibe_value_free(root);
        g_fail++;
    } else {
        const char* got = vibe_error_code_string(err.code);
        if (strcmp(got, want) == 0) {
            printf("  ok    invalid/%s  [%s]\n", name, got);
            g_pass++;
        } else {
            printf("  FAIL  invalid/%s  (got [%s], expected [%s]: %s)\n",
                   name, got, want, err.message ? err.message : "");
            g_fail++;
        }
        vibe_error_free(&err);
    }
    free(src); free(want);
}

/* Enumerate *.vibe basenames in <dir>/<sub> and run `fn` on each. */
static void walk(const char* base, const char* sub,
                 void (*fn)(const char*, const char*)) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", base, sub);
    DIR* d = opendir(path);
    if (!d) { fprintf(stderr, "cannot open %s\n", path); g_fail++; return; }
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        const char* dot = strrchr(e->d_name, '.');
        if (!dot || strcmp(dot, ".vibe") != 0) continue;
        char name[512];
        size_t n = (size_t)(dot - e->d_name);
        if (n >= sizeof name) continue;
        memcpy(name, e->d_name, n);
        name[n] = '\0';
        fn(base, name);
    }
    closedir(d);
}

int main(int argc, char** argv) {
    const char* base = (argc > 1) ? argv[1] : "tests/conformance";
    printf("VIBE conformance suite (libvibe %s, format %s)\n",
           vibe_version(), vibe_format_version());
    printf("base: %s\n\nvalid/\n", base);
    walk(base, "valid", check_valid);
    printf("\ninvalid/\n");
    walk(base, "invalid", check_invalid);

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) printf("\xE2\x9C\x93 100%% conformant\n");
    return g_fail == 0 ? 0 : 1;
}
