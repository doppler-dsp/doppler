/*
 * snr_ext.c — Python extension module snr
 *
 * Objects: 
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "snr/snr_core.h"



static PyObject *
_bind_snr_data_aided_db(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"soft", "sign_bits", NULL};
    PyObject *soft_obj = NULL;
    PyObject *sign_bits_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &soft_obj, &sign_bits_obj))
        return NULL;
    PyArrayObject *soft_arr = (PyArrayObject *)PyArray_FROM_OTF(
        soft_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!soft_arr) { return NULL; }
    const float complex *soft = (const float complex *)PyArray_DATA(soft_arr);
    size_t soft_len = (size_t)PyArray_SIZE(soft_arr);
    PyArrayObject *sign_bits_arr = (PyArrayObject *)PyArray_FROM_OTF(
        sign_bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!sign_bits_arr) { Py_DECREF(soft_arr); return NULL; }
    const uint8_t *sign_bits = (const uint8_t *)PyArray_DATA(sign_bits_arr);
    size_t sign_bits_len = (size_t)PyArray_SIZE(sign_bits_arr);
    Py_DECREF(soft_arr);
    Py_DECREF(sign_bits_arr);
    return PyFloat_FromDouble(snr_data_aided_db(soft, soft_len, sign_bits, sign_bits_len));
}

static PyObject *
_bind_snr_m2m4_db(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
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
    Py_DECREF(x_arr);
    return PyFloat_FromDouble(snr_m2m4_db(x, x_len));
}

static PyObject *
_bind_snr_data_aided_db_series(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"soft", "sign_bits", "window", NULL};
    PyObject *soft_obj = NULL;
    PyObject *sign_bits_obj = NULL;
    unsigned long long window_raw = 0ULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOK",
            _kwlist, &soft_obj, &sign_bits_obj, &window_raw))
        return NULL;
    size_t window = (size_t)window_raw;
    PyArrayObject *soft_arr = (PyArrayObject *)PyArray_FROM_OTF(
        soft_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!soft_arr) { return NULL; }
    const float complex *soft = (const float complex *)PyArray_DATA(soft_arr);
    size_t soft_len = (size_t)PyArray_SIZE(soft_arr);
    PyArrayObject *sign_bits_arr = (PyArrayObject *)PyArray_FROM_OTF(
        sign_bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!sign_bits_arr) { Py_DECREF(soft_arr); return NULL; }
    const uint8_t *sign_bits = (const uint8_t *)PyArray_DATA(sign_bits_arr);
    size_t sign_bits_len = (size_t)PyArray_SIZE(sign_bits_arr);
    npy_intp _dim = (npy_intp)(soft_len);
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_DOUBLE, 0);
    if (!_out) {Py_DECREF(soft_arr); Py_DECREF(sign_bits_arr); return NULL; }
    snr_data_aided_db_series(soft, soft_len, sign_bits, sign_bits_len, window, (double *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(soft_arr);
    Py_DECREF(sign_bits_arr);
    return _out;
}

static PyObject *
_bind_snr_m2m4_db_series(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"x", "window", NULL};
    PyObject *x_obj = NULL;
    unsigned long long window_raw = 0ULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OK",
            _kwlist, &x_obj, &window_raw))
        return NULL;
    size_t window = (size_t)window_raw;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float complex *x = (const float complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    npy_intp _dim = (npy_intp)(x_len);
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_DOUBLE, 0);
    if (!_out) {Py_DECREF(x_arr); return NULL; }
    snr_m2m4_db_series(x, x_len, window, (double *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(x_arr);
    return _out;
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef snr_module_methods[] = {
    {"snr_data_aided_db", (PyCFunction)(void *)_bind_snr_data_aided_db, METH_VARARGS | METH_KEYWORDS, "Data-aided Es/N0 (dB): strip the known sign, Es/N0 = a^2 / mean(|z-a|^2)."},
    {"snr_m2m4_db", (PyCFunction)(void *)_bind_snr_m2m4_db, METH_VARARGS | METH_KEYWORDS, "Non-data-aided moment-based (M2M4) Es/N0 (dB) for a constant-modulus signal in AWGN."},
    {"snr_data_aided_db_series", (PyCFunction)(void *)_bind_snr_data_aided_db_series, METH_VARARGS | METH_KEYWORDS, "Sliding-window data-aided Es/N0 (dB) vs index, for visualizing drift."},
    {"snr_m2m4_db_series", (PyCFunction)(void *)_bind_snr_m2m4_db_series, METH_VARARGS | METH_KEYWORDS, "Sliding-window blind (M2M4) Es/N0 (dB) vs index, for visualizing drift."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef snr_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "snr",
    .m_doc     = "Snr module.",
    .m_size    = -1,
    .m_methods = snr_module_methods,
};

PyMODINIT_FUNC
PyInit_snr(void)
{
    import_array();

    PyObject *m = PyModule_Create(&snr_moduledef);
    if (!m) return NULL;

    return m;
}
