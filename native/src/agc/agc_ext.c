/*
 * agc_ext.c — Python extension module agc
 *
 * Objects: AGC
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/agc/agc_core.h"

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

#include "agc_ext_agc.c"

static PyObject *
_bind_settling_samples (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]
      = { "loop_bw", "alpha", "gain_err_db", "tol_db", NULL };
  double loop_bw     = 0.0;
  double alpha       = 0.0;
  double gain_err_db = 0.0;
  double tol_db      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dddd", _kwlist, &loop_bw,
                                    &alpha, &gain_err_db, &tol_db))
    return NULL;
  return PyLong_FromUnsignedLongLong ((unsigned long long)dp_settling_samples (
      loop_bw, alpha, gain_err_db, tol_db));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef agc_module_methods[] = {
  { "settling_samples", (PyCFunction)(void *)_bind_settling_samples,
    METH_VARARGS | METH_KEYWORDS,
    "How many samples this loop needs to settle -- the design query a caller "
    "sizing a warm-up budget, a burst preamble or an acquisition guard has to "
    "answer. 1/(4*loop_bw) is the loop FILTER's time constant and not the "
    "object's: the detector sits inside the loop and measures in power, so a "
    "quiet input settles more slowly. Measured, the multiplier runs from "
    "about 0.8 on a loud start to nearly 5 on a quiet one with a slow "
    "detector. This runs the real loop against a constant input and counts, "
    "so there is no fitted curve to go stale. gain_err_db is POSITIVE for a "
    "quiet input, which is the slow direction and the one to budget for. "
    "Returns 0 rather than a plausible guess when the arguments are invalid. "
    "Design-time only: it allocates and iterates.\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef agc_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "agc",
  .m_doc     = "Automatic gain control: a log-domain feedback AGC (AGC) that "
               "drives signal power toward a reference level.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> import numpy as np\n"
               ">>> from doppler.agc import AGC\n"
               ">>> y = AGC(ref_db=0.0, loop_bw=0.05).steps(0.1 * np.ones(200, "
               "np.complex64))\n"
               ">>> bool(abs(y[-1]) > abs(y[0]))\n"
               "True\n",
  .m_size    = -1,
  .m_methods = agc_module_methods,
};

PyMODINIT_FUNC
PyInit_agc (void)
{
  import_array ();
  if (PyType_Ready (&AGCObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&agc_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&AGCObjType);
  if (PyModule_AddObject (m, "AGC", (PyObject *)&AGCObjType) < 0)
    {
      Py_DECREF (&AGCObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
