/*
 * spectral_ext_psd.c — PSD type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* PSDObject — wraps psd_state_t *       */
/* ======================================================== */

#include "psd/psd_core.h"

typedef struct {
    PyObject_HEAD
    psd_state_t *handle;
    float *_psd_db_buf;  /* pre-allocated output for psd_db */
    size_t _psd_db_buf_cap;  /* allocated capacity for psd_db */
    void **_psd_db_retired;  /* gh-219 deferred free */
    size_t _psd_db_retired_n;
    size_t _psd_db_retired_cap;
    float *_psd_dbhz_buf;  /* pre-allocated output for psd_dbhz */
    size_t _psd_dbhz_buf_cap;  /* allocated capacity for psd_dbhz */
    void **_psd_dbhz_retired;  /* gh-219 deferred free */
    size_t _psd_dbhz_retired_n;
    size_t _psd_dbhz_retired_cap;
    float *_power_twosided_buf;  /* pre-allocated output for power_twosided */
    size_t _power_twosided_buf_cap;  /* allocated capacity for power_twosided */
    void **_power_twosided_retired;  /* gh-219 deferred free */
    size_t _power_twosided_retired_n;
    size_t _power_twosided_retired_cap;
    float *_power_onesided_buf;  /* pre-allocated output for power_onesided */
    size_t _power_onesided_buf_cap;  /* allocated capacity for power_onesided */
    void **_power_onesided_retired;  /* gh-219 deferred free */
    size_t _power_onesided_retired_n;
    size_t _power_onesided_retired_cap;
    float *_band_power_buf;  /* pre-allocated output for band_power */
    size_t _band_power_buf_cap;  /* allocated capacity for band_power */
    void **_band_power_retired;  /* gh-219 deferred free */
    size_t _band_power_retired_n;
    size_t _band_power_retired_cap;
} PSDObject;

