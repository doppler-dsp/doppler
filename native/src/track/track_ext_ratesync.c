/*
 * track_ext_ratesync.c — RateSync type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* RateSyncObject — wraps ratesync_state_t *       */
/* ======================================================== */

#include "ratesync/ratesync_core.h"

typedef struct
{
  PyObject_HEAD ratesync_state_t *handle;
} RateSyncObject;

static void
RateSyncObj_dealloc (RateSyncObject *self)
{
  if (self->handle)
    ratesync_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
RateSyncObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  RateSyncObject *self = (RateSyncObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
RateSyncObj_init (RateSyncObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "sps",        "pulse", "beta", "span", "m",
                             "num_phases", "bn",    "zeta", "ted",  NULL };
  double       sps       = 4.0;
  const char  *pulse_str = "rrc";
  double       beta      = 0.35;
  unsigned long long span_raw       = 8;
  unsigned long long m_raw          = 2;
  unsigned long long num_phases_raw = 1024;
  double             bn             = 0.01;
  double             zeta           = 0.707;
  const char        *ted_str        = "gardner";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dsdKKKdds", kwlist, &sps,
                                    &pulse_str, &beta, &span_raw, &m_raw,
                                    &num_phases_raw, &bn, &zeta, &ted_str))
    return -1;
  int pulse = 0;
  if (strcmp (pulse_str, "iandd") == 0)
    pulse = 0;
  else if (strcmp (pulse_str, "rrc") == 0)
    pulse = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "pulse must be one of \"iandd\", \"rrc\", got '%s'",
                    pulse_str);
      return -1;
    }
  size_t span       = (size_t)span_raw;
  size_t m          = (size_t)m_raw;
  size_t num_phases = (size_t)num_phases_raw;
  int    ted        = 0;
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
  self->handle
      = ratesync_create (sps, pulse, beta, span, m, num_phases, bn, zeta, ted);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "RateSync: invalid parameter (need sps >= m, 0 <= "
                       "beta <= 1, span >= 1, m even in [2, 8], num_phases a "
                       "power of two >= 2, bn >= 0, zeta > 0)");
      return -1;
    }
  return 0;
}

static PyObject *
RateSyncObj_steps_max_out (RateSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (ratesync_steps_max_out (self->handle));
}

static PyObject *
RateSyncObj_steps (RateSyncObject *self, PyObject *args, PyObject *kwds)
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
          Py_DECREF (x_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = ratesync_steps_max_out (self->handle);
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
        n_out = ratesync_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = ratesync_steps_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = ratesync_steps (self->handle, _ng0, _ng1, _d0, _cap);
  Py_END_ALLOW_THREADS
  Py_DECREF (x_arr);
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
RateSyncObj_set_telemetry (RateSyncObject *self, PyObject *args,
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
  int      _rc   = ratesync_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
RateSyncObj_configure (RateSyncObject *self, PyObject *args, PyObject *kwds)
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
  ratesync_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
RateSyncObj_configure_lock_raw (RateSyncObject *self, PyObject *args,
                                PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "avgs", "up_thresh", "down_thresh", "n_up", "n_down", NULL };
  unsigned long long avgs_raw    = 0ULL;
  double             up_thresh   = 0.0;
  double             down_thresh = 0.0;
  unsigned long      n_up_raw    = 0UL;
  unsigned long      n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Kddkk", _kwlist, &avgs_raw,
                                    &up_thresh, &down_thresh, &n_up_raw,
                                    &n_down_raw))
    return NULL;
  size_t   avgs   = (size_t)avgs_raw;
  uint32_t n_up   = (uint32_t)n_up_raw;
  uint32_t n_down = (uint32_t)n_down_raw;
  ratesync_configure_lock_raw (self->handle, avgs, up_thresh, down_thresh,
                               n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
RateSyncObj_reset (RateSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ratesync_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
RateSyncObj_state_bytes (RateSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (ratesync_state_bytes (self->handle));
}

static PyObject *
RateSyncObj_get_state (RateSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = ratesync_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  ratesync_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
RateSyncObj_set_state (RateSyncObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != ratesync_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (ratesync_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
RateSync_getprop_bn (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ratesync_get_bn (self->handle));
}
static int
RateSync_setprop_bn (RateSyncObject *self, PyObject *value,
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
  ratesync_set_bn (self->handle, v);
  return 0;
}
static PyObject *
RateSync_getprop_timing_error (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ratesync_get_timing_error (self->handle));
}
static PyObject *
RateSync_getprop_rate (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ratesync_get_rate (self->handle));
}
static PyObject *
RateSync_getprop_ctrl (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ratesync_get_ctrl (self->handle));
}
static PyObject *
RateSync_getprop_lock_stat (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ratesync_get_lock_stat (self->handle));
}
static PyObject *
RateSync_getprop_locked (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(ratesync_get_locked (self->handle)));
}
static PyObject *
RateSync_getprop_clipped (RateSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(ratesync_get_clipped (self->handle)));
}

