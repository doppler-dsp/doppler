/*
 * track_ext_channel.c — Channel type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* ChannelObject — wraps channel_state_t *       */
/* ======================================================== */

#include "channel/channel_core.h"

typedef struct
{
  PyObject_HEAD channel_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
  uint8_t       *_bits_buf;     /* pre-allocated output for bits */
  size_t         _bits_buf_cap; /* allocated capacity for bits */
  void         **_bits_retired; /* gh-219 deferred free */
  size_t         _bits_retired_n;
  size_t         _bits_retired_cap;
} ChannelObject;

static void
ChannelObj_dealloc (ChannelObject *self)
{
  if (self->handle)
    channel_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  free (self->_bits_buf);
  for (size_t _i = 0; _i < self->_bits_retired_n; _i++)
    free (self->_bits_retired[_i]);
  free (self->_bits_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ChannelObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ChannelObject *self = (ChannelObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ChannelObj_init (ChannelObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "code",    "sps",    "init_norm_freq", "init_chip", "bn_carrier",
          "bn_code", "bn_fll", "zeta",           "spacing",   "nav_period",
          NULL };
  PyObject          *code_obj       = NULL;
  unsigned long long sps_raw        = 4;
  double             init_norm_freq = 0.0;
  double             init_chip      = 0.0;
  double             bn_carrier     = 0.05;
  double             bn_code        = 0.005;
  double             bn_fll         = 0.0;
  double             zeta           = 0.707;
  double             spacing        = 0.5;
  unsigned long long nav_period_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KdddddddK", kwlist,
                                    &code_obj, &sps_raw, &init_norm_freq,
                                    &init_chip, &bn_carrier, &bn_code, &bn_fll,
                                    &zeta, &spacing, &nav_period_raw))
    return -1;
  size_t         sps        = (size_t)sps_raw;
  size_t         nav_period = (size_t)nav_period_raw;
  PyArrayObject *code_arr   = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = channel_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, sps, init_norm_freq,
      init_chip, bn_carrier, bn_code, bn_fll, zeta, spacing, nav_period);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "channel_create returned NULL");
      return -1;
    }
  {
    size_t _max = channel_steps_max_out (self->handle);
    if (_max)
      {
        self->_steps_buf = malloc (_max * sizeof (float complex));
        if (!self->_steps_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_steps_buf_cap = _max;
      }
  }
  {
    size_t _max = channel_bits_max_out (self->handle);
    if (_max)
      {
        self->_bits_buf = malloc (_max * sizeof (uint8_t));
        if (!self->_bits_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_bits_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
ChannelObj_steps (ChannelObject *self, PyObject *args)
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
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_steps_buf || self->_steps_buf_cap < _need)
    {
      size_t _max = channel_steps_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_steps_buf
          && self->_steps_retired_n == self->_steps_retired_cap)
        {
          size_t _rcap
              = self->_steps_retired_cap ? self->_steps_retired_cap * 2 : 4;
          void **_rt = realloc (self->_steps_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_steps_retired     = _rt;
          self->_steps_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_steps_buf)
        self->_steps_retired[self->_steps_retired_n++] = self->_steps_buf;
      self->_steps_buf     = _tmp;
      self->_steps_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = channel_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                           self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_steps_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
ChannelObj_bits (ChannelObject *self, PyObject *args)
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
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_bits_buf || self->_bits_buf_cap < _need)
    {
      size_t _max = channel_bits_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_bits_buf && self->_bits_retired_n == self->_bits_retired_cap)
        {
          size_t _rcap
              = self->_bits_retired_cap ? self->_bits_retired_cap * 2 : 4;
          void **_rt = realloc (self->_bits_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_bits_retired     = _rt;
          self->_bits_retired_cap = _rcap;
        }
      uint8_t *_tmp = malloc (_max * sizeof (uint8_t));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_bits_buf)
        self->_bits_retired[self->_bits_retired_n++] = self->_bits_buf;
      self->_bits_buf     = _tmp;
      self->_bits_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = channel_bits (self->handle, _ng0, _ng1, self->_bits_buf,
                          self->_bits_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_UINT8, self->_bits_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
ChannelObj_reset (ChannelObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  channel_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
Channel_getprop_norm_freq (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (channel_get_norm_freq (self->handle));
}
static int
Channel_setprop_norm_freq (ChannelObject *self, PyObject *value,
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
  channel_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
Channel_getprop_code_phase (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (channel_get_code_phase (self->handle));
}
static PyObject *
Channel_getprop_code_rate (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (channel_get_code_rate (self->handle));
}
static PyObject *
Channel_getprop_lock_metric (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (channel_get_lock_metric (self->handle));
}
static PyObject *
Channel_getprop_bit_phase (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)channel_get_bit_phase (self->handle));
}
static PyObject *
Channel_getprop_bn_carrier (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (channel_get_bn_carrier (self->handle));
}
static int
Channel_setprop_bn_carrier (ChannelObject *self, PyObject *value,
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
  channel_set_bn_carrier (self->handle, v);
  return 0;
}
static PyObject *
Channel_getprop_bn_code (ChannelObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (channel_get_bn_code (self->handle));
}
static int
Channel_setprop_bn_code (ChannelObject *self, PyObject *value,
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
  channel_set_bn_code (self->handle, v);
  return 0;
}

static PyGetSetDef Channel_getset[]
    = { { "norm_freq", (getter)Channel_getprop_norm_freq,
          (setter)Channel_setprop_norm_freq, "Norm freq.\n", NULL },
        { "code_phase", (getter)Channel_getprop_code_phase, NULL,
          "Code phase.\n", NULL },
        { "code_rate", (getter)Channel_getprop_code_rate, NULL, "Code rate.\n",
          NULL },
        { "lock_metric", (getter)Channel_getprop_lock_metric, NULL,
          "Lock metric.\n", NULL },
        { "bit_phase", (getter)Channel_getprop_bit_phase, NULL, "Bit phase.\n",
          NULL },
        { "bn_carrier", (getter)Channel_getprop_bn_carrier,
          (setter)Channel_setprop_bn_carrier, "Bn carrier.\n", NULL },
        { "bn_code", (getter)Channel_getprop_bn_code,
          (setter)Channel_setprop_bn_code, "Bn code.\n", NULL },
        { NULL } };

static PyObject *
ChannelObj_destroy (ChannelObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      channel_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ChannelObj_enter (ChannelObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ChannelObj_exit (ChannelObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      channel_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ChannelObj_state_bytes (ChannelObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (channel_state_bytes (self->handle));
}

static PyObject *
ChannelObj_get_state (ChannelObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = channel_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  channel_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
ChannelObj_set_state (ChannelObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != channel_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (channel_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ChannelObj_methods[] = {

  { "steps", (PyCFunction)ChannelObj_steps, METH_VARARGS,
    "steps(x) -> ndarray\n"
    "\n"
    "Track carrier + code and despread a cf32 block: per sample wipe the "
    "carrier (Costas) and correlate early/prompt/late against the code (DLL), "
    "update both loops each code period, and emit one complex prompt symbol "
    "per period.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Channel\n"
    "    >>> obj = Channel(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "bits", (PyCFunction)ChannelObj_bits, METH_VARARGS,
    "bits(x) -> ndarray\n"
    "\n"
    "Same tracking kernel as steps(), but bit-sync the per-period prompts "
    "into hard data bits: nav_period prompts are coherently summed across "
    "each detected bit boundary and one 0/1 bit is emitted per data bit.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Channel\n"
    "    >>> obj = Channel(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> y = obj.bits(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "reset", (PyCFunction)ChannelObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed both loops to the create-time frequency/phase; preserve config.\n"
    "\n"
    "    >>> from doppler import Channel\n"
    "    >>> obj = Channel(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> obj.reset()\n" },
  { "destroy", (PyCFunction)ChannelObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)ChannelObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)ChannelObj_exit, METH_VARARGS, NULL },
  { "state_bytes", (PyCFunction)ChannelObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)ChannelObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)ChannelObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { NULL }
};

static PyTypeObject ChannelObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.Channel",
  .tp_basicsize                           = sizeof (ChannelObject),
  .tp_dealloc                             = (destructor)ChannelObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a tracking channel (COPIES @p code).\n",
  .tp_methods = ChannelObj_methods,
  .tp_getset  = Channel_getset,
  .tp_new     = ChannelObj_new,
  .tp_init    = (initproc)ChannelObj_init,
};
