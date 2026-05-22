/*
 * filter_ext.c — Python extension module filter
 *
 * Objects: FIR
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

/* ======================================================== */
/* FIRObject — wraps fir_state_t *       */
/* ======================================================== */

#include "fir/fir_core.h"

typedef struct {
    PyObject_HEAD
    fir_state_t *handle;
} FIRObject;

static void
FIR_dealloc(FIRObject *self)
{
    if (self->handle)
        fir_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
FIR_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FIRObject *self = (FIRObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
FIR_init(FIRObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"taps", NULL};
    PyObject *taps_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
                                     &taps_obj))
        return -1;
    PyArrayObject *taps_arr = (PyArrayObject *)PyArray_FROM_OTF(
        taps_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!taps_arr) { return -1; }
    size_t taps_len = (size_t)PyArray_SIZE(taps_arr);
    self->handle = fir_create((const float complex *)PyArray_DATA(taps_arr), taps_len);
    Py_DECREF(taps_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "fir_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
FIR_reset(FIRObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    fir_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
FIR_execute(FIRObject *self, PyObject *args)
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
    npy_intp n = PyArray_SIZE(in_arr);
    PyObject *out_arr = PyArray_SimpleNew(1, &n, NPY_COMPLEX64);
    if (!out_arr) { Py_DECREF(in_arr); return NULL; }
    size_t n_out = fir_execute(self->handle,
        (const float complex *)PyArray_DATA(in_arr), (size_t)n,
        (float complex *)PyArray_DATA((PyArrayObject *)out_arr));
    Py_DECREF(in_arr);
    if ((npy_intp)n_out == n)
        return out_arr;
    npy_intp actual = (npy_intp)n_out;
    PyArray_Dims ds = {&actual, 1};
    PyObject *result = PyArray_Newshape((PyArrayObject *)out_arr, &ds, NPY_CORDER);
    Py_DECREF(out_arr);
    return result;
}
static PyObject *
FIR_getprop_num_taps(FIRObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->num_taps);
}
static PyObject *
FIR_getprop_is_real(FIRObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyBool_FromLong((long)(fir_get_is_real(self->handle)));
}

static PyGetSetDef FIR_getset[] = {
    { "num_taps", (getter)FIR_getprop_num_taps, NULL, NULL, NULL },
    { "is_real", (getter)FIR_getprop_is_real, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
FIR_destroy(FIRObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        fir_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
FIR_enter(FIRObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
FIR_exit(FIRObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        fir_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef FIR_methods[] = {
    {"reset",    (PyCFunction)FIR_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)FIR_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Filter x and return a new output array.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FIR\n"
     "    >>> obj = FIR(np.zeros(1, dtype=np.complex64))\n"
     "    >>> y = obj.execute(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)FIR_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)FIR_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)FIR_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject FIRType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "filter.FIR",
    .tp_basicsize = sizeof(FIRObject),
    .tp_dealloc   = (destructor)FIR_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "FIR type.",
    .tp_methods   = FIR_methods,
    .tp_getset    = FIR_getset,
    .tp_new       = FIR_new,
    .tp_init      = (initproc)FIR_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef filter_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "filter",
    .m_doc     = "Filter module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_filter(void)
{
    import_array();
    if (PyType_Ready(&FIRType) < 0) return NULL;
    PyObject *m = PyModule_Create(&filter_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FIRType);
    if (PyModule_AddObject(m, "FIR", (PyObject *)&FIRType) < 0) {
        Py_DECREF(&FIRType); Py_DECREF(m); return NULL;
    }
    return m;
}
