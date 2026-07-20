/*
 * vibe-tool — a suite of cool utilities built on libvibe's public API.
 *
 *   vibe-tool json   <file>              VIBE  -> JSON  (pretty)
 *   vibe-tool json   <file> --compact    VIBE  -> JSON  (minified)
 *   vibe-tool json   <file> --from-json  JSON  -> VIBE
 *   vibe-tool tree   <file>              colorized structural tree
 *   vibe-tool stats  <file>              depth / counts / type histogram
 *   vibe-tool diff   <a> <b>             semantic (order-insensitive) diff
 *   vibe-tool select <file> <pattern>    wildcard path query (a.*.port)
 *
 * Reads from a path or from '-' (stdin). Colors auto-disable when stdout is
 * not a TTY, or with NO_COLOR / --no-color.
 *
 * Exit codes: 0 ok, 1 parse/query error, 2 usage, 3 I/O, 4 diff-found.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L   /* strdup, isatty, fileno under -std=c11 */
#endif

#include "vibe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- color -------------------------------------------------------------- */
static int g_color = 0;
#define C_RESET (g_color ? "\033[0m"  : "")
#define C_KEY   (g_color ? "\033[36m" : "")   /* cyan   */
#define C_STR   (g_color ? "\033[32m" : "")   /* green  */
#define C_NUM   (g_color ? "\033[33m" : "")   /* yellow */
#define C_BOOL  (g_color ? "\033[35m" : "")   /* magenta*/
#define C_DIM   (g_color ? "\033[90m" : "")   /* grey   */
#define C_ADD   (g_color ? "\033[32m" : "")
#define C_DEL   (g_color ? "\033[31m" : "")
#define C_CHG   (g_color ? "\033[33m" : "")

static void detect_color(int argc, char** argv) {
    int want = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "--no-color") == 0) want = 0;
    g_color = want;
}

/* ---- IO ----------------------------------------------------------------- */
/* Read a whole file (or stdin for "-") into a heap buffer. Caller frees. */
static char* slurp(const char* path, size_t* out_len) {
    FILE* f = (strcmp(path, "-") == 0) ? stdin : fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 1 << 16, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { if (f != stdin) fclose(f); return NULL; }
    for (;;) {
        if (len + 4096 > cap) {
            size_t ncap = cap * 2;
            char* nb = (char*)realloc(buf, ncap);
            if (!nb) { free(buf); if (f != stdin) fclose(f); return NULL; }
            buf = nb; cap = ncap;
        }
        size_t n = fread(buf + len, 1, 4096, f);
        len += n;
        if (n < 4096) break;
    }
    if (f != stdin) fclose(f);
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

/* Parse a file (or stdin) into a document, printing a diagnostic on failure. */
static VibeValue* load(const char* path) {
    size_t len = 0;
    char* text = slurp(path, &len);
    if (!text) { fprintf(stderr, "vibe-tool: cannot read '%s'\n", path); return NULL; }
    VibeError err;
    VibeValue* root = vibe_parse(text, len, &err);
    free(text);
    if (!root) {
        fprintf(stderr, "vibe-tool: %s:%d:%d: %s\n",
                path, err.line, err.column, err.message ? err.message : "parse error");
        vibe_error_free(&err);
        return NULL;
    }
    return root;
}

/* Return the i-th positional (non "--flag") argument, or NULL. */
static const char* positional(int argc, char** argv, int which) {
    int seen = 0;
    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') continue;   /* skip --flags */
        if (seen++ == which) return argv[i];
    }
    return NULL;
}

static int usage(FILE* out) {
    fputs(
        "vibe-tool " VIBE_VERSION_STRING " — utilities for VIBE\n\n"
        "usage:\n"
        "  vibe-tool json   <file> [--compact|--from-json]   convert VIBE <-> JSON\n"
        "  vibe-tool tree   <file>                           colorized structure tree\n"
        "  vibe-tool stats  <file>                           document statistics\n"
        "  vibe-tool diff   <a> <b>                          semantic diff\n"
        "  vibe-tool select <file> <pattern>                 wildcard path query\n\n"
        "  <file> may be '-' for stdin.  --no-color disables ANSI color.\n",
        out);
    return out == stderr ? 2 : 0;
}

