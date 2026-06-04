/*
 * spectral_ext_fft2d.c — FFT2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* FFT2DObject — wraps fft2d_state_t *       */
/* ======================================================== */

#include "fft2d/fft2d_core.h"

typedef struct {
    PyObject_HEAD
    fft2d_state_t *handle;
    double complex *_execute_cf64_buf;  /* pre-allocated output for execute_cf64 */
    size_t _execute_cf64_buf_cap;  /* allocated capacity for execute_cf64 */
    float complex *_execute_cf32_buf;  /* pre-allocated output for execute_cf32 */
    size_t _execute_cf32_buf_cap;  /* allocated capacity for execute_cf32 */
    double complex *_execute_inplace_cf64_buf;  /* pre-allocated output for execute_inplace_cf64 */
    size_t _execute_inplace_cf64_buf_cap;  /* allocated capacity for execute_inplace_cf64 */
    float complex *_execute_inplace_cf32_buf;  /* pre-allocated output for execute_inplace_cf32 */
    size_t _execute_inplace_cf32_buf_cap;  /* allocated capacity for execute_inplace_cf32 */
} FFT2DObject;

static void
FFT2DObj_dealloc(FFT2DObject *self)
{
    if (self->handle)
        fft2d_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
FFT2DObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FFT2DObject *self = (FFT2DObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
FFT2DObj_init(FFT2DObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"ny", "nx", "sign", "nthreads", NULL};
    unsigned long long ny_raw = 0ULL;
    unsigned long long nx_raw = 0ULL;
    int sign = -1;
    int nthreads = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|KKii", kwlist,
                                     &ny_raw, &nx_raw, &sign, &nthreads))
        return -1;
    size_t ny = (size_t)ny_raw;
    size_t nx = (size_t)nx_raw;
    self->handle = fft2d_create(ny, nx, sign, nthreads);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "fft2d_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
FFT2DObj_reset(FFT2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    fft2d_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
FFT2DObj_execute_cf64(FFT2DObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr) return NULL;
    Py_ssize_t n = PyArray_SIZE(in_arr);
    if (!self->_execute_cf64_buf) {
        size_t _max = fft2d_execute_cf64_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_execute_cf64_buf = malloc(_max * sizeof(double complex));
        if (!self->_execute_cf64_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = fft2d_execute_cf64(self->handle, (const double complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_cf64_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX128, self->_execute_cf64_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(in_arr);
    return arr;
}

static PyObject *
FFT2DObj_execute_cf32(FFT2DObject *self, PyObject *args)
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
    if (!self->_execute_cf32_buf) {
        size_t _max = fft2d_execute_cf32_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_execute_cf32_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_cf32_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = fft2d_execute_cf32(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_cf32_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_execute_cf32_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(in_arr);
    return arr;
}

static PyObject *
FFT2DObj_execute_inplace_cf64(FFT2DObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr) return NULL;
    Py_ssize_t n = PyArray_SIZE(in_arr);
    if (!self->_execute_inplace_cf64_buf) {
        size_t _max = fft2d_execute_inplace_cf64_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_execute_inplace_cf64_buf = malloc(_max * sizeof(double complex));
        if (!self->_execute_inplace_cf64_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = fft2d_execute_inplace_cf64(self->handle, (const double complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_inplace_cf64_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX128, self->_execute_inplace_cf64_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(in_arr);
    return arr;
}

static PyObject *
FFT2DObj_execute_inplace_cf32(FFT2DObject *self, PyObject *args)
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
    if (!self->_execute_inplace_cf32_buf) {
        size_t _max = fft2d_execute_inplace_cf32_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_execute_inplace_cf32_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_inplace_cf32_buf) { Py_DECREF(in_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = fft2d_execute_inplace_cf32(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_inplace_cf32_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_execute_inplace_cf32_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(in_arr);
    return arr;
}
static PyObject *
FFT2D_getprop_ny(FFT2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->ny);
}
static PyObject *
FFT2D_getprop_nx(FFT2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->nx);
}
static PyObject *
FFT2D_getprop_sign(FFT2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLong((long)self->handle->sign);
}

static PyGetSetDef FFT2D_getset[] = {
    { "ny", (getter)FFT2D_getprop_ny, NULL, "Ny.\n", NULL },
    { "nx", (getter)FFT2D_getprop_nx, NULL, "Nx.\n", NULL },
    { "sign", (getter)FFT2D_getprop_sign, NULL, "Sign.\n", NULL },
    { NULL }
};

static PyObject *
FFT2DObj_destroy(FFT2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        fft2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
FFT2DObj_enter(FFT2DObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
FFT2DObj_exit(FFT2DObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        fft2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef FFT2DObj_methods[] = {
    {"reset",    (PyCFunction)FFT2DObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute_cf64", (PyCFunction)FFT2DObj_execute_cf64, METH_VARARGS,
     "execute_cf64(x) -> ndarray\n"
     "\n"
     "Out-of-place 2-D CF64 FFT.  Returns ny*nx.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"execute_cf32", (PyCFunction)FFT2DObj_execute_cf32, METH_VARARGS,
     "execute_cf32(x) -> ndarray\n"
     "\n"
     "Out-of-place 2-D CF32 FFT.  Returns ny*nx.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"execute_inplace_cf64", (PyCFunction)FFT2DObj_execute_inplace_cf64, METH_VARARGS,
     "execute_inplace_cf64(x) -> ndarray\n"
     "\n"
     "In-place 2-D CF64 FFT (copies in→out, then transforms).\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"execute_inplace_cf32", (PyCFunction)FFT2DObj_execute_inplace_cf32, METH_VARARGS,
     "execute_inplace_cf32(x) -> ndarray\n"
     "\n"
     "In-place 2-D CF32 FFT (copies in→out, then transforms).\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)FFT2DObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)FFT2DObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)FFT2DObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject FFT2DObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.FFT2D",
    .tp_basicsize = sizeof(FFT2DObject),
    .tp_dealloc   = (destructor)FFT2DObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create a 2-D FFT instance.\n",
    .tp_methods   = FFT2DObj_methods,
    .tp_getset    = FFT2D_getset,
    .tp_new       = FFT2DObj_new,
    .tp_init      = (initproc)FFT2DObj_init,
};
