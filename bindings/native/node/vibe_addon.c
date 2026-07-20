/*
 * vibe — Node.js N-API native addon (a TRUE native addon, not koffi FFI).
 *
 * Compiled to vibe.node and loaded with require('./build/Release/vibe.node').
 * It #includes the real vibe.h and links libvibe.a, so the JS <-> C boundary
 * is compiled machine code, not a runtime FFI marshal.
 *
 * Exports:
 *   version()               -> string
 *   parse(string|Buffer)    -> external handle    (throws on parse error)
 *   getString(h, path)      -> string | null
 *   getInt(h, path)         -> number
 *   getFloat(h, path)       -> number
 *   getBool(h, path)        -> boolean
 *   arraySize(h, path)      -> number
 *   emit(h)                 -> string
 *   free(h)                 -> undefined
 */
#include <node_api.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vibe.h"

#define CHECK(call) do { if ((call) != napi_ok) { napi_throw_error(env, NULL, "napi call failed"); return NULL; } } while (0)

static char *arg_string(napi_env env, napi_value v)
{
    size_t len = 0;
    if (napi_get_value_string_utf8(env, v, NULL, 0, &len) != napi_ok)
        return NULL;
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    napi_get_value_string_utf8(env, v, buf, len + 1, &len);
    return buf;
}

/* External handle -> VibeValue*. We free explicitly via free(h). */
static VibeValue *handle_of(napi_env env, napi_value v)
{
    void *p = NULL;
    if (napi_get_value_external(env, v, &p) != napi_ok)
        return NULL;
    return (VibeValue *)p;
}

static napi_value fn_version(napi_env env, napi_callback_info info)
{
    napi_value out;
    CHECK(napi_create_string_utf8(env, vibe_version(), NAPI_AUTO_LENGTH, &out));
    return out;
}

static napi_value fn_parse(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    char *data = arg_string(env, argv[0]);
    if (!data) { napi_throw_error(env, NULL, "parse: expected a string"); return NULL; }

    VibeError err;
    VibeValue *root = vibe_parse(data, strlen(data), &err);
    free(data);

    if (!root) {
        char msg[512];
        snprintf(msg, sizeof msg, "%s (line %d, col %d): %s",
                 vibe_error_code_string(err.code), err.line, err.column,
                 err.message ? err.message : "parse error");
        napi_throw_error(env, NULL, msg);
        return NULL;
    }

    napi_value ext;
    CHECK(napi_create_external(env, root, NULL, NULL, &ext));
    return ext;
}

static napi_value fn_get_string(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value argv[2];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    char *path = arg_string(env, argv[1]);
    const char *s = vibe_get_string(root, path);
    free(path);
    napi_value out;
    if (!s) { CHECK(napi_get_null(env, &out)); return out; }
    CHECK(napi_create_string_utf8(env, s, NAPI_AUTO_LENGTH, &out));
    return out;
}

static napi_value fn_get_int(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value argv[2];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    char *path = arg_string(env, argv[1]);
    int64_t n = vibe_get_int(root, path);
    free(path);
    napi_value out;
    CHECK(napi_create_int64(env, n, &out));
    return out;
}

static napi_value fn_get_float(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value argv[2];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    char *path = arg_string(env, argv[1]);
    double d = vibe_get_float(root, path);
    free(path);
    napi_value out;
    CHECK(napi_create_double(env, d, &out));
    return out;
}

static napi_value fn_get_bool(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value argv[2];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    char *path = arg_string(env, argv[1]);
    bool b = vibe_get_bool(root, path);
    free(path);
    napi_value out;
    CHECK(napi_get_boolean(env, b, &out));
    return out;
}

static napi_value fn_array_size(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value argv[2];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    char *path = arg_string(env, argv[1]);
    VibeArray *arr = vibe_get_array(root, path);
    size_t n = arr ? vibe_array_size(arr) : 0;
    free(path);
    napi_value out;
    CHECK(napi_create_uint32(env, (uint32_t)n, &out));
    return out;
}

static napi_value fn_emit(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value argv[1];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    char *s = vibe_emit(root);
    napi_value out;
    if (!s) { napi_throw_error(env, NULL, "vibe_emit failed"); return NULL; }
    CHECK(napi_create_string_utf8(env, s, NAPI_AUTO_LENGTH, &out));
    vibe_free(s);
    return out;
}

/* Recursively materialize a VibeValue* into a native JS value so a VIBE
 * literal becomes a plain JS object/array/scalar — no handles, no path calls. */
static napi_value value_to_js(napi_env env, VibeValue *v)
{
    napi_value out;
    if (!v) { napi_get_null(env, &out); return out; }
    switch (vibe_value_type(v)) {
        case VIBE_TYPE_NULL:
            napi_get_null(env, &out);
            return out;
        case VIBE_TYPE_INTEGER:
            napi_create_int64(env, (int64_t)vibe_value_int(v), &out);
            return out;
        case VIBE_TYPE_FLOAT:
            napi_create_double(env, vibe_value_float(v), &out);
            return out;
        case VIBE_TYPE_BOOLEAN:
            napi_get_boolean(env, vibe_value_bool(v), &out);
            return out;
        case VIBE_TYPE_STRING:
            napi_create_string_utf8(env, vibe_value_string(v), NAPI_AUTO_LENGTH, &out);
            return out;
        case VIBE_TYPE_ARRAY: {
            VibeArray *a = v->as_array;
            size_t n = vibe_array_size(a);
            napi_create_array_with_length(env, n, &out);
            for (size_t i = 0; i < n; i++)
                napi_set_element(env, out, (uint32_t)i, value_to_js(env, vibe_array_get(a, i)));
            return out;
        }
        case VIBE_TYPE_OBJECT: {
            VibeObject *o = v->as_object;
            size_t n = vibe_object_size(o);
            napi_create_object(env, &out);
            for (size_t i = 0; i < n; i++) {
                const char *key = vibe_object_key_at(o, i);
                VibeValue *child = vibe_object_value_at(o, i);
                napi_value jv = value_to_js(env, child);
                napi_set_named_property(env, out, key ? key : "", jv);
            }
            return out;
        }
    }
    napi_get_null(env, &out);
    return out;
}

static napi_value fn_to_object(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value argv[1];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    return value_to_js(env, root);
}

static napi_value fn_free(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value argv[1];
    CHECK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    VibeValue *root = handle_of(env, argv[0]);
    if (root) vibe_value_free(root);
    napi_value undef;
    napi_get_undefined(env, &undef);
    return undef;
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor props[] = {
        {"version",   NULL, fn_version,    NULL, NULL, NULL, napi_default, NULL},
        {"parse",     NULL, fn_parse,      NULL, NULL, NULL, napi_default, NULL},
        {"getString", NULL, fn_get_string, NULL, NULL, NULL, napi_default, NULL},
        {"getInt",    NULL, fn_get_int,    NULL, NULL, NULL, napi_default, NULL},
        {"getFloat",  NULL, fn_get_float,  NULL, NULL, NULL, napi_default, NULL},
        {"getBool",   NULL, fn_get_bool,   NULL, NULL, NULL, napi_default, NULL},
        {"arraySize", NULL, fn_array_size, NULL, NULL, NULL, napi_default, NULL},
        {"emit",      NULL, fn_emit,       NULL, NULL, NULL, napi_default, NULL},
        {"toObject",  NULL, fn_to_object,  NULL, NULL, NULL, napi_default, NULL},
        {"free",      NULL, fn_free,       NULL, NULL, NULL, napi_default, NULL},
    };
    CHECK(napi_define_properties(env, exports, sizeof props / sizeof props[0], props));
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
