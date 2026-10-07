/* jm:generated resample_ext_HalfbandDecimator.c */
/*
 * resample_ext_HalfbandDecimator.c — HalfbandDecimator type for the resample
 * module.
 *
 * Included by resample_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in resample_ext_HalfbandDecimator_extra.c.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* HalfbandDecimatorObject — wraps dp_HalfbandDecimator_state_t *       */
/* ======================================================== */

#include "doppler/HalfbandDecimator/HalfbandDecimator_core.h"

typedef struct
{
  PyObject_HEAD dp_HalfbandDecimator_state_t *handle;
} HalfbandDecimatorObject;

static void
HalfbandDecimatorObj_dealloc (HalfbandDecimatorObject *self)
{
  if (self->handle)
    dp_HalfbandDecimator_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
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
  PyArrayObject *h_arr
      = jm_array_arg (h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS, "h");
  if (!h_arr)
    {
      return -1;
    }
  if (PyArray_NDIM (h_arr) != 1)
    {
      PyErr_SetString (PyExc_ValueError, "h must be a 1-D array");
      Py_DECREF (h_arr);
      return -1;
    }
  size_t h_len = (size_t)PyArray_SIZE (h_arr);
  self->handle = dp_HalfbandDecimator_create (
      (const float *)PyArray_DATA (h_arr), h_len);
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_HalfbandDecimator_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorObj_execute_max_out (HalfbandDecimatorObject *self,
                                      PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_HalfbandDecimator_execute_max_out (self->handle));
}

static PyObject *
HalfbandDecimatorObj_execute (HalfbandDecimatorObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  x_arr = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (x_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_HalfbandDecimator_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t n_out = dp_HalfbandDecimator_execute (
          self->handle, (const float _Complex *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr),
          (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (x_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "HalfbandDecimator.execute: wrote %zu elements into a "
                        "buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_HalfbandDecimator_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "HalfbandDecimator.execute: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out = dp_HalfbandDecimator_execute (
      self->handle, (const float _Complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), _d0, _cap);
  Py_DECREF (x_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "HalfbandDecimator.execute: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
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
  dp_HalfbandDecimator_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorObj_state_bytes (HalfbandDecimatorObject *self,
                                  PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_HalfbandDecimator_state_bytes (self->handle));
}

static PyObject *
HalfbandDecimatorObj_get_state (HalfbandDecimatorObject *self,
                                PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_HalfbandDecimator_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_HalfbandDecimator_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
HalfbandDecimatorObj_set_state (HalfbandDecimatorObject *self, PyObject *arg)
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
      != dp_HalfbandDecimator_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_HalfbandDecimator_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
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
  return PyFloat_FromDouble (dp_HalfbandDecimator_get_rate (self->handle));
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
      (unsigned long long)dp_HalfbandDecimator_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimator_getset[]
    = { { "rate", (getter)HalfbandDecimator_getprop_rate, NULL,
          "Fixed decimation rate — always 0.5. The halfband decimator is "
          "structurally 2:1; this property exists for API parity with "
          "Resampler and RateConverter.\n",
          NULL },
        { "num_taps", (getter)HalfbandDecimator_getprop_num_taps, NULL,
          "Number of FIR branch taps as passed to create. The all-pass "
          "(even-phase) branch has no taps; only the odd-phase FIR branch has "
          "length num_taps. The total prototype length is 2 * num_taps - 1.\n",
          NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
HalfbandDecimatorObj_destroy (HalfbandDecimatorObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_HalfbandDecimator_destroy (self->handle);
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
      dp_HalfbandDecimator_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorObj_methods[] = {

  { "execute", (PyCFunction)(void *)HalfbandDecimatorObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray\n"
    "\n"
    "Decimate x by 2 using the polyphase halfband FIR filter. Processes\n"
    "every second input sample through the FIR branch and passes the other\n"
    "branch through the all-pass (zero-delay) path. State persists between\n"
    "calls — contiguous blocks give identical output to one large block.\n"
    "Output length is floor(x_len / 2).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    CF32 input array. Length must be even for exact half-rate output;\n"
    "    odd lengths write floor(x_len/2).\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer; must hold at least floor(x_len/2) samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    CF32 decimated output; length is min(floor(x_len / 2), max_out).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import HalfbandDecimator\n"
    ">>> import numpy as np\n"
    ">>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],\n"
    "...              dtype=np.float32)\n"
    ">>> hb = HalfbandDecimator(h=h)\n"
    ">>> y = hb.execute(np.zeros(100, dtype=np.complex64))\n"
    ">>> y.shape, y.dtype\n"
    "((50,), dtype('complex64'))\n" },
  { "execute_max_out", (PyCFunction)HalfbandDecimatorObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Always returns HBDECIM_MAX_OUT.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "reset", (PyCFunction)HalfbandDecimatorObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero all delay lines. Coefficients and num_taps preserved. Call\n"
    "between signal bursts to suppress transient ringing from prior filter\n"
    "state. The next execute() after reset produces the same output as a\n"
    "freshly created decimator fed the same input.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import HalfbandDecimator\n"
    ">>> import numpy as np\n"
    ">>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],\n"
    "...              dtype=np.float32)\n"
    ">>> hb = HalfbandDecimator(h=h)\n"
    ">>> _ = hb.execute(np.ones(64, dtype=np.complex64))\n"
    ">>> hb.reset()\n"
    ">>> hb.num_taps\n"
    "5\n" },
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
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
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

static PyTypeObject HalfbandDecimatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name
  = "doppler.resample.HalfbandDecimator",
  .tp_basicsize = sizeof (HalfbandDecimatorObject),
  .tp_dealloc   = (destructor)HalfbandDecimatorObj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a HalfbandDecimator with caller-supplied FIR taps. Implements a\n"
    "2:1 polyphase halfband decimator over CF32 IQ. The caller provides the "
    "FIR\n"
    "branch coefficient array h; use ``doppler.resample.kaiser_num_taps(2,\n"
    "atten, pb, sb)`` to size it and scipy or the built-in bank helper to "
    "design\n"
    "the prototype. Output length is approximately x_len / 2 per execute() "
    "call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "h : NDArray[np.float32]\n"
    "    Float32 FIR branch coefficients. Must be a symmetric halfband "
    "prototype\n"
    "    (antisymmetric even-indexed taps zeroed).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import HalfbandDecimator\n"
    ">>> import numpy as np\n"
    ">>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],\n"
    "...              dtype=np.float32)\n"
    ">>> hb = HalfbandDecimator(h=h)\n"
    ">>> hb.num_taps, hb.rate\n"
    "(5, 0.5)\n",
  .tp_methods = HalfbandDecimatorObj_methods,
  .tp_getset  = HalfbandDecimator_getset,
  .tp_new     = HalfbandDecimatorObj_new,
  .tp_init    = (initproc)HalfbandDecimatorObj_init,
};
