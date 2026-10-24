/*
 * sample_clock_ext.c — handle extension: typed `SampleClock` over
 * `dp_sample_clock` (jm; gh-306).
 *
 * `SampleClock` wraps an opaque dp_sample_clock_t *; the resource logic
 * lives hand-written in the backing _core.c. This file is pure generated glue
 * — lifecycle, arg coercion, numpy marshaling, decoded-getter properties,
 * RAII.
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <math.h>
#include <numpy/arrayobject.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "doppler/timing/timing_core.h"

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

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
typedef struct
{
  PyObject_HEAD dp_sample_clock_t *h;
  int                              closed;
  int                              lent; /* gh-2346: the handle was lent */
  jm_inflight_t _jm_inflight;            /* gh-2272: GIL-free calls running */
} SampleClockObject;

static int
SampleClock_init (SampleClockObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "fs", "resync", NULL };
  double       fs       = 0.0;
  int          resync   = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d|i", kwlist, &fs, &resync))
    {
      return -1;
    }

  if (self->lent)
    {
      PyErr_SetString (PyExc_RuntimeError,
                       "SampleClock: its pointer was lent as a capsule; a "
                       "second __init__ would free it under the borrower");
      return -1;
    }
  if (!self->closed && self->h)
    {
      self->closed = 1;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->closed = 0;
          return -1;
        }
      free (self->h);
      self->h      = NULL;
      self->closed = 1;
    }
  self->h = (dp_sample_clock_t *)malloc (sizeof (dp_sample_clock_t));
  if (!self->h)
    {
      PyErr_NoMemory ();
      return -1;
    }
  dp_sample_clock_init (self->h, fs, resync);
  self->closed = 0;

  return 0;
}

static PyObject *
SampleClock_pace (SampleClockObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "count", NULL };
  PyObject          *count_obj = NULL;
  unsigned long long count_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &count_obj))
    return NULL;
  if (count_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (count_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      count_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((count_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || count_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "count: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t count = (size_t)count_raw;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  double r;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    r = dp_sample_clock_pace (self->h, count);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  return PyFloat_FromDouble (r);
}

static PyObject *
SampleClock_stamp (SampleClockObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  uint64_t r;
  r = dp_sample_clock_stamp (self->h);
  return PyLong_FromUnsignedLongLong ((unsigned long long)r);
}

static PyObject *
SampleClock_stamp_at (SampleClockObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "n", NULL };
  PyObject          *n_obj    = NULL;
  unsigned long long n_raw    = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &n_obj))
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
          || n_raw > (unsigned long long)UINT64_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n: %S is out of range for uint64_t [0, %llu]", _ix,
                        (unsigned long long)UINT64_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  uint64_t n = (uint64_t)n_raw;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  uint64_t r;
  r = dp_sample_clock_stamp_at (self->h, n);
  return PyLong_FromUnsignedLongLong ((unsigned long long)r);
}

