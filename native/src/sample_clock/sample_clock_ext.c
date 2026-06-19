/*
 * sample_clock_ext.c — handle extension: typed `SampleClock` over `dp_sample_clock` (jm; gh-306).
 *
 * `SampleClock` wraps an opaque dp_sample_clock_t *; the resource logic
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

#include "timing/timing_core.h"

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index(const char *const *tab, const char *s)
{
    for (int i = 0; tab[i]; i++)
        if (strcmp(tab[i], s) == 0)
            return i;
    return -1;
}

typedef struct {
    PyObject_HEAD
    dp_sample_clock_t *h;
    int       closed;
} SampleClockObject;

static int
SampleClock_init(SampleClockObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"fs", "resync", NULL};
    double fs = 0;
    int resync = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "d|i", kwlist,
            &fs, &resync)) {
        return -1;
    }

    if (!self->closed && self->h) {
        free(self->h);
        self->h = NULL;
        self->closed = 1;
    }
    self->h = (dp_sample_clock_t *)malloc(sizeof(dp_sample_clock_t));
    if (!self->h) {
        PyErr_NoMemory();
        return -1;
    }
    dp_sample_clock_init(self->h, fs, resync);
    self->closed = 0;



    return 0;
}

static PyObject *
SampleClock_pace(SampleClockObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"count", NULL};
    size_t count;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "K", kwlist,
            &count)) return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    double r;
    Py_BEGIN_ALLOW_THREADS
    r = dp_sample_clock_pace(self->h, count);
    Py_END_ALLOW_THREADS
    return PyFloat_FromDouble(r);
}

static PyObject *
SampleClock_stamp(SampleClockObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    uint64_t r;
    r = dp_sample_clock_stamp(self->h);
    return PyLong_FromUnsignedLongLong((unsigned long long)r);
}

static PyObject *
SampleClock_reset(SampleClockObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    dp_sample_clock_reset(self->h);
    Py_RETURN_NONE;
}

static PyObject *
SampleClock_resync(SampleClockObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    dp_sample_clock_resync(self->h);
    Py_RETURN_NONE;
}

static PyObject *
SampleClock_get_samples(SampleClockObject *self, void *closure)
{
    (void)closure;
    dp_sample_clock_t tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    dp_sample_clock_stats(self->h, &tmp);
    return PyLong_FromUnsignedLongLong((unsigned long long)tmp.n);
}

static PyObject *
SampleClock_get_underruns(SampleClockObject *self, void *closure)
{
    (void)closure;
    dp_sample_clock_t tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    dp_sample_clock_stats(self->h, &tmp);
    return PyLong_FromUnsignedLongLong((unsigned long long)tmp.underruns);
}

static PyObject *
SampleClock_get_max_lateness(SampleClockObject *self, void *closure)
{
    (void)closure;
    dp_sample_clock_t tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "SampleClock is closed");
        return NULL;
    }
    dp_sample_clock_stats(self->h, &tmp);
    return PyFloat_FromDouble(tmp.max_late_ns * 1e-9);
}

static PyGetSetDef SampleClock_getset[] = {
    {"samples", (getter)SampleClock_get_samples, NULL, NULL, NULL},
    {"underruns", (getter)SampleClock_get_underruns, NULL, NULL, NULL},
    {"max_lateness", (getter)SampleClock_get_max_lateness, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *
SampleClock_close(SampleClockObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->closed && self->h) {
        free(self->h);
        self->closed = 1;
    }
    Py_RETURN_NONE;
}

static void
SampleClock_dealloc(SampleClockObject *self)
{
    if (!self->closed && self->h) {
        free(self->h);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef SampleClock_methods[] = {
    {"pace", (PyCFunction)SampleClock_pace, METH_VARARGS | METH_KEYWORDS, NULL},
    {"stamp", (PyCFunction)SampleClock_stamp, METH_VARARGS, NULL},
    {"reset", (PyCFunction)SampleClock_reset, METH_VARARGS, NULL},
    {"resync", (PyCFunction)SampleClock_resync, METH_VARARGS, NULL},
    {"close", (PyCFunction)SampleClock_close, METH_NOARGS, "close() -> None"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject SampleClockType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "doppler.wfm.SampleClock",
    .tp_basicsize = sizeof(SampleClockObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc)SampleClock_init,
    .tp_dealloc   = (destructor)SampleClock_dealloc,
    .tp_getset    = SampleClock_getset,
    .tp_methods   = SampleClock_methods,
    .tp_doc       = PyDoc_STR("SampleClock — handle over `dp_sample_clock`."),
};

static struct PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT, "sample_clock", NULL, -1, NULL,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_sample_clock(void)
{
    import_array();
    if (PyType_Ready(&SampleClockType) < 0) return NULL;
    PyObject *m = PyModule_Create(&_moduledef);
    if (!m) return NULL;
    Py_INCREF(&SampleClockType);
    if (PyModule_AddObject(m, "SampleClock", (PyObject *)&SampleClockType) < 0) {
        Py_DECREF(&SampleClockType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
