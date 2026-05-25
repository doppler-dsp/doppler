/*
 * filter_ext_cic.c — CIC type for the filter module.
 *
 * Included by filter_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only filter_ext.c is compiled.
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
    static char *kwlist[] = {"R", "N", "M", NULL};
    unsigned long R_raw = 0UL;
    unsigned long N_raw = 0UL;
    unsigned long M_raw = 0UL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|kkk", kwlist,
                                     &R_raw, &N_raw, &M_raw))
        return -1;
    uint32_t R = (uint32_t)R_raw;
    uint32_t N = (uint32_t)N_raw;
    uint32_t M = (uint32_t)M_raw;
    self->handle = cic_create(R, N, M);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "cic_create returned NULL");
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
    unsigned long N_raw = 0UL;
    unsigned long M_raw = 0UL;
    if (!PyArg_ParseTuple(args, "kkk", &R_raw, &N_raw, &M_raw))
        return NULL;
    uint32_t R = (uint32_t)R_raw;
    uint32_t N = (uint32_t)N_raw;
    uint32_t M = (uint32_t)M_raw;
    cic_reconfigure(self->handle, R, N, M);
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
        if (!self->_decimate_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = cic_decimate(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_decimate_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_decimate_buf);
    if (!arr) return NULL;
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
CIC_getprop_N(CICObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLong((unsigned long)self->handle->N);
}
static PyObject *
CIC_getprop_M(CICObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLong((unsigned long)self->handle->M);
}
static PyObject *
CIC_getprop_input_scale(CICObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->input_scale);
}
static PyObject *
CIC_getprop_output_scale(CICObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->output_scale);
}

static PyGetSetDef CIC_getset[] = {
    { "R", (getter)CIC_getprop_R, NULL, NULL, NULL },
    { "N", (getter)CIC_getprop_N, NULL, NULL, NULL },
    { "M", (getter)CIC_getprop_M, NULL, NULL, NULL },
    { "input_scale", (getter)CIC_getprop_input_scale, NULL, NULL, NULL },
    { "output_scale", (getter)CIC_getprop_output_scale, NULL, NULL, NULL },
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
     "reconfigure(R, N, M) -> None\n"
     "\n"
     "reconfigure.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import CIC\n"
     "    >>> obj = CIC(1, 4, 1)\n"
     "    >>> obj.reconfigure(0, 0, 0)\n"},
    {"decimate", (PyCFunction)CICObj_decimate, METH_VARARGS,
     "decimate(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import CIC\n"
     "    >>> obj = CIC(1, 4, 1)\n"
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
    .tp_name      = "filter.CIC",
    .tp_basicsize = sizeof(CICObject),
    .tp_dealloc   = (destructor)CICObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "CIC type.",
    .tp_methods   = CICObj_methods,
    .tp_getset    = CIC_getset,
    .tp_new       = CICObj_new,
    .tp_init      = (initproc)CICObj_init,
};
