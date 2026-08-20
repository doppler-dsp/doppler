/*
 * ber_ext_frame_meter.c — FrameMeter type for the ber module.
 *
 * Included by ber_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only ber_ext.c is compiled.
 */
/* ======================================================== */
/* FrameMeterObject — wraps frame_meter_state_t *       */
/* ======================================================== */

#include "frame_meter/frame_meter_core.h"

typedef struct
{
  PyObject_HEAD frame_meter_state_t *handle;
} FrameMeterObject;

static void
FrameMeterObj_dealloc (FrameMeterObject *self)
{
  if (self->handle)
    frame_meter_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FrameMeterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FrameMeterObject *self = (FrameMeterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FrameMeterObj_init (FrameMeterObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]          = { "target_errors", "conf", NULL };
  unsigned long long target_errors_raw = 200;
  double             conf              = 0.99;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kd", kwlist,
                                    &target_errors_raw, &conf))
    return -1;
  size_t target_errors = (size_t)target_errors_raw;
  self->handle         = frame_meter_create (target_errors, conf);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError, "conf must lie in (0, 1)");
      return -1;
    }
  return 0;
}

static PyObject *
FrameMeterObj_reset (FrameMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  frame_meter_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FrameMeterObj_add (FrameMeterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "sync_ok", "crc", NULL };
  int          sync_ok   = 0;
  int          crc       = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ii", _kwlist, &sync_ok, &crc))
    return NULL;
  frame_meter_add (self->handle, sync_ok, crc);
  Py_RETURN_NONE;
}

static PyStructSequence_Field FrameMeterObj_fer_fields[] = {
  { "p_hat", "Unbiased point estimate `(r-1)/(N-1)`." },
  { "lo", "Lower confidence limit." },
  { "hi", "Upper confidence limit." },
  { "rel", "Relative standard error `1/sqrt(r)`." },
  { "conf", "config: confidence level for the interval" },
  { "errors", "running: frames not delivered" },
  { "symbols", "`N` (or bits, for a BER)." },
  { NULL, NULL },
};
static PyStructSequence_Desc FrameMeterObj_fer_desc
    = { "doppler.ber.BerInterval",
        "Error-rate point estimate with a Gamma/chi-square confidence "
        "interval. Assert on `lo`, never `p_hat`.",
        FrameMeterObj_fer_fields, 7 };
static PyTypeObject *FrameMeterObj_fer_type = NULL;

static PyObject *
FrameMeterObj_fer (FrameMeterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!FrameMeterObj_fer_type)
    {
      FrameMeterObj_fer_type
          = PyStructSequence_NewType (&FrameMeterObj_fer_desc);
      if (!FrameMeterObj_fer_type)
        return NULL;
    }
  ber_interval_t _r = frame_meter_fer (self->handle);
  PyObject      *_o = PyStructSequence_New (FrameMeterObj_fer_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.p_hat));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.lo));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.hi));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.rel));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.conf));
  PyStructSequence_SET_ITEM (
      _o, 5, PyLong_FromUnsignedLongLong ((unsigned long long)_r.errors));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLongLong ((unsigned long long)_r.symbols));
  return _o;
}

static PyStructSequence_Field FrameMeterObj_sync_miss_fields[] = {
  { "p_hat", "Unbiased point estimate `(r-1)/(N-1)`." },
  { "lo", "Lower confidence limit." },
  { "hi", "Upper confidence limit." },
  { "rel", "Relative standard error `1/sqrt(r)`." },
  { "conf", "config: confidence level for the interval" },
  { "errors", "running: frames not delivered" },
  { "symbols", "`N` (or bits, for a BER)." },
  { NULL, NULL },
};
static PyStructSequence_Desc FrameMeterObj_sync_miss_desc
    = { "doppler.ber.BerInterval",
        "Error-rate point estimate with a Gamma/chi-square confidence "
        "interval. Assert on `lo`, never `p_hat`.",
        FrameMeterObj_sync_miss_fields, 7 };
static PyTypeObject *FrameMeterObj_sync_miss_type = NULL;

static PyObject *
FrameMeterObj_sync_miss (FrameMeterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!FrameMeterObj_sync_miss_type)
    {
      FrameMeterObj_sync_miss_type
          = PyStructSequence_NewType (&FrameMeterObj_sync_miss_desc);
      if (!FrameMeterObj_sync_miss_type)
        return NULL;
    }
  ber_interval_t _r = frame_meter_sync_miss (self->handle);
  PyObject      *_o = PyStructSequence_New (FrameMeterObj_sync_miss_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.p_hat));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.lo));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.hi));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.rel));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.conf));
  PyStructSequence_SET_ITEM (
      _o, 5, PyLong_FromUnsignedLongLong ((unsigned long long)_r.errors));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLongLong ((unsigned long long)_r.symbols));
  return _o;
}

