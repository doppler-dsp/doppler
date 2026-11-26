/* jm:generated track_ext_matchedsymboltimingestimator.c */
/*
 * track_ext_matchedsymboltimingestimator.c — MatchedSymbolTimingEstimator type
 * for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in track_ext_matchedsymboltimingestimator_extra.c.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* MatchedSymbolTimingEstimatorObject — wraps dp_timing_est_state_t *       */
/* ======================================================== */

#include "doppler/timing_est/timing_est_core.h"

typedef struct
{
  PyObject_HEAD dp_timing_est_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
} MatchedSymbolTimingEstimatorObject;

static void
MatchedSymbolTimingEstimatorObj_dealloc (
    MatchedSymbolTimingEstimatorObject *self)
{
  if (self->handle)
    dp_timing_est_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MatchedSymbolTimingEstimatorObj_new (PyTypeObject *type, PyObject *args,
                                     PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  MatchedSymbolTimingEstimatorObject *self
      = (MatchedSymbolTimingEstimatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MatchedSymbolTimingEstimatorObj_init (MatchedSymbolTimingEstimatorObject *self,
                                      PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "sps", "pulse", "beta", "span", NULL };
  double             sps       = 8.0;
  const char        *pulse_str = "rrc";
  double             beta      = 0.35;
  PyObject          *span_obj  = NULL;
  unsigned long long span_raw  = 8;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dsdO", kwlist, &sps,
                                    &pulse_str, &beta, &span_obj))
    return -1;
  int pulse = 0;
  if (strcmp (pulse_str, "iandd") == 0)
    pulse = 0;
  else if (strcmp (pulse_str, "rrc") == 0)
    pulse = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "pulse must be one of \"iandd\", \"rrc\", got '%s'",
                    pulse_str);
      return -1;
    }
  if (span_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (span_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      span_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((span_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || span_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "span: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t span  = (size_t)span_raw;
  self->handle = dp_timing_est_create_matched (sps, pulse, beta, span);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "MatchedSymbolTimingEstimator: invalid parameter "
                       "(need a finite sps of 4 or more, pulse iandd or rrc, "
                       "0 <= beta <= 1, span >= 1)");
      return -1;
    }
  return 0;
}

static PyStructSequence_Field
    MatchedSymbolTimingEstimatorObj_estimate_fields[] = {
      { "tau",
        "Where the eye center sits on the estimator's symbol clock, in "
        "symbols, in (-1/2, 1/2]: the clock's phase at the instants the "
        "stream's power peaks. With a symbol-rate error it is the value at "
        "the middle of the block. For a pulse that is not band limited, read "
        "at a whole number of samples a symbol, it is off the eye center by "
        "where the samples fall in the symbol: up to 0.011 symbols for a "
        "rectangle at 4 (the validation report's finding "
        "`whole_n_rect_sampling_phase`). 0 when the block has no power." },
      { "rate",
        "How far the eye center moves on the clock each symbol of the clock, "
        "in symbols: for symbols that last `sps * (1 + eps)` samples it is "
        "`eps / (1 + eps)`. From how far `tau` moved since the previous "
        "block, so 0 until `has_rate` is 1, and right only while the timing "
        "moves under half a symbol between the two blocks' middles." },
      { "quality",
        "`|c| / sum |x|^2`, in [0, 1]: how much of the block's power is the "
        "symbol-rate line. Near 0 with no signal, with no transitions, or "
        "with a clock error that turns the line through a full cycle inside "
        "the block. 0 when the block has no power." },
      { "power", "Mean `|x|^2` over the block." },
      { "symbols",
        "The block's length in symbols of the clock: samples / sps." },
      { "has_rate",
        "1 when a previous block exists to take `rate` against, else 0." },
      { "eye", "Where an eye center sits on the INPUT's timeline: the sample "
               "index, counted from create or reset and not whole, of the eye "
               "center nearest the block's middle. Other eye centers are "
               "whole symbols (`sps` samples) either side. On a matched "
               "instance the converter's group delay and the lead-in it "
               "skipped are taken out, so the index is into the samples the "
               "estimate was handed. 0 when the block has no power." },
      { NULL, NULL },
    };
static PyStructSequence_Desc MatchedSymbolTimingEstimatorObj_estimate_desc
    = { "doppler.track.TimingEstimate",
        "One feedforward symbol-timing estimate: the eye center's place on "
        "the estimator's symbol clock, the symbol rate's error against the "
        "previous block, and how strong the symbol-rate line was.",
        MatchedSymbolTimingEstimatorObj_estimate_fields, 7 };
static PyTypeObject *MatchedSymbolTimingEstimatorObj_estimate_type = NULL;

static PyObject *
MatchedSymbolTimingEstimatorObj_estimate (
    MatchedSymbolTimingEstimatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  size_t n_in = (size_t)PyArray_SIZE (in_arr);
  if (!MatchedSymbolTimingEstimatorObj_estimate_type)
    {
      MatchedSymbolTimingEstimatorObj_estimate_type
          = PyStructSequence_NewType (
              &MatchedSymbolTimingEstimatorObj_estimate_desc);
      if (!MatchedSymbolTimingEstimatorObj_estimate_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  dp_timing_estimate_t  _r;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    _r = dp_timing_est_estimate (_jm_h, _ng0, n_in);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  PyObject *_o
      = PyStructSequence_New (MatchedSymbolTimingEstimatorObj_estimate_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.tau));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.rate));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.quality));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.power));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.symbols));
  PyStructSequence_SET_ITEM (_o, 5, PyLong_FromLong ((long)_r.has_rate));
  PyStructSequence_SET_ITEM (_o, 6, PyFloat_FromDouble (_r.eye));
  return _o;
}

