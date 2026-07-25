/*
 * track_ext_rrcsync.c — RrcSync type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* RrcSyncObject — wraps rrcsync_state_t *       */
/* ======================================================== */

#include "rrcsync/rrcsync_core.h"

typedef struct
{
  PyObject_HEAD rrcsync_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
  PyObject      *_steps_view_ref; /* gh-437 last returned view */
} RrcSyncObject;

static void
RrcSyncObj_dealloc (RrcSyncObject *self)
{
  if (self->handle)
    rrcsync_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_XDECREF (self->_steps_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
RrcSyncObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  RrcSyncObject *self = (RrcSyncObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
RrcSyncObj_init (RrcSyncObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "sps", "pulse", "beta", "span", "num_phases",
                                  "bn",  "zeta",  "ted",  NULL };
  double             sps      = 4.0;
  const char        *pulse_str      = "rrc";
  double             beta           = 0.35;
  unsigned long long span_raw       = 8;
  unsigned long long num_phases_raw = 1024;
  double             bn             = 0.005;
  double             zeta           = 0.707;
  const char        *ted_str        = "gardner";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dsdKKdds", kwlist, &sps,
                                    &pulse_str, &beta, &span_raw,
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
      = rrcsync_create (sps, pulse, beta, span, num_phases, bn, zeta, ted);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "RrcSync: need sps >= 1, beta in [0, 1], span >= 1, "
                       "num_phases a power of two >= 2, bn >= 0 and zeta > 0");
      return -1;
    }
  {
    size_t _max = rrcsync_steps_max_out (self->handle);
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
RrcSyncObj_steps_max_out (RrcSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (rrcsync_steps_max_out (self->handle));
}

static PyObject *
RrcSyncObj_steps (RrcSyncObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = rrcsync_steps_max_out (self->handle);
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
        n_out = rrcsync_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  if (self->_steps_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_steps_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_steps_view_ref) != Py_None;
#endif
    }
  if (!self->_steps_buf || self->_steps_buf_cap < _need || _view_live)
    {
      size_t _max = rrcsync_steps_max_out (self->handle);
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
    n_out = rrcsync_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                           self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_steps_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_steps_view_ref);
  self->_steps_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_steps_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
RrcSyncObj_set_telemetry (RrcSyncObject *self, PyObject *args, PyObject *kwds)
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
  int      _rc   = rrcsync_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
RrcSyncObj_configure (RrcSyncObject *self, PyObject *args, PyObject *kwds)
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
  rrcsync_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
RrcSyncObj_configure_lock_raw (RrcSyncObject *self, PyObject *args,
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
  rrcsync_configure_lock_raw (self->handle, avgs, up_thresh, down_thresh, n_up,
                              n_down);
  Py_RETURN_NONE;
}

static PyObject *
RrcSyncObj_reset (RrcSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  rrcsync_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
RrcSyncObj_state_bytes (RrcSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (rrcsync_state_bytes (self->handle));
}

static PyObject *
RrcSyncObj_get_state (RrcSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = rrcsync_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  rrcsync_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
RrcSyncObj_set_state (RrcSyncObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != rrcsync_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (rrcsync_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
RrcSync_getprop_bn (RrcSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (rrcsync_get_bn (self->handle));
}
static int
RrcSync_setprop_bn (RrcSyncObject *self, PyObject *value,
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
  rrcsync_set_bn (self->handle, v);
  return 0;
}
static PyObject *
RrcSync_getprop_timing_error (RrcSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (rrcsync_get_timing_error (self->handle));
}
static PyObject *
RrcSync_getprop_rate (RrcSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (rrcsync_get_rate (self->handle));
}
static PyObject *
RrcSync_getprop_ctrl (RrcSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (rrcsync_get_ctrl (self->handle));
}
static PyObject *
RrcSync_getprop_lock_stat (RrcSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (rrcsync_get_lock_stat (self->handle));
}
static PyObject *
RrcSync_getprop_locked (RrcSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(rrcsync_get_locked (self->handle)));
}

static PyGetSetDef RrcSync_getset[]
    = { { "bn", (getter)RrcSync_getprop_bn, (setter)RrcSync_setprop_bn,
          "Bn.\n", NULL },
        { "timing_error", (getter)RrcSync_getprop_timing_error, NULL,
          "Last normalised TED error -- the loop stress.\n", NULL },
        { "rate", (getter)RrcSync_getprop_rate, NULL,
          "Smoothed tracked samples per symbol. It departs from the nominal "
          "sps by exactly the sample-clock offset being tracked, so a caller "
          "disciplining a clock reads this.\n",
          NULL },
        { "ctrl", (getter)RrcSync_getprop_ctrl, NULL,
          "Current per-input rate deviation steering the strobe (added to the "
          "base rate of 1/sps by the resampler's control port).\n",
          NULL },
        { "lock_stat", (getter)RrcSync_getprop_lock_stat, NULL,
          "Last block-averaged lock statistic: "
          "mean(2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2)) over the "
          "configured avgs looks; compare against the configured threshold "
          "(see configure_lock_raw).\n",
          NULL },
        { "locked", (getter)RrcSync_getprop_locked, NULL,
          "Current timing-lock decision: True after the verify count of "
          "consecutive above-threshold decisions, False again after the drop "
          "count of consecutive below-threshold ones.\n",
          NULL },
        { NULL } };

static PyObject *
RrcSyncObj_destroy (RrcSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      rrcsync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
RrcSyncObj_enter (RrcSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
RrcSyncObj_exit (RrcSyncObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      rrcsync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef RrcSyncObj_methods[] = {

  { "steps", (PyCFunction)RrcSyncObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Recover symbols from an oversampled cf32 baseband block. The polyphase "
    "bank IS the root-raised-cosine matched filter and the arm its "
    "accumulator selects IS the fractional timing delay, so one dot product "
    "per strobe does both jobs -- no separate matched FIR and Farrow "
    "interpolator. A second bank displaced half a symbol supplies the Gardner "
    "transition-gate sample, which pins the on-time/mid roles structurally "
    "instead of by an output parity. Emits one symbol per recovered symbol "
    "period.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RrcSync\n"
    "    >>> obj = RrcSync(4.0, \"rrc\", 0.35, 8, 1024, 0.005, 0.707, "
    "\"gardner\")\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)RrcSyncObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)RrcSyncObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach, with None) a Telemetry context and register this "
    "loop's five probes: \"<prefix>.e\" (normalised TED error), "
    "\"<prefix>.ctrl\" (the per-input control steering the strobe), "
    "\"<prefix>.rate\" (tracked samples per symbol), \"<prefix>.lock\" (last "
    "block-averaged lock statistic) and \"<prefix>.locked\" (0/1). Emitted "
    "once per recovered symbol, thinned by decim. Setup path: call before the "
    "producer thread starts; the context is borrowed and must outlive the "
    "attachment.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RrcSync\n"
    "    >>> obj = RrcSync(4.0, \"rrc\", 0.35, 8, 1024, 0.005, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "configure", (PyCFunction)(void *)RrcSyncObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the integrator, "
    "and so the lock.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RrcSync\n"
    "    >>> obj = RrcSync(4.0, \"rrc\", 0.35, 8, 1024, 0.005, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "configure_lock_raw", (PyCFunction)(void *)RrcSyncObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(avgs, up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Set the lock detector's geometry directly: the non-coherent block size "
    "(avgs), a split declare/drop threshold pair on lock_stat (level "
    "hysteresis), and both verify counts (time hysteresis). Re-tuning clears "
    "the in-flight block sum and drops the lock so the next decision uses "
    "only looks gathered under the new config. The defaults are SymbolSync's "
    "validated operating point for the same eye-opening statistic (avgs=133, "
    "threshold=0.311, n_up=1, n_down=8); a (pfa, pd) sizing entry point is "
    "deliberately not offered until the same Monte Carlo is run against this "
    "object.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RrcSync\n"
    "    >>> obj = RrcSync(4.0, \"rrc\", 0.35, 8, 1024, 0.005, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.configure_lock_raw(0, 0.0, 0.0, 0, 0)\n" },
  { "reset", (PyCFunction)RrcSyncObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed both matched filters, the loop integrator and the lock detector "
    "to their post-create state.\n"
    "\n"
    "    >>> from doppler import RrcSync\n"
    "    >>> obj = RrcSync(4.0, \"rrc\", 0.35, 8, 1024, 0.005, 0.707, "
    "\"gardner\")\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)RrcSyncObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)RrcSyncObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)RrcSyncObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)RrcSyncObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)RrcSyncObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)RrcSyncObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject RrcSyncObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.RrcSync",
  .tp_basicsize                           = sizeof (RrcSyncObject),
  .tp_dealloc                             = (destructor)RrcSyncObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an RrcSync instance.\n",
  .tp_methods                             = RrcSyncObj_methods,
  .tp_getset                              = RrcSync_getset,
  .tp_new                                 = RrcSyncObj_new,
  .tp_init                                = (initproc)RrcSyncObj_init,
};
