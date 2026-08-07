/*
 * telemetry_ext_capture.c — Capture type for the telemetry module.
 *
 * Included by telemetry_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only telemetry_ext.c is compiled.
 */
/* ======================================================== */
/* CaptureObject — wraps dp_tlm_capture_state_t *       */
/* ======================================================== */

#include "dp_tlm_capture/dp_tlm_capture_core.h"

typedef struct
{
  PyObject_HEAD dp_tlm_capture_state_t *handle;
  PyObject                             *_tlm_owner;
  PyObject                             *_clock_owner;
} CaptureObject;

static void
CaptureObj_dealloc (CaptureObject *self)
{
  if (self->handle)
    {
      /* gh-541: tp_dealloc has no exception context — there
         is no caller to raise to, and an in-flight exception
         must not be clobbered. Discarding the status is the
         only correct choice here; the explicit teardown and
         __exit__ paths do report it. */
      (void)dp_tlm_capture_destroy (self->handle);
    }
  Py_XDECREF (self->_tlm_owner);
  Py_XDECREF (self->_clock_owner);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CaptureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CaptureObject *self = (CaptureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CaptureObj_init (CaptureObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "tlm", "block_samples", "path", "clock", NULL };
  PyObject    *tlm_obj  = NULL;
  unsigned long long block_samples_raw = 0ULL;
  PyObject          *path              = NULL; /* fspath -> bytes */
  PyObject          *clock_obj         = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OKO&O", kwlist, &tlm_obj,
                                    &block_samples_raw, PyUnicode_FSConverter,
                                    &path, &clock_obj))
    {
      Py_XDECREF (path);
      return -1;
    }
  dp_tlm_t *tlm = NULL;
  if (tlm_obj == Py_None || tlm_obj == NULL)
    {
      PyErr_SetString (
          PyExc_TypeError,
          "tlm is required and cannot be None;"
          " pass the doppler.telemetry.dp_tlm capsule or an object"
          " exposing it as ._capsule");
      {
        Py_XDECREF (path);
        return -1;
      }
    }
  PyObject *tlm_cap = tlm_obj;
  Py_INCREF (tlm_cap);
  if (!PyCapsule_CheckExact (tlm_cap))
    {
      Py_DECREF (tlm_cap);
      tlm_cap = PyObject_GetAttrString (tlm_obj, "_capsule");
      if (!tlm_cap)
        {
          if (!PyErr_ExceptionMatches (PyExc_AttributeError))
            {
              Py_XDECREF (path);
              return -1;
            }
          PyErr_Clear ();
          PyErr_Format (PyExc_TypeError,
                        "tlm must be the doppler.telemetry.dp_tlm capsule"
                        " or an object exposing it as ._capsule,"
                        " not %s",
                        Py_TYPE (tlm_obj)->tp_name);
          {
            Py_XDECREF (path);
            return -1;
          }
        }
    }
  tlm = (dp_tlm_t *)PyCapsule_GetPointer (tlm_cap, "doppler.telemetry.dp_tlm");
  Py_DECREF (tlm_cap);
  if (!tlm)
    {
      Py_XDECREF (path);
      return -1;
    }
  Py_INCREF (tlm_obj);
  Py_XSETREF (self->_tlm_owner, tlm_obj);
  size_t                   block_samples = (size_t)block_samples_raw;
  const dp_sample_clock_t *clock         = NULL;
  if (clock_obj == Py_None || clock_obj == NULL)
    {
      PyErr_SetString (
          PyExc_TypeError,
          "clock is required and cannot be None;"
          " pass the doppler.wfm.dp_sample_clock capsule or an object"
          " exposing it as ._capsule");
      {
        Py_XDECREF (path);
        return -1;
      }
    }
  PyObject *clock_cap = clock_obj;
  Py_INCREF (clock_cap);
  if (!PyCapsule_CheckExact (clock_cap))
    {
      Py_DECREF (clock_cap);
      clock_cap = PyObject_GetAttrString (clock_obj, "_capsule");
      if (!clock_cap)
        {
          if (!PyErr_ExceptionMatches (PyExc_AttributeError))
            {
              Py_XDECREF (path);
              return -1;
            }
          PyErr_Clear ();
          PyErr_Format (PyExc_TypeError,
                        "clock must be the doppler.wfm.dp_sample_clock capsule"
                        " or an object exposing it as ._capsule,"
                        " not %s",
                        Py_TYPE (clock_obj)->tp_name);
          {
            Py_XDECREF (path);
            return -1;
          }
        }
    }
  clock = (const dp_sample_clock_t *)PyCapsule_GetPointer (
      clock_cap, "doppler.wfm.dp_sample_clock");
  Py_DECREF (clock_cap);
  if (!clock)
    {
      Py_XDECREF (path);
      return -1;
    }
  Py_INCREF (clock_obj);
  Py_XSETREF (self->_clock_owner, clock_obj);
  self->handle = dp_tlm_capture_open (tlm, block_samples,
                                      PyBytes_AS_STRING (path), clock);
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "capture could not be opened: attach every probe "
                       "BEFORE opening (the ring is sized from the probe "
                       "table, so no probes means no bound), pass a non-zero "
                       "block_samples, use a context with no capture already "
                       "open, and — for the file flavour — a writable path");
      return -1;
    }
  return 0;
}

