/*
 * spectral_ext_corr2d.c — Corr2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* Corr2DObject — wraps corr2d_state_t *       */
/* ======================================================== */

#include "corr2d/corr2d_core.h"

typedef struct {
    PyObject_HEAD
    corr2d_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
} Corr2DObject;

static void
Corr2DObj_dealloc(Corr2DObject *self)
{
    if (self->handle)
        corr2d_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Corr2DObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Corr2DObject *self = (Corr2DObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Corr2DObj_init(Corr2DObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"ref", "dwell", "nthreads", NULL};
    PyObject *ref_obj = NULL;
    unsigned long long dwell_raw = 0ULL;
    int nthreads = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|Ki", kwlist,
                                     &ref_obj, &dwell_raw, &nthreads))
        return -1;
    size_t dwell = (size_t)dwell_raw;
    PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF(
        ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!ref_arr) { return -1; }
    if (PyArray_NDIM(ref_arr) != 2) {
        PyErr_SetString(PyExc_ValueError,
                        "ref must be a 2-D array");
         Py_DECREF(ref_arr); return -1;
    }
    size_t ref_dim0 = (size_t)PyArray_DIM(ref_arr, 0);
    size_t ref_dim1 = (size_t)PyArray_DIM(ref_arr, 1);
    self->handle = corr2d_create((const float complex *)PyArray_DATA(ref_arr), ref_dim0, ref_dim1, dwell, nthreads);
    Py_DECREF(ref_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "corr2d_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
Corr2DObj_reset(Corr2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    corr2d_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Corr2DObj_execute(Corr2DObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr) return NULL;
    Py_ssize_t n = PyArray_SIZE(in_arr);
    if (!self->_execute_buf) {
        size_t _max = corr2d_execute_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_execute_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = corr2d_execute(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_buf);
    if (!n_out) Py_RETURN_NONE;
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_execute_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(in_arr);
    return arr;
}
static PyObject *
Corr2D_getprop_ny(Corr2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->ny);
}
static PyObject *
Corr2D_getprop_nx(Corr2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->nx);
}
static PyObject *
Corr2D_getprop_dwell(Corr2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->dwell);
}
static PyObject *
Corr2D_getprop_count(Corr2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->count);
}

static PyGetSetDef Corr2D_getset[] = {
    { "ny", (getter)Corr2D_getprop_ny, NULL, NULL, NULL },
    { "nx", (getter)Corr2D_getprop_nx, NULL, NULL, NULL },
    { "dwell", (getter)Corr2D_getprop_dwell, NULL, NULL, NULL },
    { "count", (getter)Corr2D_getprop_count, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
Corr2DObj_destroy(Corr2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        corr2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Corr2DObj_enter(Corr2DObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Corr2DObj_exit(Corr2DObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        corr2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Corr2DObj_methods[] = {
    {"reset",    (PyCFunction)Corr2DObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)Corr2DObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Corr2D\n"
     "    >>> obj = Corr2D(np.zeros((1, 1), dtype=np.complex64), 1, 1)\n"
     "    >>> y = obj.execute(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)Corr2DObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Corr2DObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Corr2DObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject Corr2DObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Corr2D",
    .tp_basicsize = sizeof(Corr2DObject),
    .tp_dealloc   = (destructor)Corr2DObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create a 2-D FFT correlator.\n",
    .tp_methods   = Corr2DObj_methods,
    .tp_getset    = Corr2D_getset,
    .tp_new       = Corr2DObj_new,
    .tp_init      = (initproc)Corr2DObj_init,
};
