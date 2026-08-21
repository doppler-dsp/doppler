/*
 * resample_ext.c — Python extension module resample
 *
 * Objects: Resampler, HalfbandDecimator, CIC, RateConverter, Farrow,
 * HalfbandDecimatorQ15, MatchedRateConverter GENERATED — do not hand-edit.
 * Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "resample/resample_core.h"

#include "resample_ext_HalfbandDecimator.c"
#include "resample_ext_RateConverter.c"
#include "resample_ext_Resampler.c"
#include "resample_ext_cic.c"
#include "resample_ext_extra.c" /* hand-written — jm never modifies */
#include "resample_ext_farrow.c"
#include "resample_ext_hbdecim_q15.c"
#include "resample_ext_matchedrateconverter.c"

static PyObject *
_bind_ciccompmf (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char  *_kwlist[] = { "N", "R", "M", NULL };
  unsigned long N_raw     = 0UL;
  unsigned long R_raw     = 0UL;
  unsigned long M_raw     = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "kkk", _kwlist, &N_raw, &R_raw,
                                    &M_raw))
    return NULL;
  uint32_t  N    = (uint32_t)N_raw;
  uint32_t  R    = (uint32_t)R_raw;
  uint32_t  M    = (uint32_t)M_raw;
  npy_intp  _dim = (npy_intp)M;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_DOUBLE, 0);
  if (!_out)
    {
      return NULL;
    }
  ciccompmf ((double *)PyArray_DATA ((PyArrayObject *)_out), N, R, M);
  return _out;
}

static PyObject *
_bind_kaiser_beta (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "atten", NULL };
  double       atten     = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &atten))
    return NULL;
  return PyFloat_FromDouble (kaiser_beta (atten));
}