/* forward decls for subcommands (defined in companion translation via same file) */
int cmd_json(int argc, char** argv);
int cmd_tree(int argc, char** argv);
int cmd_stats(int argc, char** argv);
int cmd_diff(int argc, char** argv);
int cmd_select(int argc, char** argv);

/* ========================================================================== *
 * json — VIBE <-> JSON
 * ========================================================================== */

/* Print a JSON string literal for s (UTF-8 assumed valid; libvibe guarantees
 * it on parse). Escapes the JSON-mandatory set plus control chars. */
static void json_str(const char* s) {
    putchar('"');
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\b': fputs("\\b", stdout); break;
            case '\f': fputs("\\f", stdout); break;
            default:
                if (*p < 0x20) printf("\\u%04x", *p);
                else putchar(*p);
        }
    }
    putchar('"');
}

static void json_indent(int n, int pretty) {
    if (pretty) { putchar('\n'); for (int i = 0; i < n; i++) fputs("  ", stdout); }
}

static void json_emit(const VibeValue* v, int depth, int pretty) {
    switch (vibe_value_type(v)) {
        case VIBE_TYPE_NULL:    fputs("null", stdout); break;
        case VIBE_TYPE_BOOLEAN: fputs(vibe_value_bool(v) ? "true" : "false", stdout); break;
        case VIBE_TYPE_INTEGER: printf("%lld", (long long)vibe_value_int(v)); break;
        case VIBE_TYPE_FLOAT: {
            double d = vibe_value_float(v);
            /* JSON has no NaN/Inf; emit null to stay valid. */
            if (d != d || d > 1e308 || d < -1e308) fputs("null", stdout);
            else printf("%g", d);
            break;
        }
        case VIBE_TYPE_STRING: json_str(vibe_value_string(v)); break;
        case VIBE_TYPE_ARRAY: {
            VibeArray* a = vibe_value_array(v);
            size_t n = vibe_array_size(a);
            if (n == 0) { fputs("[]", stdout); break; }
            putchar('[');
            for (size_t i = 0; i < n; i++) {
                if (i) putchar(',');
                json_indent(depth + 1, pretty);
                json_emit(vibe_array_get(a, i), depth + 1, pretty);
            }
            json_indent(depth, pretty);
            putchar(']');
            break;
        }
        case VIBE_TYPE_OBJECT: {
            VibeObject* o = vibe_value_object(v);
            size_t n = vibe_object_size(o);
            if (n == 0) { fputs("{}", stdout); break; }
            putchar('{');
            for (size_t i = 0; i < n; i++) {
                if (i) putchar(',');
                json_indent(depth + 1, pretty);
                json_str(vibe_object_key_at(o, i));
                fputs(pretty ? ": " : ":", stdout);
                json_emit(vibe_object_value_at(o, i), depth + 1, pretty);
            }
            json_indent(depth, pretty);
            putchar('}');
            break;
        }
    }
}

/* ---- minimal JSON parser (for --from-json), builds a VibeValue tree ------ */
typedef struct { const char* p; const char* end; int err; } JP;

static void jp_ws(JP* j) {
    while (j->p < j->end && (*j->p==' '||*j->p=='\t'||*j->p=='\n'||*j->p=='\r')) j->p++;
}
static VibeValue* jp_value(JP* j);

static char* jp_string_raw(JP* j) {
    /* assumes *p == '"' */
    j->p++;
    size_t cap = 32, len = 0;
    char* b = (char*)malloc(cap);
    if (!b) { j->err = 1; return NULL; }
    while (j->p < j->end && *j->p != '"') {
        char c = *j->p++;
        if (c == '\\' && j->p < j->end) {
            char e = *j->p++;
            switch (e) {
                case 'n': c='\n'; break; case 't': c='\t'; break;
                case 'r': c='\r'; break; case 'b': c='\b'; break;
                case 'f': c='\f'; break; case '/': c='/';  break;
                case '"': c='"';  break; case '\\': c='\\'; break;
                case 'u': {
                    /* Decode \uXXXX to UTF-8 (BMP only; surrogate pairs collapsed). */
                    if (j->end - j->p < 4) { j->err=1; free(b); return NULL; }
                    unsigned cp = 0;
                    for (int k=0;k<4;k++){ char h=*j->p++; cp<<=4;
                        if(h>='0'&&h<='9')cp|=h-'0'; else if(h>='a'&&h<='f')cp|=h-'a'+10;
                        else if(h>='A'&&h<='F')cp|=h-'A'+10; else {j->err=1;free(b);return NULL;} }
                    char u[4]; int ulen;
                    if (cp<0x80){u[0]=cp;ulen=1;}
                    else if(cp<0x800){u[0]=0xC0|(cp>>6);u[1]=0x80|(cp&0x3F);ulen=2;}
                    else {u[0]=0xE0|(cp>>12);u[1]=0x80|((cp>>6)&0x3F);u[2]=0x80|(cp&0x3F);ulen=3;}
                    if (len+ulen+1>cap){cap=(len+ulen+1)*2;char*nb=realloc(b,cap);if(!nb){j->err=1;free(b);return NULL;}b=nb;}
                    memcpy(b+len,u,ulen); len+=ulen; continue;
                }
                default: c = e; break;
            }
        }
        if (len+1>=cap){cap*=2;char*nb=realloc(b,cap);if(!nb){j->err=1;free(b);return NULL;}b=nb;}
        b[len++]=c;
    }
    if (j->p >= j->end) { j->err=1; free(b); return NULL; }
    j->p++; /* closing quote */
    b[len]='\0';
    return b;
}

