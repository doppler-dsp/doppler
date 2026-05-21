/*
 * spectral_ext.c — Python extension module spectral
 *
 * Objects: FFT, FFT2D, Corr, Corr2D, Detector, Detector2D
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "spectral/spectral_core.h"
static PyObject *
_bind_kaiser_enbw(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *w_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &w_obj))
        return NULL;
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!w_arr) { return NULL; }
    const float *w = (const float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    Py_DECREF(w_arr);
    return PyFloat_FromDouble((double)kaiser_enbw(w, w_len));
}

static PyObject *
_bind_kaiser_window(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *w_obj = NULL;
    float beta = 0.0f;
    if (!PyArg_ParseTuple(args, "Of", &w_obj, &beta))
        return NULL;
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!w_arr) { return NULL; }
    const float *w = (const float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    kaiser_window(w, w_len, beta);
    Py_DECREF(w_arr);
    Py_RETURN_NONE;
}

static PyObject *
_bind_hann_window(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *w_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &w_obj))
        return NULL;
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!w_arr) { return NULL; }
    const float *w = (const float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    hann_window(w, w_len);
    Py_DECREF(w_arr);
    Py_RETURN_NONE;
}

static PyObject *
_bind_magnitude_db_cf32(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *in_obj = NULL;
    float lin_floor = 0.0f;
    float offset_db = 0.0f;
    if (!PyArg_ParseTuple(args, "Off", &in_obj, &lin_floor, &offset_db))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr) { return NULL; }
    const float complex *in = (const float complex *)PyArray_DATA(in_arr);
    size_t in_len = (size_t)PyArray_SIZE(in_arr);
    npy_intp _dim = (npy_intp)in_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_FLOAT, 0);
    if (!_out) {Py_DECREF(in_arr); return NULL; }
    magnitude_db_cf32(in, in_len, (float *)PyArray_DATA((PyArrayObject *)_out), lin_floor, offset_db);
    Py_DECREF(in_arr);
    return _out;
}

static PyObject *
_bind_magnitude_db_cf64(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *in_obj = NULL;
    double lin_floor = 0.0;
    float offset_db = 0.0f;
    if (!PyArg_ParseTuple(args, "Odf", &in_obj, &lin_floor, &offset_db))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr) { return NULL; }
    const double complex *in = (const double complex *)PyArray_DATA(in_arr);
    size_t in_len = (size_t)PyArray_SIZE(in_arr);
    npy_intp _dim = (npy_intp)in_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_FLOAT, 0);
    if (!_out) {Py_DECREF(in_arr); return NULL; }
    magnitude_db_cf64(in, in_len, (float *)PyArray_DATA((PyArrayObject *)_out), lin_floor, offset_db);
    Py_DECREF(in_arr);
    return _out;
}

static PyObject *
_bind_find_peaks_f32(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *db_obj = NULL;
    unsigned long long n_peaks_raw = 0ULL;
    float min_db = 0.0f;
    if (!PyArg_ParseTuple(args, "OKf", &db_obj, &n_peaks_raw, &min_db))
        return NULL;
    size_t n_peaks = (size_t)n_peaks_raw;
    PyArrayObject *db_arr = (PyArrayObject *)PyArray_FROM_OTF(
        db_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!db_arr) { return NULL; }
    const float *db = (const float *)PyArray_DATA(db_arr);
    size_t db_len = (size_t)PyArray_SIZE(db_arr);
    size_t _max = (size_t)n_peaks;
    dp_peak_t *_results = (dp_peak_t *)malloc(_max * sizeof(dp_peak_t));
    if (!_results) {Py_DECREF(db_arr); return PyErr_NoMemory(); }
    size_t _n = find_peaks_f32(db, db_len, n_peaks, min_db, _results);
    Py_DECREF(db_arr);
    PyObject *_lst = PyList_New((Py_ssize_t)_n);
    if (!_lst) { free(_results); return NULL; }
    for (size_t _i = 0; _i < _n; _i++) {
        PyObject *_tup = Py_BuildValue("(ff)", _results[_i].freq_norm, _results[_i].amplitude_db);
        if (!_tup) { free(_results); Py_DECREF(_lst); return NULL; }
        PyList_SET_ITEM(_lst, (Py_ssize_t)_i, _tup);
    }
    free(_results);
    return _lst;
}

/* ======================================================== */
/* FFTObject — wraps fft_state_t *       */
/* ======================================================== */

