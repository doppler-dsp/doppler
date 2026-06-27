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
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* allocated capacity for execute */
  void         **_execute_retired; /* gh-219 deferred free */
  size_t         _execute_retired_n;
  size_t         _execute_retired_cap;
} FIRObject;

static void
FIRObj_dealloc (FIRObject *self)
{
  if (self->handle)
    fir_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
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
  {
    size_t _max = fir_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_buf_cap = _max;
      }
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = fir_execute_max_out (self->handle);
      if (_cap < _omax)
        {
          PyErr_Format (PyExc_ValueError,
                        "out has %zu elements, need >= %zu (execute_max_out)",
                        _cap, _omax);
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
  if (!self->_execute_buf || self->_execute_buf_cap < _need)
    {
      size_t _max = fir_execute_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_buf
          && self->_execute_retired_n == self->_execute_retired_cap)
        {
          size_t _rcap = self->_execute_retired_cap
                             ? self->_execute_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_retired     = _rt;
          self->_execute_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_buf)
        self->_execute_retired[self->_execute_retired_n++]
            = self->_execute_buf;
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  size_t    n_out = fir_execute (self->handle,
                                 (const float complex *)PyArray_DATA (in_arr),
                                 (size_t)n, self->_execute_buf);
  npy_intp  dim   = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
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
    "Reset state to post-create defaults." },

  { "execute", (PyCFunction)FIRObj_execute, METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Filter n_in CF32 samples and write the results to out. Each output "
    "sample is the inner product of the tap vector with the current delay "
    "line.  The delay line is updated with each input sample so state carries "
    "over across successive calls — process frames of any size without gaps "
    "or overlap.  The scratch buffer is grown lazily on the first call and "
    "reused on subsequent calls of the same size.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FIR\n"
    "    >>> obj = FIR(np.zeros(1, dtype=np.complex64))\n"
    "    >>> y = obj.execute(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_max_out", (PyCFunction)FIRObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)FIRObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)FIRObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)FIRObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)FIRObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)FIRObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)FIRObj_exit, METH_VARARGS, NULL },
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
