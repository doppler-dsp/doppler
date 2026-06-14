/*
 * measure_ext.c — Python extension module measure
 *
 * Objects: ToneMeasure
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "measure/measure_core.h"

#include "measure_ext_tonemeas.c"

static PyObject *
_bind_measure_min_samples(PyObject *self, PyObject *args)
{
    (void)self;
    double fs = 0.0;
    double target_rbw = 0.0;
    int window = 0;
    float beta = 0.0f;
    if (!PyArg_ParseTuple(args, "ddif", &fs, &target_rbw, &window, &beta))
        return NULL;
    return PyLong_FromUnsignedLongLong((unsigned long long)measure_min_samples(fs, target_rbw, window, beta));
}

static PyObject *
_bind_measure_rec_nfft(PyObject *self, PyObject *args)
{
    (void)self;
    unsigned long long n_raw = 0ULL;
    unsigned long long pad_raw = 0ULL;
    if (!PyArg_ParseTuple(args, "KK", &n_raw, &pad_raw))
        return NULL;
    size_t n = (size_t)n_raw;
    size_t pad = (size_t)pad_raw;
    return PyLong_FromUnsignedLongLong((unsigned long long)measure_rec_nfft(n, pad));
}

static PyObject *
_bind_measure_proc_gain(PyObject *self, PyObject *args)
{
    (void)self;
    unsigned long long nfft_raw = 0ULL;
    if (!PyArg_ParseTuple(args, "K", &nfft_raw))
        return NULL;
    size_t nfft = (size_t)nfft_raw;
    return PyFloat_FromDouble(measure_proc_gain(nfft));
}

static PyObject *
_bind_dp_coherent_freq(PyObject *self, PyObject *args)
{
    (void)self;
    double fs = 0.0;
    double f_target = 0.0;
    unsigned long long N_raw = 0ULL;
    if (!PyArg_ParseTuple(args, "ddK", &fs, &f_target, &N_raw))
        return NULL;
    size_t N = (size_t)N_raw;
    return PyFloat_FromDouble(dp_coherent_freq(fs, f_target, N));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef measure_module_methods[] = {
    {"measure_min_samples", _bind_measure_min_samples, METH_VARARGS, "Samples needed to reach a target RBW (window 0=hann, 1=kaiser)."},
    {"measure_rec_nfft", _bind_measure_rec_nfft, METH_VARARGS, "Recommended zero-padded transform length: next_pow2(n * pad)."},
    {"measure_proc_gain", _bind_measure_proc_gain, METH_VARARGS, "FFT processing gain in dB: 10*log10(nfft / 2)."},
    {"dp_coherent_freq", _bind_dp_coherent_freq, METH_VARARGS, "Nearest leakage-free coherent test frequency (J cycles, J coprime N)."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef measure_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "measure",
    .m_doc     = "Measure module.",
    .m_size    = -1,
    .m_methods = measure_module_methods,
};

PyMODINIT_FUNC
PyInit_measure(void)
{
    import_array();
    if (PyType_Ready(&ToneMeasureObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&measure_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ToneMeasureObjType);
    if (PyModule_AddObject(m, "ToneMeasure", (PyObject *)&ToneMeasureObjType) < 0) {
        Py_DECREF(&ToneMeasureObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
