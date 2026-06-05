/*
 * spectral_ext_detector.c — Detector type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* DetectorObject — wraps detector_state_t *       */
/* ======================================================== */

#include "detector/detector_core.h"

typedef struct {
    PyObject_HEAD
    detector_state_t *handle;
} DetectorObject;

static void
DetectorObj_dealloc(DetectorObject *self)
{
    if (self->handle)
        detector_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
DetectorObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DetectorObject *self = (DetectorObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
DetectorObj_init(DetectorObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"ref", "noise_mode", "dwell", "noise_lo", "noise_hi", "threshold", "nthreads", NULL};
    PyObject *ref_obj = NULL;
    const char *noise_mode_str = "mean";
    unsigned long long dwell_raw = 0ULL;
    unsigned long long noise_lo_raw = 0ULL;
    unsigned long long noise_hi_raw = (unsigned long long)-1ULL;
    float threshold = 0.0f;
    int nthreads = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|sKKKfi", kwlist,
                                     &ref_obj, &noise_mode_str, &dwell_raw, &noise_lo_raw, &noise_hi_raw, &threshold, &nthreads))
        return -1;
    int noise_mode = 0;
    if (strcmp(noise_mode_str, "mean") == 0) noise_mode = 0;
    else if (strcmp(noise_mode_str, "median") == 0) noise_mode = 1;
    else if (strcmp(noise_mode_str, "min") == 0) noise_mode = 2;
    else if (strcmp(noise_mode_str, "max") == 0) noise_mode = 3;
    else {
        PyErr_Format(PyExc_ValueError, "noise_mode must be one of \"mean\", \"median\", \"min\", \"max\", got '%s'", noise_mode_str);
        return -1;
    }
    size_t dwell = (size_t)dwell_raw;
    size_t noise_lo = (size_t)noise_lo_raw;
    size_t noise_hi = (size_t)noise_hi_raw;
    PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF(
        ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!ref_arr) { return -1; }
    size_t ref_len = (size_t)PyArray_SIZE(ref_arr);
    self->handle = detector_create((const float complex *)PyArray_DATA(ref_arr), ref_len, dwell, noise_lo, noise_hi, noise_mode, threshold, nthreads);
    Py_DECREF(ref_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "detector_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
DetectorObj_reset(DetectorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    detector_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
DetectorObj_push(DetectorObject *self, PyObject *args)
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
    size_t n_in = (size_t)PyArray_SIZE(in_arr);
    det_result_t results[64];
    size_t n_out = detector_push(self->handle,
        (const float complex *)PyArray_DATA(in_arr), n_in,
        results, 64);
    Py_DECREF(in_arr);
    PyObject *lst = PyList_New((Py_ssize_t)n_out);
    if (!lst) return NULL;
    for (size_t i = 0; i < n_out; i++) {
        PyObject *tup = Py_BuildValue("(Kfff)", (unsigned long long)results[i].lag, results[i].peak_mag, results[i].noise_est, results[i].test_stat);
        if (!tup) { Py_DECREF(lst); return NULL; }
        PyList_SET_ITEM(lst, (Py_ssize_t)i, tup);
    }
    return lst;
}
static PyObject *
Detector_getprop_n(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->n);
}
static PyObject *
Detector_getprop_dwell(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->corr->dwell);
}
static PyObject *
Detector_getprop_count(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->corr->count);
}
static PyObject *
Detector_getprop_ring_cap(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->ring_cap);
}
static PyObject *
Detector_getprop_noise_lo(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->noise_lo);
}
static PyObject *
Detector_getprop_noise_hi(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->noise_hi);
}
static PyObject *
Detector_getprop_threshold(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble((double)self->handle->threshold);
}
static PyObject *
Detector_getprop_last_corr(DetectorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    if (!self->handle->_last_corr_valid) Py_RETURN_NONE;
    npy_intp dim = (npy_intp)self->handle->n;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->handle->out_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyGetSetDef Detector_getset[] = {
    { "n", (getter)Detector_getprop_n, NULL, NULL, NULL },
    { "dwell", (getter)Detector_getprop_dwell, NULL, NULL, NULL },
    { "count", (getter)Detector_getprop_count, NULL, NULL, NULL },
    { "ring_cap", (getter)Detector_getprop_ring_cap, NULL, NULL, NULL },
    { "noise_lo", (getter)Detector_getprop_noise_lo, NULL, NULL, NULL },
    { "noise_hi", (getter)Detector_getprop_noise_hi, NULL, NULL, NULL },
    { "threshold", (getter)Detector_getprop_threshold, NULL, NULL, NULL },
    { "last_corr", (getter)Detector_getprop_last_corr, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
DetectorObj_destroy(DetectorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        detector_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
DetectorObj_enter(DetectorObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
DetectorObj_exit(DetectorObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        detector_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef DetectorObj_methods[] = {
    {"reset",    (PyCFunction)DetectorObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"push", (PyCFunction)DetectorObj_push, METH_VARARGS,
     "push(x) -> list[tuple]\n"
     "\n"
     "Returns list of (lag, peak_mag, noise_est, test_stat,) tuples.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Detector\n"
     "    >>> obj = Detector(np.zeros(1, dtype=np.complex64), \"mean\", 1, 0, n-1, 0.0, 1)\n"
     "    >>> results = obj.push(np.zeros(4, dtype=np.complex64))\n"
     "    >>> isinstance(results, list)\n"
     "    True\n"},
    {"destroy",  (PyCFunction)DetectorObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)DetectorObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)DetectorObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject DetectorObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Detector",
    .tp_basicsize = sizeof(DetectorObject),
    .tp_dealloc   = (destructor)DetectorObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create a 1-D signal detector.\n",
    .tp_methods   = DetectorObj_methods,
    .tp_getset    = Detector_getset,
    .tp_new       = DetectorObj_new,
    .tp_init      = (initproc)DetectorObj_init,
};
