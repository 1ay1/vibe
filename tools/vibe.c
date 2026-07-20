/*
 * vibe — command-line front-end for libvibe.
 *
 *   vibe check <file>          validate; print OK or a structured error
 *   vibe fmt   <file> [-w]     re-emit canonical VIBE (to stdout, or -w in place)
 *   vibe get   <file> <path>   print the scalar at a dotted path
 *   vibe version               print library + format versions
 *
 * Exit codes: 0 success, 1 parse/lookup error, 2 usage error, 3 I/O error.
 *
 * SPDX-License-Identifier: MIT
 */

#include "vibe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(FILE* out) {
    fputs(
        "vibe " VIBE_VERSION_STRING " — the VIBE config tool\n\n"
        "usage:\n"
        "  vibe check <file>           validate a document\n"
        "  vibe fmt   <file> [-w]      reformat to canonical VIBE (-w: rewrite in place)\n"
        "  vibe get   <file> <path>    print the scalar at a dotted path\n"
        "  vibe version               print version information\n",
        out);
    return out == stderr ? 2 : 0;
}

/* Parse `path` with a fresh parser; on failure print a structured diagnostic to
 * stderr and return NULL. Caller frees the parser and the returned value. */
static VibeValue* load(VibeParser* parser, const char* path) {
    VibeValue* root = vibe_parse_file(parser, path);
    if (!root) {
        VibeError e = vibe_get_last_error(parser);
        fprintf(stderr, "%s:%d:%d: error [%s]: %s\n",
                path, e.line, e.column,
                vibe_error_code_string(e.code),
                e.message ? e.message : "unknown error");
    }
    return root;
}

static int cmd_check(const char* file) {
    VibeParser* p = vibe_parser_new();
    if (!p) { fprintf(stderr, "vibe: out of memory\n"); return 3; }
    VibeValue* root = load(p, file);
    int rc = 1;
    if (root) {
        printf("OK: %s (%zu top-level keys)\n", file,
               vibe_object_size(vibe_get_object(root, NULL)));
        vibe_value_free(root);
        rc = 0;
    }
    vibe_parser_free(p);
    return rc;
}

static int cmd_fmt(const char* file, int in_place) {
    VibeParser* p = vibe_parser_new();
    if (!p) { fprintf(stderr, "vibe: out of memory\n"); return 3; }
    VibeValue* root = load(p, file);
    if (!root) { vibe_parser_free(p); return 1; }

    int rc = 0;
    if (in_place) {
        if (!vibe_emit_file(root, file)) {
            fprintf(stderr, "vibe: failed to write '%s'\n", file);
            rc = 3;
        }
    } else {
        char* text = vibe_emit(root);
        if (!text) { fprintf(stderr, "vibe: failed to serialise\n"); rc = 3; }
        else { fputs(text, stdout); vibe_free(text); }
    }
    vibe_value_free(root);
    vibe_parser_free(p);
    return rc;
}

static int cmd_get(const char* file, const char* path) {
    VibeParser* p = vibe_parser_new();
    if (!p) { fprintf(stderr, "vibe: out of memory\n"); return 3; }
    VibeValue* root = load(p, file);
    if (!root) { vibe_parser_free(p); return 1; }

    int rc = 0;
    VibeValue* v = vibe_get(root, path);
    if (!v) {
        fprintf(stderr, "vibe: no value at '%s'\n", path);
        rc = 1;
    } else {
        switch (v->type) {
            case VIBE_TYPE_STRING:  printf("%s\n", v->as_string); break;
            case VIBE_TYPE_INTEGER: printf("%lld\n", (long long)v->as_integer); break;
            case VIBE_TYPE_FLOAT:   printf("%g\n", v->as_float); break;
            case VIBE_TYPE_BOOLEAN: printf("%s\n", v->as_boolean ? "true" : "false"); break;
            default:
                fprintf(stderr, "vibe: '%s' is a %s, not a scalar\n",
                        path, vibe_type_name(v->type));
                rc = 1;
                break;
        }
    }
    vibe_value_free(root);
    vibe_parser_free(p);
    return rc;
}

int main(int argc, char** argv) {
    if (argc < 2) return usage(stderr);

    const char* cmd = argv[1];

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("libvibe %s (VIBE format %s)\n", vibe_version(), vibe_format_version());
        return 0;
    }
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        return usage(stdout);
    }
    if (strcmp(cmd, "check") == 0) {
        if (argc != 3) return usage(stderr);
        return cmd_check(argv[2]);
    }
    if (strcmp(cmd, "fmt") == 0) {
        if (argc < 3 || argc > 4) return usage(stderr);
        int in_place = (argc == 4 && strcmp(argv[3], "-w") == 0);
        if (argc == 4 && !in_place) return usage(stderr);
        return cmd_fmt(argv[2], in_place);
    }
    if (strcmp(cmd, "get") == 0) {
        if (argc != 4) return usage(stderr);
        return cmd_get(argv[2], argv[3]);
    }

    fprintf(stderr, "vibe: unknown command '%s'\n\n", cmd);
    return usage(stderr);
}
