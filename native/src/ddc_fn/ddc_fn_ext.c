/*
 * ddc_fn_ext.c — handle extension: typed `Ddcr` over `ddcr` (jm; gh-306).
 *
 * `Ddcr` wraps an opaque ddcr_state_t *; the resource logic
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

#include "ddc/ddc_core.h"

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
    ddcr_state_t *h;
    int       closed;
} DdcrObject;

static int
Ddcr_init(DdcrObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"norm_freq", "rate", NULL};
    double norm_freq = 0.0;
    double rate = 0.25;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|dd", kwlist,
            &norm_freq, &rate)) {
        return -1;
    }

    if (!self->closed && self->h) {
        ddcr_destroy(self->h);
        self->h = NULL;
        self->closed = 1;
    }
    self->h = ddcr_create(norm_freq, rate);
    if (!self->h) {
        PyErr_SetString(PyExc_RuntimeError, "ddcr_create failed");
        return -1;
    }
    self->closed = 0;



    return 0;
}

static PyObject *
Ddcr_execute(DdcrObject *self, PyObject *args)
{
    PyObject *x_obj, *out_obj;
    if (!PyArg_ParseTuple(args, "OO", &x_obj, &out_obj)) return NULL;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Ddcr is closed");
        return NULL;
    }
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;

    /* Require the exact output dtype — no silent cast (a cast writes into a
     * temp copy instead of the caller's buffer). */
    if (!PyArray_Check(out_obj) ||
        PyArray_TYPE((PyArrayObject *)out_obj) != NPY_COMPLEX64 ||
        !PyArray_ISWRITEABLE((PyArrayObject *)out_obj)) {
        PyErr_SetString(PyExc_TypeError,
            "out must be a writable ndarray of the output dtype");
        Py_DECREF(x_arr);
        return NULL;
    }
    PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
        out_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
    if (!out_arr) { Py_DECREF(x_arr); return NULL; }

    size_t n_in    = (size_t)PyArray_SIZE(x_arr);
    size_t max_out = (size_t)PyArray_SIZE(out_arr);
    const float *in_data = (const float *)PyArray_DATA(x_arr);
    float _Complex *out_data = (float _Complex *)PyArray_DATA(out_arr);
    size_t n_out;
    Py_BEGIN_ALLOW_THREADS
    n_out = ddcr_execute(self->h, in_data, n_in, out_data, max_out);
    Py_END_ALLOW_THREADS
    Py_DECREF(x_arr);

    /* Return out_arr[:n_out] — zero-copy view into the caller's buffer. */
    PyObject *stop  = PyLong_FromSsize_t((Py_ssize_t)n_out);
    PyObject *slice = stop ? PySlice_New(NULL, stop, NULL) : NULL;
    Py_XDECREF(stop);
    PyObject *view  = slice ? PyObject_GetItem((PyObject *)out_arr, slice)
                            : NULL;
    Py_XDECREF(slice);
    Py_DECREF(out_arr);
    return view;
}

static PyObject *
Ddcr_reset(DdcrObject *self, PyObject *args)
{
    (void)args;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Ddcr is closed");
        return NULL;
    }
    ddcr_reset(self->h);
    Py_RETURN_NONE;
}

static PyObject *
Ddcr_get_norm_freq(DdcrObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Ddcr is closed");
        return NULL;
    }
    tmp = ddcr_get_norm_freq(self->h);
    return PyFloat_FromDouble(tmp);
}

static int
Ddcr_set_norm_freq(DdcrObject *self, PyObject *value, void *closure)
{
    (void)closure;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Ddcr is closed");
        return -1;
    }
    if (value == NULL) {
        PyErr_SetString(PyExc_AttributeError,
            "cannot delete 'norm_freq'");
        return -1;
    }
    double v;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    ddcr_set_norm_freq(self->h, v);
    return 0;
}

static PyObject *
Ddcr_get_rate(DdcrObject *self, void *closure)
{
    (void)closure;
    double tmp;
    if (self->closed) {
        PyErr_SetString(PyExc_RuntimeError, "Ddcr is closed");
        return NULL;
    }
    tmp = ddcr_get_rate(self->h);
    return PyFloat_FromDouble(tmp);
}

static PyGetSetDef Ddcr_getset[] = {
    {"norm_freq", (getter)Ddcr_get_norm_freq, (setter)Ddcr_set_norm_freq, NULL, NULL},
    {"rate", (getter)Ddcr_get_rate, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *
Ddcr_close(DdcrObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->closed && self->h) {
        ddcr_destroy(self->h);
        self->closed = 1;
    }
    Py_RETURN_NONE;
}

static PyObject *
Ddcr_enter(DdcrObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Ddcr_exit(DdcrObject *self, PyObject *args)
{
    (void)args;
    return Ddcr_close(self, NULL);
}
static void
Ddcr_dealloc(DdcrObject *self)
{
    if (!self->closed && self->h) {
        ddcr_destroy(self->h);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef Ddcr_methods[] = {
    {"execute", (PyCFunction)Ddcr_execute, METH_VARARGS, NULL},
    {"reset", (PyCFunction)Ddcr_reset, METH_VARARGS, NULL},
    {"close", (PyCFunction)Ddcr_close, METH_NOARGS, "close() -> None"},
    {"__enter__", (PyCFunction)Ddcr_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)Ddcr_exit, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject DdcrType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "doppler.ddc.Ddcr",
    .tp_basicsize = sizeof(DdcrObject),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc)Ddcr_init,
    .tp_dealloc   = (destructor)Ddcr_dealloc,
    .tp_getset    = Ddcr_getset,
    .tp_methods   = Ddcr_methods,
    .tp_doc       = PyDoc_STR("Ddcr — handle over `ddcr`."),
};

static struct PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT, "ddc_fn", NULL, -1, NULL,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_ddc_fn(void)
{
    import_array();
    if (PyType_Ready(&DdcrType) < 0) return NULL;
    PyObject *m = PyModule_Create(&_moduledef);
    if (!m) return NULL;
    Py_INCREF(&DdcrType);
    if (PyModule_AddObject(m, "Ddcr", (PyObject *)&DdcrType) < 0) {
        Py_DECREF(&DdcrType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
