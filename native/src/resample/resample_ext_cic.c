/*
 * resample_ext_cic.c — CIC type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* CICObject — wraps cic_state_t *       */
/* ======================================================== */

#include "cic/cic_core.h"

typedef struct {
    PyObject_HEAD
    cic_state_t *handle;
    float complex *_decimate_buf;  /* pre-allocated output for decimate */
} CICObject;

static void
CICObj_dealloc(CICObject *self)
{
    if (self->handle)
        cic_destroy(self->handle);
    free(self->_decimate_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
CICObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CICObject *self = (CICObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
CICObj_init(CICObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"R", NULL};
    unsigned long R_raw = 16UL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|k", kwlist, &R_raw))
        return -1;
    uint32_t R = (uint32_t)R_raw;
    self->handle = cic_create(R);
    if (!self->handle) {
        PyErr_Format(PyExc_ValueError,
                     "CIC: R must be a power of two in [2, 4096], got %u",
                     R);
        return -1;
    }
    return 0;
}

static PyObject *
CICObj_reset(CICObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    cic_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
CICObj_reconfigure(CICObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    unsigned long R_raw = 0UL;
    if (!PyArg_ParseTuple(args, "k", &R_raw))
        return NULL;
    cic_reconfigure(self->handle, (uint32_t)R_raw);
    Py_RETURN_NONE;
}

static PyObject *
CICObj_decimate(CICObject *self, PyObject *args)
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
    if (!self->_decimate_buf) {
        size_t _max = cic_decimate_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_decimate_buf = malloc(_max * sizeof(float complex));
        if (!self->_decimate_buf) {
            Py_DECREF(in_arr);
            PyErr_NoMemory();
            return NULL;
        }
    }
    size_t n_out = cic_decimate(
        self->handle,
        (const float complex *)PyArray_DATA(in_arr),
        (size_t)n, self->_decimate_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_decimate_buf);
    if (!arr) { Py_DECREF(in_arr); return NULL; }
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(in_arr);
    return arr;
}
static PyObject *
CIC_getprop_R(CICObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLong((unsigned long)self->handle->R);
}
static PyObject *
CIC_getprop_shift(CICObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLong((unsigned long)self->handle->shift);
}

static PyGetSetDef CIC_getset[] = {
    { "R", (getter)CIC_getprop_R, NULL, NULL, NULL },
    { "shift", (getter)CIC_getprop_shift, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
CICObj_destroy(CICObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        cic_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
CICObj_enter(CICObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
CICObj_exit(CICObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        cic_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef CICObj_methods[] = {
    {"reset",    (PyCFunction)CICObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"reconfigure", (PyCFunction)CICObj_reconfigure, METH_VARARGS,
     "reconfigure(R) -> None\n"
     "\n"
     "reconfigure.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import CIC\n"
     "    >>> obj = CIC(16)\n"
     "    >>> obj.reconfigure(0)\n"},
    {"decimate", (PyCFunction)CICObj_decimate, METH_VARARGS,
     "decimate(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import CIC\n"
     "    >>> obj = CIC(16)\n"
     "    >>> y = obj.decimate(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)CICObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)CICObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)CICObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject CICObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.CIC",
    .tp_basicsize = sizeof(CICObject),
    .tp_dealloc   = (destructor)CICObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "CIC type.",
    .tp_methods   = CICObj_methods,
    .tp_getset    = CIC_getset,
    .tp_new       = CICObj_new,
    .tp_init      = (initproc)CICObj_init,
};