static PyObject *
FrameMeterObj_state_bytes (FrameMeterObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (frame_meter_state_bytes (self->handle));
}

static PyObject *
FrameMeterObj_get_state (FrameMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = frame_meter_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  frame_meter_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
FrameMeterObj_set_state (FrameMeterObject *self, PyObject *arg)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!PyBytes_Check (arg))
    {
      PyErr_SetString (PyExc_TypeError, "set_state expects bytes");
      return NULL;
    }
  if ((size_t)PyBytes_GET_SIZE (arg) != frame_meter_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (frame_meter_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
FrameMeter_getprop_frames (FrameMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)frame_meter_get_frames (self->handle));
}
static PyObject *
FrameMeter_getprop_sync_detected (FrameMeterObject *self,
                                  void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)frame_meter_get_sync_detected (self->handle));
}
static PyObject *
FrameMeter_getprop_crc_passed (FrameMeterObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)frame_meter_get_crc_passed (self->handle));
}
static PyObject *
FrameMeter_getprop_errors (FrameMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)frame_meter_get_errors (self->handle));
}
static PyObject *
FrameMeter_getprop_enough (FrameMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)frame_meter_get_enough (self->handle));
}

static PyGetSetDef FrameMeter_getset[]
    = { { "frames", (getter)FrameMeter_getprop_frames, NULL,
          "Frames attempted.\n", NULL },
        { "sync_detected", (getter)FrameMeter_getprop_sync_detected, NULL,
          "Frames whose sync word was detected.\n", NULL },
        { "crc_passed", (getter)FrameMeter_getprop_crc_passed, NULL,
          "Frames whose CRC checked.\n", NULL },
        { "errors", (getter)FrameMeter_getprop_errors, NULL,
          "Frames not delivered: no sync detected, or a failed CRC.\n", NULL },
        { "enough", (getter)FrameMeter_getprop_enough, NULL,
          "True once `target_errors` frame errors have accumulated -- the "
          "stopping condition, so a caller loops records until the "
          "measurement has the precision it asked for rather than until a "
          "frame count someone guessed.\n",
          NULL },
        { NULL } };