static PyObject *
CaptureObj_block (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_tlm_capture_block (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "block failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CaptureObj_close (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_tlm_capture_close (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "the capture has a hole: records were dropped, which the "
                    "block bound makes impossible unless a step ran longer "
                    "than block_samples or no boundary was reached at all — "
                    "see Capture.dropped",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Capture_getprop_count (CaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_capture_count (self->handle)));
}
static PyObject *
Capture_getprop_dropped (CaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_capture_dropped (self->handle)));
}

static PyGetSetDef Capture_getset[]
    = { { "count", (getter)Capture_getprop_count, NULL,
          "Records captured so far, across memory and file alike.\n", NULL },
        { "dropped", (getter)Capture_getprop_dropped, NULL,
          "Records the ring dropped during THIS capture (latched at open "
          "against the context's monotonic counter). Non-zero means a hole.\n",
          NULL },
        { NULL } };

static PyObject *
CaptureObj_destroy (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      int rc = dp_tlm_capture_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_ValueError,
                           "the capture has a hole: records were dropped, "
                           "which the block bound makes impossible unless a "
                           "step ran longer than block_samples or no "
                           "boundary was reached at all — see "
                           "Capture.dropped");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyObject *
CaptureObj_enter (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CaptureObj_exit (CaptureObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      int rc = dp_tlm_capture_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_ValueError,
                           "the capture has a hole: records were dropped, "
                           "which the block bound makes impossible unless a "
                           "step ran longer than block_samples or no "
                           "boundary was reached at all — see "
                           "Capture.dropped");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyMethodDef CaptureObj_methods[] = {

  { "block", (PyCFunction)CaptureObj_block, METH_NOARGS,
    "block() -> int\n"
    "\n"
    "Block boundary: drains the ring to empty.\n"
    "\n"
    "Grows the ring first if probes appeared since the last boundary, which\n"
    "is safe precisely here — the ring is about to be emptied and the\n"
    "producer is between blocks. Then copies everything available into the\n"
    "active staging buffer, handing it to the sink and swapping when it can\n"
    "no longer hold another block.\n"
    "\n"
    "**May block** in file mode, if the writer still holds the other buffer.\n"
    "That wait is the backpressure that keeps the capture lossless; it\n"
    "happens at the boundary, never inside the DSP loop.\n"
    "\n"
    "Usually reached through dp_tlm_set_now() rather than called directly.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry, MemoryCapture\n"
    ">>> from doppler.wfm import SampleClock\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")\n"
    ">>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))\n"
    ">>> tlm.emit(pid, 1.5)\n"
    "\n"
    "An explicit boundary; set_now() reaches this for you:\n"
    "\n"
    ">>> cap.block()\n"
    ">>> cap.count\n"
    "1\n" },
  { "close", (PyCFunction)CaptureObj_close, METH_NOARGS,
    "close() -> int\n"
    "\n"
    "Final boundary, then flush, join, and write the sidecar.\n"
    "\n"
    "Sweeps the tail the last block left behind, drains the staging buffers,\n"
    "joins the writer thread, closes the file and writes `<path>-meta`.\n"
    "Idempotent: a second call is a no-op returning the first call's\n"
    "verdict.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry, MemoryCapture\n"
    ">>> from doppler.wfm import SampleClock\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")\n"
    ">>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))\n"
    ">>> for blk in range(4):\n"
    "...     tlm.set_now(blk * 256)\n"
    "...     tlm.emit(pid, float(blk))\n"
    ">>> cap.close()          # silent: the block contract was honoured\n"
    ">>> cap.close()          # idempotent, same verdict\n"
    "\n"
    "Breaking the contract -- here, never reaching a boundary at all -- is "
    "the\n"
    "one way to lose a record, and it is reported rather than absorbed:\n"
    "\n"
    ">>> tlm2 = Telemetry(1 << 12)\n"
    ">>> p2 = tlm2.probe(\"x\")\n"
    ">>> bad = MemoryCapture(tlm2, 8, SampleClock(1e6))\n"
    ">>> for i in range(20000):\n"
    "...     tlm2.emit(p2, float(i))\n"
    ">>> bad.close()  # doctest: +ELLIPSIS\n"
    "Traceback (most recent call last):\n"
    "ValueError: the capture has a hole: ...\n" },
  { "destroy", (PyCFunction)CaptureObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C destructor reports failure. Raised from an explicit call\n"
    "    and from ``__exit__`` alike, so a failing teardown propagates out\n"
    "    of a ``with`` block (gh-541).\n" },
  { "__enter__", (PyCFunction)CaptureObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a DpTlmCapture be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "DpTlmCapture\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CaptureObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the DpTlmCapture.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never\n"
    "suppresses one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
  { NULL }
};

static PyTypeObject CaptureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "telemetry.Capture",
  .tp_basicsize                           = sizeof (CaptureObject),
  .tp_dealloc                             = (destructor)CaptureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Capture type.\n",
  .tp_methods                             = CaptureObj_methods,
  .tp_getset                              = Capture_getset,
  .tp_new                                 = CaptureObj_new,
  .tp_init                                = (initproc)CaptureObj_init,
};
