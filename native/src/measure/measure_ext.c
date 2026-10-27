/*
 * measure_ext.c — Python extension module measure
 *
 * Objects: ToneMeasure, NPRMeasure, IMDMeasure
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/measure/measure_core.h"

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

#include "measure_ext_imdmeas.c"
#include "measure_ext_nprmeas.c"
#include "measure_ext_tonemeas.c"

static PyObject *
_bind_measure_min_samples (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = {
    "fs", "target_rbw", "bits", "dynamic_range_db", "complex_input", NULL
  };
  double             fs               = 0.0;
  double             target_rbw       = 0.0;
  PyObject          *bits_obj         = NULL;
  unsigned long long bits_raw         = 0ULL;
  double             dynamic_range_db = 0.0;
  int                complex_input    = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddOdi", _kwlist, &fs,
                                    &target_rbw, &bits_obj, &dynamic_range_db,
                                    &complex_input))
    return NULL;
  if (bits_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (bits_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      bits_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((bits_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || bits_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "bits: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t bits = (size_t)bits_raw;
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_measure_min_samples (
          fs, target_rbw, bits, dynamic_range_db, complex_input));
}

static PyObject *
_bind_measure_rec_nfft (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[] = { "n", "pad", NULL };
  PyObject          *n_obj     = NULL;
  unsigned long long n_raw     = 0ULL;
  PyObject          *pad_obj   = NULL;
  unsigned long long pad_raw   = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &n_obj,
                                    &pad_obj))
    return NULL;
  if (n_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t n = (size_t)n_raw;
  if (pad_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (pad_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      pad_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((pad_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || pad_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "pad: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t pad = (size_t)pad_raw;
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_measure_rec_nfft (n, pad));
}

static PyObject *
_bind_measure_proc_gain (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[] = { "nfft", NULL };
  PyObject          *nfft_obj  = NULL;
  unsigned long long nfft_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &nfft_obj))
    return NULL;
  if (nfft_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (nfft_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      nfft_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((nfft_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || nfft_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "nfft: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t nfft = (size_t)nfft_raw;
  return PyFloat_FromDouble (dp_measure_proc_gain (nfft));
}

static PyObject *
_bind_dp_coherent_freq (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[] = { "fs", "f_target", "N", NULL };
  double             fs        = 0.0;
  double             f_target  = 0.0;
  PyObject          *N_obj     = NULL;
  unsigned long long N_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddO", _kwlist, &fs, &f_target,
                                    &N_obj))
    return NULL;
  if (N_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (N_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      N_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((N_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || N_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "N: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t N = (size_t)N_raw;
  return PyFloat_FromDouble (dp_coherent_freq (fs, f_target, N));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef measure_module_methods[] = {
  { "measure_min_samples", (PyCFunction)(void *)_bind_measure_min_samples,
    METH_VARARGS | METH_KEYWORDS,
    "Samples for a target RBW (auto Kaiser from bits/dynamic_range_db; "
    "target_rbw<=0 -> span/1000).\n"
    "\n"
    "Plans a capture for the same auto-Kaiser window the measurement objects\n"
    "use: the dynamic-range target (from dynamic_range_db, else bits)\n"
    "selects the Kaiser beta, whose ENBW (measured via kaiser_enbw) sets the\n"
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
    "    Required capture length, or 0 on bad args.\n" },
  { "measure_rec_nfft", (PyCFunction)(void *)_bind_measure_rec_nfft,
    METH_VARARGS | METH_KEYWORDS,
    "Recommended zero-padded transform length: next_pow_two(n * pad).\n"
    "\n"
    "The same rule the measurement objects apply (with pad = ::MEASURE_PAD),\n"
    "exposed so a caller can plan an FFT size before building an analyser.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Capture length (samples).\n"
    "pad : int\n"
    "    Zero-pad factor; 0 is treated as 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The smallest power of two >= n * pad (1 when n is 0).\n" },
  { "measure_proc_gain", (PyCFunction)(void *)_bind_measure_proc_gain,
    METH_VARARGS | METH_KEYWORDS,
    "FFT processing gain in dB: 10*log10(nfft / 2).\n"
    "\n"
    "How far a single bin's noise sits below the total noise power for an\n"
    "nfft-point transform of a real capture. A transform shorter than two\n"
    "bins has no gain to report (nfft = 1 would read -3 dB and nfft = 0\n"
    "-inf), so it is refused with NaN, which no caller can mistake for a\n"
    "gain.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "nfft : int\n"
    "    Transform length (bins), at least 2.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Processing gain (dB), or NaN when nfft < 2.\n" },
  { "dp_coherent_freq", (PyCFunction)(void *)_bind_dp_coherent_freq,
    METH_VARARGS | METH_KEYWORDS,
    "Nearest leakage-free coherent test frequency (J cycles, J coprime N).\n"
    "\n"
    "Snaps `f_target` to `J * fs / N` where J is the nearest integer cycle\n"
    "count that is coprime with N — an integer number of cycles in the\n"
    "capture (no leakage) with J coprime to N (so quantisation-noise\n"
    "correlation is minimised).\n"
    "\n"
    "J starts at round(f_target * N / fs), is clamped to `[1, N-1]`, then\n"
    "moved by the smallest distance that makes it coprime with N; on a tie\n"
    "the higher J wins.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "fs : float\n"
    "    Sample rate (Hz, > 0).\n"
    "f_target : float\n"
    "    Desired test frequency (Hz).\n"
    "N : int\n"
    "    Capture length (samples, >= 2).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    The coherent frequency J * fs / N (Hz), or 0 if N < 2 or fs <= "
    "0.\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef measure_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "measure",
  .m_doc
  = "ADC and tone-quality metrics: ToneMeasure, NPRMeasure and IMDMeasure "
    "return named records — ENOB, SFDR, SINAD, THD and more — from a "
    "single-tone, noise-power-ratio, or two-tone intermodulation capture.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.measure import ToneMeasure\n"
    ">>> tm = ToneMeasure(n=4096, fs=1.024e6)\n"
    ">>> x = np.cos(2 * np.pi * 200 / 4096 * "
    "np.arange(4096)).astype(np.float32)\n"
    ">>> bool(tm.analyze(x).enob > 10)\n"
    "True\n",
  .m_size    = -1,
  .m_methods = measure_module_methods,
};

PyMODINIT_FUNC
PyInit_measure (void)
{
  import_array ();
  if (PyType_Ready (&ToneMeasureObjType) < 0)
    return NULL;
  if (!ToneMeasureObj_analyze_type)
    {
      ToneMeasureObj_analyze_type
          = PyStructSequence_NewType (&ToneMeasureObj_analyze_desc);
      if (!ToneMeasureObj_analyze_type)
        return NULL;
    }
  ToneMeasureObj_analyze_complex_type
      = ToneMeasureObj_analyze_type; /* ToneMetrics: one public name, one type
                                      */
  if (!ToneMeasureObj_time_stats_type)
    {
      ToneMeasureObj_time_stats_type
          = PyStructSequence_NewType (&ToneMeasureObj_time_stats_desc);
      if (!ToneMeasureObj_time_stats_type)
        return NULL;
    }
  if (PyType_Ready (&NPRMeasureObjType) < 0)
    return NULL;
  if (!NPRMeasureObj_analyze_type)
    {
      NPRMeasureObj_analyze_type
          = PyStructSequence_NewType (&NPRMeasureObj_analyze_desc);
      if (!NPRMeasureObj_analyze_type)
        return NULL;
    }
  if (PyType_Ready (&IMDMeasureObjType) < 0)
    return NULL;
  if (!IMDMeasureObj_analyze_type)
    {
      IMDMeasureObj_analyze_type
          = PyStructSequence_NewType (&IMDMeasureObj_analyze_desc);
      if (!IMDMeasureObj_analyze_type)
        return NULL;
    }
  PyObject *m = PyModule_Create (&measure_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&ToneMeasureObjType);
  if (PyModule_AddObject (m, "ToneMeasure", (PyObject *)&ToneMeasureObjType)
      < 0)
    {
      Py_DECREF (&ToneMeasureObjType);
      Py_DECREF (m);
      return NULL;
    }
  if (PyModule_AddObject (m, "ToneMetrics",
                          (PyObject *)ToneMeasureObj_analyze_type)
      < 0)
    {
      Py_DECREF (ToneMeasureObj_analyze_type);
      Py_DECREF (m);
      return NULL;
    }
  if (PyModule_AddObject (m, "TimeStats",
                          (PyObject *)ToneMeasureObj_time_stats_type)
      < 0)
    {
      Py_DECREF (ToneMeasureObj_time_stats_type);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&NPRMeasureObjType);
  if (PyModule_AddObject (m, "NPRMeasure", (PyObject *)&NPRMeasureObjType) < 0)
    {
      Py_DECREF (&NPRMeasureObjType);
      Py_DECREF (m);
      return NULL;
    }
  if (PyModule_AddObject (m, "NPRMetrics",
                          (PyObject *)NPRMeasureObj_analyze_type)
      < 0)
    {
      Py_DECREF (NPRMeasureObj_analyze_type);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&IMDMeasureObjType);
  if (PyModule_AddObject (m, "IMDMeasure", (PyObject *)&IMDMeasureObjType) < 0)
    {
      Py_DECREF (&IMDMeasureObjType);
      Py_DECREF (m);
      return NULL;
    }
  if (PyModule_AddObject (m, "IMDMetrics",
                          (PyObject *)IMDMeasureObj_analyze_type)
      < 0)
    {
      Py_DECREF (IMDMeasureObj_analyze_type);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
