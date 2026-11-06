/* jm:generated spectral_ext_corrdetector.c */
/*
 * spectral_ext_corrdetector.c — CorrDetector type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in spectral_ext_corrdetector_extra.c.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* CorrDetectorObject — wraps dp_correlator_state_t *       */
/* ======================================================== */

#include "doppler/correlator/correlator_core.h"

typedef struct
{
  PyObject_HEAD dp_correlator_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
} CorrDetectorObject;

static void
CorrDetectorObj_dealloc (CorrDetectorObject *self)
{
  if (self->handle)
    dp_correlator_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CorrDetectorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  CorrDetectorObject *self = (CorrDetectorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CorrDetectorObj_init (CorrDetectorObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "ref",        "dwell",     "noise_lo", "noise_hi",
                             "noise_mode", "threshold", "nthreads", NULL };
  PyObject    *ref_obj   = NULL;
  PyObject    *dwell_obj = NULL;
  unsigned long long dwell_raw      = 1;
  PyObject          *noise_lo_obj   = NULL;
  unsigned long long noise_lo_raw   = 0;
  PyObject          *noise_hi_obj   = NULL;
  unsigned long long noise_hi_raw   = (unsigned long long)-1ULL;
  const char        *noise_mode_str = "mean";
  float              threshold      = 0.0f;
  int                nthreads       = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|OOOsfi", kwlist, &ref_obj,
                                    &dwell_obj, &noise_lo_obj, &noise_hi_obj,
                                    &noise_mode_str, &threshold, &nthreads))
    return -1;
  if (dwell_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (dwell_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      dwell_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((dwell_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || dwell_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "dwell: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t dwell = (size_t)dwell_raw;
  if (noise_lo_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (noise_lo_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      noise_lo_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((noise_lo_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || noise_lo_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "noise_lo: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t noise_lo = (size_t)noise_lo_raw;
  if (noise_hi_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (noise_hi_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      noise_hi_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((noise_hi_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || noise_hi_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "noise_hi: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t noise_hi   = (size_t)noise_hi_raw;
  int    noise_mode = 0;
  if (strcmp (noise_mode_str, "mean") == 0)
    noise_mode = 0;
  else if (strcmp (noise_mode_str, "median") == 0)
    noise_mode = 1;
  else if (strcmp (noise_mode_str, "min") == 0)
    noise_mode = 2;
  else if (strcmp (noise_mode_str, "max") == 0)
    noise_mode = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "noise_mode must be one of \"mean\", \"median\", \"min\", "
                    "\"max\", got '%s'",
                    noise_mode_str);
      return -1;
    }
  PyArrayObject *ref_arr
      = jm_array_arg (ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "ref");
  if (!ref_arr)
    {
      return -1;
    }
  size_t ref_len = (size_t)PyArray_SIZE (ref_arr);
  self->handle   = dp_correlator_create_detector_1d (
      (const float _Complex *)PyArray_DATA (ref_arr), ref_len, dwell, noise_lo,
      noise_hi, noise_mode, threshold, nthreads);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "CorrDetector: invalid parameter (need a non-empty "
                       "ref and dwell >= 1)");
      return -1;
    }
  return 0;
}

static PyObject *
CorrDetectorObj_reset (CorrDetectorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_correlator_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CorrDetectorObj_push (CorrDetectorObject *self, PyObject *args)
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
  size_t              n_in = (size_t)PyArray_SIZE (in_arr);
  dp_correlator_hit_t results[1024];
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t n_out = dp_correlator_push (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), n_in,
      results, 1024);
  Py_DECREF (in_arr);
  if ((size_t)(n_out) > (size_t)(1024))
    {
      PyErr_Format (
          PyExc_RuntimeError,
          "CorrDetector.push: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(1024));
      return NULL;
    }
  PyObject *lst = PyList_New ((Py_ssize_t)n_out);
  if (!lst)
    return NULL;
  for (size_t i = 0; i < n_out; i++)
    {
      PyObject *tup = Py_BuildValue (
          "(NNNN)",
          PyLong_FromUnsignedLongLong ((unsigned long long)results[i].col),
          PyFloat_FromDouble ((double)results[i].peak_mag),
          PyFloat_FromDouble ((double)results[i].noise_est),
          PyFloat_FromDouble ((double)results[i].test_stat));
      if (!tup)
        {
          Py_DECREF (lst);
          return NULL;
        }
      PyList_SET_ITEM (lst, (Py_ssize_t)i, tup);
    }
  return lst;
}

static PyObject *
CorrDetectorObj_state_bytes (CorrDetectorObject *self,
                             PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_correlator_state_bytes (self->handle));
}

static PyObject *
CorrDetectorObj_get_state (CorrDetectorObject *self,
                           PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_correlator_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_correlator_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CorrDetectorObj_set_state (CorrDetectorObject *self, PyObject *arg)
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
      != dp_correlator_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_correlator_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
CorrDetector_getprop_n (CorrDetectorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
CorrDetector_getprop_dwell (CorrDetectorObject *self,
                            void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
CorrDetector_getprop_count (CorrDetectorObject *self,
                            void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->count);
}
static PyObject *
CorrDetector_getprop_ring_cap (CorrDetectorObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->ring_cap);
}
static PyObject *
CorrDetector_getprop_noise_lo (CorrDetectorObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->noise_lo);
}
static PyObject *
CorrDetector_getprop_noise_hi (CorrDetectorObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->noise_hi);
}
static PyObject *
CorrDetector_getprop_threshold (CorrDetectorObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->threshold);
}
static PyObject *
CorrDetector_getprop_last_corr (CorrDetectorObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!self->handle->_last_corr_valid)
    Py_RETURN_NONE;
  size_t _dim_need = (size_t)(self->handle->n);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "CorrDetector.last_corr: output of %zu elements is too large",
          _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *arr  = PyArray_SimpleNewFromData (1, &_dim, NPY_COMPLEX64,
                                              (void *)(self->handle->det_out));
  if (!arr)
    return NULL;
  Py_INCREF (self);
  if (PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self) < 0)
    {
      Py_DECREF (self);
      Py_DECREF (arr);
      return NULL;
    }
  return arr;
}

static PyGetSetDef CorrDetector_getset[] = {
  { "n", (getter)CorrDetector_getprop_n, NULL,
    "Frame / FFT length in complex samples.\n", NULL },
  { "dwell", (getter)CorrDetector_getprop_dwell, NULL,
    "Integration depth: a detection statistic is dumped every `dwell` "
    "frames.\n",
    NULL },
  { "count", (getter)CorrDetector_getprop_count, NULL,
    "Frames accumulated toward the next dump (0 ... dwell-1).\n", NULL },
  { "ring_cap", (getter)CorrDetector_getprop_ring_cap, NULL,
    "Ring buffer capacity, complex samples.\n", NULL },
  { "noise_lo", (getter)CorrDetector_getprop_noise_lo, NULL,
    "Noise bin range, first index (inclusive).\n", NULL },
  { "noise_hi", (getter)CorrDetector_getprop_noise_hi, NULL,
    "Noise bin range, last index (inclusive).\n", NULL },
  { "threshold", (getter)CorrDetector_getprop_threshold, NULL,
    "Test-stat gate: 0 fires on every dump, > 0 on test_stat above it.\n",
    NULL },
  { "last_corr", (getter)CorrDetector_getprop_last_corr, NULL,
    "The correlation vector from the most recent push() that produced a "
    "result (None before that, and after reset() or set_state(): the vector "
    "is not part of the serialized state). This is a zero-copy view into a "
    "buffer owned by the detector and reused every push() -- the next push() "
    "(even one that doesn't produce a result) overwrites it in place. Copy "
    "the array before the next push() if you need to retain it.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
CorrDetectorObj_destroy (CorrDetectorObject *self,
                         PyObject           *Py_UNUSED (ignored))
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
      dp_correlator_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrDetectorObj_enter (CorrDetectorObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CorrDetectorObj_exit (CorrDetectorObject *self, PyObject *args)
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
      dp_correlator_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef CorrDetectorObj_methods[] = {
  { "reset", (PyCFunction)CorrDetectorObj_reset, METH_NOARGS,
    "Zero the accumulator and reset the integration counter to 0.\n"
    "Equivalent to starting a fresh dwell cycle without rebuilding FFT plans\n"
    "or recomputing ref_spec. (A detector flavor resets its stream too: see\n"
    "the detector flavors' section below.)\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Corr2D(ref=ref, dwell=3)\n"
    ">>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))\n"
    ">>> c.count\n"
    "1\n"
    ">>> c.reset()\n"
    ">>> c.count\n"
    "0\n" },

  { "push", (PyCFunction)CorrDetectorObj_push, METH_VARARGS,
    "push(x) -> list[tuple]\n"
    "\n"
    "Stream an arbitrary-length CF32 chunk through a detector flavor.\n"
    "\n"
    "Takes the input in order, runs each complete n-sample frame (n = ny*nx)\n"
    "through the correlator, and on every dump computes the test statistic\n"
    "peak_mag / noise_est. Each dump that passes the threshold is one hit.\n"
    "In Python the result is always a list: of (row, col, peak_mag,\n"
    "noise_est, test_stat) tuples from CorrDetector2D, and of (lag,\n"
    "peak_mag, noise_est, test_stat) tuples from CorrDetector, whose lag is\n"
    "the hit's col.\n"
    "\n"
    "Python's push() has room for 1024 detections a call, so a push that\n"
    "completes at most 1024 frames, counting the carry, loses nothing. One\n"
    "that fills the room loses the rest of its input, every later frame\n"
    "whether or not it would have made a detection, unless the rest is\n"
    "shorter than a frame: that is kept as the carry. The next push stays on\n"
    "the frame grid only when what was lost is a whole number of frames;\n"
    "otherwise its peaks are off by the remainder. #1992 and\n"
    "just-buildit/just-makeit#2184 track sizing the list to the call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "list[tuple]\n"
    "    Python: the list of hits, empty when no dump passed the gate. C:\n"
    "    the number of hits written to result, or SIZE_MAX (nothing taken,\n"
    "    nothing changed) when state is a map flavor.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import CorrDetector, CorrDetector2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,\n"
    "...                  noise_mode=\"mean\", threshold=0.0)\n"
    ">>> results = det.push(np.ones((4, 4), dtype=np.complex64))\n"
    ">>> row, col, peak, noise, stat = results[0]\n"
    ">>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)\n"
    "(0, 0, 1.0, 1.0, 1.0)\n"
    ">>> det1 = CorrDetector(ref=ref[0], dwell=1, noise_lo=1, noise_hi=3,\n"
    "...                 noise_mode=\"mean\", threshold=0.0)\n"
    ">>> lag, peak, noise, stat = det1.push(np.ones(4, "
    "dtype=np.complex64))[0]\n"
    ">>> lag, round(peak, 4), round(noise, 4), round(stat, 4)\n"
    "(0, 1.0, 1.0, 1.0)\n" },
  { "state_bytes", (PyCFunction)CorrDetectorObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the CorrDetector has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CorrDetectorObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CorrDetector has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CorrDetectorObj_set_state, METH_O,
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
    "``RuntimeError`` if the CorrDetector has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)CorrDetectorObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)CorrDetectorObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a CorrDetector be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "CorrDetector\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CorrDetectorObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the CorrDetector.\n"
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

static PyTypeObject CorrDetectorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.spectral.CorrDetector",
  .tp_basicsize                           = sizeof (CorrDetectorObject),
  .tp_dealloc = (destructor)CorrDetectorObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a 1-D streaming detector: the 2-D detector at one row\n"
    "(docs/design/correlator.md section 2.1), so its hits are the 2-D one's "
    "at\n"
    "ny = 1, each with row 0 and the lag in col.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ref : NDArray[np.complex64]\n"
    "    Reference signal, CF32 ndarray of length ref_len.\n"
    "dwell : int, default 1\n"
    "    Int-dump depth; must be >= 1.\n"
    "noise_lo : int, default 0\n"
    "    Lower noise bin index (inclusive, 0-based).\n"
    "noise_hi : int\n"
    "    Upper noise bin index (inclusive, < n). A value at or beyond the "
    "window\n"
    "    clamps to n - 1, so the default sentinel selects the full window.\n"
    "noise_mode : Literal[\"mean\", \"median\", \"min\", \"max\"], default "
    "\"mean\"\n"
    "    How the noise bins are aggregated into the noise floor the\n"
    "    peak-to-noise statistic divides by.\n"
    "    One of ``\"mean\"``, ``\"median\"``, ``\"min\"``, ``\"max\"``.\n"
    "threshold : float, default 0.0\n"
    "    Test-stat gate; 0.0 = always emit.\n"
    "nthreads : int, default 1\n"
    "    Accepted for API compatibility; ignored.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``CorrDetector: "
    "invalid\n"
    "    parameter (need a non-empty ref and dwell >= 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import CorrDetector\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0\n"
    ">>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,\n"
    "...                noise_mode=\"mean\", threshold=0.0)\n"
    ">>> det.n, det.dwell, det.ring_cap\n"
    "(8, 1, 512)\n",
  .tp_methods = CorrDetectorObj_methods,
  .tp_getset  = CorrDetector_getset,
  .tp_new     = CorrDetectorObj_new,
  .tp_init    = (initproc)CorrDetectorObj_init,
};
