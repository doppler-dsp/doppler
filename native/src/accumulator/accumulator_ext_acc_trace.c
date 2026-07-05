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
  float *_value_buf;     /* pre-allocated output for value */
  size_t _value_buf_cap; /* allocated capacity for value */
  void **_value_retired; /* gh-219 deferred free */
  size_t _value_retired_n;
  size_t _value_retired_cap;
} AccTraceObject;

static void
AccTraceObj_dealloc (AccTraceObject *self)
{
  if (self->handle)
    acc_trace_destroy (self->handle);
  free (self->_value_buf);
  for (size_t _i = 0; _i < self->_value_retired_n; _i++)
    free (self->_value_retired[_i]);
  free (self->_value_retired);
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
  static char       *kwlist[] = { "mode", "n", "alpha", NULL };
  const char        *mode_str = "mean";
  unsigned long long n_raw    = 1024;
  double             alpha    = 0.1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|sKd", kwlist, &mode_str,
                                    &n_raw, &alpha))
    return -1;
  int mode = 0;
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
  size_t n     = (size_t)n_raw;
  self->handle = acc_trace_create (n, mode, alpha);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acc_trace_create returned NULL");
      return -1;
    }
  {
    size_t _max = acc_trace_value_max_out (self->handle);
    if (_max)
      {
        self->_value_buf = malloc (_max * sizeof (float));
        if (!self->_value_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_value_buf_cap = _max;
      }
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
                                      (float *)PyArray_DATA (out_arr));
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
  if (!self->_value_buf || self->_value_buf_cap < _need)
    {
      size_t _max = acc_trace_value_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_value_buf
          && self->_value_retired_n == self->_value_retired_cap)
        {
          size_t _rcap
              = self->_value_retired_cap ? self->_value_retired_cap * 2 : 4;
          void **_rt = realloc (self->_value_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              PyErr_NoMemory ();
              return NULL;
            }
          self->_value_retired     = _rt;
          self->_value_retired_cap = _rcap;
        }
      float *_tmp = malloc (_max * sizeof (float));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_value_buf)
        self->_value_retired[self->_value_retired_n++] = self->_value_buf;
      self->_value_buf     = _tmp;
      self->_value_buf_cap = _max;
    }
  size_t n_out = acc_trace_value (self->handle, (size_t)n, self->_value_buf);
  if (!n_out)
    Py_RETURN_NONE;
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_FLOAT, self->_value_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  return arr;
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

static PyGetSetDef AccTrace_getset[]
    = { { "n", (getter)AccTrace_getprop_n, NULL, "N.\n", NULL },
        { "alpha", (getter)AccTrace_getprop_alpha,
          (setter)AccTrace_setprop_alpha, "Alpha.\n", NULL },
        { "count", (getter)AccTrace_getprop_count, NULL, "Count.\n", NULL },
        { "mode", (getter)AccTrace_getprop_mode, NULL, "Mode.\n", NULL },
        { NULL } };

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
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccTrace\n"
    "    >>> obj = AccTrace(\"mean\", 1024, 0.1)\n"
    "    >>> obj.accumulate(np.zeros(4, dtype=np.float32))\n" },
  { "reset", (PyCFunction)AccTraceObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Discard the running trace; the next accumulate re-seeds it.\n"
    "\n"
    "    >>> from doppler import AccTrace\n"
    "    >>> obj = AccTrace(\"mean\", 1024, 0.1)\n"
    "    >>> obj.reset()\n" },
  { "value", (PyCFunction)AccTraceObj_value, METH_VARARGS | METH_KEYWORDS,
    "value(n=1) -> ndarray\n"
    "\n"
    "Copy the current averaged trace (None before any accumulate).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccTrace\n"
    "    >>> obj = AccTrace(\"mean\", 1024, 0.1)\n"
    "    >>> y = obj.value(4)\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "value_max_out", (PyCFunction)AccTraceObj_value_max_out, METH_NOARGS,
    "value_max_out() -> int\n\nMax output length value() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)AccTraceObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)AccTraceObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)AccTraceObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)AccTraceObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)AccTraceObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)AccTraceObj_exit, METH_VARARGS, NULL },
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
