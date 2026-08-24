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
#include <complex.h>
#include <math.h>
#include <numpy/arrayobject.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "timing/timing_core.h"

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

typedef struct
{
  PyObject_HEAD dp_sample_clock_t *h;
  int                              closed;
} SampleClockObject;

static int
SampleClock_init (SampleClockObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "fs", "resync", NULL };
  double       fs       = 0;
  int          resync   = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d|i", kwlist, &fs, &resync))
    {
      return -1;
    }

  if (!self->closed && self->h)
    {
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
  static char *kwlist[] = { "count", NULL };
  size_t       count;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", kwlist, &count))
    return NULL;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "SampleClock is closed");
      return NULL;
    }
  double r;
  Py_BEGIN_ALLOW_THREADS
    r = dp_sample_clock_pace (self->h, count);
  Py_END_ALLOW_THREADS
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
  static char *kwlist[] = { "n", NULL };
  uint64_t     n;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", kwlist, &n))
    return NULL;
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
  uint64_t observed_timestamp_ns;
  uint64_t n_at_observation;
  uint64_t tolerance_ns;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KKK", kwlist,
                                    &observed_timestamp_ns, &n_at_observation,
                                    &tolerance_ns))
    return NULL;
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
  /* Borrowed: NULL destructor, so the capsule never
     frees a pointer SampleClock still owns. */
  return PyCapsule_New ((void *)(self->h), "doppler.wfm.dp_sample_clock",
                        NULL);
}
static PyGetSetDef SampleClock_getset[]
    = { { "samples", (getter)SampleClock_get_samples, NULL, NULL, NULL },
        { "underruns", (getter)SampleClock_get_underruns, NULL, NULL, NULL },
        { "max_lateness", (getter)SampleClock_get_max_lateness, NULL, NULL,
          NULL },
        { "_capsule", (getter)SampleClock_get__capsule, NULL,
          "Borrowed doppler.wfm.dp_sample_clock capsule for this handle.\n\n"
          "Non-owning: the capsule does not free the handle, and\n"
          "is only valid while this object is alive and open.",
          NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
SampleClock_close (SampleClockObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->h)
    {
      free (self->h);
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static void
SampleClock_dealloc (SampleClockObject *self)
{
  if (!self->closed && self->h)
    {
      free (self->h);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyMethodDef SampleClock_methods[] = {
  { "pace", (PyCFunction)SampleClock_pace, METH_VARARGS | METH_KEYWORDS,
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
  { "stamp_at", (PyCFunction)SampleClock_stamp_at,
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
  { "track", (PyCFunction)SampleClock_track, METH_VARARGS | METH_KEYWORDS,
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
    "Release the handle and free resources." },
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
  .tp_doc = PyDoc_STR ("SampleClock — handle over `dp_sample_clock`."),
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
