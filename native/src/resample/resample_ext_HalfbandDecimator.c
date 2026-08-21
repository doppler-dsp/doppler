/*
 * resample_ext_HalfbandDecimator.c — HalfbandDecimator type for the resample
 * module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* HalfbandDecimatorObject — wraps HalfbandDecimator_state_t *       */
/* ======================================================== */

#include "HalfbandDecimator/HalfbandDecimator_core.h"
#include "dp_state_pyhelp.h"

typedef struct
{
  PyObject_HEAD HalfbandDecimator_state_t *handle;
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* elements allocated above */
} HalfbandDecimatorObject;

static void
HalfbandDecimatorObj_dealloc (HalfbandDecimatorObject *self)
{
  if (self->handle)
    HalfbandDecimator_destroy (self->handle);
  free (self->_execute_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  HalfbandDecimatorObject *self
      = (HalfbandDecimatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimatorObj_init (HalfbandDecimatorObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[] = { "h", NULL };
  PyObject    *h_obj    = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &h_obj))
    return -1;
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      return -1;
    }
  if (PyArray_NDIM (h_arr) != 1)
    {
      Py_DECREF (h_arr);
      PyErr_SetString (PyExc_ValueError, "h must be a 1-D float32 array");
      return -1;
    }
  size_t h_len = (size_t)PyArray_SIZE (h_arr);
  /* HalfbandDecimator_create(h, h_len) — the array first, matching every
     other create in the tree and the manifest jm renders from */
  self->handle
      = HalfbandDecimator_create ((const float *)PyArray_DATA (h_arr), h_len);
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "HalfbandDecimator_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorObj_execute (HalfbandDecimatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  if (!self->_execute_buf)
    {
      size_t _max = HalfbandDecimator_execute_max_out (self->handle);
      if (!_max)
        _max = (size_t)PyArray_SIZE (x_arr);
      self->_execute_buf     = malloc (_max * sizeof (float complex));
      self->_execute_buf_cap = _max;
      if (!self->_execute_buf)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = HalfbandDecimator_execute (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), self->_execute_buf,
      self->_execute_buf_cap);
  Py_DECREF (x_arr);
  npy_intp       dim = (npy_intp)n_out;
  PyArrayObject *out_arr
      = (PyArrayObject *)PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!out_arr)
    return NULL;
  memcpy (PyArray_DATA (out_arr), self->_execute_buf,
          n_out * sizeof (float complex));
  return (PyObject *)out_arr;
}

static PyObject *
HalfbandDecimatorObj_reset (HalfbandDecimatorObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  HalfbandDecimator_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
HalfbandDecimator_getprop_rate (HalfbandDecimatorObject *self,
                                void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (HalfbandDecimator_get_rate (self->handle));
}
static PyObject *
HalfbandDecimator_getprop_num_taps (HalfbandDecimatorObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)HalfbandDecimator_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimator_getset[]
    = { { "rate", (getter)HalfbandDecimator_getprop_rate, NULL,
          "Always returns 0.5.\n", NULL },
        { "num_taps", (getter)HalfbandDecimator_getprop_num_taps, NULL,
          "Returns the FIR branch length passed to create.\n", NULL },
        { NULL } };

static PyObject *
HalfbandDecimatorObj_destroy (HalfbandDecimatorObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      HalfbandDecimator_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorObj_enter (HalfbandDecimatorObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HalfbandDecimatorObj_exit (HalfbandDecimatorObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      HalfbandDecimator_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* serializable (gh-400): the standard state triplet, generated by the
 * shared macro (see dp_state_pyhelp.h) — byte-identical to jm's output.
 * The matching PyMethodDef rows are below. */
DP_PY_STATE_METHODS (HalfbandDecimatorObj, HalfbandDecimatorObject,
                     self->handle, HalfbandDecimator)

static PyObject *
HalfbandDecimatorObj_execute_max_out (HalfbandDecimatorObject *self,
                                      PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (HalfbandDecimator_execute_max_out (self->handle));
}

static PyMethodDef HalfbandDecimatorObj_methods[] = {

  { "execute", (PyCFunction)HalfbandDecimatorObj_execute, METH_VARARGS,
    "execute(x) -> ndarray\n"
    "\n"
    "Decimate x(0..x_len-1) by 2 into out(0..n_out-1).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import HalfbandDecimator\n"
    "    >>> obj = HalfbandDecimator(np.zeros(1, dtype=np.float32))\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)HalfbandDecimatorObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero delay lines.  Coefficients preserved.\n"
    "\n"
    "    >>> from doppler import HalfbandDecimator\n"
    "    >>> obj = HalfbandDecimator(np.zeros(1, dtype=np.float32))\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)HalfbandDecimatorObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the HalfbandDecimator has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)HalfbandDecimatorObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the HalfbandDecimator has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)HalfbandDecimatorObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the HalfbandDecimator has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)HalfbandDecimatorObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)HalfbandDecimatorObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a HalfbandDecimator be used in a `with` statement so its C\n"
    "resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "HalfbandDecimator\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)HalfbandDecimatorObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the HalfbandDecimator.\n"
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
  { "execute_max_out", (PyCFunction)HalfbandDecimatorObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { NULL }
};

static PyTypeObject HalfbandDecimatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.HalfbandDecimator",
  .tp_basicsize                           = sizeof (HalfbandDecimatorObject),
  .tp_dealloc = (destructor)HalfbandDecimatorObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a HalfbandDecimator.\n",
  .tp_methods = HalfbandDecimatorObj_methods,
  .tp_getset  = HalfbandDecimator_getset,
  .tp_new     = HalfbandDecimatorObj_new,
  .tp_init    = (initproc)HalfbandDecimatorObj_init,
};
