/*
 * spectral_ext_detector2d.c — CorrDetector2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* CorrDetector2DObject — wraps detector2d_state_t *       */
/* ======================================================== */

#include "detector2d/detector2d_core.h"

typedef struct
{
  PyObject_HEAD detector2d_state_t *handle;
} CorrDetector2DObject;

static void
CorrDetector2DObj_dealloc (CorrDetector2DObject *self)
{
  if (self->handle)
    detector2d_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CorrDetector2DObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CorrDetector2DObject *self
      = (CorrDetector2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CorrDetector2DObj_init (CorrDetector2DObject *self, PyObject *args,
                        PyObject *kwds)
{
  /* Hand-patch (sacred fragment): the parameter ORDER here must track
     objects/detector.toml, because the .pyi is hand-owned and states that
     order. An older jm hoisted the string_enum `noise_mode` to the front;
     it no longer does, but this fragment is hand-owned for the 2-D ref
     marshaling below, so it never picked the fix up and the stub and the
     binding disagreed. Keep the two in step by hand. */
  static char *kwlist[] = { "ref",        "dwell",     "noise_lo", "noise_hi",
                            "noise_mode", "threshold", "nthreads", NULL };
  PyObject    *ref_obj  = NULL;
  unsigned long long dwell_raw      = 1ULL;
  unsigned long long noise_lo_raw   = 0ULL;
  unsigned long long noise_hi_raw   = (unsigned long long)-1ULL;
  const char        *noise_mode_str = "mean";
  float              threshold      = 0.0f;
  int                nthreads       = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKKsfi", kwlist, &ref_obj,
                                    &dwell_raw, &noise_lo_raw, &noise_hi_raw,
                                    &noise_mode_str, &threshold, &nthreads))
    return -1;
  int noise_mode = 0;
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
  size_t         dwell    = (size_t)dwell_raw;
  size_t         noise_lo = (size_t)noise_lo_raw;
  size_t         noise_hi = (size_t)noise_hi_raw;
  PyArrayObject *ref_arr  = (PyArrayObject *)PyArray_FROM_OTF (
      ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!ref_arr)
    {
      return -1;
    }
  if (PyArray_NDIM (ref_arr) != 2)
    {
      PyErr_SetString (PyExc_ValueError, "ref must be a 2-D array");
      Py_DECREF (ref_arr);
      return -1;
    }
  size_t ref_dim0 = (size_t)PyArray_DIM (ref_arr, 0);
  size_t ref_dim1 = (size_t)PyArray_DIM (ref_arr, 1);
  self->handle    = detector2d_create (
      (const float complex *)PyArray_DATA (ref_arr), ref_dim0, ref_dim1, dwell,
      noise_lo, noise_hi, noise_mode, threshold, nthreads);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "detector2d_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CorrDetector2DObj_reset (CorrDetector2DObject *self,
                         PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  detector2d_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CorrDetector2DObj_push (CorrDetector2DObject *self, PyObject *args)
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
  size_t         n_in = (size_t)PyArray_SIZE (in_arr);
  det_result2d_t results[64];
  size_t n_out = detector2d_push (self->handle,
                                  (const float complex *)PyArray_DATA (in_arr),
                                  n_in, results, 64);
  Py_DECREF (in_arr);
  PyObject *lst = PyList_New ((Py_ssize_t)n_out);
  if (!lst)
    return NULL;
  for (size_t i = 0; i < n_out; i++)
    {
      PyObject *tup = Py_BuildValue (
          "(KKfff)", (unsigned long long)results[i].row,
          (unsigned long long)results[i].col, results[i].peak_mag,
          results[i].noise_est, results[i].test_stat);
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
CorrDetector2D_getprop_ny (CorrDetector2DObject *self,
                           void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
CorrDetector2D_getprop_nx (CorrDetector2DObject *self,
                           void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
CorrDetector2D_getprop_n (CorrDetector2DObject *self,
                          void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
CorrDetector2D_getprop_dwell (CorrDetector2DObject *self,
                              void                 *Py_UNUSED (closure))
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
CorrDetector2D_getprop_count (CorrDetector2DObject *self,
                              void                 *Py_UNUSED (closure))
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
CorrDetector2D_getprop_ring_cap (CorrDetector2DObject *self,
                                 void                 *Py_UNUSED (closure))
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
CorrDetector2D_getprop_noise_lo (CorrDetector2DObject *self,
                                 void                 *Py_UNUSED (closure))
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
CorrDetector2D_getprop_noise_hi (CorrDetector2DObject *self,
                                 void                 *Py_UNUSED (closure))
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
CorrDetector2D_getprop_threshold (CorrDetector2DObject *self,
                                  void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->threshold);
}
static PyObject *
CorrDetector2D_getprop_last_corr (CorrDetector2DObject *self,
                                  void                 *Py_UNUSED (closure))
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

static PyGetSetDef CorrDetector2D_getset[]
    = { { "ny", (getter)CorrDetector2D_getprop_ny, NULL, "Number of rows.\n",
          NULL },
        { "nx", (getter)CorrDetector2D_getprop_nx, NULL,
          "Number of columns.\n", NULL },
        { "n", (getter)CorrDetector2D_getprop_n, NULL,
          "ny * nx — total frame length.\n", NULL },
        { "dwell", (getter)CorrDetector2D_getprop_dwell, NULL,
          "Integration depth.\n", NULL },
        { "count", (getter)CorrDetector2D_getprop_count, NULL,
          "Frames accumulated (0 … dwell-1).\n", NULL },
        { "ring_cap", (getter)CorrDetector2D_getprop_ring_cap, NULL,
          "Ring buffer capacity in complex samples.\n", NULL },
        { "noise_lo", (getter)CorrDetector2D_getprop_noise_lo, NULL,
          "Noise bin range lower bound (inclusive).\n", NULL },
        { "noise_hi", (getter)CorrDetector2D_getprop_noise_hi, NULL,
          "Noise bin range upper bound (inclusive).\n", NULL },
        { "threshold", (getter)CorrDetector2D_getprop_threshold, NULL,
          "0 = always fire; >0 = gate on test_stat.\n", NULL },
        { "last_corr", (getter)CorrDetector2D_getprop_last_corr, NULL,
          "The correlation vector from the most recent push() that produced a "
          "result (None before that). This is a zero-copy view into a buffer "
          "owned by the detector and reused every push() -- the next push() "
          "(even one that doesn't produce a result) overwrites it in place. "
          "Copy the array before the next push() if you need to retain it.\n",
          NULL },
        { NULL } };

static PyObject *
CorrDetector2DObj_destroy (CorrDetector2DObject *self,
                           PyObject             *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      detector2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrDetector2DObj_enter (CorrDetector2DObject *self,
                         PyObject             *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CorrDetector2DObj_exit (CorrDetector2DObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      detector2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrDetector2DObj_state_bytes (CorrDetector2DObject *self,
                               PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (detector2d_state_bytes (self->handle));
}

static PyObject *
CorrDetector2DObj_get_state (CorrDetector2DObject *self,
                             PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = detector2d_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  detector2d_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CorrDetector2DObj_set_state (CorrDetector2DObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != detector2d_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (detector2d_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CorrDetector2DObj_methods[] = {
  { "reset", (PyCFunction)CorrDetector2DObj_reset, METH_NOARGS,
    "Reset the 2-D correlator, ring buffer, and last-corr flag. Discards any "
    "partial frame buffered in the ring and zeroes the coherent accumulator.  "
    "The reference spectrum and FFT plans are preserved." },

  { "push", (PyCFunction)CorrDetector2DObj_push, METH_VARARGS,
    "push(x) -> list[tuple]\n"
    "\n"
    "Stream an arbitrary-length CF32 chunk through the 2-D detector. "
    "Identical to detector_push() except frames are ny*nx complex samples and "
    "each detection event carries (row, col) for the peak location instead of "
    "a single lag index.  In Python the result is always a list of (row, col, "
    "peak_mag, noise_est, test_stat) tuples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "list[tuple]\n"
    "    Number of det_result2d_t entries written to result.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import CorrDetector2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,\n"
    "...                  noise_mode=\"mean\", threshold=0.0)\n"
    ">>> results = det.push(np.ones((4, 4), dtype=np.complex64))\n"
    ">>> len(results)\n"
    "1\n"
    ">>> row, col, peak, noise, stat = results[0]\n"
    ">>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)\n"
    "(0, 0, 1.0, 1.0, 1.0)\n" },
  { "destroy", (PyCFunction)CorrDetector2DObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)CorrDetector2DObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Detector2d be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Detector2d\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CorrDetector2DObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Detector2d.\n"
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
  { "state_bytes", (PyCFunction)CorrDetector2DObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the CorrDetector2DObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CorrDetector2DObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CorrDetector2DObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CorrDetector2DObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the CorrDetector2DObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { NULL }
};

static PyTypeObject CorrDetector2DObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.CorrDetector2D",
  .tp_basicsize                           = sizeof (CorrDetector2DObject),
  .tp_dealloc = (destructor)CorrDetector2DObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a 2-D signal detector.\n",
  .tp_methods = CorrDetector2DObj_methods,
  .tp_getset  = CorrDetector2D_getset,
  .tp_new     = CorrDetector2DObj_new,
  .tp_init    = (initproc)CorrDetector2DObj_init,
};
