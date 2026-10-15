/* jm:generated detection_ext_syncword.c */
/*
 * detection_ext_syncword.c — SyncFinder type for the detection module.
 *
 * Included by detection_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in detection_ext_syncword_extra.c.
 * Do NOT compile this file directly — only detection_ext.c is compiled.
 */
/* ======================================================== */
/* SyncFinderObject — wraps dp_syncword_state_t *       */
/* ======================================================== */

#include "doppler/syncword/syncword_core.h"

typedef struct
{
  PyObject_HEAD dp_syncword_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
} SyncFinderObject;

static void
SyncFinderObj_dealloc (SyncFinderObject *self)
{
  if (self->handle)
    dp_syncword_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SyncFinderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  SyncFinderObject *self = (SyncFinderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SyncFinderObj_init (SyncFinderObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]   = { "marker", NULL };
  PyObject    *marker_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &marker_obj))
    return -1;
  PyArrayObject *marker_arr
      = jm_array_arg (marker_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "marker");
  if (!marker_arr)
    {
      return -1;
    }
  size_t marker_len = (size_t)PyArray_SIZE (marker_arr);
  self->handle      = dp_syncword_create (
      (const uint8_t *)PyArray_DATA (marker_arr), marker_len);
  Py_DECREF (marker_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "SyncFinder: the marker must be a non-empty array of "
                       "0/1 bits, one per element");
      return -1;
    }
  return 0;
}

static PyStructSequence_Field SyncFinderObj_find_fields[] = {
  { "found", "A marker was found: 1 yes, 0 no." },
  { "offset", "Bit index where the marker starts." },
  { "inverted",
    "The stream is complemented — a BPSK carrier recovered through a "
    "180-degree ambiguity delivers every bit inverted, and a marker no "
    "randomiser covers is the only thing in a frame that can report it." },
  { "errors", "Hamming distance to the marker at that offset, in the polarity "
              "reported." },
  { NULL, NULL },
};
static PyStructSequence_Desc SyncFinderObj_find_desc = {
  "doppler.detection.SyncHit",
  "Where a marker was found, and in which polarity. `found` is the verdict: "
  "the other three fields mean nothing without it, which is why the record "
  "carries it rather than spelling a miss as a sentinel offset.",
  SyncFinderObj_find_fields, 4
};
static PyTypeObject *SyncFinderObj_find_type = NULL;