static VibeValue* jp_value(JP* j) {
    jp_ws(j);
    if (j->p >= j->end) { j->err=1; return NULL; }
    char c = *j->p;
    if (c == '{') {
        j->p++;
        VibeValue* obj = vibe_value_new_object();
        jp_ws(j);
        if (j->p<j->end && *j->p=='}') { j->p++; return obj; }
        for (;;) {
            jp_ws(j);
            if (j->p>=j->end || *j->p!='"') { j->err=1; vibe_value_free(obj); return NULL; }
            char* key = jp_string_raw(j);
            if (j->err) { vibe_value_free(obj); return NULL; }
            jp_ws(j);
            if (j->p>=j->end || *j->p!=':') { j->err=1; free(key); vibe_value_free(obj); return NULL; }
            j->p++;
            VibeValue* val = jp_value(j);
            if (j->err) { free(key); vibe_value_free(obj); return NULL; }
            vibe_object_set(vibe_value_object(obj), key, val);
            free(key);
            jp_ws(j);
            if (j->p<j->end && *j->p==',') { j->p++; continue; }
            if (j->p<j->end && *j->p=='}') { j->p++; break; }
            j->err=1; vibe_value_free(obj); return NULL;
        }
        return obj;
    }
    if (c == '[') {
        j->p++;
        VibeValue* arr = vibe_value_new_array();
        jp_ws(j);
        if (j->p<j->end && *j->p==']') { j->p++; return arr; }
        for (;;) {
            VibeValue* val = jp_value(j);
            if (j->err) { vibe_value_free(arr); return NULL; }
            /* VIBE arrays hold only scalars; a nested array/object is dropped
             * to a string form so the conversion stays lossless-ish. */
            if (vibe_is_object(val) || vibe_is_array(val)) {
                char* s = vibe_emit(val);
                vibe_value_free(val);
                val = vibe_value_new_string(s ? s : "");
                vibe_free(s);
            }
            vibe_array_push(vibe_value_array(arr), val);
            jp_ws(j);
            if (j->p<j->end && *j->p==',') { j->p++; continue; }
            if (j->p<j->end && *j->p==']') { j->p++; break; }
            j->err=1; vibe_value_free(arr); return NULL;
        }
        return arr;
    }
    if (c == '"') {
        char* s = jp_string_raw(j);
        if (j->err) return NULL;
        VibeValue* v = vibe_value_new_string(s);
        free(s);
        return v;
    }
    if (!strncmp(j->p, "true", 4))  { j->p+=4; return vibe_value_new_boolean(1); }
    if (!strncmp(j->p, "false", 5)) { j->p+=5; return vibe_value_new_boolean(0); }
    if (!strncmp(j->p, "null", 4))  { j->p+=4; return vibe_value_new_null(); }
    /* number */
    {
        const char* s = j->p;
        int isf = 0;
        while (j->p<j->end && (*j->p=='-'||*j->p=='+'||*j->p=='.'||*j->p=='e'||*j->p=='E'||(*j->p>='0'&&*j->p<='9'))) {
            if (*j->p=='.'||*j->p=='e'||*j->p=='E') isf=1;
            j->p++;
        }
        if (j->p==s) { j->err=1; return NULL; }
        char tmp[64]; size_t n=(size_t)(j->p-s); if(n>=sizeof tmp)n=sizeof tmp-1;
        memcpy(tmp,s,n); tmp[n]='\0';
        if (isf) return vibe_value_new_float(strtod(tmp,NULL));
        return vibe_value_new_integer((int64_t)strtoll(tmp,NULL,10));
    }
}

