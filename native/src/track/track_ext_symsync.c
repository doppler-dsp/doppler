/*
 * track_ext_symsync.c — SymbolSync type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* SymbolSyncObject — wraps symsync_state_t *       */
/* ======================================================== */

#include "symsync/symsync_core.h"

typedef struct
{
  PyObject_HEAD symsync_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
} SymbolSyncObject;

static void
SymbolSyncObj_dealloc (SymbolSyncObject *self)
{
  if (self->handle)
    symsync_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SymbolSyncObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  SymbolSyncObject *self = (SymbolSyncObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SymbolSyncObj_init (SymbolSyncObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "sps", "bn", "zeta", "order", "ted", NULL };
  unsigned long long sps_raw   = 4;
  double             bn        = 0.01;
  double             zeta      = 0.707;
  const char        *order_str = "cubic";
  const char        *ted_str   = "gardner";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kddss", kwlist, &sps_raw,
                                    &bn, &zeta, &order_str, &ted_str))
    return -1;
  size_t sps   = (size_t)sps_raw;
  int    order = 0;
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
  int ted = 0;
  if (strcmp (ted_str, "gardner") == 0)
    ted = 0;
  else if (strcmp (ted_str, "dttl") == 0)
    ted = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "ted must be one of \"gardner\", \"dttl\", got '%s'",
                    ted_str);
      return -1;
    }
  self->handle = symsync_create (sps, bn, zeta, order, ted);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "symsync_create returned NULL");
      return -1;
    }
  {
    size_t _max = symsync_steps_max_out (self->handle);
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
SymbolSyncObj_steps_max_out (SymbolSyncObject *self,
                             PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (symsync_steps_max_out (self->handle));
}

static PyObject *
SymbolSyncObj_steps (SymbolSyncObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = symsync_steps_max_out (self->handle);
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
        n_out = symsync_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
      size_t _max = symsync_steps_max_out (self->handle);
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
    n_out = symsync_steps (self->handle, _ng0, _ng1, self->_steps_buf,
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
SymbolSyncObj_set_telemetry (SymbolSyncObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "tlm", "prefix", "decim", NULL };
  PyObject     *tlm_obj   = Py_None;
  const char   *prefix    = NULL;
  unsigned long decim_raw = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Os|k", _kwlist, &tlm_obj,
                                    &prefix, &decim_raw))
    return NULL;
  dp_tlm_t *tlm = NULL;
  if (tlm_obj != Py_None)
    {
      PyObject *tlm_cap = tlm_obj;
      Py_INCREF (tlm_cap);
      if (!PyCapsule_CheckExact (tlm_cap))
        {
          Py_DECREF (tlm_cap);
          tlm_cap = PyObject_GetAttrString (tlm_obj, "_capsule");
          if (!tlm_cap)
            return NULL;
        }
      tlm = (dp_tlm_t *)PyCapsule_GetPointer (tlm_cap,
                                              "doppler.telemetry.dp_tlm");
      Py_DECREF (tlm_cap);
      if (!tlm)
        return NULL;
    }
  uint32_t decim = (uint32_t)decim_raw;
  int      _rc   = symsync_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_configure (SymbolSyncObject *self, PyObject *args,
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
  symsync_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_reset (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  symsync_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_state_bytes (SymbolSyncObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (symsync_state_bytes (self->handle));
}

static PyObject *
SymbolSyncObj_get_state (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = symsync_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  symsync_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
SymbolSyncObj_set_state (SymbolSyncObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != symsync_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (symsync_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
SymbolSync_getprop_bn (SymbolSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_bn (self->handle));
}
static int
SymbolSync_setprop_bn (SymbolSyncObject *self, PyObject *value,
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
  symsync_set_bn (self->handle, v);
  return 0;
}
static PyObject *
SymbolSync_getprop_timing_error (SymbolSyncObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_timing_error (self->handle));
}
static PyObject *
SymbolSync_getprop_rate (SymbolSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_rate (self->handle));
}

static PyObject *
SymbolSync_getprop_lock_metric (SymbolSyncObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_lock_metric (self->handle));
}

static PyObject *
SymbolSync_getprop_locked (SymbolSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(symsync_get_locked (self->handle)));
}

static PyGetSetDef SymbolSync_getset[]
    = { { "bn", (getter)SymbolSync_getprop_bn, (setter)SymbolSync_setprop_bn,
          "Bn.\n", NULL },
        { "timing_error", (getter)SymbolSync_getprop_timing_error, NULL,
          "Timing error.\n", NULL },
        { "rate", (getter)SymbolSync_getprop_rate, NULL, "Rate.\n", NULL },
        { "lock_metric", (getter)SymbolSync_getprop_lock_metric, NULL,
          "Last timing-lock statistic: the eye-concentration ratio EMA q = "
          "|on-time|^2/(|on-time|^2+|mid-symbol|^2); compare against the "
          "configured thresholds (see configure_lock).\n",
          NULL },
        { "locked", (getter)SymbolSync_getprop_locked, NULL,
          "Current timing-lock decision: True after the verify count of "
          "consecutive above-threshold symbols, False again after the drop "
          "count of consecutive below-threshold ones (see configure_lock).\n",
          NULL },
        { NULL } };

static PyObject *
SymbolSyncObj_destroy (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      symsync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_enter (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
SymbolSyncObj_exit (SymbolSyncObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      symsync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_configure_lock (SymbolSyncObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "up_thresh", "down_thresh", "n_up", "n_down", NULL };
  double        up_thresh   = 0.0;
  double        down_thresh = 0.0;
  unsigned long n_up_raw    = 0UL;
  unsigned long n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_up_raw, &n_down_raw))
    return NULL;
  uint32_t n_up   = (uint32_t)n_up_raw;
  uint32_t n_down = (uint32_t)n_down_raw;
  symsync_configure_lock (self->handle, up_thresh, down_thresh, n_up, n_down);
  Py_RETURN_NONE;
}

static PyMethodDef SymbolSyncObj_methods[] = {

  { "steps", (PyCFunction)SymbolSyncObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Recover symbol timing from an oversampled cf32 baseband block: a "
    "timing-error detector (Gardner or DTTL, see the `ted` param) drives an "
    "integer timing NCO whose post-wrap value gives the interpolation "
    "fraction for free, and a Farrow interpolator emits one symbol-rate "
    "sample per recovered symbol instant.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(4, 0.01, 0.707, \"cubic\", \"gardner\")\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)SymbolSyncObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)SymbolSyncObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the timing loop's "
    "probes on it. Registers three probes, emitted once per recovered symbol "
    "and further thinned by decim: \"<prefix>.e\" (the normalised TED error — "
    "the loop stress), \"<prefix>.freq\" (the loop-filter control steering "
    "the timing NCO, fractional rate offset) and \"<prefix>.rate\" (the "
    "smoothed tracked samples/symbol).  Passing NULL detaches.  Setup path, "
    "never hot: call before the producer thread starts stepping; the context "
    "is borrowed and must outlive the attachment (SPSC rules in "
    "telemetry/telemetry.h).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(4, 0.01, 0.707, \"cubic\", \"gardner\")\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "configure", (PyCFunction)(void *)SymbolSyncObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserve the timing "
    "estimate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(4, 0.01, 0.707, \"cubic\", \"gardner\")\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "reset", (PyCFunction)SymbolSyncObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the timing loop to its nominal rate and zero phase.\n"
    "\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(4, 0.01, 0.707, \"cubic\", \"gardner\")\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)SymbolSyncObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)SymbolSyncObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)SymbolSyncObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)SymbolSyncObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)SymbolSyncObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)SymbolSyncObj_exit, METH_VARARGS, NULL },
  { "configure_lock", (PyCFunction)(void *)SymbolSyncObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune the timing lock detector: locked flips up after n_up consecutive "
    "recovered symbols with the lock-metric EMA above up_thresh, and drops "
    "after n_down consecutive symbols below down_thresh (level + time "
    "hysteresis; see detection.LockDet). The metric is the eye-concentration "
    "ratio q = |on-time|^2 / (|on-time|^2 + |mid-symbol|^2): at correct "
    "timing the on-time interpolant sits at the eye's peak and the mid-symbol "
    "(transition-gate) interpolant sits closer to a zero crossing, so q rises "
    "above its H0 mean of 0.5 (symmetric energy split when no eye is "
    "established). A live lock survives the re-tune; the in-flight verify run "
    "restarts.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(4, 0.01, 0.707, \"cubic\", \"gardner\")\n"
    "    >>> obj.configure_lock(0.0, 0.0, 0, 0)\n" },
  { NULL }
};

static PyTypeObject SymbolSyncObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.SymbolSync",
  .tp_basicsize                           = sizeof (SymbolSyncObject),
  .tp_dealloc                             = (destructor)SymbolSyncObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "SymbolSync type.\n",
  .tp_methods                             = SymbolSyncObj_methods,
  .tp_getset                              = SymbolSync_getset,
  .tp_new                                 = SymbolSyncObj_new,
  .tp_init                                = (initproc)SymbolSyncObj_init,
};
