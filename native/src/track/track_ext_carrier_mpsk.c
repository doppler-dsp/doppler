/*
 * track_ext_carrier_mpsk.c — CarrierMpsk type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* CarrierMpskObject — wraps carrier_mpsk_state_t *       */
/* ======================================================== */

#include "carrier_mpsk/carrier_mpsk_core.h"

typedef struct
{
  PyObject_HEAD carrier_mpsk_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
} CarrierMpskObject;

static void
CarrierMpskObj_dealloc (CarrierMpskObject *self)
{
  if (self->handle)
    carrier_mpsk_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CarrierMpskObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CarrierMpskObject *self = (CarrierMpskObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CarrierMpskObj_init (CarrierMpskObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "bn", "zeta", "init_norm_freq", "tsamps", "bn_fll", "m", NULL };
  double             bn             = 0.05;
  double             zeta           = 0.707;
  double             init_norm_freq = 0.0;
  unsigned long long tsamps_raw     = 64;
  double             bn_fll         = 0.0;
  int                m              = 4;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddKdi", kwlist, &bn, &zeta,
                                    &init_norm_freq, &tsamps_raw, &bn_fll, &m))
    return -1;
  size_t tsamps = (size_t)tsamps_raw;
  self->handle
      = carrier_mpsk_create (bn, zeta, init_norm_freq, tsamps, bn_fll, m);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "carrier_mpsk_create returned NULL");
      return -1;
    }
  {
    size_t _max = carrier_mpsk_steps_max_out (self->handle);
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
CarrierMpskObj_steps (CarrierMpskObject *self, PyObject *args)
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
      size_t _max = carrier_mpsk_steps_max_out (self->handle);
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
    n_out = carrier_mpsk_steps (self->handle, _ng0, _ng1, self->_steps_buf,
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
CarrierMpskObj_configure (CarrierMpskObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bn", "zeta", NULL };
  double       bn        = 0.0;
  double       zeta      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", _kwlist, &bn, &zeta))
    return NULL;
  carrier_mpsk_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
CarrierMpskObj_reset (CarrierMpskObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  carrier_mpsk_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
CarrierMpsk_getprop_bn (CarrierMpskObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_bn (self->handle));
}
static int
CarrierMpsk_setprop_bn (CarrierMpskObject *self, PyObject *value,
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
  carrier_mpsk_set_bn (self->handle, v);
  return 0;
}
static PyObject *
CarrierMpsk_getprop_norm_freq (CarrierMpskObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_norm_freq (self->handle));
}
static int
CarrierMpsk_setprop_norm_freq (CarrierMpskObject *self, PyObject *value,
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
  carrier_mpsk_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
CarrierMpsk_getprop_lock_metric (CarrierMpskObject *self,
                                 void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_lock_metric (self->handle));
}
static PyObject *
CarrierMpsk_getprop_last_error (CarrierMpskObject *self,
                                void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_last_error (self->handle));
}
static PyObject *
CarrierMpsk_getprop_bn_fll (CarrierMpskObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_bn_fll (self->handle));
}
static int
CarrierMpsk_setprop_bn_fll (CarrierMpskObject *self, PyObject *value,
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
  carrier_mpsk_set_bn_fll (self->handle, v);
  return 0;
}
static PyObject *
CarrierMpsk_getprop_m (CarrierMpskObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)carrier_mpsk_get_m (self->handle));
}

static PyGetSetDef CarrierMpsk_getset[]
    = { { "bn", (getter)CarrierMpsk_getprop_bn, (setter)CarrierMpsk_setprop_bn,
          "Bn.\n", NULL },
        { "norm_freq", (getter)CarrierMpsk_getprop_norm_freq,
          (setter)CarrierMpsk_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock_metric", (getter)CarrierMpsk_getprop_lock_metric, NULL,
          "Lock metric.\n", NULL },
        { "last_error", (getter)CarrierMpsk_getprop_last_error, NULL,
          "Last error.\n", NULL },
        { "bn_fll", (getter)CarrierMpsk_getprop_bn_fll,
          (setter)CarrierMpsk_setprop_bn_fll, "Bn fll.\n", NULL },
        { "m", (getter)CarrierMpsk_getprop_m, NULL, "M.\n", NULL },
        { NULL } };

static PyObject *
CarrierMpskObj_destroy (CarrierMpskObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      carrier_mpsk_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CarrierMpskObj_enter (CarrierMpskObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CarrierMpskObj_exit (CarrierMpskObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      carrier_mpsk_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* serializable (gh-400): state-blob triplet, sibling to reset.  Hand-added
 * (this fragment is sacred — step/steps bindings); mirrors jm's generated form
 * for the `serializable` flag, which also emits the matching track.pyi stubs.
 */
static PyObject *
CarrierMpskObj_state_bytes (CarrierMpskObject *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_mpsk_state_bytes (self->handle));
}

static PyObject *
CarrierMpskObj_get_state (CarrierMpskObject *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = carrier_mpsk_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  carrier_mpsk_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CarrierMpskObj_set_state (CarrierMpskObject *self, PyObject *arg)
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
      != carrier_mpsk_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (carrier_mpsk_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CarrierMpskObj_methods[] = {

  { "steps", (PyCFunction)CarrierMpskObj_steps, METH_VARARGS,
    "steps(x) -> ndarray\n"
    "\n"
    "De-rotate a cf32 block with the integer-NCO carrier, coherently "
    "integrate over each tsamps-sample symbol, run the decision-directed "
    "M-PSK discriminator (slice to the nearest constellation point, error "
    "Im(P*conj(ahat))/|P|), and emit one complex prompt symbol per symbol. "
    "The loop tracks a small residual carrier (bulk Doppler removed "
    "upstream); it locks to one of m phases, so resolve the M-fold ambiguity "
    "downstream (mpsk_diff_demap or a sync word). At m=2 this is exactly the "
    "BPSK Costas loop.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import CarrierMpsk\n"
    "    >>> obj = CarrierMpsk(0.05, 0.707, 0.0, 64, 0.0, 4)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "configure", (PyCFunction)(void *)CarrierMpskObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the "
    "frequency/phase estimate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import CarrierMpsk\n"
    "    >>> obj = CarrierMpsk(0.05, 0.707, 0.0, 64, 0.0, 4)\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "reset", (PyCFunction)CarrierMpskObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time frequency/phase; preserve config.\n"
    "\n"
    "    >>> from doppler import CarrierMpsk\n"
    "    >>> obj = CarrierMpsk(0.05, 0.707, 0.0, 64, 0.0, 4)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)CarrierMpskObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)CarrierMpskObj_get_state, METH_NOARGS,
    "Serialize the loop state to bytes." },
  { "set_state", (PyCFunction)CarrierMpskObj_set_state, METH_O,
    "Restore loop state from a get_state() blob." },
  { "destroy", (PyCFunction)CarrierMpskObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)CarrierMpskObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)CarrierMpskObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject CarrierMpskObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.CarrierMpsk",
  .tp_basicsize                           = sizeof (CarrierMpskObject),
  .tp_dealloc                             = (destructor)CarrierMpskObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an M-PSK carrier loop instance.\n",
  .tp_methods = CarrierMpskObj_methods,
  .tp_getset  = CarrierMpsk_getset,
  .tp_new     = CarrierMpskObj_new,
  .tp_init    = (initproc)CarrierMpskObj_init,
};
