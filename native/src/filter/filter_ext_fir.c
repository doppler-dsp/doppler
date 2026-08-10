/*
 * filter_ext_fir.c — FIR type for the filter module.
 *
 * Included by filter_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only filter_ext.c is compiled.
 */
/* ======================================================== */
/* FIRObject — wraps fir_state_t *       */
/* ======================================================== */

#include "fir/fir_core.h"

typedef struct
{
  PyObject_HEAD fir_state_t *handle;
} FIRObject;

static void
FIRObj_dealloc (FIRObject *self)
{
  if (self->handle)
    fir_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FIRObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FIRObject *self = (FIRObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FIRObj_init (FIRObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "taps", NULL };
  PyObject    *taps_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &taps_obj))
    return -1;
  /* dtype dispatch: float → fir_create_real, float complex → fir_create */
  {
    PyArrayObject *_taps_probe = (PyArrayObject *)PyArray_CheckFromAny (
        taps_obj, NULL, 1, 1, NPY_ARRAY_C_CONTIGUOUS, NULL);
    int _taps_real = _taps_probe && (PyArray_TYPE (_taps_probe) == NPY_FLOAT);
    Py_XDECREF (_taps_probe);
    if (_taps_real)
      {
        PyArrayObject *taps_arr = (PyArrayObject *)PyArray_FROM_OTF (
            taps_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
        if (!taps_arr)
          {
            return -1;
          }
        size_t taps_len = (size_t)PyArray_SIZE (taps_arr);
        self->handle = fir_create_real ((const float *)PyArray_DATA (taps_arr),
                                        taps_len);
        Py_DECREF (taps_arr);
      }
    else
      {
        PyArrayObject *taps_arr = (PyArrayObject *)PyArray_FROM_OTF (
            taps_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
        if (!taps_arr)
          {
            return -1;
          }
        size_t taps_len = (size_t)PyArray_SIZE (taps_arr);
        self->handle    = fir_create (
            (const float complex *)PyArray_DATA (taps_arr), taps_len);
        Py_DECREF (taps_arr);
      }
  }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fir_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
FIRObj_reset (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  fir_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FIRObj_execute_max_out (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fir_execute_max_out (self->handle));
}

static PyObject *
FIRObj_execute (FIRObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fir_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fir_execute (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = fir_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = fir_execute (self->handle,
                              (const float complex *)PyArray_DATA (in_arr),
                              (size_t)n, _d0);
  Py_DECREF (in_arr);
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
FIRObj_state_bytes (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fir_state_bytes (self->handle));
}

static PyObject *
FIRObj_get_state (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = fir_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  fir_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
FIRObj_set_state (FIRObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != fir_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (fir_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
FIR_getprop_num_taps (FIRObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->num_taps);
}
static PyObject *
FIR_getprop_is_real (FIRObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(fir_get_is_real (self->handle)));
}

static PyGetSetDef FIR_getset[] = {
  { "num_taps", (getter)FIR_getprop_num_taps, NULL,
    "Number of tap coefficients supplied at creation. This equals the filter "
    "group delay plus one, and determines the minimum input block length for "
    "which no latency is observable.\n",
    NULL },
  { "is_real", (getter)FIR_getprop_is_real, NULL,
    "True when the filter was created with real-valued tap coefficients. "
    "Real-tap filters (fir_create_real) use a cheaper inner loop: 1 FMA/tap "
    "versus the 2 FMA + lane permute required for complex multiplication. Use "
    "this flag to confirm which constructor path was used at runtime.\n",
    NULL },
  { NULL }
};

static PyObject *
FIRObj_destroy (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fir_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FIRObj_enter (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FIRObj_exit (FIRObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fir_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FIRObj_methods[] = {
  { "reset", (PyCFunction)FIRObj_reset, METH_NOARGS,
    "Zero the delay line; preserve taps and scratch capacity. After a reset "
    "the filter behaves identically to a freshly constructed instance of the "
    "same length, without paying the allocation cost again. Call this between "
    "unrelated signal segments to prevent inter-segment leakage through the "
    "delay line." },

  { "execute", (PyCFunction)(void *)FIRObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Filter n_in CF32 samples and write the results to out. Each output "
    "sample is the inner product of the tap vector with the current delay "
    "line.  The delay line is updated with each input sample so state carries "
    "over across successive calls — process frames of any size without gaps "
    "or overlap.  The scratch buffer is grown lazily on the first call and "
    "reused on subsequent calls of the same size.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of output samples written (always == n_in).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.filter import FIR\n"
    ">>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)\n"
    ">>> fir = FIR(taps)\n"
    ">>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)\n"
    ">>> y = fir.execute(x)\n"
    ">>> y.dtype\n"
    "dtype('complex64')\n"
    ">>> y.shape\n"
    "(3,)\n"
    ">>> [round(float(v.real), 4) for v in y]\n"
    "[0.25, 0.5, 0.25]\n" },
  { "execute_max_out", (PyCFunction)FIRObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)FIRObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the FIR has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)FIRObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the FIR has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)FIRObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the FIR has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)FIRObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)FIRObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a FIR be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "FIR\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)FIRObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the FIR.\n"
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

static PyTypeObject FIRObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "filter.FIR",
  .tp_basicsize                           = sizeof (FIRObject),
  .tp_dealloc                             = (destructor)FIRObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a FIR filter from complex CF32 tap coefficients. Implements a "
    "direct-form FIR convolution: `y[n]` = sum_k `h[k]`*`x[n-k]`. The tap "
    "array is copied at creation; the caller may free it afterward. Use "
    "fir_create_real() instead when all imaginary parts are zero — that path "
    "costs 1 FMA/tap versus 2 FMA + permute + mul here.\n",
  .tp_methods = FIRObj_methods,
  .tp_getset  = FIR_getset,
  .tp_new     = FIRObj_new,
  .tp_init    = (initproc)FIRObj_init,
};