#include "fft/fft_core.h"

typedef struct {
    PyObject_HEAD
    fft_state_t *handle;
    double complex *_execute_cf64_buf;  /* pre-allocated output for execute_cf64 */
    float complex *_execute_cf32_buf;  /* pre-allocated output for execute_cf32 */
    double complex *_execute_inplace_cf64_buf;  /* pre-allocated output for execute_inplace_cf64 */
    float complex *_execute_inplace_cf32_buf;  /* pre-allocated output for execute_inplace_cf32 */
} FFTObject;

static void
FFT_dealloc(FFTObject *self)
{
    if (self->handle)
        fft_destroy(self->handle);
    free(self->_execute_cf64_buf);
    free(self->_execute_cf32_buf);
    free(self->_execute_inplace_cf64_buf);
    free(self->_execute_inplace_cf32_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
FFT_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FFTObject *self = (FFTObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
FFT_init(FFTObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"n", "sign", "nthreads", NULL};
    unsigned long long n_raw = 0ULL;
    int sign = -1;
    int nthreads = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Kii", kwlist,
                                     &n_raw, &sign, &nthreads))
        return -1;
    size_t n = (size_t)n_raw;
    self->handle = fft_create(n, sign, nthreads);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "fft_create returned NULL");
        return -1;
    }
    self->_execute_cf64_buf = malloc(
        fft_execute_cf64_max_out(self->handle) * sizeof(double complex));
    if (!self->_execute_cf64_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_cf32_buf = malloc(
        fft_execute_cf32_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_cf32_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_inplace_cf64_buf = malloc(
        fft_execute_inplace_cf64_max_out(self->handle) * sizeof(double complex));
    if (!self->_execute_inplace_cf64_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_inplace_cf32_buf = malloc(
        fft_execute_inplace_cf32_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_inplace_cf32_buf) { PyErr_NoMemory(); return -1; }
    return 0;
}

static PyObject *
FFT_reset(FFTObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    fft_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
FFT_execute_cf64(FFTObject *self, PyObject *args)
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
    size_t n_out = fft_execute_cf64(self->handle, (const double complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_cf64_buf);
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
FFT_execute_cf32(FFTObject *self, PyObject *args)
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
    size_t n_out = fft_execute_cf32(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_cf32_buf);
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
FFT_execute_inplace_cf64(FFTObject *self, PyObject *args)
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
    size_t n_out = fft_execute_inplace_cf64(self->handle, (const double complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_inplace_cf64_buf);
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
FFT_execute_inplace_cf32(FFTObject *self, PyObject *args)
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
    size_t n_out = fft_execute_inplace_cf32(self->handle, (const float complex *)PyArray_DATA(in_arr), (size_t)n, self->_execute_inplace_cf32_buf);
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
FFT_getprop_n(FFTObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->n);
}
static PyObject *
FFT_getprop_sign(FFTObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLong((long)self->handle->sign);
}

static PyGetSetDef FFT_getset[] = {
    { "n", (getter)FFT_getprop_n, NULL, NULL, NULL },
    { "sign", (getter)FFT_getprop_sign, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
FFT_destroy(FFTObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        fft_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
FFT_enter(FFTObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
FFT_exit(FFTObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        fft_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef FFT_methods[] = {
    {"reset",    (PyCFunction)FFT_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute_cf64", (PyCFunction)FFT_execute_cf64, METH_VARARGS,
     "execute_cf64(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT\n"
     "    >>> obj = FFT(1024, -1, 1)\n"
     "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"execute_cf32", (PyCFunction)FFT_execute_cf32, METH_VARARGS,
     "execute_cf32(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT\n"
     "    >>> obj = FFT(1024, -1, 1)\n"
     "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"execute_inplace_cf64", (PyCFunction)FFT_execute_inplace_cf64, METH_VARARGS,
     "execute_inplace_cf64(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT\n"
     "    >>> obj = FFT(1024, -1, 1)\n"
     "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"execute_inplace_cf32", (PyCFunction)FFT_execute_inplace_cf32, METH_VARARGS,
     "execute_inplace_cf32(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT\n"
     "    >>> obj = FFT(1024, -1, 1)\n"
     "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)FFT_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)FFT_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)FFT_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject FFTType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.FFT",
    .tp_basicsize = sizeof(FFTObject),
    .tp_dealloc   = (destructor)FFT_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "FFT type.",
    .tp_methods   = FFT_methods,
    .tp_getset    = FFT_getset,
    .tp_new       = FFT_new,
    .tp_init      = (initproc)FFT_init,
};
/* ======================================================== */
/* FFT2DObject — wraps fft2d_state_t *       */
/* ======================================================== */

#include "fft2d/fft2d_core.h"

typedef struct {
    PyObject_HEAD
    fft2d_state_t *handle;
    double complex *_execute_cf64_buf;  /* pre-allocated output for execute_cf64 */
    float complex *_execute_cf32_buf;  /* pre-allocated output for execute_cf32 */
    double complex *_execute_inplace_cf64_buf;  /* pre-allocated output for execute_inplace_cf64 */
    float complex *_execute_inplace_cf32_buf;  /* pre-allocated output for execute_inplace_cf32 */
} FFT2DObject;

static void
FFT2D_dealloc(FFT2DObject *self)
{
    if (self->handle)
        fft2d_destroy(self->handle);
    free(self->_execute_cf64_buf);
    free(self->_execute_cf32_buf);
    free(self->_execute_inplace_cf64_buf);
    free(self->_execute_inplace_cf32_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
FFT2D_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FFT2DObject *self = (FFT2DObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
FFT2D_init(FFT2DObject *self, PyObject *args, PyObject *kwds)
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
    self->_execute_cf64_buf = malloc(
        fft2d_execute_cf64_max_out(self->handle) * sizeof(double complex));
    if (!self->_execute_cf64_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_cf32_buf = malloc(
        fft2d_execute_cf32_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_cf32_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_inplace_cf64_buf = malloc(
        fft2d_execute_inplace_cf64_max_out(self->handle) * sizeof(double complex));
    if (!self->_execute_inplace_cf64_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_inplace_cf32_buf = malloc(
        fft2d_execute_inplace_cf32_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_inplace_cf32_buf) { PyErr_NoMemory(); return -1; }
    return 0;
}

static PyObject *
FFT2D_reset(FFT2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    fft2d_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
FFT2D_execute_cf64(FFT2DObject *self, PyObject *args)
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
FFT2D_execute_cf32(FFT2DObject *self, PyObject *args)
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
FFT2D_execute_inplace_cf64(FFT2DObject *self, PyObject *args)
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
FFT2D_execute_inplace_cf32(FFT2DObject *self, PyObject *args)
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
    { "ny", (getter)FFT2D_getprop_ny, NULL, NULL, NULL },
    { "nx", (getter)FFT2D_getprop_nx, NULL, NULL, NULL },
    { "sign", (getter)FFT2D_getprop_sign, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
FFT2D_destroy(FFT2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        fft2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
FFT2D_enter(FFT2DObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
FFT2D_exit(FFT2DObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        fft2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef FFT2D_methods[] = {
    {"reset",    (PyCFunction)FFT2D_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute_cf64", (PyCFunction)FFT2D_execute_cf64, METH_VARARGS,
     "execute_cf64(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"execute_cf32", (PyCFunction)FFT2D_execute_cf32, METH_VARARGS,
     "execute_cf32(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"execute_inplace_cf64", (PyCFunction)FFT2D_execute_inplace_cf64, METH_VARARGS,
     "execute_inplace_cf64(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"execute_inplace_cf32", (PyCFunction)FFT2D_execute_inplace_cf32, METH_VARARGS,
     "execute_inplace_cf32(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FFT2D\n"
     "    >>> obj = FFT2D(64, 64, -1, 1)\n"
     "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)FFT2D_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)FFT2D_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)FFT2D_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject FFT2DType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.FFT2D",
    .tp_basicsize = sizeof(FFT2DObject),
    .tp_dealloc   = (destructor)FFT2D_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "FFT2D type.",
    .tp_methods   = FFT2D_methods,
    .tp_getset    = FFT2D_getset,
    .tp_new       = FFT2D_new,
    .tp_init      = (initproc)FFT2D_init,
};
/* ======================================================== */
/* CorrObject — wraps corr_state_t *       */
/* ======================================================== */

#include "corr/corr_core.h"

typedef struct {
    PyObject_HEAD
    corr_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
} CorrObject;

static void
Corr_dealloc(CorrObject *self)
{
    if (self->handle)
        corr_destroy(self->handle);
    free(self->_execute_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Corr_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CorrObject *self = (CorrObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Corr_init(CorrObject *self, PyObject *args, PyObject *kwds)
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
    self->_execute_buf = malloc(
        corr_execute_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_buf) { PyErr_NoMemory(); return -1; }
    return 0;
}

static PyObject *
Corr_reset(CorrObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    corr_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Corr_execute(CorrObject *self, PyObject *args)
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
    { "n", (getter)Corr_getprop_n, NULL, NULL, NULL },
    { "dwell", (getter)Corr_getprop_dwell, NULL, NULL, NULL },
    { "count", (getter)Corr_getprop_count, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
Corr_destroy(CorrObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        corr_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Corr_enter(CorrObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Corr_exit(CorrObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        corr_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Corr_methods[] = {
    {"reset",    (PyCFunction)Corr_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)Corr_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Corr\n"
     "    >>> obj = Corr(np.zeros(1, dtype=np.complex64), 1, 1)\n"
     "    >>> y = obj.execute(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)Corr_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Corr_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Corr_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject CorrType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Corr",
    .tp_basicsize = sizeof(CorrObject),
    .tp_dealloc   = (destructor)Corr_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Corr type.",
    .tp_methods   = Corr_methods,
    .tp_getset    = Corr_getset,
    .tp_new       = Corr_new,
    .tp_init      = (initproc)Corr_init,
};
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
Corr2D_dealloc(Corr2DObject *self)
{
    if (self->handle)
        corr2d_destroy(self->handle);
    free(self->_execute_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Corr2D_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Corr2DObject *self = (Corr2DObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Corr2D_init(Corr2DObject *self, PyObject *args, PyObject *kwds)
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
    self->_execute_buf = malloc(
        corr2d_execute_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_buf) { PyErr_NoMemory(); return -1; }
    return 0;
}

static PyObject *
Corr2D_reset(Corr2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    corr2d_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Corr2D_execute(Corr2DObject *self, PyObject *args)
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
Corr2D_destroy(Corr2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        corr2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Corr2D_enter(Corr2DObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Corr2D_exit(Corr2DObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        corr2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Corr2D_methods[] = {
    {"reset",    (PyCFunction)Corr2D_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)Corr2D_execute, METH_VARARGS,
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
    {"destroy",  (PyCFunction)Corr2D_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Corr2D_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Corr2D_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject Corr2DType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Corr2D",
    .tp_basicsize = sizeof(Corr2DObject),
    .tp_dealloc   = (destructor)Corr2D_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Corr2D type.",
    .tp_methods   = Corr2D_methods,
    .tp_getset    = Corr2D_getset,
    .tp_new       = Corr2D_new,
    .tp_init      = (initproc)Corr2D_init,
};
/* ======================================================== */
/* DetectorObject — wraps detector_state_t *       */
/* ======================================================== */

#include "detector/detector_core.h"

typedef struct {
    PyObject_HEAD
    detector_state_t *handle;
} DetectorObject;

static void
Detector_dealloc(DetectorObject *self)
{
    if (self->handle)
        detector_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Detector_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DetectorObject *self = (DetectorObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Detector_init(DetectorObject *self, PyObject *args, PyObject *kwds)
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
Detector_reset(DetectorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    detector_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Detector_push(DetectorObject *self, PyObject *args)
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
Detector_destroy(DetectorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        detector_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Detector_enter(DetectorObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Detector_exit(DetectorObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        detector_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Detector_methods[] = {
    {"reset",    (PyCFunction)Detector_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"push", (PyCFunction)Detector_push, METH_VARARGS,
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
    {"destroy",  (PyCFunction)Detector_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Detector_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Detector_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject DetectorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Detector",
    .tp_basicsize = sizeof(DetectorObject),
    .tp_dealloc   = (destructor)Detector_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Detector type.",
    .tp_methods   = Detector_methods,
    .tp_getset    = Detector_getset,
    .tp_new       = Detector_new,
    .tp_init      = (initproc)Detector_init,
};
/* ======================================================== */
/* Detector2DObject — wraps detector2d_state_t *       */
/* ======================================================== */

#include "detector2d/detector2d_core.h"

typedef struct {
    PyObject_HEAD
    detector2d_state_t *handle;
} Detector2DObject;

static void
Detector2D_dealloc(Detector2DObject *self)
{
    if (self->handle)
        detector2d_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Detector2D_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Detector2DObject *self = (Detector2DObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Detector2D_init(Detector2DObject *self, PyObject *args, PyObject *kwds)
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
    if (PyArray_NDIM(ref_arr) != 2) {
        PyErr_SetString(PyExc_ValueError,
                        "ref must be a 2-D array");
         Py_DECREF(ref_arr); return -1;
    }
    size_t ref_dim0 = (size_t)PyArray_DIM(ref_arr, 0);
    size_t ref_dim1 = (size_t)PyArray_DIM(ref_arr, 1);
    self->handle = detector2d_create((const float complex *)PyArray_DATA(ref_arr), ref_dim0, ref_dim1, dwell, noise_lo, noise_hi, noise_mode, threshold, nthreads);
    Py_DECREF(ref_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "detector2d_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
Detector2D_reset(Detector2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    detector2d_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Detector2D_push(Detector2DObject *self, PyObject *args)
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
    det_result2d_t results[64];
    size_t n_out = detector2d_push(self->handle,
        (const float complex *)PyArray_DATA(in_arr), n_in,
        results, 64);
    Py_DECREF(in_arr);
    PyObject *lst = PyList_New((Py_ssize_t)n_out);
    if (!lst) return NULL;
    for (size_t i = 0; i < n_out; i++) {
        PyObject *tup = Py_BuildValue("(KKfff)", (unsigned long long)results[i].row, (unsigned long long)results[i].col, results[i].peak_mag, results[i].noise_est, results[i].test_stat);
        if (!tup) { Py_DECREF(lst); return NULL; }
        PyList_SET_ITEM(lst, (Py_ssize_t)i, tup);
    }
    return lst;
}
static PyObject *
Detector2D_getprop_ny(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->ny);
}
static PyObject *
Detector2D_getprop_nx(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->nx);
}
static PyObject *
Detector2D_getprop_n(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->n);
}
static PyObject *
Detector2D_getprop_dwell(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->corr->dwell);
}
static PyObject *
Detector2D_getprop_count(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->corr->count);
}
static PyObject *
Detector2D_getprop_ring_cap(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->ring_cap);
}
static PyObject *
Detector2D_getprop_noise_lo(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->noise_lo);
}
static PyObject *
Detector2D_getprop_noise_hi(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->noise_hi);
}
static PyObject *
Detector2D_getprop_threshold(Detector2DObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble((double)self->handle->threshold);
}
static PyObject *
Detector2D_getprop_last_corr(Detector2DObject *self, void *Py_UNUSED(closure))
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

static PyGetSetDef Detector2D_getset[] = {
    { "ny", (getter)Detector2D_getprop_ny, NULL, NULL, NULL },
    { "nx", (getter)Detector2D_getprop_nx, NULL, NULL, NULL },
    { "n", (getter)Detector2D_getprop_n, NULL, NULL, NULL },
    { "dwell", (getter)Detector2D_getprop_dwell, NULL, NULL, NULL },
    { "count", (getter)Detector2D_getprop_count, NULL, NULL, NULL },
    { "ring_cap", (getter)Detector2D_getprop_ring_cap, NULL, NULL, NULL },
    { "noise_lo", (getter)Detector2D_getprop_noise_lo, NULL, NULL, NULL },
    { "noise_hi", (getter)Detector2D_getprop_noise_hi, NULL, NULL, NULL },
    { "threshold", (getter)Detector2D_getprop_threshold, NULL, NULL, NULL },
    { "last_corr", (getter)Detector2D_getprop_last_corr, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
Detector2D_destroy(Detector2DObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        detector2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Detector2D_enter(Detector2DObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Detector2D_exit(Detector2DObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        detector2d_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Detector2D_methods[] = {
    {"reset",    (PyCFunction)Detector2D_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"push", (PyCFunction)Detector2D_push, METH_VARARGS,
     "push(x) -> list[tuple]\n"
     "\n"
     "Returns list of (row, col, peak_mag, noise_est, test_stat,) tuples.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Detector2D\n"
     "    >>> obj = Detector2D(np.zeros((1, 1), dtype=np.complex64), \"mean\", 1, 0, ny*nx-1, 0.0, 1)\n"
     "    >>> results = obj.push(np.zeros(4, dtype=np.complex64))\n"
     "    >>> isinstance(results, list)\n"
     "    True\n"},
    {"destroy",  (PyCFunction)Detector2D_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Detector2D_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Detector2D_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject Detector2DType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.Detector2D",
    .tp_basicsize = sizeof(Detector2DObject),
    .tp_dealloc   = (destructor)Detector2D_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Detector2D type.",
    .tp_methods   = Detector2D_methods,
    .tp_getset    = Detector2D_getset,
    .tp_new       = Detector2D_new,
    .tp_init      = (initproc)Detector2D_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Spectral_methods[] = {
    {"kaiser_enbw", _bind_kaiser_enbw, METH_VARARGS, "kaiser_enbw."},
    {"kaiser_window", _bind_kaiser_window, METH_VARARGS, "kaiser_window."},
    {"hann_window", _bind_hann_window, METH_VARARGS, "hann_window."},
    {"magnitude_db_cf32", _bind_magnitude_db_cf32, METH_VARARGS, "magnitude_db_cf32."},
    {"magnitude_db_cf64", _bind_magnitude_db_cf64, METH_VARARGS, "magnitude_db_cf64."},
    {"find_peaks_f32", _bind_find_peaks_f32, METH_VARARGS, "find_peaks_f32."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef spectral_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "spectral",
    .m_doc     = "Spectral module.",
    .m_size    = -1,
    .m_methods = Spectral_methods,
};

PyMODINIT_FUNC
PyInit_spectral(void)
{
    import_array();
    if (PyType_Ready(&FFTType) < 0) return NULL;
    if (PyType_Ready(&FFT2DType) < 0) return NULL;
    if (PyType_Ready(&CorrType) < 0) return NULL;
    if (PyType_Ready(&Corr2DType) < 0) return NULL;
    if (PyType_Ready(&DetectorType) < 0) return NULL;
    if (PyType_Ready(&Detector2DType) < 0) return NULL;
    PyObject *m = PyModule_Create(&spectral_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FFTType);
    if (PyModule_AddObject(m, "FFT", (PyObject *)&FFTType) < 0) {
        Py_DECREF(&FFTType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&FFT2DType);
    if (PyModule_AddObject(m, "FFT2D", (PyObject *)&FFT2DType) < 0) {
        Py_DECREF(&FFT2DType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CorrType);
    if (PyModule_AddObject(m, "Corr", (PyObject *)&CorrType) < 0) {
        Py_DECREF(&CorrType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&Corr2DType);
    if (PyModule_AddObject(m, "Corr2D", (PyObject *)&Corr2DType) < 0) {
        Py_DECREF(&Corr2DType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&DetectorType);
    if (PyModule_AddObject(m, "Detector", (PyObject *)&DetectorType) < 0) {
        Py_DECREF(&DetectorType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&Detector2DType);
    if (PyModule_AddObject(m, "Detector2D", (PyObject *)&Detector2DType) < 0) {
        Py_DECREF(&Detector2DType); Py_DECREF(m); return NULL;
    }
    return m;
}
