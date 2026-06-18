/*
 * wfm_sink_ext.c — handle extension: typed `ZmqSink` over `wfm_zmq_sink` (jm; gh-306).
 *
 * `ZmqSink` wraps an opaque wfm_zmq_sink_t *; the resource logic
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

#include "wfm/wfm_sink.h"

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

typedef struct {
    PyObject_HEAD
    wfm_zmq_sink_t *h;
    int       closed;
    int sample_type;
} ZmqSinkObject;

static int
ZmqSink_init(ZmqSinkObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"endpoint", "sample_type", NULL};
    const char * endpoint = 0;
    const char *sample_type = "cf32";
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ss", kwlist,
            &endpoint, &sample_type)) {
        return -1;
    }
    int _arg_sample_type = _enum_index(_enum_stype, sample_type);
    if (_arg_sample_type < 0) {
        PyErr_Format(PyExc_ValueError, "invalid sample_type '%s'", sample_type);
        return -1;
    }
    self->h = wfm_zmq_sink_open(endpoint, _arg_sample_type);
    if (!self->h) {
        PyErr_SetString(PyExc_RuntimeError, "wfm_zmq_sink_open failed");
        return -1;
    }
    self->closed = 0;
    self->sample_type = _arg_sample_type;


    return 0;
}

static PyObject *
ZmqSink_send(ZmqSinkObject *self, PyObject *args)
{
    PyObject *x_obj;
    double fs;
    double fc;
    if (!PyArg_ParseTuple(args, "Odd", &x_obj, &fs, &fc))
        return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "ZmqSink is closed");
        return NULL;
    }
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;
    size_t n_in = (size_t)PyArray_SIZE(x_arr);
    const float _Complex *in_data = (const float _Complex *)PyArray_DATA(x_arr);
    int r;
    Py_BEGIN_ALLOW_THREADS
    r = wfm_zmq_sink_send(self->h, in_data, n_in, fs, fc);
    Py_END_ALLOW_THREADS
    Py_DECREF(x_arr);
    return PyLong_FromLong((long)r);
}

static PyObject *
ZmqSink_track_clipping(ZmqSinkObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"on", NULL};
    int on = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist,
            &on)) return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "ZmqSink is closed");
        return NULL;
    }
    wfm_zmq_sink_track_clipping(self->h, on);
    Py_RETURN_NONE;
}

static PyObject *
ZmqSink_get_clip_fraction(ZmqSinkObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "ZmqSink is closed");
        return NULL;
    }
    tmp = wfm_zmq_sink_clip_fraction(self->h);
    return PyFloat_FromDouble(tmp);
}

static PyObject *
ZmqSink_get_peak_dbfs(ZmqSinkObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "ZmqSink is closed");
        return NULL;
    }
    tmp = wfm_zmq_sink_peak(self->h);
    return PyFloat_FromDouble(tmp > 0 ? 20*log10(tmp) : -INFINITY);
}

static PyObject *
ZmqSink_get_clipped(ZmqSinkObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "ZmqSink is closed");
        return NULL;
    }
    tmp = wfm_zmq_sink_peak(self->h);
    return PyBool_FromLong((long)(tmp > 1.0 && self->sample_type >= 2));
}

static PyGetSetDef ZmqSink_getset[] = {
    {"clip_fraction", (getter)ZmqSink_get_clip_fraction, NULL, NULL, NULL},
    {"peak_dbfs", (getter)ZmqSink_get_peak_dbfs, NULL, NULL, NULL},
    {"clipped", (getter)ZmqSink_get_clipped, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *
ZmqSink_close(ZmqSinkObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->closed && self->h) {
        wfm_zmq_sink_close(self->h);
        self->closed = 1;
    }
    Py_RETURN_NONE;
}

static PyObject *
ZmqSink_enter(ZmqSinkObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
ZmqSink_exit(ZmqSinkObject *self, PyObject *args)
{
    (void)args;
    return ZmqSink_close(self, NULL);
}
static void
ZmqSink_dealloc(ZmqSinkObject *self)
{
    if (!self->closed && self->h) {
        wfm_zmq_sink_close(self->h);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef ZmqSink_methods[] = {
    {"send", (PyCFunction)ZmqSink_send, METH_VARARGS, NULL},
    {"track_clipping", (PyCFunction)ZmqSink_track_clipping, METH_VARARGS | METH_KEYWORDS, NULL},
    {"close", (PyCFunction)ZmqSink_close, METH_NOARGS, "close() -> None"},
    {"__enter__", (PyCFunction)ZmqSink_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)ZmqSink_exit, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject ZmqSinkType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "doppler.wfm.ZmqSink",
    .tp_basicsize = sizeof(ZmqSinkObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc)ZmqSink_init,
    .tp_dealloc   = (destructor)ZmqSink_dealloc,
    .tp_getset    = ZmqSink_getset,
    .tp_methods   = ZmqSink_methods,
    .tp_doc       = PyDoc_STR("ZmqSink — handle over `wfm_zmq_sink`."),
};

static struct PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT, "wfm_sink", NULL, -1, NULL,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_wfm_sink(void)
{
    import_array();
    if (PyType_Ready(&ZmqSinkType) < 0) return NULL;
    PyObject *m = PyModule_Create(&_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ZmqSinkType);
    if (PyModule_AddObject(m, "ZmqSink", (PyObject *)&ZmqSinkType) < 0) {
        Py_DECREF(&ZmqSinkType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
