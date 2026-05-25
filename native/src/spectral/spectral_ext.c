/*
 * spectral_ext.c — Python extension module spectral
 *
 * Objects: FFT, FFT2D, Corr, Corr2D, Detector, Detector2D
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "spectral/spectral_core.h"

#include "spectral_ext_fft.c"
#include "spectral_ext_fft2d.c"
#include "spectral_ext_corr.c"
#include "spectral_ext_corr2d.c"
#include "spectral_ext_detector.c"
#include "spectral_ext_detector2d.c"

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
    if (PyType_Ready(&FFTObjType) < 0) return NULL;
    if (PyType_Ready(&FFT2DObjType) < 0) return NULL;
    if (PyType_Ready(&CorrObjType) < 0) return NULL;
    if (PyType_Ready(&Corr2DObjType) < 0) return NULL;
    if (PyType_Ready(&DetectorObjType) < 0) return NULL;
    if (PyType_Ready(&Detector2DObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&spectral_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FFTObjType);
    if (PyModule_AddObject(m, "FFT", (PyObject *)&FFTObjType) < 0) {
        Py_DECREF(&FFTObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&FFT2DObjType);
    if (PyModule_AddObject(m, "FFT2D", (PyObject *)&FFT2DObjType) < 0) {
        Py_DECREF(&FFT2DObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CorrObjType);
    if (PyModule_AddObject(m, "Corr", (PyObject *)&CorrObjType) < 0) {
        Py_DECREF(&CorrObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&Corr2DObjType);
    if (PyModule_AddObject(m, "Corr2D", (PyObject *)&Corr2DObjType) < 0) {
        Py_DECREF(&Corr2DObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&DetectorObjType);
    if (PyModule_AddObject(m, "Detector", (PyObject *)&DetectorObjType) < 0) {
        Py_DECREF(&DetectorObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&Detector2DObjType);
    if (PyModule_AddObject(m, "Detector2D", (PyObject *)&Detector2DObjType) < 0) {
        Py_DECREF(&Detector2DObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