int cmd_json(int argc, char** argv) {
    if (argc < 1) return usage(stderr);
    const char* path = positional(argc, argv, 0);
    if (!path) return usage(stderr);
    int compact = 0, from_json = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--compact"))        compact = 1;
        else if (!strcmp(argv[i], "--from-json")) from_json = 1;
    }

    if (from_json) {
        size_t len = 0;
        char* text = slurp(path, &len);
        if (!text) { fprintf(stderr, "vibe-tool: cannot read '%s'\n", path); return 3; }
        JP j = { text, text + len, 0 };
        VibeValue* root = jp_value(&j);
        jp_ws(&j);
        if (j.err || !root) {
            fprintf(stderr, "vibe-tool: invalid JSON\n");
            free(text); if (root) vibe_value_free(root); return 1;
        }
        free(text);
        char* out = vibe_emit(root);
        if (out) { fputs(out, stdout); vibe_free(out); }
        vibe_value_free(root);
        return 0;
    }

    VibeValue* root = load(path);
    if (!root) return 1;
    json_emit(root, 0, !compact);
    putchar('\n');
    vibe_value_free(root);
    return 0;
}

/* ========================================================================== *
 * tree — colorized structural view
 * ========================================================================== */
static void tree_scalar(const VibeValue* v) {
    switch (vibe_value_type(v)) {
        case VIBE_TYPE_STRING:  printf("%s%s%s", C_STR, vibe_value_string(v), C_RESET); break;
        case VIBE_TYPE_INTEGER: printf("%s%lld%s", C_NUM, (long long)vibe_value_int(v), C_RESET); break;
        case VIBE_TYPE_FLOAT:   printf("%s%g%s", C_NUM, vibe_value_float(v), C_RESET); break;
        case VIBE_TYPE_BOOLEAN: printf("%s%s%s", C_BOOL, vibe_value_bool(v) ? "true":"false", C_RESET); break;
        case VIBE_TYPE_NULL:    printf("%snull%s", C_DIM, C_RESET); break;
        default: break;
    }
}

/* prefix carries the accumulated │/space columns; `last` marks the final child. */
static void tree_node(const VibeValue* v, const char* prefix, int last, const char* label) {
    const char* branch = last ? "\u2514\u2500 " : "\u251c\u2500 ";
    if (label) printf("%s%s%s%s%s", prefix, C_DIM, branch, C_RESET, "");

    VibeType t = vibe_value_type(v);
    if (t == VIBE_TYPE_OBJECT || t == VIBE_TYPE_ARRAY) {
        if (label) printf("%s%s%s %s{%zu}%s\n", C_KEY, label, C_RESET, C_DIM,
                          t==VIBE_TYPE_OBJECT ? vibe_object_size(vibe_value_object(v))
                                              : vibe_array_size(vibe_value_array(v)), C_RESET);
        char child_prefix[4096];
        snprintf(child_prefix, sizeof child_prefix, "%s%s", prefix,
                 label ? (last ? "   " : "\u2502  ") : "");
        if (t == VIBE_TYPE_OBJECT) {
            VibeObject* o = vibe_value_object(v);
            size_t n = vibe_object_size(o);
            for (size_t i = 0; i < n; i++)
                tree_node(vibe_object_value_at(o, i), child_prefix, i+1==n, vibe_object_key_at(o, i));
        } else {
            VibeArray* a = vibe_value_array(v);
            size_t n = vibe_array_size(a);
            for (size_t i = 0; i < n; i++) {
                char idx[24]; snprintf(idx, sizeof idx, "[%zu]", i);
                tree_node(vibe_array_get(a, i), child_prefix, i+1==n, idx);
            }
        }
    } else {
        if (label) printf("%s%s%s = ", C_KEY, label, C_RESET);
        tree_scalar(v);
        putchar('\n');
    }
}