static void
PSDObj_dealloc(PSDObject *self)
{
    if (self->handle)
        psd_destroy(self->handle);
    free(self->_psd_db_buf);
    for (size_t _i = 0; _i < self->_psd_db_retired_n; _i++)
        free(self->_psd_db_retired[_i]);
    free(self->_psd_db_retired);
    free(self->_psd_dbhz_buf);
    for (size_t _i = 0; _i < self->_psd_dbhz_retired_n; _i++)
        free(self->_psd_dbhz_retired[_i]);
    free(self->_psd_dbhz_retired);
    free(self->_power_twosided_buf);
    for (size_t _i = 0; _i < self->_power_twosided_retired_n; _i++)
        free(self->_power_twosided_retired[_i]);
    free(self->_power_twosided_retired);
    free(self->_power_onesided_buf);
    for (size_t _i = 0; _i < self->_power_onesided_retired_n; _i++)
        free(self->_power_onesided_retired[_i]);
    free(self->_power_onesided_retired);
    free(self->_band_power_buf);
    for (size_t _i = 0; _i < self->_band_power_retired_n; _i++)
        free(self->_band_power_retired[_i]);
    free(self->_band_power_retired);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
PSDObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PSDObject *self = (PSDObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
PSDObj_init(PSDObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"window", "mode", "n", "fs", "beta", "pad", "full_scale", "alpha", NULL};
    const char *window_str = "hann";
    const char *mode_str = "mean";
    unsigned long long n_raw = 1024;
    double fs = 1.0;
    float beta = 0.0f;
    unsigned long long pad_raw = 1;
    double full_scale = 1.0;
    double alpha = 0.1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ssKdfKdd", kwlist,
                                     &window_str, &mode_str, &n_raw, &fs, &beta, &pad_raw, &full_scale, &alpha))
        return -1;
    int window = 0;
    if (strcmp(window_str, "hann") == 0) window = 0;
    else if (strcmp(window_str, "kaiser") == 0) window = 1;
    else {
        PyErr_Format(PyExc_ValueError, "window must be one of \"hann\", \"kaiser\", got '%s'", window_str);
        return -1;
    }
    int mode = 0;
    if (strcmp(mode_str, "mean") == 0) mode = 0;
    else if (strcmp(mode_str, "exp") == 0) mode = 1;
    else if (strcmp(mode_str, "maxhold") == 0) mode = 2;
    else if (strcmp(mode_str, "minhold") == 0) mode = 3;
    else {
        PyErr_Format(PyExc_ValueError, "mode must be one of \"mean\", \"exp\", \"maxhold\", \"minhold\", got '%s'", mode_str);
        return -1;
    }
    size_t n = (size_t)n_raw;
    size_t pad = (size_t)pad_raw;
    self->handle = psd_create(n, fs, window, beta, pad, full_scale, mode, alpha);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "psd_create returned NULL");
        return -1;
    }
    {
        size_t _max = psd_psd_db_max_out(self->handle);
        if (_max) {
        self->_psd_db_buf = malloc(_max * sizeof(float));
        if (!self->_psd_db_buf) { PyErr_NoMemory(); return -1; }
            self->_psd_db_buf_cap = _max;
        }
    }
    {
        size_t _max = psd_psd_dbhz_max_out(self->handle);
        if (_max) {
        self->_psd_dbhz_buf = malloc(_max * sizeof(float));
        if (!self->_psd_dbhz_buf) { PyErr_NoMemory(); return -1; }
            self->_psd_dbhz_buf_cap = _max;
        }
    }
    {
        size_t _max = psd_power_twosided_max_out(self->handle);
        if (_max) {
        self->_power_twosided_buf = malloc(_max * sizeof(float));
        if (!self->_power_twosided_buf) { PyErr_NoMemory(); return -1; }
            self->_power_twosided_buf_cap = _max;
        }
    }
    {
        size_t _max = psd_power_onesided_max_out(self->handle);
        if (_max) {
        self->_power_onesided_buf = malloc(_max * sizeof(float));
        if (!self->_power_onesided_buf) { PyErr_NoMemory(); return -1; }
            self->_power_onesided_buf_cap = _max;
        }
    }
    {
        size_t _max = psd_band_power_max_out(self->handle);
        if (_max) {
        self->_band_power_buf = malloc(_max * sizeof(float));
        if (!self->_band_power_buf) { PyErr_NoMemory(); return -1; }
            self->_band_power_buf_cap = _max;
        }
    }
    return 0;
}








static PyObject *
PSDObj_accumulate(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"x", NULL};
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &x_obj))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float complex *x = (const float complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    psd_accumulate(self->handle, x, x_len);
    Py_DECREF(x_arr);
    Py_RETURN_NONE;
}

static PyObject *
PSDObj_accumulate_real(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"x", NULL};
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &x_obj))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float *x = (const float *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    psd_accumulate_real(self->handle, x, x_len);
    Py_DECREF(x_arr);
    Py_RETURN_NONE;
}