static PyObject *
SyncFinderObj_find (SyncFinderObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]      = { "bits", "max_errors", NULL };
  PyObject    *bits_obj       = NULL;
  long long    max_errors_raw = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|L", _kwlist, &bits_obj,
                                    &max_errors_raw))
    return NULL;
  if (max_errors_raw < 0 || max_errors_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "max_errors: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)max_errors_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t       max_errors = (uint32_t)max_errors_raw;
  PyArrayObject *bits_arr
      = jm_array_arg (bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "bits");
  if (!bits_arr)
    {
      return NULL;
    }
  const uint8_t *bits     = (const uint8_t *)PyArray_DATA (bits_arr);
  size_t         bits_len = (size_t)PyArray_SIZE (bits_arr);
  if (!SyncFinderObj_find_type)
    {
      SyncFinderObj_find_type
          = PyStructSequence_NewType (&SyncFinderObj_find_desc);
      if (!SyncFinderObj_find_type)
        {
          Py_DECREF (bits_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  syncword_hit_t _r;
  void          *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    _r = dp_syncword_find (_jm_h, bits, bits_len, max_errors);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (bits_arr);
  PyObject *_o = PyStructSequence_New (SyncFinderObj_find_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyLong_FromLong ((long)_r.found));
  PyStructSequence_SET_ITEM (
      _o, 1, PyLong_FromUnsignedLongLong ((unsigned long long)_r.offset));
  PyStructSequence_SET_ITEM (_o, 2, PyLong_FromLong ((long)_r.inverted));
  PyStructSequence_SET_ITEM (
      _o, 3, PyLong_FromUnsignedLong ((unsigned long)_r.errors));
  return _o;
}

static PyObject *
SyncFinderObj_pfa (SyncFinderObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]      = { "max_errors", NULL };
  long long    max_errors_raw = 0LL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "L", _kwlist, &max_errors_raw))
    return NULL;
  if (max_errors_raw < 0 || max_errors_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "max_errors: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)max_errors_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t max_errors = (uint32_t)max_errors_raw;
  double   y          = dp_syncword_pfa (self->handle, max_errors);
  return PyFloat_FromDouble (y);
}

static PyObject *
SyncFinderObj_max_errors_for (SyncFinderObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]       = { "window_bits", "pfa", NULL };
  PyObject          *window_bits_obj = NULL;
  unsigned long long window_bits_raw = 0ULL;
  double             pfa             = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", _kwlist,
                                    &window_bits_obj, &pfa))
    return NULL;
  if (window_bits_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (window_bits_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      window_bits_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((window_bits_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || window_bits_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "window_bits: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t window_bits = (size_t)window_bits_raw;
  int    y = dp_syncword_max_errors_for (self->handle, window_bits, pfa);
  return PyLong_FromLong ((long)y);
}
static PyObject *
SyncFinder_getprop_nbits (SyncFinderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nbits);
}

static PyGetSetDef SyncFinder_getset[]
    = { { "nbits", (getter)SyncFinder_getprop_nbits, NULL,
          "Marker length in bits.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
SyncFinderObj_destroy (SyncFinderObject *self, PyObject *Py_UNUSED (ignored))
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
      dp_syncword_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
SyncFinderObj_enter (SyncFinderObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
SyncFinderObj_exit (SyncFinderObject *self, PyObject *args)
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
      dp_syncword_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef SyncFinderObj_methods[] = {

  { "find", (PyCFunction)(void *)SyncFinderObj_find,
    METH_VARARGS | METH_KEYWORDS,
    "find(bits, max_errors) -> SyncHit record (found, offset, inverted, "
    "errors)\n"
    "\n"
    "Find the first marker in bits, either polarity.\n"
    "\n"
    "The FIRST offset whose Hamming distance to the marker, or to its\n"
    "complement, is at most max_errors. First rather than best, because a\n"
    "best-match search has to see the whole stream before it can answer and\n"
    "a synchroniser reading a live capture cannot wait for that.\n"
    "\n"
    "Choose max_errors with `max_errors_for`, against the window this caller\n"
    "actually searches — the marker length is the wrong thing to halve.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    Unpacked bits, one per byte.\n"
    "max_errors : int\n"
    "    Largest tolerated Hamming distance, in bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "SyncHit\n"
    "    A record whose found says whether the rest of it means anything; a\n"
    "    miss returns it zeroed.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.detection import SyncFinder\n"
    ">>> m = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)\n"
    ">>> rx = np.concatenate([np.zeros(20, np.uint8), 1 - m])\n"
    ">>> hit = SyncFinder(m).find(rx, max_errors=1)\n"
    ">>> hit.found, hit.offset, hit.inverted\n"
    "(1, 20, 1)\n" },
  { "pfa", (PyCFunction)(void *)SyncFinderObj_pfa,
    METH_VARARGS | METH_KEYWORDS,
    "pfa(max_errors) -> float\n"
    "\n"
    "Probability that ONE random offset false-hits this marker at a\n"
    "tolerance of max_errors.\n"
    "\n"
    "`2 * sum_{i <= max_errors} C(n, i) / 2^n`, the factor of two because\n"
    "`find` searches the complement too. Measured against the 32-bit CCSDS\n"
    "marker, this tracks the observed false-alarm rate to within 20 % at\n"
    "every threshold where the count supports a rate\n"
    "(`src/doppler/tests/validation/ccsds_tm/results.md` §2.2).\n"
    "\n"
    "This is the PER-OFFSET number. What a synchroniser cares about is its\n"
    "whole window; `max_errors_for` is this inverted through it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "max_errors : int\n"
    "    Tolerance in bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Probability in &#91;0, 1&#93;.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.detection import SyncFinder\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> f = SyncFinder(asm_bits())\n"
    ">>> # the marker and its complement, out of 2**32 windows\n"
    ">>> round(f.pfa(0) * 2**32)\n"
    "2\n"
    ">>> # ...plus each one's 32 one-bit neighbours\n"
    ">>> round(f.pfa(1) * 2**32)\n"
    "66\n" },
  { "max_errors_for", (PyCFunction)(void *)SyncFinderObj_max_errors_for,
    METH_VARARGS | METH_KEYWORDS,
    "max_errors_for(window_bits, pfa) -> int\n"
    "\n"
    "The largest tolerance whose false-frame rate over a search window\n"
    "still meets pfa.\n"
    "\n"
    "The question `find`'s signature cannot ask. Every offset ahead of the\n"
    "true marker is an independent chance to win the race, so the\n"
    "probability the window produces a false frame is `1 - (1 -\n"
    "pfa(t))^window_bits`, which rises with `t`. The largest `t` that still\n"
    "holds is the most tolerant threshold a caller can afford — and it falls\n"
    "as they search further, which is the whole of doppler#897.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "window_bits : int\n"
    "    Offsets tried AHEAD of the marker: the length of stream searched,\n"
    "    not the length of the frame.\n"
    "pfa : float\n"
    "    Tolerated probability of a false frame over that window.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Tolerance in bits, or -1 when even an exact match exceeds pfa over\n"
    "    that window.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.detection import SyncFinder\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> f = SyncFinder(asm_bits())\n"
    ">>> f.max_errors_for(window_bits=96, pfa=1e-3)\n"
    "3\n"
    ">>> f.max_errors_for(window_bits=100000, pfa=1e-3)   # search further\n"
    "0\n" },
  { "destroy", (PyCFunction)SyncFinderObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)SyncFinderObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a SyncFinder be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "SyncFinder\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)SyncFinderObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the SyncFinder.\n"
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

static PyTypeObject SyncFinderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.detection.SyncFinder",
  .tp_basicsize                           = sizeof (SyncFinderObject),
  .tp_dealloc                             = (destructor)SyncFinderObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a searcher for marker.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "marker : NDArray[np.uint8]\n"
    "    Unpacked bits, one per byte; only the LSB is used.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``SyncFinder: the\n"
    "    marker must be a non-empty array of 0/1 bits, one per element``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.detection import SyncFinder\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> asm = asm_bits()          # 0x1ACFFC1D, no transcription\n"
    ">>> f = SyncFinder(asm)\n"
    ">>> f.nbits\n"
    "32\n"
    ">>> rx = np.concatenate([np.zeros(96, np.uint8), asm])\n"
    ">>> hit = f.find(rx, max_errors=f.max_errors_for(96, pfa=1e-3))\n"
    ">>> hit.found, hit.offset, hit.inverted\n"
    "(1, 96, 0)\n",
  .tp_methods = SyncFinderObj_methods,
  .tp_getset  = SyncFinder_getset,
  .tp_new     = SyncFinderObj_new,
  .tp_init    = (initproc)SyncFinderObj_init,
};
