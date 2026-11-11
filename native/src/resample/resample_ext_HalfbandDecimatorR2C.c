/* jm:generated resample_ext_HalfbandDecimatorR2C.c */
/*
 * resample_ext_HalfbandDecimatorR2C.c — HalfbandDecimatorR2C type for the
 * resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in resample_ext_HalfbandDecimatorR2C_extra.c.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* HalfbandDecimatorR2CObject — wraps dp_HalfbandDecimatorR2C_state_t * */
/* ======================================================== */

#include "doppler/HalfbandDecimatorR2C/HalfbandDecimatorR2C_core.h"

typedef struct
{
  PyObject_HEAD dp_HalfbandDecimatorR2C_state_t *handle;
} HalfbandDecimatorR2CObject;

static void
HalfbandDecimatorR2CObj_dealloc (HalfbandDecimatorR2CObject *self)
{
  if (self->handle)
    dp_HalfbandDecimatorR2C_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorR2CObj_new (PyTypeObject *type, PyObject *args,
                             PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  HalfbandDecimatorR2CObject *self
      = (HalfbandDecimatorR2CObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimatorR2CObj_init (HalfbandDecimatorR2CObject *self, PyObject *args,
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
  self->handle = dp_HalfbandDecimatorR2C_create (
      (const float *)PyArray_DATA (h_arr), h_len);
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_HalfbandDecimatorR2C_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorR2CObj_execute_max_out (HalfbandDecimatorR2CObject *self,
                                         PyObject                   *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t x_len = 0;
  if (!PyArg_ParseTuple (args, "n", &x_len))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_HalfbandDecimatorR2C_execute_max_out (self->handle, (size_t)x_len));
}

static PyObject *
HalfbandDecimatorR2CObj_execute (HalfbandDecimatorR2CObject *self,
                                 PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *x_obj     = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float *x     = (const float *)PyArray_DATA (x_arr);
  size_t       x_len = (size_t)PyArray_SIZE (x_arr);
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
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax = dp_HalfbandDecimatorR2C_execute_max_out (
          self->handle, (size_t)PyArray_SIZE (x_arr));
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t n_out = dp_HalfbandDecimatorR2C_execute (
          self->handle, x, x_len, (float _Complex *)PyArray_DATA (out_arr),
          _cap);
      Py_DECREF (x_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "HalfbandDecimatorR2C.execute: wrote %zu elements "
                        "into a buffer of %zu",
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
  if (!self->handle)
    {
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_HalfbandDecimatorR2C_execute_max_out (
      self->handle, (size_t)PyArray_SIZE (x_arr));
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "HalfbandDecimatorR2C.execute: output of %zu elements is too large",
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
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t n_out
      = dp_HalfbandDecimatorR2C_execute (self->handle, x, x_len, _d0, _cap);
  Py_DECREF (x_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "HalfbandDecimatorR2C.execute: wrote %zu elements into a "
                    "buffer of %zu",
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
HalfbandDecimatorR2CObj_reset (HalfbandDecimatorR2CObject *self,
                               PyObject                   *Py_UNUSED (ignored))
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
  dp_HalfbandDecimatorR2C_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorR2CObj_state_bytes (HalfbandDecimatorR2CObject *self,
                                     PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_HalfbandDecimatorR2C_state_bytes (self->handle));
}

static PyObject *
HalfbandDecimatorR2CObj_get_state (HalfbandDecimatorR2CObject *self,
                                   PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_HalfbandDecimatorR2C_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_HalfbandDecimatorR2C_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
HalfbandDecimatorR2CObj_set_state (HalfbandDecimatorR2CObject *self,
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
      != dp_HalfbandDecimatorR2C_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_HalfbandDecimatorR2C_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
HalfbandDecimatorR2C_getprop_rate (HalfbandDecimatorR2CObject *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (dp_HalfbandDecimatorR2C_get_rate (self->handle));
}
static PyObject *
HalfbandDecimatorR2C_getprop_num_taps (HalfbandDecimatorR2CObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_HalfbandDecimatorR2C_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimatorR2C_getset[]
    = { { "rate", (getter)HalfbandDecimatorR2C_getprop_rate, NULL,
          "Fixed decimation rate — always 0.5.\n", NULL },
        { "num_taps", (getter)HalfbandDecimatorR2C_getprop_num_taps, NULL,
          "Number of FIR branch taps as passed to create.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
HalfbandDecimatorR2CObj_destroy (HalfbandDecimatorR2CObject *self,
                                 PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_HalfbandDecimatorR2C_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorR2CObj_enter (HalfbandDecimatorR2CObject *self,
                               PyObject                   *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HalfbandDecimatorR2CObj_exit (HalfbandDecimatorR2CObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_HalfbandDecimatorR2C_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorR2CObj_methods[] = {

  { "execute", (PyCFunction)(void *)HalfbandDecimatorR2CObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray\n"
    "\n"
    "Decimate real x by 2 to CF32, shifting fs/4 to DC. Each (even, odd)\n"
    "input pair yields one complex output. State persists between calls: a\n"
    "sample left unpaired at the end of a block (counting one held from the\n"
    "previous call) is held for the next, so contiguous blocks give\n"
    "identical output to one large block.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.float32]\n"
    "    Real F32 input array.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    CF32 output buffer; must hold at least (x_len + 1) / 2 samples,\n"
    "    which is dp_HalfbandDecimatorR2C_execute_max_out(). Python refuses\n"
    "    a shorter `out=`. In C a shorter one drops nothing: the call stops\n"
    "    at a pair boundary and dp_hbdecim_r2c_consumed() says how much\n"
    "    input it took.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of CF32 samples written to out: min((x_len + held) / 2,\n"
    "    max_out). 0 if x_len or max_out is 0.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import HalfbandDecimatorR2C\n"
    ">>> import numpy as np\n"
    ">>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],\n"
    "...              dtype=np.float32)\n"
    ">>> hb = HalfbandDecimatorR2C(h=h)\n"
    ">>> y = hb.execute(np.zeros(100, dtype=np.float32))\n"
    ">>> y.shape, y.dtype\n"
    "((50,), dtype('complex64'))\n" },
  { "execute_max_out", (PyCFunction)HalfbandDecimatorR2CObj_execute_max_out,
    METH_VARARGS,
    "execute_max_out(x_len) -> int\n"
    "\n"
    "The most outputs one dp_HalfbandDecimatorR2C_execute() call over\n"
    "x_len inputs can write: `(x_len + 1) / 2`.\n"
    "\n"
    "One output per input pair, and at most one sample held over from the\n"
    "previous call, so `(x_len + held) / 2` never exceeds this.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x_len : int\n"
    "    Number of inputs the call will be given.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    `(x_len + 1) / 2`; 0 for an empty call.\n" },
  { "reset", (PyCFunction)HalfbandDecimatorR2CObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero the delay lines and the fs/4 phase. Coefficients and num_taps\n"
    "preserved. The next execute() after reset produces the same output as a\n"
    "freshly created decimator fed the same input. Any sample held for the\n"
    "next call is dropped too.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import HalfbandDecimatorR2C\n"
    ">>> import numpy as np\n"
    ">>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],\n"
    "...              dtype=np.float32)\n"
    ">>> hb = HalfbandDecimatorR2C(h=h)\n"
    ">>> x = np.ones(64, dtype=np.float32)\n"
    ">>> a = hb.execute(x)\n"
    ">>> hb.reset()\n"
    ">>> bool(np.array_equal(hb.execute(x), a))\n"
    "True\n" },
  { "state_bytes", (PyCFunction)HalfbandDecimatorR2CObj_state_bytes,
    METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the HalfbandDecimatorR2C has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)HalfbandDecimatorR2CObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the HalfbandDecimatorR2C has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)HalfbandDecimatorR2CObj_set_state, METH_O,
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
    "``RuntimeError`` if the HalfbandDecimatorR2C has already been\n"
    "destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)HalfbandDecimatorR2CObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)HalfbandDecimatorR2CObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a HalfbandDecimatorR2C be used in a `with` statement so its C\n"
    "resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "HalfbandDecimatorR2C\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)HalfbandDecimatorR2CObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the HalfbandDecimatorR2C.\n"
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

static PyTypeObject HalfbandDecimatorR2CObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name
  = "doppler.resample.HalfbandDecimatorR2C",
  .tp_basicsize = sizeof (HalfbandDecimatorR2CObject),
  .tp_dealloc   = (destructor)HalfbandDecimatorR2CObj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a real-input halfband decimator with caller-supplied FIR taps.\n"
    "Implements a 2:1 polyphase halfband decimator from real F32 at fs to "
    "CF32\n"
    "at fs/2, with an embedded fs/4 shift: a tone at f_in leaves at f_in - "
    "fs/4\n"
    "(input-normalised 0.25 lands at DC). The FIR branch is the same one\n"
    "HalfbandDecimator takes; size it with "
    "``doppler.resample.kaiser_num_taps(2,\n"
    "atten, pb, sb)``.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "h : NDArray[np.float32]\n"
    "    Float32 FIR branch coefficients (the branch of the halfband "
    "prototype\n"
    "    with more than one nonzero tap, not the whole prototype). Copied; "
    "must\n"
    "    be non-NULL.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import HalfbandDecimatorR2C\n"
    ">>> import numpy as np\n"
    ">>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],\n"
    "...              dtype=np.float32)\n"
    ">>> hb = HalfbandDecimatorR2C(h=h)\n"
    ">>> hb.num_taps, hb.rate\n"
    "(5, 0.5)\n",
  .tp_methods = HalfbandDecimatorR2CObj_methods,
  .tp_getset  = HalfbandDecimatorR2C_getset,
  .tp_new     = HalfbandDecimatorR2CObj_new,
  .tp_init    = (initproc)HalfbandDecimatorR2CObj_init,
};
