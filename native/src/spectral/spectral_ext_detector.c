/*
 * spectral_ext_detector.c — CorrDetector type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* CorrDetectorObject — wraps detector_state_t *       */
/* ======================================================== */

#include "detector/detector_core.h"

typedef struct
{
  PyObject_HEAD detector_state_t *handle;
} CorrDetectorObject;

static void
CorrDetectorObj_dealloc (CorrDetectorObject *self)
{
  if (self->handle)
    detector_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CorrDetectorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CorrDetectorObject *self = (CorrDetectorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CorrDetectorObj_init (CorrDetectorObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "ref",        "dwell",     "noise_lo", "noise_hi",
                            "noise_mode", "threshold", "nthreads", NULL };
  PyObject    *ref_obj  = NULL;
  unsigned long long dwell_raw      = 1;
  unsigned long long noise_lo_raw   = 0;
  unsigned long long noise_hi_raw   = (unsigned long long)-1ULL;
  const char        *noise_mode_str = "mean";
  float              threshold      = 0.0f;
  int                nthreads       = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKKsfi", kwlist, &ref_obj,
                                    &dwell_raw, &noise_lo_raw, &noise_hi_raw,
                                    &noise_mode_str, &threshold, &nthreads))
    return -1;
  size_t dwell      = (size_t)dwell_raw;
  size_t noise_lo   = (size_t)noise_lo_raw;
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
  PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF (
      ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!ref_arr)
    {
      return -1;
    }
  size_t ref_len = (size_t)PyArray_SIZE (ref_arr);
  self->handle   = detector_create (
      (const float complex *)PyArray_DATA (ref_arr), ref_len, dwell, noise_lo,
      noise_hi, noise_mode, threshold, nthreads);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "detector_create returned NULL");
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
  detector_reset (self->handle);
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t       n_in = (size_t)PyArray_SIZE (in_arr);
  det_result_t results[64];
  size_t n_out = detector_push (self->handle,
                                (const float complex *)PyArray_DATA (in_arr),
                                n_in, results, 64);
  Py_DECREF (in_arr);
  PyObject *lst = PyList_New ((Py_ssize_t)n_out);
  if (!lst)
    return NULL;
  for (size_t i = 0; i < n_out; i++)
    {
      PyObject *tup = Py_BuildValue (
          "(NNNN)",
          PyLong_FromUnsignedLongLong ((unsigned long long)results[i].lag),
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
  return PyLong_FromSize_t (detector_state_bytes (self->handle));
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
  size_t    _n = detector_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  detector_get_state (self->handle, PyBytes_AS_STRING (_b));
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
  if ((size_t)PyBytes_GET_SIZE (arg) != detector_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (detector_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
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
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->corr->dwell);
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
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->corr->count);
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
  npy_intp  dim = (npy_intp)self->handle->n;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->handle->out_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  return arr;
}

static PyGetSetDef CorrDetector_getset[] = {
  { "n", (getter)CorrDetector_getprop_n, NULL,
    "Frame / FFT length in complex samples.\n", NULL },
  { "dwell", (getter)CorrDetector_getprop_dwell, NULL,
    "Integration depth; dump every dwell calls.\n", NULL },
  { "count", (getter)CorrDetector_getprop_count, NULL,
    "Frames accumulated so far (0 … dwell-1).\n", NULL },
  { "ring_cap", (getter)CorrDetector_getprop_ring_cap, NULL,
    "Ring buffer capacity in complex samples.\n", NULL },
  { "noise_lo", (getter)CorrDetector_getprop_noise_lo, NULL,
    "Noise bin range lower bound (inclusive).\n", NULL },
  { "noise_hi", (getter)CorrDetector_getprop_noise_hi, NULL,
    "Noise bin range upper bound (inclusive).\n", NULL },
  { "threshold", (getter)CorrDetector_getprop_threshold, NULL,
    "0 = always fire; >0 = gate on test_stat.\n", NULL },
  { "last_corr", (getter)CorrDetector_getprop_last_corr, NULL,
    "The correlation vector from the most recent push() that produced a "
    "result (None before that). This is a zero-copy view into a buffer owned "
    "by the detector and reused every push() -- the next push() (even one "
    "that doesn't produce a result) overwrites it in place. Copy the array "
    "before the next push() if you need to retain it.\n",
    NULL },
  { NULL }
};

static PyObject *
CorrDetectorObj_destroy (CorrDetectorObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      detector_destroy (self->handle);
      self->handle = NULL;
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
      detector_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CorrDetectorObj_methods[] = {
  { "reset", (PyCFunction)CorrDetectorObj_reset, METH_NOARGS,
    "Reset the correlator, ring buffer, and last-corr flag. Discards "
    "any partial frame buffered in the ring and zeroes the coherent "
    "accumulator.  Equivalent to starting fresh from the same reference "
    "without rebuilding any internal object." },

  { "push", (PyCFunction)CorrDetectorObj_push, METH_VARARGS,
    "push(x) -> list[tuple]\n"
    "\n"
    "Stream an arbitrary-length CF32 chunk through the detector "
    "pipeline. Writes samples into the ring buffer, drains complete "
    "n-sample frames through the correlator, and on every int-dump "
    "computes the test statistic peak_mag / noise_est.  Detections that "
    "pass the threshold are appended to the Python return list as (lag, "
    "peak_mag, noise_est, test_stat) tuples. In Python the result is "
    "always a list, even when empty.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "list[tuple]\n"
    "    Number of det_result_t entries written to result.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import CorrDetector\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0\n"
    ">>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,\n"
    "...                noise_mode=\"mean\", threshold=0.0)\n"
    ">>> results = det.push(np.ones(8, dtype=np.complex64))\n"
    ">>> len(results)\n"
    "1\n"
    ">>> lag, peak, noise, stat = results[0]\n"
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
    "Raises ``RuntimeError`` if the CorrDetectorObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CorrDetectorObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CorrDetectorObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CorrDetectorObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the CorrDetectorObj has already been destroyed.\n"
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
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)CorrDetectorObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Detector be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Detector\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CorrDetectorObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Detector.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
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

static PyTypeObject CorrDetectorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.CorrDetector",
  .tp_basicsize                           = sizeof (CorrDetectorObject),
  .tp_dealloc = (destructor)CorrDetectorObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Allocate a 1-D streaming signal detector backed by an FFT "
                "correlator. Combines a corr_state_t with a double-mapped ring "
                "buffer so that arbitrary chunk sizes can be pushed.  After every "
                "int-dump the peak-to-noise test statistic is compared against "
                "threshold; a det_result_t is emitted when it passes.  Setting "
                "threshold to 0.0 unconditionally fires on every dump. The ring "
                "capacity is next_pow2(max(n, 512)) complex samples.\n",
  .tp_methods = CorrDetectorObj_methods,
  .tp_getset  = CorrDetector_getset,
  .tp_new     = CorrDetectorObj_new,
  .tp_init    = (initproc)CorrDetectorObj_init,
};