int cmd_tree(int argc, char** argv) {
    if (argc < 1) return usage(stderr);
    const char* path = positional(argc, argv, 0);
    if (!path) return usage(stderr);
    VibeValue* root = load(path);
    if (!root) return 1;
    printf("%s%s%s\n", C_DIM, strcmp(path,"-")?path:"(stdin)", C_RESET);
    /* Root is an object: print its children at top level. */
    VibeObject* o = vibe_value_object(root);
    size_t n = o ? vibe_object_size(o) : 0;
    for (size_t i = 0; i < n; i++)
        tree_node(vibe_object_value_at(o, i), "", i+1==n, vibe_object_key_at(o, i));
    vibe_value_free(root);
    return 0;
}

/* ========================================================================== *
 * stats — document statistics
 * ========================================================================== */
typedef struct {
    size_t objects, arrays, strings, ints, floats, bools, nulls;
    size_t keys;          /* total object keys */
    size_t max_depth;     /* deepest nesting */
    size_t leaves;        /* scalar values */
} Stats;

static void stats_walk(const VibeValue* v, size_t depth, Stats* s) {
    if (depth > s->max_depth) s->max_depth = depth;
    switch (vibe_value_type(v)) {
        case VIBE_TYPE_OBJECT: {
            s->objects++;
            VibeObject* o = vibe_value_object(v);
            size_t n = vibe_object_size(o);
            s->keys += n;
            for (size_t i = 0; i < n; i++) stats_walk(vibe_object_value_at(o, i), depth+1, s);
            break;
        }
        case VIBE_TYPE_ARRAY: {
            s->arrays++;
            VibeArray* a = vibe_value_array(v);
            size_t n = vibe_array_size(a);
            for (size_t i = 0; i < n; i++) stats_walk(vibe_array_get(a, i), depth+1, s);
            break;
        }
        case VIBE_TYPE_STRING:  s->strings++; s->leaves++; break;
        case VIBE_TYPE_INTEGER: s->ints++;    s->leaves++; break;
        case VIBE_TYPE_FLOAT:   s->floats++;  s->leaves++; break;
        case VIBE_TYPE_BOOLEAN: s->bools++;   s->leaves++; break;
        case VIBE_TYPE_NULL:    s->nulls++;   s->leaves++; break;
    }
}

static void bar(size_t v, size_t max) {
    int w = max ? (int)((v * 24 + max/2) / max) : 0;
    printf("%s", C_NUM);
    for (int i = 0; i < w; i++) fputs("\u2588", stdout);
    printf("%s", C_RESET);
}

int cmd_stats(int argc, char** argv) {
    if (argc < 1) return usage(stderr);
    const char* path = positional(argc, argv, 0);
    if (!path) return usage(stderr);
    size_t len = 0;
    char* text = slurp(path, &len);
    if (!text) { fprintf(stderr, "vibe-tool: cannot read '%s'\n", path); return 3; }
    VibeError err;
    VibeValue* root = vibe_parse(text, len, &err);
    if (!root) {
        fprintf(stderr, "vibe-tool: %s:%d:%d: %s\n", path, err.line, err.column,
                err.message ? err.message : "parse error");
        vibe_error_free(&err); free(text); return 1;
    }
    Stats s; memset(&s, 0, sizeof s);
    stats_walk(root, 0, &s);

    printf("%s%s%s  %s%zu bytes%s\n", C_KEY, strcmp(path,"-")?path:"(stdin)", C_RESET, C_DIM, len, C_RESET);
    printf("  nesting depth : %zu\n", s.max_depth);
    printf("  total keys    : %zu\n", s.keys);
    printf("  objects       : %zu\n", s.objects);
    printf("  arrays        : %zu\n", s.arrays);
    printf("  scalar values : %zu\n", s.leaves);
    printf("\n  value types:\n");
    struct { const char* name; size_t v; } rows[] = {
        {"string ", s.strings}, {"integer", s.ints}, {"float  ", s.floats},
        {"boolean", s.bools},  {"null   ", s.nulls},
    };
    size_t maxv = 0;
    for (unsigned i = 0; i < sizeof rows/sizeof rows[0]; i++) if (rows[i].v > maxv) maxv = rows[i].v;
    for (unsigned i = 0; i < sizeof rows/sizeof rows[0]; i++) {
        printf("    %s %6zu  ", rows[i].name, rows[i].v);
        bar(rows[i].v, maxv);
        putchar('\n');
    }
    vibe_value_free(root); free(text);
    return 0;
}

