/*
 * vibe — Ruby C-extension (a TRUE native extension, not fiddle FFI).
 *
 * Built with extconf.rb -> Makefile -> `make`, producing vibe.so which
 * Ruby loads with `require "vibe"`. It #includes the real vibe.h and links
 * libvibe.a, so the Ruby <-> C boundary is compiled native code.
 *
 * Ruby surface:
 *   Vibe.version                     -> String
 *   Vibe.parse(str)                  -> Vibe::Doc      (raises Vibe::Error)
 *   doc.get_string(path)             -> String | nil
 *   doc.get_int(path)                -> Integer
 *   doc.get_float(path)              -> Float
 *   doc.get_bool(path)               -> true/false
 *   doc.array_size(path)             -> Integer
 *   doc.emit                         -> String
 */
#include <ruby.h>
#include <string.h>
#include "vibe.h"

static VALUE mVibe;
static VALUE cDoc;
static VALUE eVibeError;

/* ---- Doc wraps VibeValue* with GC-managed free ------------------------- */
static void doc_free(void *p)
{
    if (p) vibe_value_free((VibeValue *)p);
}

static const rb_data_type_t doc_type = {
    "Vibe::Doc",
    { NULL, doc_free, NULL, },
    NULL, NULL,
    RUBY_TYPED_FREE_IMMEDIATELY,
};

static VibeValue *doc_ptr(VALUE self)
{
    VibeValue *v;
    TypedData_Get_Struct(self, VibeValue, &doc_type, v);
    return v;
}

/* ---- module functions -------------------------------------------------- */
static VALUE vibe_m_version(VALUE self)
{
    return rb_str_new_cstr(vibe_version());
}

static VALUE vibe_m_parse(VALUE self, VALUE rstr)
{
    Check_Type(rstr, T_STRING);
    VibeError err;
    VibeValue *root = vibe_parse(RSTRING_PTR(rstr), (size_t)RSTRING_LEN(rstr), &err);
    if (!root) {
        rb_raise(eVibeError, "%s (line %d, col %d): %s",
                 vibe_error_code_string(err.code), err.line, err.column,
                 err.message ? err.message : "parse error");
    }
    return TypedData_Wrap_Struct(cDoc, &doc_type, root);
}

/* ---- Doc methods ------------------------------------------------------- */
static VALUE doc_get_string(VALUE self, VALUE path)
{
    const char *s = vibe_get_string(doc_ptr(self), StringValueCStr(path));
    return s ? rb_str_new_cstr(s) : Qnil;
}

static VALUE doc_get_int(VALUE self, VALUE path)
{
    return LL2NUM(vibe_get_int(doc_ptr(self), StringValueCStr(path)));
}

static VALUE doc_get_float(VALUE self, VALUE path)
{
    return DBL2NUM(vibe_get_float(doc_ptr(self), StringValueCStr(path)));
}

static VALUE doc_get_bool(VALUE self, VALUE path)
{
    return vibe_get_bool(doc_ptr(self), StringValueCStr(path)) ? Qtrue : Qfalse;
}

static VALUE doc_array_size(VALUE self, VALUE path)
{
    VibeArray *arr = vibe_get_array(doc_ptr(self), StringValueCStr(path));
    return SIZET2NUM(arr ? vibe_array_size(arr) : 0);
}

static VALUE doc_emit(VALUE self)
{
    char *s = vibe_emit(doc_ptr(self));
    if (!s) rb_raise(eVibeError, "vibe_emit failed");
    VALUE out = rb_str_new_cstr(s);
    vibe_free(s);
    return out;
}

void Init_vibe(void)
{
    mVibe = rb_define_module("Vibe");
    eVibeError = rb_define_class_under(mVibe, "Error", rb_eStandardError);

    rb_define_module_function(mVibe, "version", vibe_m_version, 0);
    rb_define_module_function(mVibe, "parse", vibe_m_parse, 1);

    cDoc = rb_define_class_under(mVibe, "Doc", rb_cObject);
    rb_undef_alloc_func(cDoc);
    rb_define_method(cDoc, "get_string", doc_get_string, 1);
    rb_define_method(cDoc, "get_int", doc_get_int, 1);
    rb_define_method(cDoc, "get_float", doc_get_float, 1);
    rb_define_method(cDoc, "get_bool", doc_get_bool, 1);
    rb_define_method(cDoc, "array_size", doc_array_size, 1);
    rb_define_method(cDoc, "emit", doc_emit, 0);
}