static PyGetSetDef RateSync_getset[] = {
  { "bn", (getter)RateSync_getprop_bn, (setter)RateSync_setprop_bn, "Bn.\n",
    NULL },
  { "timing_error", (getter)RateSync_getprop_timing_error, NULL,
    "Last normalised TED error — the loop stress.\n", NULL },
  { "rate", (getter)RateSync_getprop_rate, NULL,
    "Smoothed tracked samples per symbol. Departs from the nominal `sps` by "
    "exactly the sample-clock offset being tracked, so it is the estimator a "
    "rate-disciplining caller reads.\n",
    NULL },
  { "ctrl", (getter)RateSync_getprop_ctrl, NULL,
    "Current per-input rate deviation steering the terminal stage's "
    "accumulator.\n",
    NULL },
  { "lock_stat", (getter)RateSync_getprop_lock_stat, NULL,
    "Last block-averaged lock statistic: "
    "mean(2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2)) over the configured "
    "avgs looks. This, not an error-vector magnitude, is the honest lock "
    "indicator -- a single cycle slip during acquisition drags a windowed EVM "
    "by 20 dB while the eye stays wide open at +0.75.\n",
    NULL },
  { "locked", (getter)RateSync_getprop_locked, NULL,
    "Current timing-lock decision: True after the verify count of consecutive "
    "above-threshold decisions, False again after the drop count of "
    "consecutive below-threshold ones.\n",
    NULL },
  { "clipped", (getter)RateSync_getprop_clipped, NULL,
    "True if the cascade's CIC stage has clipped its input since the last "
    "reset(). A CIC bounds its input to +-1.0 and clips silently past that, "
    "which no timing metric reveals -- an overdriven front end degrades EVM "
    "by 25 dB with a perfectly healthy lock. Always False when the plan "
    "contains no CIC stage.\n",
    NULL },
  { NULL }
};

static PyObject *
RateSyncObj_destroy (RateSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ratesync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
RateSyncObj_enter (RateSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
RateSyncObj_exit (RateSyncObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      ratesync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef RateSyncObj_methods[] = {

  { "steps", (PyCFunction)(void *)RateSyncObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Recover symbols from an oversampled cf32 baseband block. The owned "
    "RateConverter's terminal stage IS the matched filter, and the polyphase "
    "arm its accumulator selects IS the fractional timing delay, so one dot "
    "product does the rate conversion, the matched filtering and the "
    "interpolation. Every m-th output is an on-time strobe and the output m/2 "
    "back is the transition gate; a Gardner or DTTL detector drives a PI loop "
    "that steers the terminal stage's control port. State carries across "
    "calls, so contiguous blocks give the same symbols as one large block.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateSync\n"
    "    >>> obj = RateSync(4.0, \"rrc\", 0.35, 8, 2, 1024, 0.01, 0.707, "
    "\"gardner\")\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)RateSyncObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)RateSyncObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the probes.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateSync\n"
    "    >>> obj = RateSync(4.0, \"rrc\", 0.35, 8, 2, 1024, 0.01, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "configure", (PyCFunction)(void *)RateSyncObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserve the timing "
    "estimate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateSync\n"
    "    >>> obj = RateSync(4.0, \"rrc\", 0.35, 8, 2, 1024, 0.01, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "configure_lock_raw", (PyCFunction)(void *)RateSyncObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(avgs, up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Direct control of the lock detector's geometry: an explicit non-coherent "
    "block size (avgs), a split declare/drop threshold pair on lock_stat "
    "(level hysteresis), and both verify counts (time hysteresis) "
    "independently. Re-tuning clears the in-flight block sum and drops the "
    "lock so the next decision uses only looks gathered under the new config. "
    "The (pfa, pd) sizing entry point symsync exposes is deliberately not "
    "mirrored here: its constants were calibrated against symsync's own "
    "geometry by Monte Carlo, and re-exposing the formula for a different "
    "front end without repeating that validation would assert a calibration "
    "nobody measured.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateSync\n"
    "    >>> obj = RateSync(4.0, \"rrc\", 0.35, 8, 2, 1024, 0.01, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.configure_lock_raw(0, 0.0, 0.0, 0, 0)\n" },
  { "reset", (PyCFunction)RateSyncObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the timing loop, the cascade's filter memories, the strobe ring "
    "and the prime countdown.\n"
    "\n"
    "    >>> from doppler import RateSync\n"
    "    >>> obj = RateSync(4.0, \"rrc\", 0.35, 8, 2, 1024, 0.01, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)RateSyncObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)RateSyncObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)RateSyncObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)RateSyncObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)RateSyncObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)RateSyncObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject RateSyncObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.RateSync",
  .tp_basicsize                           = sizeof (RateSyncObject),
  .tp_dealloc                             = (destructor)RateSyncObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "RateSync type.\n",
  .tp_methods                             = RateSyncObj_methods,
  .tp_getset                              = RateSync_getset,
  .tp_new                                 = RateSyncObj_new,
  .tp_init                                = (initproc)RateSyncObj_init,
};
