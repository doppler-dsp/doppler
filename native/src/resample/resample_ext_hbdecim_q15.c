/* jm:generated resample_ext_hbdecim_q15.c */
/*
 * resample_ext_hbdecim_q15.c — HalfbandDecimatorQ15 type for the resample
 * module.
 *
 * Included by resample_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in resample_ext_hbdecim_q15_extra.c.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* HalfbandDecimatorQ15Object — wraps dp_hbdecim_q15_state_t *       */
/* ======================================================== */

#include "doppler/hbdecim_q15/hbdecim_q15_core.h"

typedef struct
{
  PyObject_HEAD dp_hbdecim_q15_state_t *handle;
} HalfbandDecimatorQ15Object;

static void
HalfbandDecimatorQ15Obj_dealloc (HalfbandDecimatorQ15Object *self)
{
  if (self->handle)
    dp_hbdecim_q15_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorQ15Obj_new (PyTypeObject *type, PyObject *args,
                             PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  HalfbandDecimatorQ15Object *self
      = (HalfbandDecimatorQ15Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimatorQ15Obj_init (HalfbandDecimatorQ15Object *self, PyObject *args,
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
  size_t h_len = (size_t)PyArray_SIZE (h_arr);
  self->handle
      = dp_hbdecim_q15_create (h_len, (const float *)PyArray_DATA (h_arr));
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_hbdecim_q15_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorQ15Obj_execute_max_out (HalfbandDecimatorQ15Object *self,
                                         PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _mo_need = (size_t)(dp_hbdecim_q15_execute_max_out (self->handle));
  if (_mo_need > (size_t)(SIZE_MAX / 2))
    {
      PyErr_Format (PyExc_OverflowError,
                    "HalfbandDecimatorQ15.execute_max_out: output of %zu "
                    "samples of 2 elements is too large",
                    _mo_need);
      return NULL;
    }
  size_t _mo = (size_t)_mo_need;
  return PyLong_FromSize_t (_mo * 2);
}

static PyObject *
HalfbandDecimatorQ15Obj_execute (HalfbandDecimatorQ15Object *self,
                                 PyObject *args, PyObject *kwds)
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
  x_arr = jm_array_arg (x_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_INT16
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
          = jm_array_arg (out_obj, NPY_INT16,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr) / 2;
      size_t _omax    = dp_hbdecim_q15_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr) / 2
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr) / 2);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError,
                        "out has %zu samples of 2 elements, need >= %zu", _cap,
                        _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t n_out = dp_hbdecim_q15_execute (
          self->handle, (const int16_t *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr) / 2, (int16_t *)PyArray_DATA (out_arr),
          _cap);
      Py_DECREF (x_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "HalfbandDecimatorQ15.execute: wrote %zu samples of 2 "
                        "elements into a buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)(n_out * 2);
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_INT16,
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
  size_t _need = (size_t)PyArray_SIZE (x_arr) / 2;
  size_t _cap  = dp_hbdecim_q15_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)(NPY_MAX_INTP / 2))
    {
      Py_DECREF (x_arr);
      PyErr_Format (PyExc_OverflowError,
                    "HalfbandDecimatorQ15.execute: output of %zu samples of 2 "
                    "elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp _adim = (npy_intp)_adim_need;
  _adim *= 2;
  PyObject *arr0 = PyArray_SimpleNew (1, &_adim, NPY_INT16);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  int16_t *_d0   = (int16_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = dp_hbdecim_q15_execute (
      self->handle, (const int16_t *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr) / 2, _d0, _cap);
  Py_DECREF (x_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "HalfbandDecimatorQ15.execute: wrote %zu samples of 2 "
                    "elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)(n_out * 2);
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
HalfbandDecimatorQ15Obj_reset (HalfbandDecimatorQ15Object *self,
                               PyObject                   *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_hbdecim_q15_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorQ15Obj_state_bytes (HalfbandDecimatorQ15Object *self,
                                     PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_hbdecim_q15_state_bytes (self->handle));
}

static PyObject *
HalfbandDecimatorQ15Obj_get_state (HalfbandDecimatorQ15Object *self,
                                   PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_hbdecim_q15_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_hbdecim_q15_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
HalfbandDecimatorQ15Obj_set_state (HalfbandDecimatorQ15Object *self,
                                   PyObject                   *arg)
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
      != dp_hbdecim_q15_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_hbdecim_q15_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
HalfbandDecimatorQ15_getprop_num_taps (HalfbandDecimatorQ15Object *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_hbdecim_q15_get_num_taps (self->handle));
}
static PyObject *
HalfbandDecimatorQ15_getprop_rate (HalfbandDecimatorQ15Object *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dp_hbdecim_q15_get_rate (self->handle));
}

static PyGetSetDef HalfbandDecimatorQ15_getset[] = {
  { "num_taps", (getter)HalfbandDecimatorQ15_getprop_num_taps, NULL,
    "FIR branch length as supplied to the constructor. This is the count of "
    "non-zero symmetric taps in the FIR branch, not the full sparse halfband "
    "prototype length.  Useful for introspection when chaining multiple "
    "stages with programmatically computed filter banks.\n",
    NULL },
  { "rate", (getter)HalfbandDecimatorQ15_getprop_rate, NULL,
    "The sample-rate reduction factor; always 0.5 for 2:1 decimation. Exposed "
    "as a read-only property so pipelines can query the rate of each stage "
    "programmatically without hard-coding the 2:1 assumption.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
HalfbandDecimatorQ15Obj_destroy (HalfbandDecimatorQ15Object *self,
                                 PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_hbdecim_q15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorQ15Obj_enter (HalfbandDecimatorQ15Object *self,
                               PyObject                   *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HalfbandDecimatorQ15Obj_exit (HalfbandDecimatorQ15Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_hbdecim_q15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorQ15Obj_methods[] = {

  { "execute", (PyCFunction)(void *)HalfbandDecimatorQ15Obj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray\n"
    "\n"
    "Decimate a block of interleaved IQ int16 samples by 2. Input must be\n"
    "interleaved int16_t IQ pairs (I₀ Q₀ I₁ Q₁ …); pass a 1-D array of\n"
    "2*n_complex elements. Each pair of complex input samples produces one\n"
    "complex output sample, so an array of length 2N yields at most N output\n"
    "pairs (2N int16 output values). If n_in is odd the trailing IQ pair is\n"
    "buffered and consumed on the next call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.int16]\n"
    "    Input.\n"
    "out : npt.NDArray[np.int16] | None\n"
    "    Output buffer; caller must provide space for 2*max_out int16_t\n"
    "    values (one interleaved I/Q pair per output).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.int16]\n"
    "    min(available, max_out) COMPLEX samples -- twice that many int16_t\n"
    "    values.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.resample import HalfbandDecimatorQ15\n"
    ">>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)\n"
    ">>> dec = HalfbandDecimatorQ15(h)\n"
    ">>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)\n"
    ">>> y = dec.execute(x)\n"
    ">>> y.dtype\n"
    "dtype('int16')\n"
    ">>> y.shape\n"
    "(4,)\n"
    ">>> y.tolist()\n"
    "[0, 0, 625, 0]\n" },
  { "execute_max_out", (PyCFunction)HalfbandDecimatorQ15Obj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Maximum output samples for a given input length.\n"
    "\n"
    "Returns 0 to trigger the lazy-alloc path in the Python glue: the output\n"
    "buffer is sized to n_in on first call (always sufficient for 2:1).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "reset", (PyCFunction)HalfbandDecimatorQ15Obj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero all delay rings and clear the pending-sample flag. After a\n"
    "reset the decimator behaves identically to a freshly constructed\n"
    "instance: the four dual-write delay rings are zeroed and has_pending is\n"
    "cleared, so no partial IQ pair carries over. Call this between\n"
    "unrelated signal segments to prevent inter-segment leakage.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.resample import HalfbandDecimatorQ15\n"
    ">>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)\n"
    ">>> dec = HalfbandDecimatorQ15(h)\n"
    ">>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)\n"
    ">>> _ = dec.execute(x)\n"
    ">>> dec.reset()\n"
    ">>> y = dec.execute(x)\n"
    ">>> y.tolist()\n"
    "[0, 0, 625, 0]\n" },
  { "state_bytes", (PyCFunction)HalfbandDecimatorQ15Obj_state_bytes,
    METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the HalfbandDecimatorQ15 has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)HalfbandDecimatorQ15Obj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the HalfbandDecimatorQ15 has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)HalfbandDecimatorQ15Obj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the HalfbandDecimatorQ15 has already been\n"
    "destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)HalfbandDecimatorQ15Obj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)HalfbandDecimatorQ15Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a HalfbandDecimatorQ15 be used in a `with` statement so its C\n"
    "resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "HalfbandDecimatorQ15\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)HalfbandDecimatorQ15Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the HalfbandDecimatorQ15.\n"
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

static PyTypeObject HalfbandDecimatorQ15ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name
  = "doppler.resample.HalfbandDecimatorQ15",
  .tp_basicsize = sizeof (HalfbandDecimatorQ15Object),
  .tp_dealloc   = (destructor)HalfbandDecimatorQ15Obj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate and initialise a fixed-point halfband 2:1 decimator. The FIR\n"
    "branch coefficients are supplied as float and converted internally to "
    "Q15\n"
    "with a x0.5 polyphase rate scaling. The full halfband prototype is "
    "sparse\n"
    "(every other tap is zero); supply only the non-zero FIR branch taps, "
    "not\n"
    "the full sparse prototype.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "h : NDArray[np.float32]\n"
    "    Float FIR branch coefficients of length num_taps. Must be symmetric\n"
    "    (`h[k]` == `h[num_taps-1-k]`).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.resample import HalfbandDecimatorQ15\n"
    ">>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)\n"
    ">>> dec = HalfbandDecimatorQ15(h)\n"
    ">>> dec.num_taps\n"
    "3\n"
    ">>> dec.rate\n"
    "0.5\n",
  .tp_methods = HalfbandDecimatorQ15Obj_methods,
  .tp_getset  = HalfbandDecimatorQ15_getset,
  .tp_new     = HalfbandDecimatorQ15Obj_new,
  .tp_init    = (initproc)HalfbandDecimatorQ15Obj_init,
};