static PyObject *
_bind_kaiser_num_taps (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]  = { "num_phases", "atten", "pb", "sb", NULL };
  int          num_phases = 0;
  double       atten      = 0.0;
  double       pb         = 0.0;
  double       sb         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "iddd", _kwlist, &num_phases,
                                    &atten, &pb, &sb))
    return NULL;
  return PyLong_FromLong ((long)kaiser_num_taps (num_phases, atten, pb, sb));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef resample_module_methods[] = {
  { "ciccompmf", (PyCFunction)(void *)_bind_ciccompmf,
    METH_VARARGS | METH_KEYWORDS,
    "Design a CIC passband-droop compensator FIR filter. Implements the\n"
    "closed-form Bernoulli-series maximally-flat-error method from Molnar &\n"
    "Vucic (IEEE TCAS-II 58(12):926-930, 2011, DOI\n"
    "10.1109/TCSII.2011.2172522). The compensator runs at the *decimated*\n"
    "(output) rate and should be applied after the CIC stage. DC gain is\n"
    "exactly 1.0. Odd M gives symmetric linear-phase taps; even M gives\n"
    "half-sample-shifted linear-phase taps.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "N : int\n"
    "    CIC filter order (number of integrator/comb stages, >= 1).\n"
    "R : int\n"
    "    CIC decimation factor (>= 2).\n"
    "M : int\n"
    "    Number of compensator taps: odd M in `[1, 19]`, even M in `[1,\n"
    "    18]`. The Bernoulli table is nine entries, so the two parities do\n"
    "    not reach the same length; anything outside its own range yields\n"
    "    the all-zero filter above.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import ciccompmf\n"
    ">>> import numpy as np\n"
    ">>> h = ciccompmf(4, 16, 5)\n"
    ">>> h.shape, h.dtype\n"
    "((5,), dtype('float64'))\n"
    ">>> [round(float(v), 4) for v in h]\n"
    "[0.029, -0.282, 1.5061, -0.282, 0.029]\n" },
  { "kaiser_beta", (PyCFunction)(void *)_bind_kaiser_beta,
    METH_VARARGS | METH_KEYWORDS,
    "Compute the Kaiser window beta parameter from stopband attenuation.\n"
    "Uses the standard Kaiser-Hamming formulae: atten > 50 dB: beta = 0.1102\n"
    "* (atten - 8.7) 21 <= atten <= 50 dB: beta = 0.5842*(atten-21)^0.4 +\n"
    "0.07886*(atten-21) atten < 21 dB: beta = 0.0 (rectangular window)\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "atten : float\n"
    "    Desired stopband attenuation in dB (positive value).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Kaiser beta parameter (>= 0.0).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import kaiser_beta\n"
    ">>> round(kaiser_beta(60.0), 4)\n"
    "5.6533\n"
    ">>> kaiser_beta(20.0)\n"
    "0.0\n" },
  { "kaiser_num_taps", (PyCFunction)(void *)_bind_kaiser_num_taps,
    METH_VARARGS | METH_KEYWORDS,
    "Estimate the taps-per-phase count for a polyphase Kaiser FIR bank.\n"
    "Applies the Kaiser length formula to the per-phase normalised prototype\n"
    "(pb/num_phases, sb/num_phases), rounds up to the next odd symmetrical\n"
    "length, then divides by num_phases to give taps per branch. The result\n"
    "is the minimum num_taps argument to pass to Resampler_create_custom().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "num_phases : int\n"
    "    Number of polyphase branches (power of two). A value below 1 is not\n"
    "    a bank; the function returns 0.\n"
    "atten : float\n"
    "    Desired stopband attenuation in dB.\n"
    "pb : float\n"
    "    Normalised passband edge (0 < pb < sb < 1).\n"
    "sb : float\n"
    "    Normalised stopband edge.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Taps per polyphase branch (>= 1).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import kaiser_num_taps\n"
    ">>> kaiser_num_taps(4096, 60.0, 0.4, 0.6)\n"
    "19\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef resample_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "resample",
  .m_doc     = "Sample-rate conversion: polyphase resampling (Resampler, "
               "RateConverter), halfband decimation (HalfbandDecimator), CIC "
               "decimation, and Farrow fractional resampling.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> import numpy as np\n"
               ">>> from doppler.resample import Resampler\n"
               ">>> Resampler(rate=2.0).execute(np.ones(100, np.complex64)).size\n"
               "200\n",
  .m_size    = -1,
  .m_methods = resample_module_methods,
};

PyMODINIT_FUNC
PyInit_resample (void)
{
  import_array ();
  if (PyType_Ready (&ResamplerObjType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorObjType) < 0)
    return NULL;
  if (PyType_Ready (&CICObjType) < 0)
    return NULL;
  if (PyType_Ready (&RateConverterObjType) < 0)
    return NULL;
  if (PyType_Ready (&FarrowObjType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorQ15ObjType) < 0)
    return NULL;
  if (PyType_Ready (&MatchedRateConverterObjType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorDpType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorR2CType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&resample_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&ResamplerObjType);
  if (PyModule_AddObject (m, "Resampler", (PyObject *)&ResamplerObjType) < 0)
    {
      Py_DECREF (&ResamplerObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorObjType);
  if (PyModule_AddObject (m, "HalfbandDecimator",
                          (PyObject *)&HalfbandDecimatorObjType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&CICObjType);
  if (PyModule_AddObject (m, "CIC", (PyObject *)&CICObjType) < 0)
    {
      Py_DECREF (&CICObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&RateConverterObjType);
  if (PyModule_AddObject (m, "RateConverter",
                          (PyObject *)&RateConverterObjType)
      < 0)
    {
      Py_DECREF (&RateConverterObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&FarrowObjType);
  if (PyModule_AddObject (m, "Farrow", (PyObject *)&FarrowObjType) < 0)
    {
      Py_DECREF (&FarrowObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorQ15ObjType);
  if (PyModule_AddObject (m, "HalfbandDecimatorQ15",
                          (PyObject *)&HalfbandDecimatorQ15ObjType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorQ15ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&MatchedRateConverterObjType);
  if (PyModule_AddObject (m, "MatchedRateConverter",
                          (PyObject *)&MatchedRateConverterObjType)
      < 0)
    {
      Py_DECREF (&MatchedRateConverterObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorDpType);
  if (PyModule_AddObject (m, "HalfbandDecimatorDp",
                          (PyObject *)&HalfbandDecimatorDpType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorDpType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorR2CType);
  if (PyModule_AddObject (m, "HalfbandDecimatorR2C",
                          (PyObject *)&HalfbandDecimatorR2CType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorR2CType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
