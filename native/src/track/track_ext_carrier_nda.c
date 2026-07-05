/*
 * track_ext_carrier_nda.c — CarrierNda type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* CarrierNdaObject — wraps carrier_nda_state_t *       */
/* ======================================================== */

#include "carrier_nda/carrier_nda_core.h"

typedef struct
{
  PyObject_HEAD carrier_nda_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
} CarrierNdaObject;

static void
CarrierNdaObj_dealloc (CarrierNdaObject *self)
{
  if (self->handle)
    carrier_nda_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CarrierNdaObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CarrierNdaObject *self = (CarrierNdaObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CarrierNdaObj_init (CarrierNdaObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "bn", "zeta", "init_norm_freq", "sps", "n", "m", NULL };
  double             bn             = 0.01;
  double             zeta           = 0.707;
  double             init_norm_freq = 0.0;
  unsigned long long sps_raw        = 8;
  int                n              = 4;
  int                m              = 4;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddKii", kwlist, &bn, &zeta,
                                    &init_norm_freq, &sps_raw, &n, &m))
    return -1;
  size_t sps   = (size_t)sps_raw;
  self->handle = carrier_nda_create (bn, zeta, init_norm_freq, sps, n, m);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "carrier_nda_create returned NULL");
      return -1;
    }
  {
    size_t _max = carrier_nda_steps_max_out (self->handle);
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
  return 0;
}

static PyObject *
CarrierNdaObj_steps_max_out (CarrierNdaObject *self,
                             PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_nda_steps_max_out (self->handle));
}

static PyObject *
CarrierNdaObj_steps (CarrierNdaObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = carrier_nda_steps_max_out (self->handle);
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
        n_out = carrier_nda_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  if (!self->_steps_buf || self->_steps_buf_cap < _need)
    {
      size_t _max = carrier_nda_steps_max_out (self->handle);
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
    n_out = carrier_nda_steps (self->handle, _ng0, _ng1, self->_steps_buf,
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
CarrierNdaObj_reset (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  carrier_nda_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CarrierNdaObj_state_bytes (CarrierNdaObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_nda_state_bytes (self->handle));
}

static PyObject *
CarrierNdaObj_get_state (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = carrier_nda_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  carrier_nda_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CarrierNdaObj_set_state (CarrierNdaObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != carrier_nda_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (carrier_nda_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
CarrierNda_getprop_norm_freq (CarrierNdaObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_norm_freq (self->handle));
}
static int
CarrierNda_setprop_norm_freq (CarrierNdaObject *self, PyObject *value,
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
  carrier_nda_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
CarrierNda_getprop_lock (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_lock (self->handle));
}
static PyObject *
CarrierNda_getprop_last_error (CarrierNdaObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_last_error (self->handle));
}
static PyObject *
CarrierNda_getprop_bn (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_bn (self->handle));
}
static int
CarrierNda_setprop_bn (CarrierNdaObject *self, PyObject *value,
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
  carrier_nda_set_bn (self->handle, v);
  return 0;
}
static PyObject *
CarrierNda_getprop_m (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)carrier_nda_get_m (self->handle));
}
static PyObject *
CarrierNda_getprop_n (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)carrier_nda_get_n (self->handle));
}
static PyObject *
CarrierNda_getprop_sps (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)carrier_nda_get_sps (self->handle));
}

static PyGetSetDef CarrierNda_getset[]
    = { { "norm_freq", (getter)CarrierNda_getprop_norm_freq,
          (setter)CarrierNda_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock", (getter)CarrierNda_getprop_lock, NULL, "Lock.\n", NULL },
        { "last_error", (getter)CarrierNda_getprop_last_error, NULL,
          "Last error.\n", NULL },
        { "bn", (getter)CarrierNda_getprop_bn, (setter)CarrierNda_setprop_bn,
          "Bn.\n", NULL },
        { "m", (getter)CarrierNda_getprop_m, NULL, "M.\n", NULL },
        { "n", (getter)CarrierNda_getprop_n, NULL, "N.\n", NULL },
        { "sps", (getter)CarrierNda_getprop_sps, NULL, "Sps.\n", NULL },
        { NULL } };

static PyObject *
CarrierNdaObj_destroy (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      carrier_nda_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CarrierNdaObj_enter (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CarrierNdaObj_exit (CarrierNdaObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      carrier_nda_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CarrierNdaObj_methods[] = {

  { "steps", (PyCFunction)CarrierNdaObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "De-rotate a cf32 block with the integer-NCO carrier and return the "
    "de-rotated samples (one per input sample). Internally the loop runs a "
    "non-data-aided M-th-power discriminator on an I/Q arm integrate-and-dump "
    "at n dumps per symbol and steers the NCO, so it acquires the carrier "
    "with no symbol timing and no data present (it strips the M-PSK "
    "modulation by raising the arm sample to the Mth power). It locks to one "
    "of m phases (M-fold ambiguity), resolved downstream. Read norm_freq for "
    "the tracked carrier and lock for the carrier lock metric.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import CarrierNda\n"
    "    >>> obj = CarrierNda(0.01, 0.707, 0.0, 8, 4, 4)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)CarrierNdaObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "reset", (PyCFunction)CarrierNdaObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time frequency/phase; preserve config.\n"
    "\n"
    "    >>> from doppler import CarrierNda\n"
    "    >>> obj = CarrierNda(0.01, 0.707, 0.0, 8, 4, 4)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)CarrierNdaObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)CarrierNdaObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)CarrierNdaObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)CarrierNdaObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)CarrierNdaObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)CarrierNdaObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject CarrierNdaObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.CarrierNda",
  .tp_basicsize                           = sizeof (CarrierNdaObject),
  .tp_dealloc                             = (destructor)CarrierNdaObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an NDA carrier loop instance.\n",
  .tp_methods = CarrierNdaObj_methods,
  .tp_getset  = CarrierNda_getset,
  .tp_new     = CarrierNdaObj_new,
  .tp_init    = (initproc)CarrierNdaObj_init,
};