static PyObject *
PSDObj_reset(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    psd_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
PSDObj_psd_db_max_out(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromSize_t(
        psd_psd_db_max_out(self->handle));
}

static PyObject *
PSDObj_psd_db(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"count", "out", NULL};
    Py_ssize_t n = 1;
    PyObject *out_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|nO",
            _kwlist, &n, &out_obj))
        return NULL;
    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_FLOAT,
            NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
        if (!out_arr) { return NULL; }
        size_t _cap = (size_t)PyArray_SIZE(out_arr);
        size_t _omax = psd_psd_db_max_out(self->handle);
        if (_cap < _omax) {
            PyErr_Format(PyExc_ValueError,
                "out has %zu elements, need >= %zu (psd_db_max_out)",
                _cap, _omax);
            Py_DECREF(out_arr); return NULL;
        }
        size_t n_out = psd_psd_db(self->handle, (size_t)n, (float *)PyArray_DATA(out_arr));
        if (!n_out) { Py_DECREF(out_arr); Py_RETURN_NONE; }
        npy_intp _odim = (npy_intp)n_out;
        PyObject *_oview = PyArray_SimpleNewFromData(
            1, &_odim, NPY_FLOAT, PyArray_DATA(out_arr));
        if (!_oview) { Py_DECREF(out_arr); return NULL; }
        PyArray_SetBaseObject((PyArrayObject *)_oview, (PyObject *)out_arr);
        return _oview;
    }
    size_t _need = (size_t)n;
    if (!self->_psd_db_buf || self->_psd_db_buf_cap < _need) {
        size_t _max = psd_psd_db_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        if (self->_psd_db_buf && self->_psd_db_retired_n == self->_psd_db_retired_cap) {
            size_t _rcap = self->_psd_db_retired_cap ? self->_psd_db_retired_cap * 2 : 4;
            void **_rt = realloc(self->_psd_db_retired, _rcap * sizeof(void *));
            if (!_rt) { PyErr_NoMemory(); return NULL; }
            self->_psd_db_retired = _rt;
            self->_psd_db_retired_cap = _rcap;
        }
        float *_tmp = malloc(_max * sizeof(float));
        if (!_tmp) { PyErr_NoMemory(); return NULL; }
        if (self->_psd_db_buf)
            self->_psd_db_retired[self->_psd_db_retired_n++] = self->_psd_db_buf;
        self->_psd_db_buf = _tmp;
        self->_psd_db_buf_cap = _max;
    }
    size_t n_out = psd_psd_db(self->handle, (size_t)n, self->_psd_db_buf);
    if (!n_out) Py_RETURN_NONE;
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_FLOAT, self->_psd_db_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
PSDObj_psd_dbhz_max_out(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromSize_t(
        psd_psd_dbhz_max_out(self->handle));
}

static PyObject *
PSDObj_psd_dbhz(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"count", "out", NULL};
    Py_ssize_t n = 1;
    PyObject *out_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|nO",
            _kwlist, &n, &out_obj))
        return NULL;
    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_FLOAT,
            NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
        if (!out_arr) { return NULL; }
        size_t _cap = (size_t)PyArray_SIZE(out_arr);
        size_t _omax = psd_psd_dbhz_max_out(self->handle);
        if (_cap < _omax) {
            PyErr_Format(PyExc_ValueError,
                "out has %zu elements, need >= %zu (psd_dbhz_max_out)",
                _cap, _omax);
            Py_DECREF(out_arr); return NULL;
        }
        size_t n_out = psd_psd_dbhz(self->handle, (size_t)n, (float *)PyArray_DATA(out_arr));
        if (!n_out) { Py_DECREF(out_arr); Py_RETURN_NONE; }
        npy_intp _odim = (npy_intp)n_out;
        PyObject *_oview = PyArray_SimpleNewFromData(
            1, &_odim, NPY_FLOAT, PyArray_DATA(out_arr));
        if (!_oview) { Py_DECREF(out_arr); return NULL; }
        PyArray_SetBaseObject((PyArrayObject *)_oview, (PyObject *)out_arr);
        return _oview;
    }
    size_t _need = (size_t)n;
    if (!self->_psd_dbhz_buf || self->_psd_dbhz_buf_cap < _need) {
        size_t _max = psd_psd_dbhz_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        if (self->_psd_dbhz_buf && self->_psd_dbhz_retired_n == self->_psd_dbhz_retired_cap) {
            size_t _rcap = self->_psd_dbhz_retired_cap ? self->_psd_dbhz_retired_cap * 2 : 4;
            void **_rt = realloc(self->_psd_dbhz_retired, _rcap * sizeof(void *));
            if (!_rt) { PyErr_NoMemory(); return NULL; }
            self->_psd_dbhz_retired = _rt;
            self->_psd_dbhz_retired_cap = _rcap;
        }
        float *_tmp = malloc(_max * sizeof(float));
        if (!_tmp) { PyErr_NoMemory(); return NULL; }
        if (self->_psd_dbhz_buf)
            self->_psd_dbhz_retired[self->_psd_dbhz_retired_n++] = self->_psd_dbhz_buf;
        self->_psd_dbhz_buf = _tmp;
        self->_psd_dbhz_buf_cap = _max;
    }
    size_t n_out = psd_psd_dbhz(self->handle, (size_t)n, self->_psd_dbhz_buf);
    if (!n_out) Py_RETURN_NONE;
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_FLOAT, self->_psd_dbhz_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
PSDObj_power_twosided_max_out(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromSize_t(
        psd_power_twosided_max_out(self->handle));
}