/* ========================================================================== *
 * select — wildcard path query (segments split on '.', '*' matches any key)
 * ========================================================================== */
static void sel_print_path(const char* path, const VibeValue* v) {
    printf("%s%s%s = ", C_KEY, path, C_RESET);
    switch (vibe_value_type(v)) {
        case VIBE_TYPE_STRING:  printf("%s%s%s\n", C_STR, vibe_value_string(v), C_RESET); break;
        case VIBE_TYPE_INTEGER: printf("%s%lld%s\n", C_NUM, (long long)vibe_value_int(v), C_RESET); break;
        case VIBE_TYPE_FLOAT:   printf("%s%g%s\n", C_NUM, vibe_value_float(v), C_RESET); break;
        case VIBE_TYPE_BOOLEAN: printf("%s%s%s\n", C_BOOL, vibe_value_bool(v)?"true":"false", C_RESET); break;
        case VIBE_TYPE_NULL:    printf("%snull%s\n", C_DIM, C_RESET); break;
        default: {
            char* s = vibe_emit((VibeValue*)v);
            if (s) { /* one-line summary for containers */
                printf("%s%s%s\n", C_DIM,
                       vibe_is_array(v) ? "[array]" : "[object]", C_RESET);
                vibe_free(s);
            }
        }
    }
}

/* Recursively match remaining pattern segments (segs[si..nseg]) against node v,
 * accumulating the concrete path in `acc`. */
static int sel_match(const VibeValue* v, char** segs, int si, int nseg,
                     char* acc, size_t acclen, int* count) {
    if (si == nseg) { sel_print_path(acc, v); (*count)++; return 1; }
    const char* seg = segs[si];
    int wild = (strcmp(seg, "*") == 0);

    if (vibe_is_object(v)) {
        VibeObject* o = vibe_value_object(v);
        size_t n = vibe_object_size(o);
        for (size_t i = 0; i < n; i++) {
            const char* k = vibe_object_key_at(o, i);
            if (!wild && strcmp(k, seg) != 0) continue;
            char next[4096];
            snprintf(next, sizeof next, "%s%s%s", acc, acclen ? "." : "", k);
            sel_match(vibe_object_value_at(o, i), segs, si+1, nseg, next, strlen(next), count);
        }
    } else if (vibe_is_array(v) && wild) {
        VibeArray* a = vibe_value_array(v);
        size_t n = vibe_array_size(a);
        for (size_t i = 0; i < n; i++) {
            char next[4096];
            snprintf(next, sizeof next, "%s[%zu]", acc, i);
            sel_match(vibe_array_get(a, i), segs, si+1, nseg, next, strlen(next), count);
        }
    }
    return 0;
}

int cmd_select(int argc, char** argv) {
    if (argc < 2) return usage(stderr);
    const char* path = positional(argc, argv, 0);
    const char* pattern = positional(argc, argv, 1);
    if (!path || !pattern) return usage(stderr);
    VibeValue* root = load(path);
    if (!root) return 1;

    /* split pattern on '.' */
    char* pat = strdup(pattern);
    char* segs[64]; int nseg = 0;
    for (char* t = strtok(pat, "."); t && nseg < 64; t = strtok(NULL, ".")) segs[nseg++] = t;

    char acc[4096]; acc[0] = '\0';
    int count = 0;
    sel_match(root, segs, 0, nseg, acc, 0, &count);
    free(pat);
    vibe_value_free(root);
    if (count == 0) { fprintf(stderr, "vibe-tool: no match for '%s'\n", pattern); return 1; }
    return 0;
}

/* ========================================================================== *
 * diff — semantic, order-insensitive comparison of two documents
 * ========================================================================== */
static int diff_node(const VibeValue* a, const VibeValue* b, const char* path, int* changes);

/* Print a value on one line: scalars as-is, arrays/objects as a compact emit. */
static void diff_compact(const VibeValue* v) {
    VibeType t = vibe_value_type(v);
    if (t == VIBE_TYPE_ARRAY || t == VIBE_TYPE_OBJECT) {
        char* s = vibe_emit((VibeValue*)v);
        if (s) {
            /* collapse newlines to spaces for a single-line preview */
            for (char* p = s; *p; p++) if (*p == '\n') *p = ' ';
            printf("%s", s);
            vibe_free(s);
        }
    } else {
        tree_scalar(v);
    }
}

