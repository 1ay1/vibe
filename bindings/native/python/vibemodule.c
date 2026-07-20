/*
 * vibe — CPython C-API extension module (a TRUE native extension, not FFI).
 *
 * Builds to vibe.cpython-*.so and is imported with `import vibe`.
 * It #includes the real vibe.h and links libvibe.a, so the Python <-> C
 * boundary is compiled native code, not a runtime dlopen/ctypes marshal.
 *
 * Exposes:
 *   vibe.version()                 -> str
 *   vibe.parse(bytes|str)          -> Doc            (raises vibe.VibeError)
 *   Doc.get_string(path)           -> str | None
 *   Doc.get_int(path)              -> int
 *   Doc.get_float(path)            -> float
 *   Doc.get_bool(path)             -> bool
 *   Doc.array_size(path)           -> int
 *   Doc.emit()                     -> str
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "vibe.h"

static PyObject *VibeErr; /* the Python exception object */

/* ---- Doc object: owns a VibeValue* ------------------------------------- */
typedef struct {
    PyObject_HEAD
    VibeValue *root;
} DocObject;

static void
Doc_dealloc(DocObject *self)
{
    if (self->root)
        vibe_value_free(self->root);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Doc_get_string(DocObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;
    const char *s = vibe_get_string(self->root, path);
    if (!s)
        Py_RETURN_NONE;
    return PyUnicode_FromString(s);
}

static PyObject *
Doc_get_int(DocObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;
    return PyLong_FromLongLong((long long)vibe_get_int(self->root, path));
}

static PyObject *
Doc_get_float(DocObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;
    return PyFloat_FromDouble(vibe_get_float(self->root, path));
}

static PyObject *
Doc_get_bool(DocObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;
    if (vibe_get_bool(self->root, path))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *
Doc_array_size(DocObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;
    VibeArray *arr = vibe_get_array(self->root, path);
    if (!arr)
        return PyLong_FromLong(0);
    return PyLong_FromSize_t(vibe_array_size(arr));
}

static PyObject *
Doc_emit(DocObject *self, PyObject *Py_UNUSED(ignored))
{
    char *s = vibe_emit(self->root);
    if (!s) {
        PyErr_SetString(PyExc_MemoryError, "vibe_emit failed");
        return NULL;
    }
    PyObject *out = PyUnicode_FromString(s);
    vibe_free(s);
    return out;
}

/* Materialize a VibeValue* into native Python objects (dict/list/scalars). */
static PyObject *
vibe_to_py(VibeValue *v)
{
    if (!v)
        Py_RETURN_NONE;
    switch (vibe_value_type(v)) {
        case VIBE_TYPE_NULL:
            Py_RETURN_NONE;
        case VIBE_TYPE_INTEGER:
            return PyLong_FromLongLong((long long)vibe_value_int(v));
        case VIBE_TYPE_FLOAT:
            return PyFloat_FromDouble(vibe_value_float(v));
        case VIBE_TYPE_BOOLEAN:
            if (vibe_value_bool(v)) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case VIBE_TYPE_STRING:
            return PyUnicode_FromString(vibe_value_string(v));
        case VIBE_TYPE_ARRAY: {
            VibeArray *a = vibe_value_array(v);
            size_t n = a ? vibe_array_size(a) : 0;
            PyObject *list = PyList_New((Py_ssize_t)n);
            if (!list) return NULL;
            for (size_t i = 0; i < n; i++) {
                PyObject *el = vibe_to_py(vibe_array_get(a, i));
                if (!el) { Py_DECREF(list); return NULL; }
                PyList_SET_ITEM(list, (Py_ssize_t)i, el);
            }
            return list;
        }
        case VIBE_TYPE_OBJECT: {
            VibeObject *o = vibe_value_object(v);
            size_t n = o ? vibe_object_size(o) : 0;
            PyObject *d = PyDict_New();
            if (!d) return NULL;
            for (size_t i = 0; i < n; i++) {
                const char *key = vibe_object_key_at(o, i);
                PyObject *val = vibe_to_py(vibe_object_value_at(o, i));
                if (!val) { Py_DECREF(d); return NULL; }
                if (PyDict_SetItemString(d, key ? key : "", val) < 0) {
                    Py_DECREF(val); Py_DECREF(d); return NULL;
                }
                Py_DECREF(val);
            }
            return d;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *
Doc_to_dict(DocObject *self, PyObject *Py_UNUSED(ignored))
{
    return vibe_to_py(self->root);
}

static PyMethodDef Doc_methods[] = {
    {"get_string", (PyCFunction)Doc_get_string, METH_VARARGS, "Get a string by path (None if absent)."},
    {"get_int",    (PyCFunction)Doc_get_int,    METH_VARARGS, "Get an int by path."},
    {"get_float",  (PyCFunction)Doc_get_float,  METH_VARARGS, "Get a float by path."},
    {"get_bool",   (PyCFunction)Doc_get_bool,   METH_VARARGS, "Get a bool by path."},
    {"array_size", (PyCFunction)Doc_array_size, METH_VARARGS, "Number of elements in the array at path."},
    {"to_dict",    (PyCFunction)Doc_to_dict,    METH_NOARGS,  "Materialize the document into native Python dict/list/scalars."},
    {"emit",       (PyCFunction)Doc_emit,       METH_NOARGS,  "Serialize the document back to VIBE text."},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject DocType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_vibe.Doc",
    .tp_doc = "A parsed VIBE document.",
    .tp_basicsize = sizeof(DocObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)Doc_dealloc,
    .tp_methods = Doc_methods,
};

/* ---- module-level functions -------------------------------------------- */
static PyObject *
mod_version(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args))
{
    return PyUnicode_FromString(vibe_version());
}

static PyObject *
mod_parse(PyObject *Py_UNUSED(self), PyObject *args)
{
    const char *data;
    Py_ssize_t len;
    if (!PyArg_ParseTuple(args, "s#", &data, &len))
        return NULL;

    VibeError err;
    VibeValue *root = vibe_parse(data, (size_t)len, &err);
    if (!root) {
        PyErr_Format(VibeErr,
                     "%s (line %d, col %d): %s",
                     vibe_error_code_string(err.code),
                     err.line, err.column,
                     err.message ? err.message : "parse error");
        return NULL;
    }

    DocObject *doc = PyObject_New(DocObject, &DocType);
    if (!doc) {
        vibe_value_free(root);
        return NULL;
    }
    doc->root = root;
    return (PyObject *)doc;
}

static PyMethodDef mod_methods[] = {
    {"version", mod_version, METH_NOARGS,  "Return the libvibe version string."},
    {"parse",   mod_parse,   METH_VARARGS, "Parse VIBE text, returning a Doc (raises vibe.VibeError)."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef vibemodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_vibe",
    .m_doc = "Native CPython C-API bindings for libvibe (low-level core).",
    .m_size = -1,
    .m_methods = mod_methods,
};

PyMODINIT_FUNC
PyInit__vibe(void)
{
    if (PyType_Ready(&DocType) < 0)
        return NULL;

    PyObject *m = PyModule_Create(&vibemodule);
    if (!m)
        return NULL;

    VibeErr = PyErr_NewException("_vibe.VibeError", NULL, NULL);
    Py_XINCREF(VibeErr);
    if (PyModule_AddObject(m, "VibeError", VibeErr) < 0)
        goto fail;

    Py_INCREF(&DocType);
    if (PyModule_AddObject(m, "Doc", (PyObject *)&DocType) < 0) {
        Py_DECREF(&DocType);
        goto fail;
    }
    return m;

fail:
    Py_XDECREF(VibeErr);
    Py_DECREF(m);
    return NULL;
}
