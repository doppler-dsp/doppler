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
    static char *_kwlist[] = {"fs", "target_rbw", "bits", "dynamic_range_db", "complex_input", NULL};
    double fs = 0.0;
    double target_rbw = 0.0;
    unsigned long long bits_raw = 0ULL;
    double dynamic_range_db = 0.0;
    int complex_input = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ddKdi",
            _kwlist, &fs, &target_rbw, &bits_raw, &dynamic_range_db, &complex_input))
        return NULL;
    size_t bits = (size_t)bits_raw;
    return PyLong_FromUnsignedLongLong((unsigned long long)measure_min_samples(fs, target_rbw, bits, dynamic_range_db, complex_input));
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
    {"measure_min_samples", (PyCFunction)(void *)_bind_measure_min_samples, METH_VARARGS | METH_KEYWORDS,
     "Samples for a target RBW (auto Kaiser from bits/dynamic_range_db; target_rbw<=0 -> span/1000).\n"
     "\n"
     "Plans a capture for the same auto-Kaiser window the measurement objects\n"
     "use: the dynamic-range target (from dynamic_range_db, else bits) selects\n"
     "the Kaiser beta, whose ENBW (measured via kaiser_enbw) sets the\n"
     "bins-per-RBW. RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "fs : float\n"
     "    Sample rate (Hz, > 0).\n"
     "target_rbw : float\n"
     "    Desired resolution bandwidth (Hz). When <= 0 it defaults to\n"
     "    span/1000, where span = fs/2 for real captures and fs for complex\n"
     "    (complex_input).\n"
     "bits : int\n"
     "    ADC depth: sets the dynamic-range target when no explicit override\n"
     "    is given.\n"
     "dynamic_range_db : float\n"
     "    Explicit dynamic-range target (dB); used when > 0.\n"
     "complex_input : int\n"
     "    Non-zero if the capture is complex (span = fs).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Required capture length, or 0 on bad args.\n"},
    {"measure_rec_nfft", (PyCFunction)(void *)_bind_measure_rec_nfft, METH_VARARGS | METH_KEYWORDS,
     "Recommended zero-padded transform length: next_pow2(n * pad).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "n : int\n"
     "    Input.\n"
     "pad : int\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Output.\n"},
    {"measure_proc_gain", (PyCFunction)(void *)_bind_measure_proc_gain, METH_VARARGS | METH_KEYWORDS,
     "FFT processing gain in dB: 10*log10(nfft / 2).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "nfft : int\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Output.\n"},
    {"dp_coherent_freq", (PyCFunction)(void *)_bind_dp_coherent_freq, METH_VARARGS | METH_KEYWORDS,
     "Nearest leakage-free coherent test frequency (J cycles, J coprime N).\n"
     "\n"
     "Snaps `f_target` to `J * fs / N` where J is the nearest integer cycle\n"
     "count that is coprime with N — an integer number of cycles in the\n"
     "capture (no leakage) with J coprime to N (so quantisation-noise\n"
     "correlation is minimised).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "fs : float\n"
     "    Input.\n"
     "f_target : float\n"
     "    Input.\n"
     "N : int\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    The coherent frequency (Hz), or 0 on bad args.\n"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef measure_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "measure",
    .m_doc     = "ADC and tone-quality metrics: ToneMeasure, NPRMeasure and IMDMeasure return named records — ENOB, SFDR, SINAD, THD and more — from a single-tone, noise-power-ratio, or two-tone intermodulation capture.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> import numpy as np\n"
     ">>> from doppler.measure import ToneMeasure\n"
     ">>> tm = ToneMeasure(n=4096, fs=1.024e6)\n"
     ">>> x = np.cos(2 * np.pi * 200 / 4096 * np.arange(4096)).astype(np.float32)\n"
     ">>> bool(tm.analyze(x).enob > 10)\n"
     "True\n",
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
