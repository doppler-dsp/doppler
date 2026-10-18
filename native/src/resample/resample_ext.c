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
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/resample/resample_core.h"

#ifndef JM_ARRAY_ARG_DEFINED
#define JM_ARRAY_ARG_DEFINED
/* Convert a Python argument for an array parameter to an ndarray of
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, except that for a
 * one-byte element type a byte buffer (bytes, bytearray, memoryview) is its
 * bytes, one element per byte (gh-1700). `name` is the parameter, for the
 * message. `hint` is its declared str_hint, or NULL. Declaring one is the
 * opt-in to refusing text (gh-1824): a str, or a bytes numpy would parse as
 * a number, and a str's refusal ends with the hint (gh-1756). With NULL,
 * numpy converts a str as it converts anything else. Returns a new
 * reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  int text     = PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj));
  if (hint && text)
    {
      /* The hint says where text goes instead: a str's refusal only. */
      int say = PyUnicode_Check (obj);
      PyErr_Format (PyExc_TypeError,
                    "%s must be an array of numbers, not %.200s%s%s", name,
                    Py_TYPE (obj)->tp_name, say ? ": " : "", say ? hint : "");
      return NULL;
    }
  if (one_byte && !PyArray_Check (obj) && PyObject_CheckBuffer (obj))
    {
      PyObject *view = PyMemoryView_FromObject (obj);
      if (!view)
        return NULL;
      if (PyMemoryView_GET_BUFFER (view)->itemsize == 1)
        {
          PyObject *raw = PyArray_FromBuffer (
              view, PyArray_DescrFromType (typenum), -1, 0);
          Py_DECREF (view);
          if (!raw)
            return NULL;
          PyObject *arr = PyArray_FROM_OTF (raw, typenum, requirements);
          Py_DECREF (raw);
          return (PyArrayObject *)arr;
        }
      Py_DECREF (view);
    }
  return (PyArrayObject *)PyArray_FROM_OTF (obj, typenum, requirements);
}
/* `unused` (gh-1747): emitted into every extension translation unit,
 * including one that takes no array, or calls only jm_array_arg_hint above
 * -- which needs no mark, since this wrapper always calls it. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

#ifndef JM_INFLIGHT_DEFINED
#define JM_INFLIGHT_DEFINED
/* gh-2272: the GIL-free calls running on one object, so its explicit
 * teardown can wait for them instead of freeing under them. `n` is read
 * and written only with the GIL held. `idle` exists only once a teardown
 * has had to wait: that teardown holds it, and the last call to leave
 * gives it back. */
typedef struct
{
  Py_ssize_t         n;
  int                waiting;
  PyThread_type_lock idle;
} jm_inflight_t;

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline void
jm_inflight_enter (jm_inflight_t *g)
{
  g->n++;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline void
jm_inflight_leave (jm_inflight_t *g)
{
  if (--g->n == 0 && g->waiting)
    {
      g->waiting = 0;
      PyThread_release_lock (g->idle);
    }
}

/* Called with the GIL held, after the object refuses new calls. Returns
 * with none running: 0, or -1 with MemoryError when no lock could be had,
 * in which case nothing has been waited for. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline int
jm_inflight_wait (jm_inflight_t *g)
{
  while (g->n > 0)
    {
      if (!g->idle)
        {
          g->idle = PyThread_allocate_lock ();
          if (!g->idle)
            {
              PyErr_NoMemory ();
              return -1;
            }
        }
      /* Take the lock, then block on it again with the GIL released:
       * the last jm_inflight_leave releases it. The GIL is dropped by
       * function here, not by the macro pair, so the macro still marks
       * exactly the calls a binding runs without it. */
      (void)PyThread_acquire_lock (g->idle, NOWAIT_LOCK);
      g->waiting        = 1;
      PyThreadState *ts = PyEval_SaveThread ();
      (void)PyThread_acquire_lock (g->idle, WAIT_LOCK);
      PyEval_RestoreThread (ts);
      PyThread_release_lock (g->idle);
    }
  return 0;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline void
jm_inflight_free (jm_inflight_t *g)
{
  if (g->idle)
    {
      PyThread_free_lock (g->idle);
      g->idle = NULL;
    }
}
#endif /* JM_INFLIGHT_DEFINED */

#ifndef JM_CAPSULE_LEND_DEFINED
#define JM_CAPSULE_LEND_DEFINED
/* gh-2328: a lent capsule's context is the object that owns its pointer.
 * The destructor releases that reference and nothing else -- the pointer is
 * the owner's to free -- so whoever keeps the capsule keeps the owner. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static void
jm_capsule_lend_release (PyObject *cap)
{
  Py_XDECREF ((PyObject *)PyCapsule_GetContext (cap));
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static PyObject *
jm_capsule_lend (void *ptr, const char *name, PyObject *owner)
{
  PyObject *cap = PyCapsule_New (ptr, name, jm_capsule_lend_release);
  if (!cap)
    return NULL;
  Py_INCREF (owner);
  if (PyCapsule_SetContext (cap, owner) < 0)
    {
      Py_DECREF (owner);
      Py_DECREF (cap);
      return NULL;
    }
  return cap;
}
#endif /* JM_CAPSULE_LEND_DEFINED */

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
  static char *_kwlist[] = { "N", "R", "M", NULL };
  long long    N_raw     = 0LL;
  long long    R_raw     = 0LL;
  long long    M_raw     = 0LL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "LLL", _kwlist, &N_raw, &R_raw,
                                    &M_raw))
    return NULL;
  if (N_raw < 0 || N_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "N: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)N_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t N = (uint32_t)N_raw;
  if (R_raw < 0 || R_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "R: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)R_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t R = (uint32_t)R_raw;
  if (M_raw < 0 || M_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "M: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)M_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t M         = (uint32_t)M_raw;
  size_t   _dim_need = (size_t)(M);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "ciccompmf: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_DOUBLE, 0);
  if (!_out)
    {
      return NULL;
    }
  dp_ciccompmf ((double *)PyArray_DATA ((PyArrayObject *)_out), N, R, M);
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
  return PyFloat_FromDouble (dp_kaiser_beta (atten));
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
  return PyLong_FromLong (
      (long)dp_kaiser_num_taps (num_phases, atten, pb, sb));
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
    "is the minimum num_taps argument to pass to\n"
    "dp_Resampler_create_custom().\n"
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
