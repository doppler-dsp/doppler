/*
 * accumulator_ext_acc_trace.c — AccTrace type for the accumulator module.
 *
 * Included by accumulator_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only accumulator_ext.c is compiled.
 */
/* ======================================================== */
/* AccTraceObject — wraps acc_trace_state_t *       */
/* ======================================================== */

#include "acc_trace/acc_trace_core.h"

typedef struct
{
  PyObject_HEAD acc_trace_state_t *handle;
} AccTraceObject;

static void
AccTraceObj_dealloc (AccTraceObject *self)
{
  if (self->handle)
    acc_trace_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AccTraceObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AccTraceObject *self = (AccTraceObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AccTraceObj_init (AccTraceObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "n", "mode", "alpha", NULL };
  unsigned long long n_raw    = 1024;
  const char        *mode_str = "mean";
  double             alpha    = 0.1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Ksd", kwlist, &n_raw,
                                    &mode_str, &alpha))
    return -1;
  size_t n    = (size_t)n_raw;
  int    mode = 0;
  if (strcmp (mode_str, "mean") == 0)
    mode = 0;
  else if (strcmp (mode_str, "exp") == 0)
    mode = 1;
  else if (strcmp (mode_str, "maxhold") == 0)
    mode = 2;
  else if (strcmp (mode_str, "minhold") == 0)
    mode = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "mode must be one of \"mean\", \"exp\", \"maxhold\", "
                    "\"minhold\", got '%s'",
                    mode_str);
      return -1;
    }
  self->handle = acc_trace_create (n, mode, alpha);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acc_trace_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AccTraceObj_accumulate (AccTraceObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "p", NULL };
  PyObject    *p_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &p_obj))
    return NULL;
  PyArrayObject *p_arr = (PyArrayObject *)PyArray_FROM_OTF (
      p_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!p_arr)
    {
      return NULL;
    }
  const float *p     = (const float *)PyArray_DATA (p_arr);
  size_t       p_len = (size_t)PyArray_SIZE (p_arr);
  acc_trace_accumulate (self->handle, p, p_len);
  Py_DECREF (p_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccTraceObj_reset (AccTraceObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  acc_trace_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AccTraceObj_value_max_out (AccTraceObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acc_trace_value_max_out (self->handle));
}

static PyObject *
AccTraceObj_value (AccTraceObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = acc_trace_value_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = acc_trace_value (self->handle, (size_t)n,
                                      (float *)PyArray_DATA (out_arr), _cap);
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _cap  = acc_trace_value_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = acc_trace_value (self->handle, (size_t)n, _d0, _cap);
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
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
AccTraceObj_state_bytes (AccTraceObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acc_trace_state_bytes (self->handle));
}

static PyObject *
AccTraceObj_get_state (AccTraceObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = acc_trace_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  acc_trace_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AccTraceObj_set_state (AccTraceObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != acc_trace_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (acc_trace_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
AccTrace_getprop_n (AccTraceObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
AccTrace_getprop_alpha (AccTraceObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->alpha);
}
static int
AccTrace_setprop_alpha (AccTraceObject *self, PyObject *value,
                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  self->handle->alpha = v;
  return 0;
}
static PyObject *
AccTrace_getprop_count (AccTraceObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(size_t)self->handle->count);
}
static PyObject *
AccTrace_getprop_mode (AccTraceObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)(int)self->handle->mode);
}

static PyGetSetDef AccTrace_getset[] = {
  { "n", (getter)AccTrace_getprop_n, NULL, "Trace length (bins).\n", NULL },
  { "alpha", (getter)AccTrace_getprop_alpha, (setter)AccTrace_setprop_alpha,
    "EMA smoothing factor (exp mode).\n", NULL },
  { "count", (getter)AccTrace_getprop_count, NULL,
    "Frames folded in so far.\n", NULL },
  { "mode", (getter)AccTrace_getprop_mode, NULL, "Reduction mode.\n", NULL },
  { NULL }
};

static PyObject *
AccTraceObj_destroy (AccTraceObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acc_trace_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccTraceObj_enter (AccTraceObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AccTraceObj_exit (AccTraceObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acc_trace_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AccTraceObj_methods[] = {

  { "accumulate", (PyCFunction)(void *)AccTraceObj_accumulate,
    METH_VARARGS | METH_KEYWORDS,
    "accumulate(p) -> None\n"
    "\n"
    "Fold one length-n frame into the running trace.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "p : NDArray[np.float32]\n"
    "    Input frame (float32).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccTrace\n"
    ">>> acc = AccTrace(n=4, mode=\"mean\")\n"
    ">>> acc.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))\n"
    ">>> acc.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))\n"
    ">>> acc.value().tolist()\n"
    "[2.0, 4.0, 6.0, 8.0]\n" },
  { "reset", (PyCFunction)AccTraceObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Discard the running trace; the next accumulate re-seeds it.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccTrace\n"
    ">>> acc = AccTrace(n=4, mode=\"mean\")\n"
    ">>> acc.accumulate(np.ones(4, dtype=np.float32))\n"
    ">>> acc.reset()\n"
    ">>> acc.count\n"
    "0\n" },
  { "value", (PyCFunction)(void *)AccTraceObj_value,
    METH_VARARGS | METH_KEYWORDS,
    "value(n=1) -> ndarray\n"
    "\n"
    "Copy the current averaged trace (None before any accumulate).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccTrace\n"
    "    >>> obj = AccTrace(1024, \"mean\", 0.1)\n"
    "    >>> y = obj.value(4)\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "value_max_out", (PyCFunction)AccTraceObj_value_max_out, METH_NOARGS,
    "value_max_out() -> int\n\nMax output length value() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)AccTraceObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AccTrace has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AccTraceObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AccTrace has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AccTraceObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AccTrace has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AccTraceObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AccTraceObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AccTrace be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AccTrace\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AccTraceObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the AccTrace.\n"
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

static PyTypeObject AccTraceObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "accumulator.AccTrace",
  .tp_basicsize                           = sizeof (AccTraceObject),
  .tp_dealloc                             = (destructor)AccTraceObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a length-n trace accumulator.\n",
  .tp_methods = AccTraceObj_methods,
  .tp_getset  = AccTrace_getset,
  .tp_new     = AccTraceObj_new,
  .tp_init    = (initproc)AccTraceObj_init,
};