static PyObject *
MatchedSymbolTimingEstimatorObj_reset (
    MatchedSymbolTimingEstimatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_timing_est_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
MatchedSymbolTimingEstimatorObj_state_bytes (
    MatchedSymbolTimingEstimatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_timing_est_state_bytes (self->handle));
}

static PyObject *
MatchedSymbolTimingEstimatorObj_get_state (
    MatchedSymbolTimingEstimatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_timing_est_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_timing_est_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
MatchedSymbolTimingEstimatorObj_set_state (
    MatchedSymbolTimingEstimatorObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg)
      != dp_timing_est_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_timing_est_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
MatchedSymbolTimingEstimator_getprop_sps (
    MatchedSymbolTimingEstimatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->sps);
}

static PyGetSetDef MatchedSymbolTimingEstimator_getset[]
    = { { "sps", (getter)MatchedSymbolTimingEstimator_getprop_sps, NULL,
          "Samples per symbol of the stream handed to estimate.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
MatchedSymbolTimingEstimatorObj_destroy (
    MatchedSymbolTimingEstimatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_timing_est_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
MatchedSymbolTimingEstimatorObj_enter (
    MatchedSymbolTimingEstimatorObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MatchedSymbolTimingEstimatorObj_exit (MatchedSymbolTimingEstimatorObject *self,
                                      PyObject                           *args)
{
  (void)args;
  if (self->handle)
    {
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_timing_est_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef MatchedSymbolTimingEstimatorObj_methods[] = {

  { "estimate", (PyCFunction)MatchedSymbolTimingEstimatorObj_estimate,
    METH_VARARGS,
    "estimate(x) -> TimingEstimate record (tau, rate, quality, power, "
    "symbols, has_rate, eye)\n"
    "\n"
    "Estimate the symbol timing from one block of samples (matched-filtered "
    "for SymbolTimingEstimator, as they arrive for "
    "MatchedSymbolTimingEstimator); returns a TimingEstimate. The block is "
    "the whole call, so its length sets the estimate's variance; consecutive "
    "calls stay on one symbol clock, and from the second call on the result "
    "also carries the symbol rate's error.\n"
    "\n"
    "The block is the whole call: its length sets the estimate's variance\n"
    "and the time the estimate takes. Consecutive calls stay on one symbol\n"
    "clock, and from the second call that had power the result carries the\n"
    "symbol rate's error.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Matched-filtered samples, `sps` a symbol.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "TimingEstimate\n"
    "    The block's estimate.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import SymbolTimingEstimator\n"
    ">>> n = np.arange(4 * 256)\n"
    ">>> # power peaking a quarter symbol into each symbol, on any carrier\n"
    ">>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * (n / 4 - 0.25)))\n"
    ">>> x = (x * np.exp(1j * 0.3 * n)).astype(np.complex64)\n"
    ">>> e = SymbolTimingEstimator(sps=4.0).estimate(x)\n"
    ">>> round(e.tau, 3), round(e.quality, 3), e.symbols, e.has_rate\n"
    "(0.25, 0.25, 256.0, 0)\n" },
  { "reset", (PyCFunction)MatchedSymbolTimingEstimatorObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Return the symbol clock to zero and forget the previous block and\n"
    "any block in progress; keep `sps`.\n"
    "\n"
    "The next estimate is then the first again: its `tau` is on a clock that\n"
    "starts at the next sample, and it carries no `rate`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import SymbolTimingEstimator\n"
    ">>> n = np.arange(1024)\n"
    ">>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * n / "
    "4)).astype(np.complex64)\n"
    ">>> te = SymbolTimingEstimator(sps=4.0)\n"
    ">>> te.estimate(x).has_rate, te.estimate(x).has_rate\n"
    "(0, 1)\n"
    ">>> te.reset()\n"
    ">>> te.estimate(x).has_rate\n"
    "0\n" },
  { "state_bytes", (PyCFunction)MatchedSymbolTimingEstimatorObj_state_bytes,
    METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the MatchedSymbolTimingEstimator has already\n"
    "been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)MatchedSymbolTimingEstimatorObj_get_state,
    METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures the state that evolves as the object runs, including any value\n"
    "a setter changed after construction, so a blob taken now and restored\n"
    "later resumes from this point. Construction parameters are not\n"
    "restored: restore into an object built the same way. A core may check\n"
    "some of them and refuse a blob built differently.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the MatchedSymbolTimingEstimator has already\n"
    "been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)MatchedSymbolTimingEstimatorObj_set_state,
    METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place, including any value a setter\n"
    "changed after construction. Construction parameters are not restored:\n"
    "restore into an object built the same way. Length is validated against\n"
    "`state_bytes()` before the blob is handed to the C core, which may\n"
    "reject it too: a core may check some construction parameters and refuse\n"
    "a blob built differently.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the MatchedSymbolTimingEstimator has already been\n"
    "destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)MatchedSymbolTimingEstimatorObj_destroy,
    METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)MatchedSymbolTimingEstimatorObj_enter,
    METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a MatchedSymbolTimingEstimator be used in a `with` statement so\n"
    "its C resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "MatchedSymbolTimingEstimator\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)MatchedSymbolTimingEstimatorObj_exit,
    METH_VARARGS,
    "Exit a context manager, releasing the MatchedSymbolTimingEstimator.\n"
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
  { NULL, NULL, 0, NULL }
};

static PyTypeObject MatchedSymbolTimingEstimatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name
  = "doppler.track.MatchedSymbolTimingEstimator",
  .tp_basicsize = sizeof (MatchedSymbolTimingEstimatorObject),
  .tp_dealloc   = (destructor)MatchedSymbolTimingEstimatorObj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create an estimator with a matched filter of its own, for a stream of\n"
    "sps samples a symbol that nothing has filtered.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sps : float, default 8.0\n"
    "    Samples per symbol of the input, any real number of 4 or more.\n"
    "pulse : Literal[\"iandd\", \"rrc\"], default \"rrc\"\n"
    "    The pulse the matched filter is built for.\n"
    "    One of ``\"iandd\"``, ``\"rrc\"``.\n"
    "beta : float, default 0.35\n"
    "    Root-raised-cosine roll-off, 0 to 1; ignored for the rectangle.\n"
    "span : int, default 8\n"
    "    One-sided root-raised-cosine span in symbols, 1 or more; ignored "
    "for\n"
    "    the rectangle.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is\n"
    "    ``MatchedSymbolTimingEstimator: invalid parameter (need a finite sps "
    "of\n"
    "    4 or more, pulse iandd or rrc, 0 <= beta <= 1, span >= 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MatchedSymbolTimingEstimator\n"
    ">>> n = np.arange(4096)\n"
    ">>> # +1 -1 +1 ... through a root-raised cosine at 8 samples a symbol,\n"
    ">>> # with a symbol instant at sample 3 and every 8 after it\n"
    ">>> x = (0.25 * np.cos(np.pi * (n - 3.0) / 8.0)).astype(np.complex64)\n"
    ">>> te = MatchedSymbolTimingEstimator(sps=8.0, pulse=\"rrc\")\n"
    ">>> e = te.estimate(x)\n"
    ">>> bool(abs(e.eye % 8.0 - 3.0) < 0.1), e.symbols\n"
    "(True, 490.25)\n",
  .tp_methods = MatchedSymbolTimingEstimatorObj_methods,
  .tp_getset  = MatchedSymbolTimingEstimator_getset,
  .tp_new     = MatchedSymbolTimingEstimatorObj_new,
  .tp_init    = (initproc)MatchedSymbolTimingEstimatorObj_init,
};
