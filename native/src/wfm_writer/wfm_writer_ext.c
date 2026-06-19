/*
 * wfm_writer_ext.c — handle extension: typed `Writer` over `wfm_writer` (jm; gh-306).
 *
 * `Writer` wraps an opaque wfm_writer_t *; the resource logic
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

#include "wfm/wfm_writer.h"

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index(const char *const *tab, const char *s)
{
    for (int i = 0; tab[i]; i++)
        if (strcmp(tab[i], s) == 0)
            return i;
    return -1;
}

static const char *const _enum_ftype[] = {
    "raw",
    "csv",
    "blue",
    "sigmf",
    NULL,
};

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

typedef struct {
    PyObject_HEAD
    wfm_writer_t *h;
    int       closed;
    int sample_type;
} WriterObject;

static int
Writer_init(WriterObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"path", "file_type", "sample_type", "endian", "fs", "fc", "total", "headroom", NULL};
    PyObject *path = NULL;  /* fspath -> bytes */
    const char *file_type = "raw";
    const char *sample_type = "cf32";
    const char *endian = "le";
    double fs = 1e6;
    double fc = 0.0;
    size_t total = 0;
    double headroom = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|sssddKd", kwlist,
            PyUnicode_FSConverter, &path, &file_type, &sample_type, &endian, &fs, &fc, &total, &headroom)) {
    Py_XDECREF(path);
        return -1;
    }
    int _arg_file_type = _enum_index(_enum_ftype, file_type);
    if (_arg_file_type < 0) {
        PyErr_Format(PyExc_ValueError, "invalid file_type '%s'", file_type);
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
        wfm_writer_close(self->h);
        self->h = NULL;
        self->closed = 1;
    }
    self->h = wfm_writer_open_path(PyBytes_AS_STRING(path), _arg_file_type, _arg_sample_type, _arg_endian, fs, fc, total, headroom);
    Py_XDECREF(path);
    if (!self->h) {
        PyErr_SetString(PyExc_RuntimeError, "wfm_writer_open_path failed");
        return -1;
    }
    self->closed = 0;
    self->sample_type = _arg_sample_type;


    return 0;
}

static PyObject *
Writer_write(WriterObject *self, PyObject *args)
{
    PyObject *x_obj;
    if (!PyArg_ParseTuple(args, "O", &x_obj)) return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Writer is closed");
        return NULL;
    }
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;
    size_t n_in = (size_t)PyArray_SIZE(x_arr);
    const float _Complex *in_data = (const float _Complex *)PyArray_DATA(x_arr);
    size_t r;
    Py_BEGIN_ALLOW_THREADS
    r = wfm_writer_write(self->h, in_data, n_in);
    Py_END_ALLOW_THREADS
    Py_DECREF(x_arr);
    return PyLong_FromUnsignedLongLong((unsigned long long)r);
}

static PyObject *
Writer_track_clipping(WriterObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"on", NULL};
    int on = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist,
            &on)) return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Writer is closed");
        return NULL;
    }
    wfm_writer_track_clipping(self->h, on);
    Py_RETURN_NONE;
}

static PyObject *
Writer_get_clip_fraction(WriterObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Writer is closed");
        return NULL;
    }
    tmp = wfm_writer_clip_fraction(self->h);
    return PyFloat_FromDouble(tmp);
}

static PyObject *
Writer_get_peak_dbfs(WriterObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Writer is closed");
        return NULL;
    }
    tmp = wfm_writer_peak(self->h);
    return PyFloat_FromDouble(tmp > 0 ? 20*log10(tmp) : -INFINITY);
}

static PyObject *
Writer_get_clipped(WriterObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Writer is closed");
        return NULL;
    }
    tmp = wfm_writer_peak(self->h);
    return PyBool_FromLong((long)(tmp > 1.0 && self->sample_type >= 2));
}

static PyGetSetDef Writer_getset[] = {
    {"clip_fraction", (getter)Writer_get_clip_fraction, NULL, NULL, NULL},
    {"peak_dbfs", (getter)Writer_get_peak_dbfs, NULL, NULL, NULL},
    {"clipped", (getter)Writer_get_clipped, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *
Writer_close(WriterObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->closed && self->h) {
        int _rc = wfm_writer_close(self->h);
        self->h = NULL;
        self->closed = 1;
        if (_rc != 0) {
            PyErr_Format(PyExc_RuntimeError,
                "wfm_writer_close failed (rc=%d)", (int)_rc);
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *
Writer_enter(WriterObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Writer_exit(WriterObject *self, PyObject *args)
{
    (void)args;
    return Writer_close(self, NULL);
}
static void
Writer_dealloc(WriterObject *self)
{
    if (!self->closed && self->h) {
        wfm_writer_close(self->h);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef Writer_methods[] = {
    {"write", (PyCFunction)Writer_write, METH_VARARGS, NULL},
    {"track_clipping", (PyCFunction)Writer_track_clipping, METH_VARARGS | METH_KEYWORDS, NULL},
    {"close", (PyCFunction)Writer_close, METH_NOARGS, "close() -> None"},
    {"__enter__", (PyCFunction)Writer_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)Writer_exit, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject WriterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "doppler.wfm.Writer",
    .tp_basicsize = sizeof(WriterObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc)Writer_init,
    .tp_dealloc   = (destructor)Writer_dealloc,
    .tp_getset    = Writer_getset,
    .tp_methods   = Writer_methods,
    .tp_doc       = PyDoc_STR("Writer — handle over `wfm_writer`."),
};

static struct PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT, "wfm_writer", NULL, -1, NULL,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_wfm_writer(void)
{
    import_array();
    if (PyType_Ready(&WriterType) < 0) return NULL;
    PyObject *m = PyModule_Create(&_moduledef);
    if (!m) return NULL;
    Py_INCREF(&WriterType);
    if (PyModule_AddObject(m, "Writer", (PyObject *)&WriterType) < 0) {
        Py_DECREF(&WriterType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
