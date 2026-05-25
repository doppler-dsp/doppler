/*
 * resample_ext.c — Python extension module resample
 *
 * Objects: Resampler, HalfbandDecimator, CIC
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "resample/resample_core.h"

#include "resample_ext_Resampler.c"
#include "resample_ext_HalfbandDecimator.c"
#include "resample_ext_cic.c"
#include "resample_ext_extra.c"  /* hand-written — jm never modifies */

static PyObject *
_bind_ciccompmf(PyObject *self, PyObject *args)
{
    (void)self;
    unsigned long N_raw = 0UL;
    unsigned long R_raw = 0UL;
    unsigned long M_raw = 0UL;
    if (!PyArg_ParseTuple(args, "kkk", &N_raw, &R_raw, &M_raw))
        return NULL;
    uint32_t N = (uint32_t)N_raw;
    uint32_t R = (uint32_t)R_raw;
    uint32_t M = (uint32_t)M_raw;
    npy_intp _dim = (npy_intp)M;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_DOUBLE, 0);
    if (!_out) { return NULL; }
    ciccompmf((double *)PyArray_DATA((PyArrayObject *)_out), N, R, M);
    return _out;
}

static PyObject *
_bind_kaiser_beta(PyObject *self, PyObject *args)
{
    (void)self;
    double atten = 0.0;
    if (!PyArg_ParseTuple(args, "d", &atten))
        return NULL;
    return PyFloat_FromDouble(kaiser_beta(atten));
}

static PyObject *
_bind_kaiser_num_taps(PyObject *self, PyObject *args)
{
    (void)self;
    int num_phases = 0;
    double atten = 0.0;
    double pb = 0.0;
    double sb = 0.0;
    if (!PyArg_ParseTuple(args, "iddd", &num_phases, &atten, &pb, &sb))
        return NULL;
    return PyLong_FromLong((long)kaiser_num_taps(num_phases, atten, pb, sb));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Resample_methods[] = {
    {"ciccompmf", _bind_ciccompmf, METH_VARARGS, "ciccompmf."},
    {"kaiser_beta", _bind_kaiser_beta, METH_VARARGS, "kaiser_beta."},
    {"kaiser_num_taps", _bind_kaiser_num_taps, METH_VARARGS, "kaiser_num_taps."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef resample_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "resample",
    .m_doc     = "Resample module.",
    .m_size    = -1,
    .m_methods = Resample_methods,
};

PyMODINIT_FUNC
PyInit_resample(void)
{
    import_array();
    if (PyType_Ready(&ResamplerObjType) < 0) return NULL;
    if (PyType_Ready(&HalfbandDecimatorObjType) < 0) return NULL;
    if (PyType_Ready(&CICObjType) < 0) return NULL;
    if (PyType_Ready(&HalfbandDecimatorDpType) < 0) return NULL;
    if (PyType_Ready(&HalfbandDecimatorR2CType) < 0) return NULL;
    PyObject *m = PyModule_Create(&resample_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ResamplerObjType);
    if (PyModule_AddObject(m, "Resampler", (PyObject *)&ResamplerObjType) < 0) {
        Py_DECREF(&ResamplerObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbandDecimatorObjType);
    if (PyModule_AddObject(m, "HalfbandDecimator", (PyObject *)&HalfbandDecimatorObjType) < 0) {
        Py_DECREF(&HalfbandDecimatorObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CICObjType);
    if (PyModule_AddObject(m, "CIC", (PyObject *)&CICObjType) < 0) {
        Py_DECREF(&CICObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbandDecimatorDpType);
    if (PyModule_AddObject(m, "HalfbandDecimatorDp", (PyObject *)&HalfbandDecimatorDpType) < 0) {
        Py_DECREF(&HalfbandDecimatorDpType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbandDecimatorR2CType);
    if (PyModule_AddObject(m, "HalfbandDecimatorR2C", (PyObject *)&HalfbandDecimatorR2CType) < 0) {
        Py_DECREF(&HalfbandDecimatorR2CType); Py_DECREF(m); return NULL;
    }
    return m;
}
