/*
 * wfmgen_ext.c — Python extension module wfmgen
 *
 * Objects: PN, Synth
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "wfmgen/wfmgen_core.h"

#include "wfmgen_ext_pn.c"
#include "wfmgen_ext_synth.c"

static PyObject *
_bind_bpsk_map(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *bits_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &bits_obj))
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
_bind_qpsk_map(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *syms_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &syms_obj))
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
_bind_wfm_awgn_amplitude(PyObject *self, PyObject *args)
{
    (void)self;
    float snr_db = 0.0f;
    float signal_power = 0.0f;
    if (!PyArg_ParseTuple(args, "ff", &snr_db, &signal_power))
        return NULL;
    return PyFloat_FromDouble((double)wfm_awgn_amplitude(snr_db, signal_power));
}

static PyObject *
_bind_wfm_ebno_to_snr_db(PyObject *self, PyObject *args)
{
    (void)self;
    float ebno_db = 0.0f;
    int bits_per_symbol = 0;
    float samples_per_symbol = 0.0f;
    if (!PyArg_ParseTuple(args, "fif", &ebno_db, &bits_per_symbol, &samples_per_symbol))
        return NULL;
    return PyFloat_FromDouble((double)wfm_ebno_to_snr_db(ebno_db, bits_per_symbol, samples_per_symbol));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef wfmgen_module_methods[] = {
    {"bpsk_map", _bind_bpsk_map, METH_VARARGS, "Map bits {0,1} to BPSK symbols {+1,-1} (cf32)."},
    {"qpsk_map", _bind_qpsk_map, METH_VARARGS, "Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32)."},
    {"wfm_awgn_amplitude", _bind_wfm_awgn_amplitude, METH_VARARGS, "AWGN amplitude for a target SNR (dB, over fs) given signal power."},
    {"wfm_ebno_to_snr_db", _bind_wfm_ebno_to_snr_db, METH_VARARGS, "Convert Eb/No (dB) to SNR (dB over fs)."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef wfmgen_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "wfmgen",
    .m_doc     = "Wfmgen module.",
    .m_size    = -1,
    .m_methods = wfmgen_module_methods,
};

PyMODINIT_FUNC
PyInit_wfmgen(void)
{
    import_array();
    if (PyType_Ready(&PNObjType) < 0) return NULL;
    if (PyType_Ready(&SynthObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&wfmgen_moduledef);
    if (!m) return NULL;
    Py_INCREF(&PNObjType);
    if (PyModule_AddObject(m, "PN", (PyObject *)&PNObjType) < 0) {
        Py_DECREF(&PNObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&SynthObjType);
    if (PyModule_AddObject(m, "Synth", (PyObject *)&SynthObjType) < 0) {
        Py_DECREF(&SynthObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