static PyObject *
PSDObj_power_twosided(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"count", "out", NULL};
    Py_ssize_t n = 1;
    PyObject *out_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|nO",
            _kwlist, &n, &out_obj))
        return NULL;
    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_FLOAT,
            NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
        if (!out_arr) { return NULL; }
        size_t _cap = (size_t)PyArray_SIZE(out_arr);
        size_t _omax = psd_power_twosided_max_out(self->handle);
        if (_cap < _omax) {
            PyErr_Format(PyExc_ValueError,
                "out has %zu elements, need >= %zu (power_twosided_max_out)",
                _cap, _omax);
            Py_DECREF(out_arr); return NULL;
        }
        size_t n_out = psd_power_twosided(self->handle, (size_t)n, (float *)PyArray_DATA(out_arr));
        if (!n_out) { Py_DECREF(out_arr); Py_RETURN_NONE; }
        npy_intp _odim = (npy_intp)n_out;
        PyObject *_oview = PyArray_SimpleNewFromData(
            1, &_odim, NPY_FLOAT, PyArray_DATA(out_arr));
        if (!_oview) { Py_DECREF(out_arr); return NULL; }
        PyArray_SetBaseObject((PyArrayObject *)_oview, (PyObject *)out_arr);
        return _oview;
    }
    size_t _need = (size_t)n;
    if (!self->_power_twosided_buf || self->_power_twosided_buf_cap < _need) {
        size_t _max = psd_power_twosided_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        if (self->_power_twosided_buf && self->_power_twosided_retired_n == self->_power_twosided_retired_cap) {
            size_t _rcap = self->_power_twosided_retired_cap ? self->_power_twosided_retired_cap * 2 : 4;
            void **_rt = realloc(self->_power_twosided_retired, _rcap * sizeof(void *));
            if (!_rt) { PyErr_NoMemory(); return NULL; }
            self->_power_twosided_retired = _rt;
            self->_power_twosided_retired_cap = _rcap;
        }
        float *_tmp = malloc(_max * sizeof(float));
        if (!_tmp) { PyErr_NoMemory(); return NULL; }
        if (self->_power_twosided_buf)
            self->_power_twosided_retired[self->_power_twosided_retired_n++] = self->_power_twosided_buf;
        self->_power_twosided_buf = _tmp;
        self->_power_twosided_buf_cap = _max;
    }
    size_t n_out = psd_power_twosided(self->handle, (size_t)n, self->_power_twosided_buf);
    if (!n_out) Py_RETURN_NONE;
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_FLOAT, self->_power_twosided_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
PSDObj_power_onesided_max_out(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromSize_t(
        psd_power_onesided_max_out(self->handle));
}

