/*
 * impairment_ext_doppler_channel.c — DopplerChannel type for the impairment
 * module.
 *
 * Included by impairment_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only impairment_ext.c is compiled.
 */
/* ======================================================== */
/* DopplerChannelObject — wraps doppler_channel_state_t *       */
/* ======================================================== */

#include "doppler_channel/doppler_channel_core.h"

typedef struct
{
  PyObject_HEAD doppler_channel_state_t *handle;
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* allocated capacity for execute */
  void         **_execute_retired; /* gh-219 deferred free */
  size_t         _execute_retired_n;
  size_t         _execute_retired_cap;
  PyObject      *_execute_view_ref; /* gh-437 last returned view */
} DopplerChannelObject;

static void
DopplerChannelObj_dealloc (DopplerChannelObject *self)
{
  if (self->handle)
    doppler_channel_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
  Py_XDECREF (self->_execute_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DopplerChannelObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DopplerChannelObject *self
      = (DopplerChannelObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DopplerChannelObj_init (DopplerChannelObject *self, PyObject *args,
                        PyObject *kwds)
{
  static char *kwlist[]
      = { "fs", "carrier_hz", "doppler_ppm", "doppler_rate_ppm_s", NULL };
  double fs                 = 1000000.0;
  double carrier_hz         = 0.0;
  double doppler_ppm        = 0.0;
  double doppler_rate_ppm_s = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddd", kwlist, &fs,
                                    &carrier_hz, &doppler_ppm,
                                    &doppler_rate_ppm_s))
    return -1;
  self->handle = doppler_channel_create (fs, carrier_hz, doppler_ppm,
                                         doppler_rate_ppm_s);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "doppler_channel_create returned NULL");
      return -1;
    }
  {
    size_t _max = doppler_channel_execute_max_out (self->handle);
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
DopplerChannelObj_execute_max_out (DopplerChannelObject *self,
                                   PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (doppler_channel_execute_max_out (self->handle));
}

static PyObject *
DopplerChannelObj_execute (DopplerChannelObject *self, PyObject *args,
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
      size_t _omax    = doppler_channel_execute_max_out (self->handle);
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
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex       *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = doppler_channel_execute (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _need      = (size_t)PyArray_SIZE (x_arr);
  int    _view_live = 0;
  if (self->_execute_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_execute_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_execute_view_ref) != Py_None;
#endif
    }
  if (!self->_execute_buf || self->_execute_buf_cap < _need || _view_live)
    {
      size_t _max = doppler_channel_execute_max_out (self->handle);
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
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_retired     = _rt;
          self->_execute_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_buf)
        self->_execute_retired[self->_execute_retired_n++]
            = self->_execute_buf;
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = doppler_channel_execute (
        self->handle, _ng0, _ng1, self->_execute_buf, self->_execute_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_execute_view_ref);
  self->_execute_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_execute_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
DopplerChannelObj_reset (DopplerChannelObject *self,
                         PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  doppler_channel_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DopplerChannelObj_state_bytes (DopplerChannelObject *self,
                               PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (doppler_channel_state_bytes (self->handle));
}

static PyObject *
DopplerChannelObj_get_state (DopplerChannelObject *self,
                             PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = doppler_channel_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  doppler_channel_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DopplerChannelObj_set_state (DopplerChannelObject *self, PyObject *arg)
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
      != doppler_channel_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (doppler_channel_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
DopplerChannel_getprop_fs (DopplerChannelObject *self,
                           void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
DopplerChannel_getprop_carrier_hz (DopplerChannelObject *self,
                                   void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->carrier_hz);
}
static PyObject *
DopplerChannel_getprop_doppler_ppm (DopplerChannelObject *self,
                                    void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_ppm);
}
static PyObject *
DopplerChannel_getprop_doppler_rate_ppm_s (DopplerChannelObject *self,
                                           void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_rate_ppm_s);
}
static PyObject *
DopplerChannel_getprop_elapsed_s (DopplerChannelObject *self,
                                  void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (doppler_channel_get_elapsed_s (self->handle));
}
static PyObject *
DopplerChannel_getprop_offset_hz (DopplerChannelObject *self,
                                  void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (doppler_channel_get_offset_hz (self->handle));
}

static PyGetSetDef DopplerChannel_getset[]
    = { { "fs", (getter)DopplerChannel_getprop_fs, NULL, "Fs.\n", NULL },
        { "carrier_hz", (getter)DopplerChannel_getprop_carrier_hz, NULL,
          "Carrier hz.\n", NULL },
        { "doppler_ppm", (getter)DopplerChannel_getprop_doppler_ppm, NULL,
          "Doppler ppm.\n", NULL },
        { "doppler_rate_ppm_s",
          (getter)DopplerChannel_getprop_doppler_rate_ppm_s, NULL,
          "Doppler rate ppm s.\n", NULL },
        { "elapsed_s", (getter)DopplerChannel_getprop_elapsed_s, NULL,
          "Receive time in seconds consumed so far, the `t` every Doppler "
          "quantity is evaluated at. Advances by `n/fs` per `execute(x)` call "
          "and is zeroed by `reset()`.\n",
          NULL },
        { "offset_hz", (getter)DopplerChannel_getprop_offset_hz, NULL,
          "Instantaneous carrier offset `fc * d(t)` in Hz at the current "
          "`elapsed_s` -- the frequency a receiver would have to tune out "
          "right now. Read-only diagnostic; with a non-zero "
          "`doppler_rate_ppm_s` it ramps as the stream advances.\n",
          NULL },
        { NULL } };

static PyObject *
DopplerChannelObj_destroy (DopplerChannelObject *self,
                           PyObject             *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      doppler_channel_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DopplerChannelObj_enter (DopplerChannelObject *self,
                         PyObject             *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DopplerChannelObj_exit (DopplerChannelObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      doppler_channel_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DopplerChannelObj_methods[] = {

  { "execute", (PyCFunction)DopplerChannelObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Apply clock Doppler to a block of complex baseband.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DopplerChannel\n"
    "    >>> obj = DopplerChannel(1000000.0, 0.0, 0.0, 0.0)\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_max_out", (PyCFunction)DopplerChannelObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "reset", (PyCFunction)DopplerChannelObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Reset DopplerChannel to its post-create state.\n"
    "\n"
    "    >>> from doppler import DopplerChannel\n"
    "    >>> obj = DopplerChannel(1000000.0, 0.0, 0.0, 0.0)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)DopplerChannelObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)DopplerChannelObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)DopplerChannelObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)DopplerChannelObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DopplerChannelObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DopplerChannelObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject DopplerChannelObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "impairment.DopplerChannel",
  .tp_basicsize                           = sizeof (DopplerChannelObject),
  .tp_dealloc = (destructor)DopplerChannelObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "DopplerChannel type.\n",
  .tp_methods = DopplerChannelObj_methods,
  .tp_getset  = DopplerChannel_getset,
  .tp_new     = DopplerChannelObj_new,
  .tp_init    = (initproc)DopplerChannelObj_init,
};
