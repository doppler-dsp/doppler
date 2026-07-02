/*
 * wfm_plan_ext.c — handle extension: typed `Plan` over `wfm_plan` (jm; gh-306).
 *
 * `Plan` wraps an opaque wfm_plan_t *; the resource logic
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

#include "wfm/wfm_plan.h"

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
    wfm_plan_t *h;
    int       closed;
} PlanObject;

static int
Plan_init(PlanObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"spec_json", NULL};
    const char *spec_json = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist,
            &spec_json)) {
        return -1;
    }

    if (!self->closed && self->h) {
        wfm_plan_destroy(self->h);
        self->h = NULL;
        self->closed = 1;
    }
    self->h = wfm_plan_prepare(spec_json);
    if (!self->h) {
        PyErr_SetString(PyExc_RuntimeError, "wfm_plan_prepare failed");
        return -1;
    }
    self->closed = 0;



    return 0;
}

static PyObject *
Plan_render(PlanObject *self, PyObject *args)
{
    const char *overrides_json = NULL;
    if (!PyArg_ParseTuple(args, "s", &overrides_json))
        return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Plan is closed");
        return NULL;
    }
    npy_intp _n = (npy_intp)wfm_plan_len(self->h);
    PyObject *arr = PyArray_SimpleNew(1, &_n, NPY_COMPLEX64);
    if (!arr) return NULL;
    float _Complex *_out = (float _Complex *)PyArray_DATA((PyArrayObject *)arr);
    size_t _got;
    Py_BEGIN_ALLOW_THREADS
    _got = wfm_plan_render(self->h, overrides_json, _out);
    Py_END_ALLOW_THREADS
    PyArray_DIMS((PyArrayObject *)arr)[0] = (npy_intp)_got; /* trim */
    return arr;
}

static PyObject *
Plan_at(PlanObject *self, PyObject *args)
{
    double snr_raw = 0;
    unsigned long long seed_raw = 0;
    if (!PyArg_ParseTuple(args, "dK", &snr_raw, &seed_raw))
        return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Plan is closed");
        return NULL;
    }
    npy_intp _n = (npy_intp)wfm_plan_len(self->h);
    PyObject *arr = PyArray_SimpleNew(1, &_n, NPY_COMPLEX64);
    if (!arr) return NULL;
    float _Complex *_out = (float _Complex *)PyArray_DATA((PyArrayObject *)arr);
    size_t _got;
    Py_BEGIN_ALLOW_THREADS
    _got = wfm_plan_at(self->h, snr_raw, (uint64_t)seed_raw, _out);
    Py_END_ALLOW_THREADS
    PyArray_DIMS((PyArrayObject *)arr)[0] = (npy_intp)_got; /* trim */
    return arr;
}

static PyObject *
Plan_length(PlanObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Plan is closed");
        return NULL;
    }
    size_t r;
    r = wfm_plan_len(self->h);
    return PyLong_FromUnsignedLongLong((unsigned long long)r);
}

static PyObject *
Plan_n_sources(PlanObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Plan is closed");
        return NULL;
    }
    size_t r;
    r = wfm_plan_n_sources(self->h);
    return PyLong_FromUnsignedLongLong((unsigned long long)r);
}

static PyObject *
Plan_anchor_seed(PlanObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Plan is closed");
        return NULL;
    }
    uint64_t r;
    r = wfm_plan_anchor_seed(self->h);
    return PyLong_FromUnsignedLongLong((unsigned long long)r);
}


static PyGetSetDef Plan_getset[] = {
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *
Plan_close(PlanObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->closed && self->h) {
        wfm_plan_destroy(self->h);
        self->closed = 1;
    }
    Py_RETURN_NONE;
}

static PyObject *
Plan_enter(PlanObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Plan_exit(PlanObject *self, PyObject *args)
{
    (void)args;
    return Plan_close(self, NULL);
}
static void
Plan_dealloc(PlanObject *self)
{
    if (!self->closed && self->h) {
        wfm_plan_destroy(self->h);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef Plan_methods[] = {
    {"render", (PyCFunction)Plan_render, METH_VARARGS, NULL},
    {"at", (PyCFunction)Plan_at, METH_VARARGS, NULL},
    {"length", (PyCFunction)Plan_length, METH_VARARGS, NULL},
    {"n_sources", (PyCFunction)Plan_n_sources, METH_VARARGS, NULL},
    {"anchor_seed", (PyCFunction)Plan_anchor_seed, METH_VARARGS, NULL},
    {"close", (PyCFunction)Plan_close, METH_NOARGS, "close() -> None"},
    {"__enter__", (PyCFunction)Plan_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)Plan_exit, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject PlanType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "doppler.wfm.Plan",
    .tp_basicsize = sizeof(PlanObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc)Plan_init,
    .tp_dealloc   = (destructor)Plan_dealloc,
    .tp_getset    = Plan_getset,
    .tp_methods   = Plan_methods,
    .tp_doc       = PyDoc_STR("Plan — handle over `wfm_plan`."),
};

static struct PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT, "wfm_plan", NULL, -1, NULL,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_wfm_plan(void)
{
    import_array();
    if (PyType_Ready(&PlanType) < 0) return NULL;
    PyObject *m = PyModule_Create(&_moduledef);
    if (!m) return NULL;
    Py_INCREF(&PlanType);
    if (PyModule_AddObject(m, "Plan", (PyObject *)&PlanType) < 0) {
        Py_DECREF(&PlanType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