static PyObject *
PSDObj_power_onesided(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"count", "out", NULL};
    Py_ssize_t n = 1;
    PyObject *out_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|nO",
            _kwlist, &n, &out_obj))
        return NULL;
    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_FLOAT,
            NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
        if (!out_arr) { return NULL; }
        size_t _cap = (size_t)PyArray_SIZE(out_arr);
        size_t _omax = psd_power_onesided_max_out(self->handle);
        if (_cap < _omax) {
            PyErr_Format(PyExc_ValueError,
                "out has %zu elements, need >= %zu (power_onesided_max_out)",
                _cap, _omax);
            Py_DECREF(out_arr); return NULL;
        }
        size_t n_out = psd_power_onesided(self->handle, (size_t)n, (float *)PyArray_DATA(out_arr));
        if (!n_out) { Py_DECREF(out_arr); Py_RETURN_NONE; }
        npy_intp _odim = (npy_intp)n_out;
        PyObject *_oview = PyArray_SimpleNewFromData(
            1, &_odim, NPY_FLOAT, PyArray_DATA(out_arr));
        if (!_oview) { Py_DECREF(out_arr); return NULL; }
        PyArray_SetBaseObject((PyArrayObject *)_oview, (PyObject *)out_arr);
        return _oview;
    }
    size_t _need = (size_t)n;
    if (!self->_power_onesided_buf || self->_power_onesided_buf_cap < _need) {
        size_t _max = psd_power_onesided_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        if (self->_power_onesided_buf && self->_power_onesided_retired_n == self->_power_onesided_retired_cap) {
            size_t _rcap = self->_power_onesided_retired_cap ? self->_power_onesided_retired_cap * 2 : 4;
            void **_rt = realloc(self->_power_onesided_retired, _rcap * sizeof(void *));
            if (!_rt) { PyErr_NoMemory(); return NULL; }
            self->_power_onesided_retired = _rt;
            self->_power_onesided_retired_cap = _rcap;
        }
        float *_tmp = malloc(_max * sizeof(float));
        if (!_tmp) { PyErr_NoMemory(); return NULL; }
        if (self->_power_onesided_buf)
            self->_power_onesided_retired[self->_power_onesided_retired_n++] = self->_power_onesided_buf;
        self->_power_onesided_buf = _tmp;
        self->_power_onesided_buf_cap = _max;
    }
    size_t n_out = psd_power_onesided(self->handle, (size_t)n, self->_power_onesided_buf);
    if (!n_out) Py_RETURN_NONE;
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_FLOAT, self->_power_onesided_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
PSDObj_band_power(PSDObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *bands_obj = NULL;
    PyArrayObject *bands_arr = NULL;
    if (!PyArg_ParseTuple(args, "O", &bands_obj))
        return NULL;
    bands_arr = (PyArrayObject *)PyArray_FROM_OTF(
        bands_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
    if (!bands_arr) return NULL;
    size_t _need = (size_t)PyArray_SIZE(bands_arr);
    if (!self->_band_power_buf || self->_band_power_buf_cap < _need) {
        size_t _max = psd_band_power_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        if (self->_band_power_buf && self->_band_power_retired_n == self->_band_power_retired_cap) {
            size_t _rcap = self->_band_power_retired_cap ? self->_band_power_retired_cap * 2 : 4;
            void **_rt = realloc(self->_band_power_retired, _rcap * sizeof(void *));
            if (!_rt) { Py_DECREF(bands_arr); PyErr_NoMemory(); return NULL; }
            self->_band_power_retired = _rt;
            self->_band_power_retired_cap = _rcap;
        }
        float *_tmp = malloc(_max * sizeof(float));
        if (!_tmp) { Py_DECREF(bands_arr); PyErr_NoMemory(); return NULL; }
        if (self->_band_power_buf)
            self->_band_power_retired[self->_band_power_retired_n++] = self->_band_power_buf;
        self->_band_power_buf = _tmp;
        self->_band_power_buf_cap = _max;
    }
    size_t n_out = psd_band_power(self->handle, (const double *)PyArray_DATA(bands_arr), (size_t)PyArray_SIZE(bands_arr), self->_band_power_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_FLOAT, self->_band_power_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(bands_arr);
    return arr;
}

static PyObject *
PSDObj_total_band_power(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"bands", NULL};
    PyObject *bands_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &bands_obj))
        return NULL;
    PyArrayObject *bands_arr = (PyArrayObject *)PyArray_FROM_OTF(
        bands_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
    if (!bands_arr) { return NULL; }
    const double *bands = (const double *)PyArray_DATA(bands_arr);
    size_t bands_len = (size_t)PyArray_SIZE(bands_arr);
    double y = psd_total_band_power(self->handle, bands, bands_len);
    Py_DECREF(bands_arr);
    return PyFloat_FromDouble(y);
}

static PyObject *
PSDObj_occupied_bw(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"fraction", NULL};
    double fraction = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "d",
            _kwlist, &fraction))
        return NULL;
    double y = psd_occupied_bw(self->handle, fraction);
    return PyFloat_FromDouble(y);
}

static PyObject *
PSDObj_noise_floor(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    double y = psd_noise_floor(self->handle);
    return PyFloat_FromDouble(y);
}

