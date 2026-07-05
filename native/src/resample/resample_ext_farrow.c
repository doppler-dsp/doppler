/*
 * resample_ext_farrow.c — Farrow type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* FarrowObject — wraps farrow_state_t *       */
/* ======================================================== */

#include "farrow/farrow_core.h"

typedef struct
{
  PyObject_HEAD farrow_state_t *handle;
  float complex *_delay_buf;     /* pre-allocated output for delay */
  size_t         _delay_buf_cap; /* allocated capacity for delay */
  void         **_delay_retired; /* gh-219 deferred free */
  size_t         _delay_retired_n;
  size_t         _delay_retired_cap;
} FarrowObject;

static void
FarrowObj_dealloc (FarrowObject *self)
{
  if (self->handle)
    farrow_destroy (self->handle);
  free (self->_delay_buf);
  for (size_t _i = 0; _i < self->_delay_retired_n; _i++)
    free (self->_delay_retired[_i]);
  free (self->_delay_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FarrowObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FarrowObject *self = (FarrowObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FarrowObj_init (FarrowObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "order", NULL };
  const char  *order_str = "cubic";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|s", kwlist, &order_str))
    return -1;
  int order = 0;
  if (strcmp (order_str, "linear") == 0)
    order = 0;
  else if (strcmp (order_str, "parabolic") == 0)
    order = 1;
  else if (strcmp (order_str, "cubic") == 0)
    order = 2;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "order must be one of \"linear\", \"parabolic\", "
                    "\"cubic\", got '%s'",
                    order_str);
      return -1;
    }
  self->handle = farrow_create (order);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "farrow_create returned NULL");
      return -1;
    }
  {
    size_t _max = farrow_delay_max_out (self->handle);
    if (_max)
      {
        self->_delay_buf = malloc (_max * sizeof (float complex));
        if (!self->_delay_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_delay_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
FarrowObj_delay_max_out (FarrowObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (farrow_delay_max_out (self->handle));
}

static PyObject *
FarrowObj_delay (FarrowObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "mu", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  double         mu        = 0;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od|O", _kwlist, &x_obj, &mu,
                                    &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;

  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = farrow_delay_max_out (self->handle);
      size_t _n_in    = (size_t)PyArray_SIZE (x_arr);
      size_t _min_cap = _omax > _n_in ? _omax : _n_in;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      const float complex *_ng0o = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1o = _n_in;
      float complex       *_ng2o = (float complex *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = farrow_delay (self->handle, _ng0o, _ng1o, mu, _ng2o, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
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

  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_delay_buf || self->_delay_buf_cap < _need)
    {
      size_t _max = farrow_delay_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_delay_buf
          && self->_delay_retired_n == self->_delay_retired_cap)
        {
          size_t _rcap
              = self->_delay_retired_cap ? self->_delay_retired_cap * 2 : 4;
          void **_rt = realloc (self->_delay_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_delay_retired     = _rt;
          self->_delay_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_delay_buf)
        self->_delay_retired[self->_delay_retired_n++] = self->_delay_buf;
      self->_delay_buf     = _tmp;
      self->_delay_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = farrow_delay (self->handle, _ng0, _ng1, mu, self->_delay_buf,
                          self->_delay_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_delay_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
FarrowObj_reset (FarrowObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  farrow_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FarrowObj_state_bytes (FarrowObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (farrow_state_bytes (self->handle));
}

static PyObject *
FarrowObj_get_state (FarrowObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = farrow_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  farrow_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
FarrowObj_set_state (FarrowObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != farrow_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (farrow_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Farrow_getprop_group_delay (FarrowObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)farrow_get_group_delay (self->handle));
}

static PyGetSetDef Farrow_getset[]
    = { { "group_delay", (getter)Farrow_getprop_group_delay, NULL,
          "Group delay.\n", NULL },
        { NULL } };

static PyObject *
FarrowObj_destroy (FarrowObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      farrow_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FarrowObj_enter (FarrowObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FarrowObj_exit (FarrowObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      farrow_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FarrowObj_methods[] = {

  { "delay", (PyCFunction)(void *)FarrowObj_delay,
    METH_VARARGS | METH_KEYWORDS,
    "delay(x, mu, out=None) -> ndarray\n"
    "\n"
    "Apply a constant fractional delay of `mu` samples to a cf32 block via "
    "the Farrow interpolator; output[i] is the input interpolated at i - "
    "group_delay + mu. The first group_delay samples are filling-transient. "
    "Without out=, the returned array is a view into a buffer reused on "
    "the next call (see delay_max_out() to size an out= buffer for an "
    "independent, alias-free result).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Farrow\n"
    "    >>> obj = Farrow(\"cubic\")\n"
    "    >>> y = obj.delay(np.zeros(4), 0.0)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "delay_max_out", (PyCFunction)FarrowObj_delay_max_out, METH_NOARGS,
    "delay_max_out() -> int\n\nMax output length delay() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "reset", (PyCFunction)FarrowObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Clear the interpolator delay line.\n"
    "\n"
    "    >>> from doppler import Farrow\n"
    "    >>> obj = Farrow(\"cubic\")\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)FarrowObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)FarrowObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)FarrowObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)FarrowObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)FarrowObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)FarrowObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject FarrowObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.Farrow",
  .tp_basicsize                           = sizeof (FarrowObject),
  .tp_dealloc                             = (destructor)FarrowObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create a Farrow interpolator.\n",
  .tp_methods                             = FarrowObj_methods,
  .tp_getset                              = Farrow_getset,
  .tp_new                                 = FarrowObj_new,
  .tp_init                                = (initproc)FarrowObj_init,
};