static void diff_line(char sign, const char* color, const char* path, const VibeValue* v) {
    printf("%s%c %s%s", color, sign, path, C_RESET);
    if (v) {
        char* s = vibe_emit((VibeValue*)v);
        /* show a compact one-liner */
        if (s) {
            char* nl = strchr(s, '\n');
            printf("%s = %.*s%s", color, nl ? (int)(nl - s) : (int)strlen(s), s,
                   nl ? " \u2026" : "");
            vibe_free(s);
        }
        printf("%s", C_RESET);
    }
    putchar('\n');
}

static int diff_node(const VibeValue* a, const VibeValue* b, const char* path, int* changes) {
    VibeType ta = vibe_value_type(a), tb = vibe_value_type(b);
    if (ta != tb) {
        diff_line('~', C_CHG, path, NULL);
        printf("    %s- %s%s\n", C_DEL, vibe_type_name(ta), C_RESET);
        printf("    %s+ %s%s\n", C_ADD, vibe_type_name(tb), C_RESET);
        (*changes)++;
        return 1;
    }
    if (ta == VIBE_TYPE_OBJECT) {
        VibeObject* oa = vibe_value_object(a);
        VibeObject* ob = vibe_value_object(b);
        size_t na = vibe_object_size(oa);
        /* keys in A: changed or removed */
        for (size_t i = 0; i < na; i++) {
            const char* k = vibe_object_key_at(oa, i);
            char sub[4096]; snprintf(sub, sizeof sub, "%s%s%s", path, path[0]?".":"", k);
            VibeValue* bv = vibe_get((VibeValue*)b, k);
            if (!bv) diff_line('-', C_DEL, sub, vibe_object_value_at(oa, i));
            else diff_node(vibe_object_value_at(oa, i), bv, sub, changes);
            if (!bv) (*changes)++;
        }
        /* keys only in B: added */
        size_t nb = vibe_object_size(ob);
        for (size_t i = 0; i < nb; i++) {
            const char* k = vibe_object_key_at(ob, i);
            if (!vibe_get((VibeValue*)a, k)) {
                char sub[4096]; snprintf(sub, sizeof sub, "%s%s%s", path, path[0]?".":"", k);
                diff_line('+', C_ADD, sub, vibe_object_value_at(ob, i));
                (*changes)++;
            }
        }
        return 0;
    }
    if (!vibe_value_equals(a, b)) {
        diff_line('~', C_CHG, path, NULL);
        printf("    %s- ", C_DEL); diff_compact(a); printf("%s\n", C_RESET);
        printf("    %s+ ", C_ADD); diff_compact(b); printf("%s\n", C_RESET);
        (*changes)++;
    }
    return 0;
}

int cmd_diff(int argc, char** argv) {
    if (argc < 2) return usage(stderr);
    const char* pa = positional(argc, argv, 0);
    const char* pb = positional(argc, argv, 1);
    if (!pa || !pb) return usage(stderr);
    VibeValue* a = load(pa); if (!a) return 1;
    VibeValue* b = load(pb); if (!b) { vibe_value_free(a); return 1; }
    int changes = 0;
    diff_node(a, b, "", &changes);
    if (changes == 0) printf("%sdocuments are semantically equal%s\n", C_DIM, C_RESET);
    vibe_value_free(a); vibe_value_free(b);
    return changes ? 4 : 0;
}

int main(int argc, char** argv) {
    detect_color(argc, argv);
    if (argc < 2) return usage(stderr);
    const char* cmd = argv[1];
    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) return usage(stdout);
    if (strcmp(cmd, "version") == 0) {
        printf("vibe-tool %s (libvibe %s, VIBE format %s)\n",
               VIBE_VERSION_STRING, vibe_version(), vibe_format_version());
        return 0;
    }
    if (strcmp(cmd, "json") == 0)   return cmd_json(argc - 2, argv + 2);
    if (strcmp(cmd, "tree") == 0)   return cmd_tree(argc - 2, argv + 2);
    if (strcmp(cmd, "stats") == 0)  return cmd_stats(argc - 2, argv + 2);
    if (strcmp(cmd, "diff") == 0)   return cmd_diff(argc - 2, argv + 2);
    if (strcmp(cmd, "select") == 0) return cmd_select(argc - 2, argv + 2);
    fprintf(stderr, "vibe-tool: unknown command '%s'\n\n", cmd);
    return usage(stderr);
}