static PyObject *
PSDObj_snr(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"lo_hz", "hi_hz", NULL};
    double lo_hz = 0.0;
    double hi_hz = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dd",
            _kwlist, &lo_hz, &hi_hz))
        return NULL;
    double y = psd_snr(self->handle, lo_hz, hi_hz);
    return PyFloat_FromDouble(y);
}

static PyObject *
PSDObj_sfdr(PSDObject *self, PyObject *args, PyObject *kwds)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    static char *_kwlist[] = {"min_db", NULL};
    float min_db = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "f",
            _kwlist, &min_db))
        return NULL;
    double y = psd_sfdr(self->handle, min_db);
    return PyFloat_FromDouble(y);
}
static PyObject *
PSD_getprop_n(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->n);
}
static PyObject *
PSD_getprop_nfft(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->nfft);
}
static PyObject *
PSD_getprop_fs(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->fs);
}
static PyObject *
PSD_getprop_enbw(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->enbw);
}
static PyObject *
PSD_getprop_rbw(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->enbw * self->handle->fs / (double)self->handle->n);
}
static PyObject *
PSD_getprop_count(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)(size_t)self->handle->avg->count);
}
static PyObject *
PSD_getprop_mode(PSDObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLong((long)(int)self->handle->avg->mode);
}

static PyGetSetDef PSD_getset[] = {
    { "n", (getter)PSD_getprop_n, NULL, "N.\n", NULL },
    { "nfft", (getter)PSD_getprop_nfft, NULL, "Nfft.\n", NULL },
    { "fs", (getter)PSD_getprop_fs, NULL, "Fs.\n", NULL },
    { "enbw", (getter)PSD_getprop_enbw, NULL, "Enbw.\n", NULL },
    { "rbw", (getter)PSD_getprop_rbw, NULL, "Rbw.\n", NULL },
    { "count", (getter)PSD_getprop_count, NULL, "Count.\n", NULL },
    { "mode", (getter)PSD_getprop_mode, NULL, "Mode.\n", NULL },
    { NULL }
};

