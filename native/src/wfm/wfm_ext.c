/*
 * wfm_ext.c — Python extension module wfm
 *
 * Objects: PN, _SynthEngine
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "wfm/wfm_core.h"

#include "wfm_ext_pn.c"
#include "wfm_ext_wfm_synth.c"

static PyObject *
_bind_bpsk_map(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"bits", NULL};
    PyObject *bits_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &bits_obj))
        return NULL;
    PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF(
        bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!bits_arr) { return NULL; }
    const uint8_t *bits = (const uint8_t *)PyArray_DATA(bits_arr);
    size_t bits_len = (size_t)PyArray_SIZE(bits_arr);
    npy_intp _dim = (npy_intp)bits_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_COMPLEX64, 0);
    if (!_out) {Py_DECREF(bits_arr); return NULL; }
    bpsk_map(bits, bits_len, (float complex *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(bits_arr);
    return _out;
}

static PyObject *
_bind_qpsk_map(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"syms", NULL};
    PyObject *syms_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &syms_obj))
        return NULL;
    PyArrayObject *syms_arr = (PyArrayObject *)PyArray_FROM_OTF(
        syms_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!syms_arr) { return NULL; }
    const uint8_t *syms = (const uint8_t *)PyArray_DATA(syms_arr);
    size_t syms_len = (size_t)PyArray_SIZE(syms_arr);
    npy_intp _dim = (npy_intp)syms_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_COMPLEX64, 0);
    if (!_out) {Py_DECREF(syms_arr); return NULL; }
    qpsk_map(syms, syms_len, (float complex *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(syms_arr);
    return _out;
}

static PyObject *
_bind_wfm_awgn_amplitude(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr_db", "signal_power", NULL};
    float snr_db = 0.0f;
    float signal_power = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ff",
            _kwlist, &snr_db, &signal_power))
        return NULL;
    return PyFloat_FromDouble((double)wfm_awgn_amplitude(snr_db, signal_power));
}

static PyObject *
_bind_wfm_ebno_to_snr_db(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"ebno_db", "bits_per_symbol", "samples_per_symbol", NULL};
    float ebno_db = 0.0f;
    int bits_per_symbol = 0;
    float samples_per_symbol = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "fif",
            _kwlist, &ebno_db, &bits_per_symbol, &samples_per_symbol))
        return NULL;
    return PyFloat_FromDouble((double)wfm_ebno_to_snr_db(ebno_db, bits_per_symbol, samples_per_symbol));
}

static PyObject *
_bind_mls_poly(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"n", NULL};
    unsigned long n_raw = 0UL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "k",
            _kwlist, &n_raw))
        return NULL;
    uint32_t n = (uint32_t)n_raw;
    return PyLong_FromUnsignedLongLong((unsigned long long)mls_poly(n));
}

static PyObject *
_bind_crc16(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"bits", NULL};
    PyObject *bits_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &bits_obj))
        return NULL;
    PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF(
        bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!bits_arr) { return NULL; }
    const uint8_t *bits = (const uint8_t *)PyArray_DATA(bits_arr);
    size_t bits_len = (size_t)PyArray_SIZE(bits_arr);
    Py_DECREF(bits_arr);
    return PyLong_FromUnsignedLong((unsigned long)crc16(bits, bits_len));
}

static PyObject *
_bind_rrc_taps(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"beta", "sps", "span", NULL};
    double beta = 0.0;
    int sps = 0;
    int span = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dii",
            _kwlist, &beta, &sps, &span))
        return NULL;
    npy_intp _dim = (npy_intp)(2 * span * sps + 1);
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_FLOAT, 0);
    if (!_out) { return NULL; }
    rrc_taps(beta, sps, span, (float *)PyArray_DATA((PyArrayObject *)_out));
    return _out;
}

static PyObject *
_bind_dsss_spread(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"syms", "code", "sf", NULL};
    PyObject *syms_obj = NULL;
    PyObject *code_obj = NULL;
    int sf = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOi",
            _kwlist, &syms_obj, &code_obj, &sf))
        return NULL;
    PyArrayObject *syms_arr = (PyArrayObject *)PyArray_FROM_OTF(
        syms_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!syms_arr) { return NULL; }
    const float complex *syms = (const float complex *)PyArray_DATA(syms_arr);
    size_t syms_len = (size_t)PyArray_SIZE(syms_arr);
    PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF(
        code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!code_arr) { Py_DECREF(syms_arr); return NULL; }
    const uint8_t *code = (const uint8_t *)PyArray_DATA(code_arr);
    size_t code_len = (size_t)PyArray_SIZE(code_arr);
    npy_intp _dim = (npy_intp)(syms_len * sf);
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_COMPLEX64, 0);
    if (!_out) {Py_DECREF(syms_arr); Py_DECREF(code_arr); return NULL; }
    dsss_spread(syms, syms_len, code, code_len, sf, (float complex *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(syms_arr);
    Py_DECREF(code_arr);
    return _out;
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef wfm_module_methods[] = {
    {"bpsk_map", (PyCFunction)(void *)_bind_bpsk_map, METH_VARARGS | METH_KEYWORDS, "Map bits {0,1} to BPSK symbols {+1,-1} (cf32)."},
    {"qpsk_map", (PyCFunction)(void *)_bind_qpsk_map, METH_VARARGS | METH_KEYWORDS, "Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32)."},
    {"wfm_awgn_amplitude", (PyCFunction)(void *)_bind_wfm_awgn_amplitude, METH_VARARGS | METH_KEYWORDS, "AWGN amplitude for a target SNR (dB, over fs) given signal power."},
    {"wfm_ebno_to_snr_db", (PyCFunction)(void *)_bind_wfm_ebno_to_snr_db, METH_VARARGS | METH_KEYWORDS, "Convert Eb/No (dB) to SNR (dB over fs)."},
    {"mls_poly", (PyCFunction)(void *)_bind_mls_poly, METH_VARARGS | METH_KEYWORDS, "Maximal-length-sequence primitive polynomial for an LFSR of length n."},
    {"crc16", (PyCFunction)(void *)_bind_crc16, METH_VARARGS | METH_KEYWORDS, "CRC-16-CCITT (poly 0x1021, init 0xFFFF) over an unpacked 0/1 bit array, MSB-first — the DSSS burst frame trailer wfmgen appends and BurstDemod validates."},
    {"rrc_taps", (PyCFunction)(void *)_bind_rrc_taps, METH_VARARGS | METH_KEYWORDS, "Root-raised-cosine pulse-shaping taps (2*span*sps+1 unit-energy cf32 taps)."},
    {"dsss_spread", (PyCFunction)(void *)_bind_dsss_spread, METH_VARARGS | METH_KEYWORDS, "Direct-sequence spread syms by the ±1 chip code; yields len(syms)*sf chips."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef wfm_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "wfm",
    .m_doc     = "Wfm module.",
    .m_size    = -1,
    .m_methods = wfm_module_methods,
};

PyMODINIT_FUNC
PyInit_wfm(void)
{
    import_array();
    if (PyType_Ready(&PNObjType) < 0) return NULL;
    if (PyType_Ready(&_SynthEngineType) < 0) return NULL;
    PyObject *m = PyModule_Create(&wfm_moduledef);
    if (!m) return NULL;
    Py_INCREF(&PNObjType);
    if (PyModule_AddObject(m, "PN", (PyObject *)&PNObjType) < 0) {
        Py_DECREF(&PNObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&_SynthEngineType);
    if (PyModule_AddObject(m, "_SynthEngine", (PyObject *)&_SynthEngineType) < 0) {
        Py_DECREF(&_SynthEngineType); Py_DECREF(m); return NULL;
    }
    return m;
}