static PyObject *
SampleClock_track (SampleClockObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "observed_timestamp_ns", "n_at_observation", "tolerance_ns", NULL };
  PyObject          *observed_timestamp_ns_obj = NULL;
  unsigned long long observed_timestamp_ns_raw = 0ULL;
  PyObject          *n_at_observation_obj      = NULL;
  unsigned long long n_at_observation_raw      = 0ULL;
  PyObject          *tolerance_ns_obj          = NULL;
  unsigned long long tolerance_ns_raw          = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOO", kwlist,
                                    &observed_timestamp_ns_obj,
                                    &n_at_observation_obj, &tolerance_ns_obj))
    return NULL;
  if (observed_timestamp_ns_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (observed_timestamp_ns_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      observed_timestamp_ns_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((observed_timestamp_ns_raw == (unsigned long long)-1
           && PyErr_Occurred ())
          || observed_timestamp_ns_raw > (unsigned long long)UINT64_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "observed_timestamp_ns: %S is out of range for "
                        "uint64_t [0, %llu]",
                        _ix, (unsigned long long)UINT64_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  uint64_t observed_timestamp_ns = (uint64_t)observed_timestamp_ns_raw;
  if (n_at_observation_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_at_observation_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_at_observation_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_at_observation_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_at_observation_raw > (unsigned long long)UINT64_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "n_at_observation: %S is out of range for uint64_t [0, %llu]",
              _ix, (unsigned long long)UINT64_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  uint64_t n_at_observation = (uint64_t)n_at_observation_raw;
  if (tolerance_ns_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (tolerance_ns_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      tolerance_ns_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((tolerance_ns_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || tolerance_ns_raw > (unsigned long long)UINT64_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "tolerance_ns: %S is out of range for uint64_t [0, %llu]", _ix,
              (unsigned long long)UINT64_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  uint64_t tolerance_ns = (uint64_t)tolerance_ns_raw;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  int r;
  r = dp_sample_clock_track (self->h, observed_timestamp_ns, n_at_observation,
                             tolerance_ns);
  return PyLong_FromLong ((long)r);
}

static PyObject *
SampleClock_reset (SampleClockObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  dp_sample_clock_reset (self->h);
  Py_RETURN_NONE;
}

static PyObject *
SampleClock_resync (SampleClockObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  dp_sample_clock_resync (self->h);
  Py_RETURN_NONE;
}

static PyObject *
SampleClock_get_samples (SampleClockObject *self, void *closure)
{
  (void)closure;
  dp_sample_clock_t tmp;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  dp_sample_clock_stats (self->h, &tmp);
  return PyLong_FromUnsignedLongLong ((unsigned long long)tmp.n);
}

static PyObject *
SampleClock_get_underruns (SampleClockObject *self, void *closure)
{
  (void)closure;
  dp_sample_clock_t tmp;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  dp_sample_clock_stats (self->h, &tmp);
  return PyLong_FromUnsignedLongLong ((unsigned long long)tmp.underruns);
}

static PyObject *
SampleClock_get_max_lateness (SampleClockObject *self, void *closure)
{
  (void)closure;
  dp_sample_clock_t tmp;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  dp_sample_clock_stats (self->h, &tmp);
  return PyFloat_FromDouble (tmp.max_late_ns * 1e-9);
}

static PyObject *
SampleClock_get__capsule (SampleClockObject *self, void *Py_UNUSED (closure))
{
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  /* Lent, not owned: the capsule never frees the pointer, and
     holds SampleClock, so the pointer lives as long as it. */
  /* gh-2346: lent once, so close() leaves the free to dealloc. */
  self->lent = 1;
  return jm_capsule_lend ((void *)(self->h), "doppler.wfm.dp_sample_clock",
                          (PyObject *)self);
}
static PyGetSetDef SampleClock_getset[]
    = { { "samples", (getter)SampleClock_get_samples, NULL, NULL, NULL },
        { "underruns", (getter)SampleClock_get_underruns, NULL, NULL, NULL },
        { "max_lateness", (getter)SampleClock_get_max_lateness, NULL, NULL,
          NULL },
        { "_capsule", (getter)SampleClock_get__capsule, NULL,
          "Borrowed doppler.wfm.dp_sample_clock capsule for this handle.\n\n"
          "It never frees the handle, and keeps this object alive;\n"
          "it is valid until the handle is closed.",
          NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
SampleClock_close (SampleClockObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->h)
    {
      self->closed = 1;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->closed = 0;
          return NULL;
        }
      if (self->lent)
        {
          self->closed = 1;
        }
      else
        {
          free (self->h);
          self->closed = 1;
        }
    }
  Py_RETURN_NONE;
}

static void
SampleClock_dealloc (SampleClockObject *self)
{
  if (self->h && (!self->closed || self->lent))
    {
      free (self->h);
    }
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyMethodDef SampleClock_methods[] = {
  { "pace", (PyCFunction)(void (*) (void))SampleClock_pace,
    METH_VARARGS | METH_KEYWORDS,
    "Advance by count samples and sleep until that block's deadline\n"
    "(``epoch + n/fs``). Returns the slack in seconds measured before\n"
    "sleeping: ``>= 0`` means early (and it slept that long); ``< 0`` means\n"
    "it arrived late — an underrun, which is counted (and the epoch\n"
    "re-anchored when ``resync`` is set), with no sleep.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Output.\n" },
  { "stamp", (PyCFunction)SampleClock_stamp, METH_VARARGS,
    "Ideal wall-clock timestamp (ns since the UNIX epoch) of the next\n"
    "sample to be produced — sample index ``n``. Call it before pace() to\n"
    "tag the block you are about to emit, or after to tag the following\n"
    "block. Equivalent to ``dp_sample_clock_stamp_at(c, c->n)``.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "stamp_at", (PyCFunction)(void (*) (void))SampleClock_stamp_at,
    METH_VARARGS | METH_KEYWORDS,
    "Ideal wall-clock timestamp (ns since the UNIX epoch) of an ARBITRARY\n"
    "sample index n — past, present, or future, not just the clock's own\n"
    "live position. The receive-side counterpart of dp_sample_clock_stamp():\n"
    "a block emitting several per-record outputs from one buffered input\n"
    "(e.g. several detections spanning different epochs from one streamed\n"
    "message) stamps each at its own historical sample offset instead of\n"
    "reusing the whole buffer's single arrival time.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "track", (PyCFunction)(void (*) (void))SampleClock_track,
    METH_VARARGS | METH_KEYWORDS,
    "Reconcile c's epoch_real_ns against one OBSERVED (timestamp, sample\n"
    "index) pair read off an incoming stream header — the receive-side dual\n"
    "of pace()'s resync: instead of sleeping toward a deadline, this adopts\n"
    "or corrects the epoch from ground truth the sender already stamped.\n"
    "\n"
    "The FIRST call always adopts observed_timestamp_ns as the epoch\n"
    "(``has_anchor`` starts false — a fresh clock has no real observation\n"
    "yet, so there is nothing to compare against). Every later call only\n"
    "re-anchors if the discrepancy between the observation and what the\n"
    "clock's current model predicts exceeds tolerance_ns (same\n"
    "step-correction semantics as pace()'s own resync, applied to tracking\n"
    "instead of sleeping) — this corrects accumulated epoch OFFSET only, it\n"
    "does not model sample-rate SKEW, exactly like pace()'s resync.\n"
    "\n"
    "Rejects (no-op, returns 0) any observation with n_at_observation less\n"
    "than the clock's current n outright: a stale, out-of-order, or\n"
    "redelivered header must never walk the epoch backward. Never treat two\n"
    "reconciled observations as literal replay-safe state — always resync\n"
    "from an ARRIVING message, not a cached one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "observed_timestamp_ns : int\n"
    "    Input.\n"
    "n_at_observation : int\n"
    "    Input.\n"
    "tolerance_ns : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Nonzero if this call adopted or re-anchored the epoch; 0 if it was\n"
    "    accepted as already consistent, or rejected as stale.\n" },
  { "reset", (PyCFunction)SampleClock_reset, METH_VARARGS,
    "Re-capture both epochs and zero the counters — a fresh clock at n=0.\n" },
  { "resync", (PyCFunction)SampleClock_resync, METH_VARARGS,
    "Re-anchor the pacing epoch to \"now\" without clearing ``n`` or\n"
    "counters, dropping any accumulated lateness so future blocks pace\n"
    "forward from the present. (pace() does this automatically when\n"
    "``resync`` is set.)\n" },
  { "close", (PyCFunction)SampleClock_close, METH_NOARGS,
    "Release the handle and free resources. Once its `_capsule` has been "
    "taken, close() only marks it closed: the teardown waits until the last "
    "borrower lets go (gh-2346), so a file or socket it holds is not released "
    "here, and a status the close function would report is not raised." },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject SampleClockType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.wfm.SampleClock",
  .tp_basicsize                           = sizeof (SampleClockObject),
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_new                                 = PyType_GenericNew,
  .tp_init                                = (initproc)SampleClock_init,
  .tp_dealloc                             = (destructor)SampleClock_dealloc,
  .tp_getset                              = SampleClock_getset,
  .tp_methods                             = SampleClock_methods,
  .tp_doc = PyDoc_STR ("SampleClock handle.\n"
                       "\n"
                       "Parameters\n"
                       "----------\n"
                       "fs : float\n"
                       "resync : int, default 0\n"),
};

static struct PyModuleDef _moduledef = {
  PyModuleDef_HEAD_INIT, "sample_clock", NULL, -1, NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_sample_clock (void)
{
  import_array ();
  if (PyType_Ready (&SampleClockType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&SampleClockType);
  if (PyModule_AddObject (m, "SampleClock", (PyObject *)&SampleClockType) < 0)
    {
      Py_DECREF (&SampleClockType);
      Py_DECREF (m);
      return NULL;
    }
  /* gh-1117: adopt dp_interrupt_guard's process-global state from its owner.
   */
  {
    void     *dp_interrupt_guard_state_ptr (void);
    void      dp_interrupt_guard_state_adopt (void *shared);
    PyObject *_own = PyImport_ImportModule ("doppler.interrupt.interrupt");
    if (!_own)
      {
        Py_DECREF (m);
        return NULL;
      }
    PyObject *_pg = PyObject_GetAttrString (_own, "_jm_pg_dp_interrupt_guard");
    Py_DECREF (_own);
    if (!_pg)
      {
        Py_DECREF (m);
        return NULL;
      }
    void *_p = PyCapsule_GetPointer (
        _pg, "doppler.dp_interrupt_guard._jm_procglobal");
    Py_DECREF (_pg);
    if (!_p)
      {
        Py_DECREF (m);
        return NULL;
      }
    dp_interrupt_guard_state_adopt (_p);
  }
  return m;
}