static PyObject *
PSDObj_destroy(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        psd_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
PSDObj_enter(PSDObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
PSDObj_exit(PSDObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        psd_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef PSDObj_methods[] = {

    {"accumulate", (PyCFunction)(void *)PSDObj_accumulate, METH_VARARGS | METH_KEYWORDS,
     "accumulate(x) -> None\n"
     "\n"
     "Window, FFT and fold floor(n_in/n) cf32 frames into the average.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.accumulate(np.zeros(4, dtype=np.complex64))\n"},
    {"accumulate_real", (PyCFunction)(void *)PSDObj_accumulate_real, METH_VARARGS | METH_KEYWORDS,
     "accumulate_real(x) -> None\n"
     "\n"
     "Window, zero-pad, FFT and fold floor(n_in/n) real frames into the average.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.accumulate_real(np.zeros(4, dtype=np.float32))\n"},
    {"reset", (PyCFunction)PSDObj_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "Discard the running average; counters return to zero.\n"
     "\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.reset()\n"},
    {"psd_db", (PyCFunction)PSDObj_psd_db, METH_VARARGS | METH_KEYWORDS,
     "psd_db(n=1) -> ndarray\n"
     "\n"
     "Averaged power spectrum in dB (None before any accumulate).\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> y = obj.psd_db(4)\n"
     "    >>> y.dtype\n"
     "    dtype('float32')\n"},
    {"psd_db_max_out", (PyCFunction)PSDObj_psd_db_max_out,
     METH_NOARGS, "psd_db_max_out() -> int\n\nMax output length psd_db() can produce for the current state.\nUse to size the ``out=`` buffer."},
    {"psd_dbhz", (PyCFunction)PSDObj_psd_dbhz, METH_VARARGS | METH_KEYWORDS,
     "psd_dbhz(n=1) -> ndarray\n"
     "\n"
     "Averaged power spectral density in dB/Hz (None before any accumulate).\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> y = obj.psd_dbhz(4)\n"
     "    >>> y.dtype\n"
     "    dtype('float32')\n"},
    {"psd_dbhz_max_out", (PyCFunction)PSDObj_psd_dbhz_max_out,
     METH_NOARGS, "psd_dbhz_max_out() -> int\n\nMax output length psd_dbhz() can produce for the current state.\nUse to size the ``out=`` buffer."},
    {"power_twosided", (PyCFunction)PSDObj_power_twosided, METH_VARARGS | METH_KEYWORDS,
     "power_twosided(n=1) -> ndarray\n"
     "\n"
     "Averaged linear power, DC-centred two-sided (length nfft); cg^2-normalised.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> y = obj.power_twosided(4)\n"
     "    >>> y.dtype\n"
     "    dtype('float32')\n"},
    {"power_twosided_max_out", (PyCFunction)PSDObj_power_twosided_max_out,
     METH_NOARGS, "power_twosided_max_out() -> int\n\nMax output length power_twosided() can produce for the current state.\nUse to size the ``out=`` buffer."},
    {"power_onesided", (PyCFunction)PSDObj_power_onesided, METH_VARARGS | METH_KEYWORDS,
     "power_onesided(n=1) -> ndarray\n"
     "\n"
     "Averaged linear power, one-sided fold (length nfft/2+1); cg^2-normalised.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> y = obj.power_onesided(4)\n"
     "    >>> y.dtype\n"
     "    dtype('float32')\n"},
    {"power_onesided_max_out", (PyCFunction)PSDObj_power_onesided_max_out,
     METH_NOARGS, "power_onesided_max_out() -> int\n\nMax output length power_onesided() can produce for the current state.\nUse to size the ``out=`` buffer."},
    {"band_power", (PyCFunction)PSDObj_band_power, METH_VARARGS,
     "band_power(bands) -> ndarray\n"
     "\n"
     "Integrated power per band in dB; bands = [lo0,hi0,lo1,hi1,...] Hz.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> y = obj.band_power(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('float32')\n"},
    {"total_band_power", (PyCFunction)(void *)PSDObj_total_band_power, METH_VARARGS | METH_KEYWORDS,
     "total_band_power(bands) -> float\n"
     "\n"
     "Total integrated power across all bands in dB.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.total_band_power(np.zeros(4, dtype=np.float64))\n"
     "    0.0\n"},
    {"occupied_bw", (PyCFunction)(void *)PSDObj_occupied_bw, METH_VARARGS | METH_KEYWORDS,
     "occupied_bw(fraction) -> float\n"
     "\n"
     "Occupied bandwidth in Hz holding the given fraction of total power.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.occupied_bw(0.0)\n"
     "    0.0\n"},
    {"noise_floor", (PyCFunction)PSDObj_noise_floor, METH_NOARGS,
     "noise_floor() -> float\n"
     "\n"
     "Median of the averaged dB trace (noise-floor estimate).\n"
     "\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.noise_floor()\n"
     "    0.0\n"},
    {"snr", (PyCFunction)(void *)PSDObj_snr, METH_VARARGS | METH_KEYWORDS,
     "snr(lo_hz, hi_hz) -> float\n"
     "\n"
     "Peak-in-band level minus noise floor, in dB.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.snr(0.0, 0.0)\n"
     "    0.0\n"},
    {"sfdr", (PyCFunction)(void *)PSDObj_sfdr, METH_VARARGS | METH_KEYWORDS,
     "sfdr(min_db) -> float\n"
     "\n"
     "Spurious-free dynamic range in dB from the top two peaks.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PSD\n"
     "    >>> obj = PSD(\"hann\", \"mean\", 1024, 1.0, 0.0, 1, 1.0, 0.1)\n"
     "    >>> obj.sfdr(0.0)\n"
     "    0.0\n"},
    {"destroy",  (PyCFunction)PSDObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)PSDObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)PSDObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject PSDObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "spectral.PSD",
    .tp_basicsize = sizeof(PSDObject),
    .tp_dealloc   = (destructor)PSDObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create an averaging PSD estimator.\n",
    .tp_methods   = PSDObj_methods,
    .tp_getset    = PSD_getset,
    .tp_new       = PSDObj_new,
    .tp_init      = (initproc)PSDObj_init,
};
