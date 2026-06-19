/*
 * wfm_reader_ext.c — handle extension: typed `Reader` over `wfm_reader` (jm; gh-306).
 *
 * `Reader` wraps an opaque wfm_reader_t *; the resource logic
 * lives hand-written in the backing _core.c. This file is pure generated glue —
 * lifecycle, arg coercion, numpy marshaling, decoded-getter properties, RAII.
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "wfm/wfm_reader.h"

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index(const char *const *tab, const char *s)
{
    for (int i = 0; tab[i]; i++)
        if (strcmp(tab[i], s) == 0)
            return i;
    return -1;
}

static const char *const _enum_stype[] = {
    "cf32",
    "cf64",
    "ci32",
    "ci16",
    "ci8",
    NULL,
};

static const char *const _enum_endian[] = {
    "le",
    "be",
    NULL,
};

static const char *const _enum_ftype[] = {
    "raw",
    "csv",
    "blue",
    "sigmf",
    NULL,
};

typedef struct {
    PyObject_HEAD
    wfm_reader_t *h;
    int       closed;
    wfm_reader_info_t _g0;
} ReaderObject;

static int
Reader_init(ReaderObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"path", "sample_type", "endian", NULL};
    PyObject *path = NULL;  /* fspath -> bytes */
    const char *sample_type = "cf32";
    const char *endian = "le";
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|ss", kwlist,
            PyUnicode_FSConverter, &path, &sample_type, &endian)) {
    Py_XDECREF(path);
        return -1;
    }
    int _arg_sample_type = _enum_index(_enum_stype, sample_type);
    if (_arg_sample_type < 0) {
        PyErr_Format(PyExc_ValueError, "invalid sample_type '%s'", sample_type);
        Py_XDECREF(path);
        return -1;
    }
    int _arg_endian = _enum_index(_enum_endian, endian);
    if (_arg_endian < 0) {
        PyErr_Format(PyExc_ValueError, "invalid endian '%s'", endian);
        Py_XDECREF(path);
        return -1;
    }
    if (!self->closed && self->h) {
        wfm_reader_close(self->h);
        self->h = NULL;
        self->closed = 1;
    }
    self->h = wfm_reader_open(PyBytes_AS_STRING(path), _arg_sample_type, _arg_endian);
    Py_XDECREF(path);
    if (!self->h) {
        PyErr_SetString(PyExc_RuntimeError, "wfm_reader_open failed");
        return -1;
    }
    self->closed = 0;


    wfm_reader_info(self->h, &self->_g0);
    return 0;
}

static PyObject *
Reader_read(ReaderObject *self, PyObject *args)
{
    Py_ssize_t n;
    if (!PyArg_ParseTuple(args, "n", &n)) return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Reader is closed");
        return NULL;
    }
    if (n < 0) {
        PyErr_SetString(PyExc_ValueError, "count must be >= 0");
        return NULL;
    }
    npy_intp dims[] = {n};
    /* Independent numpy-owned array — never a view into a grow-on-demand
     * buffer (gh-219): the returned array owns its data outright. */
    PyObject *arr = PyArray_SimpleNew(1, dims, NPY_COMPLEX64);
    if (!arr) return NULL;
    float _Complex *out = (float _Complex *)PyArray_DATA((PyArrayObject *)arr);
    size_t got;
    Py_BEGIN_ALLOW_THREADS
    got = wfm_reader_read(self->h, out, (size_t)n);
    Py_END_ALLOW_THREADS
    PyArray_DIMS((PyArrayObject *)arr)[0] = (npy_intp)got; /* trim */
    return arr;
}

static PyObject *
Reader_get_file_type(ReaderObject *self, void *closure)
{
    (void)closure;
    wfm_reader_info_t tmp = self->_g0;
    return PyUnicode_FromString(_enum_ftype[tmp.file_type]);
}

static PyObject *
Reader_get_sample_type(ReaderObject *self, void *closure)
{
    (void)closure;
    wfm_reader_info_t tmp = self->_g0;
    return PyUnicode_FromString(_enum_stype[tmp.sample_type]);
}

static PyObject *
Reader_get_endian(ReaderObject *self, void *closure)
{
    (void)closure;
    wfm_reader_info_t tmp = self->_g0;
    return PyUnicode_FromString(_enum_endian[tmp.endian]);
}

static PyObject *
Reader_get_fs(ReaderObject *self, void *closure)
{
    (void)closure;
    wfm_reader_info_t tmp = self->_g0;
    return PyFloat_FromDouble(tmp.fs);
}

static PyObject *
Reader_get_fc(ReaderObject *self, void *closure)
{
    (void)closure;
    wfm_reader_info_t tmp = self->_g0;
    return PyFloat_FromDouble(tmp.fc);
}

static PyObject *
Reader_get_num_samples(ReaderObject *self, void *closure)
{
    (void)closure;
    wfm_reader_info_t tmp = self->_g0;
    return PyLong_FromUnsignedLongLong((unsigned long long)tmp.num_samples);
}

static PyGetSetDef Reader_getset[] = {
    {"file_type", (getter)Reader_get_file_type, NULL, NULL, NULL},
    {"sample_type", (getter)Reader_get_sample_type, NULL, NULL, NULL},
    {"endian", (getter)Reader_get_endian, NULL, NULL, NULL},
    {"fs", (getter)Reader_get_fs, NULL, NULL, NULL},
    {"fc", (getter)Reader_get_fc, NULL, NULL, NULL},
    {"num_samples", (getter)Reader_get_num_samples, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *
Reader_close(ReaderObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->closed && self->h) {
        wfm_reader_close(self->h);
        self->closed = 1;
    }
    Py_RETURN_NONE;
}

static PyObject *
Reader_enter(ReaderObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Reader_exit(ReaderObject *self, PyObject *args)
{
    (void)args;
    return Reader_close(self, NULL);
}
static void
Reader_dealloc(ReaderObject *self)
{
    if (!self->closed && self->h) {
        wfm_reader_close(self->h);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef Reader_methods[] = {
    {"read", (PyCFunction)Reader_read, METH_VARARGS, NULL},
    {"close", (PyCFunction)Reader_close, METH_NOARGS, "close() -> None"},
    {"__enter__", (PyCFunction)Reader_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)Reader_exit, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject ReaderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "doppler.wfm.Reader",
    .tp_basicsize = sizeof(ReaderObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc)Reader_init,
    .tp_dealloc   = (destructor)Reader_dealloc,
    .tp_getset    = Reader_getset,
    .tp_methods   = Reader_methods,
    .tp_doc       = PyDoc_STR("Reader — handle over `wfm_reader`."),
};

static struct PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT, "wfm_reader", NULL, -1, NULL,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_wfm_reader(void)
{
    import_array();
    if (PyType_Ready(&ReaderType) < 0) return NULL;
    PyObject *m = PyModule_Create(&_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ReaderType);
    if (PyModule_AddObject(m, "Reader", (PyObject *)&ReaderType) < 0) {
        Py_DECREF(&ReaderType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
