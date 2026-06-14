/*
 * measure_ext.c — Python extension module measure
 *
 * Objects: ToneMeasure, NPRMeasure, IMDMeasure
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "measure/measure_core.h"

#include "measure_ext_tonemeas.c"
#include "measure_ext_nprmeas.c"
#include "measure_ext_imdmeas.c"

static PyObject *
_bind_measure_min_samples(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"fs", "target_rbw", "window", "beta", NULL};
    double fs = 0.0;
    double target_rbw = 0.0;
    int window = 0;
    float beta = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ddif",
            _kwlist, &fs, &target_rbw, &window, &beta))
        return NULL;
    return PyLong_FromUnsignedLongLong((unsigned long long)measure_min_samples(fs, target_rbw, window, beta));
}

static PyObject *
_bind_measure_rec_nfft(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"n", "pad", NULL};
    unsigned long long n_raw = 0ULL;
    unsigned long long pad_raw = 0ULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "KK",
            _kwlist, &n_raw, &pad_raw))
        return NULL;
    size_t n = (size_t)n_raw;
    size_t pad = (size_t)pad_raw;
    return PyLong_FromUnsignedLongLong((unsigned long long)measure_rec_nfft(n, pad));
}

static PyObject *
_bind_measure_proc_gain(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"nfft", NULL};
    unsigned long long nfft_raw = 0ULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "K",
            _kwlist, &nfft_raw))
        return NULL;
    size_t nfft = (size_t)nfft_raw;
    return PyFloat_FromDouble(measure_proc_gain(nfft));
}

static PyObject *
_bind_dp_coherent_freq(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"fs", "f_target", "N", NULL};
    double fs = 0.0;
    double f_target = 0.0;
    unsigned long long N_raw = 0ULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ddK",
            _kwlist, &fs, &f_target, &N_raw))
        return NULL;
    size_t N = (size_t)N_raw;
    return PyFloat_FromDouble(dp_coherent_freq(fs, f_target, N));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef measure_module_methods[] = {
    {"measure_min_samples", (PyCFunction)(void *)_bind_measure_min_samples, METH_VARARGS | METH_KEYWORDS, "Samples needed to reach a target RBW (window 0=hann, 1=kaiser)."},
    {"measure_rec_nfft", (PyCFunction)(void *)_bind_measure_rec_nfft, METH_VARARGS | METH_KEYWORDS, "Recommended zero-padded transform length: next_pow2(n * pad)."},
    {"measure_proc_gain", (PyCFunction)(void *)_bind_measure_proc_gain, METH_VARARGS | METH_KEYWORDS, "FFT processing gain in dB: 10*log10(nfft / 2)."},
    {"dp_coherent_freq", (PyCFunction)(void *)_bind_dp_coherent_freq, METH_VARARGS | METH_KEYWORDS, "Nearest leakage-free coherent test frequency (J cycles, J coprime N)."},
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
    if (PyType_Ready(&NPRMeasureObjType) < 0) return NULL;
    if (PyType_Ready(&IMDMeasureObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&measure_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ToneMeasureObjType);
    if (PyModule_AddObject(m, "ToneMeasure", (PyObject *)&ToneMeasureObjType) < 0) {
        Py_DECREF(&ToneMeasureObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&NPRMeasureObjType);
    if (PyModule_AddObject(m, "NPRMeasure", (PyObject *)&NPRMeasureObjType) < 0) {
        Py_DECREF(&NPRMeasureObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&IMDMeasureObjType);
    if (PyModule_AddObject(m, "IMDMeasure", (PyObject *)&IMDMeasureObjType) < 0) {
        Py_DECREF(&IMDMeasureObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
