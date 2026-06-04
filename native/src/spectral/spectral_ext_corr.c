/*
 * spectral_ext_corr.c — Corr type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* CorrObject — wraps corr_state_t *       */
/* ======================================================== */

#include "corr/corr_core.h"

typedef struct {
    PyObject_HEAD
    corr_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
    size_t _execute_buf_cap;  /* allocated capacity for execute */
} CorrObject;

static void
CorrObj_dealloc(CorrObject *self)
{
    if (self->handle)
        corr_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
CorrObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CorrObject *self = (CorrObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
CorrObj_init(CorrObject *self, PyObject *args, PyObject *kwds)
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
    size_t ref_len = (size_t)PyArray_SIZE(ref_arr);
    self->handle = corr_create((const float complex *)PyArray_DATA(ref_arr), ref_len, dwell, nthreads);
    Py_DECREF(ref_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "corr_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
CorrObj_reset(CorrObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    corr_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
CorrObj_execute(CorrObject *self, PyObject *args)
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
        size_t _max = corr_execute_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_execute_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = corr_execute(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_buf);
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
Corr_getprop_n(CorrObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->n);
}
static PyObject *
Corr_getprop_dwell(CorrObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->dwell);
}
static PyObject *
Corr_getprop_count(CorrObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->count);
}

static PyGetSetDef Corr_getset[] = {
    { "n", (getter)Corr_getprop_n, NULL, "N.\n", NULL },
    { "dwell", (getter)Corr_getprop_dwell, NULL, "Dwell.\n", NULL },
    { "count", (getter)Corr_getprop_count, NULL, "Count.\n", NULL },
    { NULL }
};

static PyObject *
CorrObj_destroy(CorrObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        corr_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
CorrObj_enter(CorrObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
CorrObj_exit(CorrObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        corr_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef CorrObj_methods[] = {
    {"reset",    (PyCFunction)CorrObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)CorrObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Correlate one frame and optionally dump the accumulator.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Corr\n"
     "    >>> obj = Corr(np.zeros(1, dtype=np.complex64), 1, 1)\n"
     "    >>> y = obj.execute(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)CorrObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)CorrObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)CorrObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject CorrObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Corr",
    .tp_basicsize = sizeof(CorrObject),
    .tp_dealloc   = (destructor)CorrObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create a 1-D FFT correlator.\n",
    .tp_methods   = CorrObj_methods,
    .tp_getset    = Corr_getset,
    .tp_new       = CorrObj_new,
    .tp_init      = (initproc)CorrObj_init,
};