static PyObject *
FrameMeterObj_destroy (FrameMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      frame_meter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FrameMeterObj_enter (FrameMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FrameMeterObj_exit (FrameMeterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      frame_meter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FrameMeterObj_methods[] = {
  { "reset", (PyCFunction)FrameMeterObj_reset, METH_NOARGS,
    "Clear every counter; the configuration is untouched.\n" },

  { "add", (PyCFunction)(void *)FrameMeterObj_add,
    METH_VARARGS | METH_KEYWORDS,
    "add(sync_ok, crc) -> None\n"
    "\n"
    "Record one frame's outcome. `sync_ok` is the DETECTOR's own decision\n"
    "(ber_align_t.ok, or burst_demod's frame offset validity) -- never a\n"
    "threshold applied afterwards to a statistic. `crc` is\n"
    "wfm_frame_crc_ok()'s return passed straight through: 1 pass, 0 fail, -1\n"
    "the frame carries no CRC. A frame is an error when its sync was not\n"
    "detected, or when it was and the CRC failed; with no CRC carried, a\n"
    "detected frame counts as delivered, because counting it as an error\n"
    "would measure the frame format rather than the receiver.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sync_ok : int\n"
    "    non-zero when the frame's sync word was detected. Pass the\n"
    "    detector's own decision — `ber_align_t::ok`, or `burst_demod`'s\n"
    "    frame_offset validity — never a threshold applied afterwards to a\n"
    "    statistic.\n"
    "crc : int\n"
    "    `wfm_frame_crc_ok()`'s return, passed straight through: 1 pass, 0\n"
    "    fail, -1 the frame carries no CRC. A frame counts as an error when\n"
    "    its sync was not detected, or when it was and the CRC failed. With\n"
    "    `crc = -1` a detected frame counts as delivered, because nothing\n"
    "    about it can be checked.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ber import FrameMeter\n"
    ">>> met = FrameMeter(target_errors=10)\n"
    ">>> met.add(1, 1)    # found, and it checked\n"
    ">>> met.add(1, 0)    # found, and the CRC failed\n"
    ">>> met.add(0, 0)    # never found: still a frame you did not deliver\n"
    ">>> met.add(1, -1)   # found, no CRC: delivered but not CHECKED\n"
    ">>> met.frames, met.sync_detected, met.crc_passed, met.errors\n"
    "(4, 3, 1, 2)\n" },
  { "fer", (PyCFunction)FrameMeterObj_fer, METH_VARARGS,
    "fer() -> BerInterval record (p_hat, lo, hi, rel, conf, errors, symbols)\n"
    "\n"
    "Frame error rate with its exact interval, as a BerInterval. Assert\n"
    "on `lo`, never on `p_hat`: comparing the lower limit against a spec is\n"
    "the form that cannot flake on counting noise. A frame that was never\n"
    "detected counts as an error -- a frame you did not detect is a frame\n"
    "you did not deliver.\n"
    "\n"
    "`ber_confidence(errors, frames, conf)` — the same interval `ber_meter`\n"
    "reports, which is generic over trials and therefore applies to frames\n"
    "unchanged. Assert on `lo`, never on `p_hat`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BerInterval\n"
    "    the rate with its exact interval.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ber import FrameMeter\n"
    ">>> met = FrameMeter(target_errors=4)\n"
    ">>> for i in range(20):\n"
    "...     met.add(1, 0 if i % 5 == 0 else 1)\n"
    ">>> met.enough\n"
    "1\n"
    ">>> fer = met.fer()\n"
    ">>> round(fer.p_hat, 3), fer.lo < fer.p_hat < fer.hi\n"
    "(0.158, True)\n" },
  { "sync_miss", (PyCFunction)FrameMeterObj_sync_miss, METH_VARARGS,
    "sync_miss() -> BerInterval record (p_hat, lo, hi, rel, conf, errors, "
    "symbols)\n"
    "\n"
    "Sync MISS rate with its exact interval, as a BerInterval. Reported\n"
    "as a miss rather than a detection rate so it is an ERROR rate like\n"
    "every other number in this module and the same interval applies\n"
    "unchanged. This is what turns 'is this sync word long enough at this\n"
    "Es/N0' into a measurement rather than a judgement.\n"
    "\n"
    "Reported as a miss rate rather than a detection rate so it is an ERROR\n"
    "rate like every other number here, and so the same interval applies\n"
    "without reinterpretation. **This is what turns \"is this sync word long\n"
    "enough at this Es/N0\" into a measurement** — `ber_align_detect()`\n"
    "already returns `margin_db` and `runner_db` per attempt, and\n"
    "accumulating the decisions is what answers the question with a number.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BerInterval\n"
    "    the miss rate with its exact interval.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ber import FrameMeter\n"
    ">>> met = FrameMeter()\n"
    ">>> for i in range(50):\n"
    "...     met.add(0 if i % 10 == 0 else 1, 1)\n"
    ">>> met.frames, met.sync_detected\n"
    "(50, 45)\n"
    ">>> miss = met.sync_miss()\n"
    ">>> round(miss.p_hat, 3), miss.hi > miss.p_hat\n"
    "(0.082, True)\n" },
  { "state_bytes", (PyCFunction)FrameMeterObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the FrameMeter has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)FrameMeterObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the FrameMeter has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)FrameMeterObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the FrameMeter has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)FrameMeterObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)FrameMeterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a FrameMeter be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "FrameMeter\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)FrameMeterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the FrameMeter.\n"
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

static PyTypeObject FrameMeterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ber.FrameMeter",
  .tp_basicsize                           = sizeof (FrameMeterObject),
  .tp_dealloc                             = (destructor)FrameMeterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an accumulator.\n"
                                            "\n"
                                            "Parameters\n"
                                            "----------\n"
                                            "target_errors : int, default 200\n"
                                            "    frame errors to accumulate before `enough`; 0 is taken as\n"
                                            "    BER_TARGET_ERRORS.\n"
                                            "conf : float, default 0.99\n"
                                            "    confidence level in (0, 1); 0 is taken as BER_CONF.\n"
                                            "\n"
                                            "Raises\n"
                                            "------\n"
                                            "ValueError\n"
                                            "    If construction fails. The exception message is ``conf must "
                                            "lie in (0,\n"
                                            "    1)``.\n"
                                            "\n"
                                            "Examples\n"
                                            "--------\n"
                                            "Create with defaults:\n"
                                            "\n"
                                            ">>> from doppler import FrameMeter\n"
                                            ">>> obj = FrameMeter(target_errors=200, conf=0.99)\n",
  .tp_methods                             = FrameMeterObj_methods,
  .tp_getset                              = FrameMeter_getset,
  .tp_new                                 = FrameMeterObj_new,
  .tp_init                                = (initproc)FrameMeterObj_init,
};
