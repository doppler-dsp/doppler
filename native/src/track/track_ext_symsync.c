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
} SymbolSyncObject;

static void
SymbolSyncObj_dealloc (SymbolSyncObject *self)
{
  if (self->handle)
    symsync_destroy (self->handle);
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
  size_t _cap  = symsync_steps_max_out (self->handle);
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
    n_out = symsync_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
SymbolSyncObj_configure_lock (SymbolSyncObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]   = { "rolloff", "esno_min_db", "pfa", "pd", NULL };
  double       rolloff     = 0.0;
  double       esno_min_db = 0.0;
  double       pfa         = 0.0;
  double       pd          = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dddd", _kwlist, &rolloff,
                                    &esno_min_db, &pfa, &pd))
    return NULL;
  int _rc
      = symsync_configure_lock (self->handle, rolloff, esno_min_db, pfa, pd);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_lock failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_configure_lock_raw (SymbolSyncObject *self, PyObject *args,
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
  symsync_configure_lock_raw (self->handle, avgs, up_thresh, down_thresh, n_up,
                              n_down);
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
SymbolSync_getprop_lock_stat (SymbolSyncObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_lock_stat (self->handle));
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
          "loop noise bandwidth (retained).\n", NULL },
        { "timing_error", (getter)SymbolSync_getprop_timing_error, NULL,
          "Timing error.\n", NULL },
        { "rate", (getter)SymbolSync_getprop_rate, NULL, "Rate.\n", NULL },
        { "lock_stat", (getter)SymbolSync_getprop_lock_stat, NULL,
          "Last block-averaged lock statistic: "
          "mean(2*(|on-time|^2-|mid-symbol|^2)/(|on-time|^2+|mid-symbol|^2)) "
          "over the configured avgs looks; compare against the configured "
          "threshold (see configure_lock).\n",
          NULL },
        { "locked", (getter)SymbolSync_getprop_locked, NULL,
          "Current timing-lock decision: True after the verify count of "
          "consecutive above-threshold decisions, False again after the drop "
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

static PyMethodDef SymbolSyncObj_methods[] = {

  { "steps", (PyCFunction)(void *)SymbolSyncObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Recover symbol timing from an oversampled cf32 baseband block: a "
    "timing-error detector (Gardner or DTTL, see the `ted` param) drives an "
    "integer timing NCO whose post-wrap value gives the interpolation "
    "fraction for free, and a Farrow interpolator emits one symbol-rate "
    "sample per recovered symbol instant.\n"
    "\n"
    "symsync_step() in a loop, with the TED specialised per detector. Each\n"
    "input sample feeds the Farrow interpolator and advances the integer\n"
    "timing NCO; on a mid-symbol crossing the transition-gate interpolant is\n"
    "stored, and on a wrap the on-time interpolant is formed, the selected\n"
    "TED (Gardner or DTTL) measures the timing error, the PI loop steers the\n"
    "NCO rate, and one symbol-rate sample is emitted at the recovered\n"
    "instant. State carries across calls, so contiguous blocks give the same\n"
    "symbols as one large block.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Oversampled input samples (~sps samples per symbol).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of recovered symbols written to out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import SymbolSync\n"
    ">>> ss = SymbolSync(sps=4, bn=0.02, zeta=0.707)\n"
    ">>> x = np.repeat([1.0, -1.0, 1.0, -1.0], 4 * 32).astype(np.complex64)\n"
    ">>> y = ss.steps(x)             # oversampled -> one sample/symbol\n"
    ">>> y.shape[0]\n"
    "127\n"
    ">>> sorted(set(np.where(y.real >= 0, 1, -1).tolist()))  # got +/-1\n"
    "[-1, 1]\n"
    ">>> round(ss.rate, 1)              # tracked samples/symbol\n"
    "4.0\n" },
  { "steps_max_out", (PyCFunction)SymbolSyncObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)SymbolSyncObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the timing loop's "
    "probes on it. Registers five probes, emitted once per recovered symbol "
    "and further thinned by decim: \"<prefix>.e\" (the normalised TED error — "
    "the loop stress), \"<prefix>.freq\" (the loop-filter control steering "
    "the timing NCO, fractional rate offset), \"<prefix>.rate\" (the smoothed "
    "tracked samples/symbol), \"<prefix>.lock\" (the last block-averaged "
    "lock_signal, held between avgs-look updates) and \"<prefix>.locked\" "
    "(the verify-counted lockdet decision, 0/1). Passing NULL detaches.  "
    "Setup path, never hot: call before the producer thread starts stepping; "
    "the context is borrowed and must outlive the attachment (SPSC rules in "
    "dp_tlm/dp_tlm_core.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"sync\" or \"rx.sync\".\n"
    "decim : int\n"
    "    Emit every decim-th symbol; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import SymbolSync\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)\n"
    ">>> ss.set_telemetry(tlm, \"sync\")\n"
    ">>> sorted(tlm.probe_names)\n"
    "['sync.e', 'sync.freq', 'sync.lock', 'sync.locked', 'sync.rate']\n"
    ">>> x = np.repeat([1 + 1j, -1 - 1j], 4 * 64).astype(np.complex64)\n"
    ">>> _ = ss.steps(x)\n"
    ">>> recs = tlm.read()   # five records per recovered symbol\n"
    ">>> len(recs) > 0 and len(recs) % 5 == 0\n"
    "True\n" },
  { "configure", (PyCFunction)(void *)SymbolSyncObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserve the timing "
    "estimate.\n"
    "\n"
    "Retunes the PI timing loop in place: the proportional/integral gains "
    "are\n"
    "recomputed from the new noise bandwidth and damping, while the NCO\n"
    "phase, tracked rate and loop-filter integrator carry over — so a locked\n"
    "loop is re-bandwidthed (e.g. narrowed after acquisition) without losing\n"
    "lock.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float\n"
    "    Loop noise bandwidth, normalised to the symbol rate (>= 0).\n"
    "zeta : float\n"
    "    Damping factor (0.707 = critically damped).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import SymbolSync\n"
    ">>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)\n"
    ">>> ss.configure(bn=0.05, zeta=1.0)   # widen + over-damp, to acquire\n"
    ">>> round(ss.bn, 3)\n"
    "0.05\n" },
  { "configure_lock", (PyCFunction)(void *)SymbolSyncObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(rolloff, esno_min_db, pfa, pd) -> int\n"
    "\n"
    "Tune the always-on timing-lock detector to a target (pfa, pd) at a given "
    "link operating point. The statistic is a Gardner-style eye-opening "
    "ratio, lock_signal = "
    "2*(|on-time|^2-|mid-symbol|^2)/(|on-time|^2+|mid-symbol|^2), "
    "non-coherently block-averaged over avgs looks before each decision "
    "(mirroring Dll's tumbling-window CFAR pattern). avgs and the declare "
    "threshold are sized from a Gaussian approximation: a per-look mean is "
    "estimated from rolloff and esno_min_db, then the classic N = "
    "variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold = "
    "Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation gives (avgs, threshold). "
    "No level hysteresis by default (up=down=threshold, matching "
    "Dll.configure_lock's shape); n_up=1, n_down=8. Raises ValueError if "
    "pfa/pd are outside (0, 1) or pd does not exceed pfa. Read the result "
    "from the locked / lock_stat properties.\n"
    "\n"
    "Sizes the non-coherent block size (avgs) and declare threshold from a\n"
    "Gaussian sizing of the eye-opening statistic lock_signal =\n"
    "2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2): a per-look mean\n"
    "(mean_lock_detect, from rolloff and the minimum operating Es/N0) drives\n"
    "the classic N = variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold =\n"
    "Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation, implemented directly\n"
    "from a formula supplied by a doppler user (not re-derived against a\n"
    "primary source), with \"variance\" set from a direct measurement of\n"
    "lock_signal's real per-look variance under noise (~1.343,\n"
    "5,000,000-sample Monte Carlo) rather than the placeholder \"8\" this "
    "API\n"
    "originally shipped with -- see symsync_core.c's\n"
    "SYMSYNC_LOCK_STAT_VARIANCE comment for the full derivation (a\n"
    "factor-of-2 correction for the erfcinv-vs-Q^-1 convention applies on "
    "top\n"
    "of the measured variance; the two hypotheses were empirically compared\n"
    "before picking one). Empirically validated at the default operating\n"
    "point (avgs=133, threshold=0.311): 429 false declares over 500,000\n"
    "independent noise-only blocks against a nominal pfa=1e-3 (8.58e-4,\n"
    "correctly sized with safe margin, not accidentally oversized); "
    "2000/2000\n"
    "true declares at the esno_min design SNR against a nominal pd=0.9 -- "
    "see\n"
    "native/validation/symsync_lock.c for the harness. No level hysteresis "
    "by\n"
    "default (up = down = threshold, matching dll_configure_lock's shape);\n"
    "the raw escape hatch (symsync_configure_lock_raw) exposes split\n"
    "thresholds, an explicit avgs, and independent n_up/n_down.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rolloff : float\n"
    "    Matched-filter excess bandwidth (e.g. 0.35 for a typical RRC\n"
    "    system).\n"
    "esno_min_db : float\n"
    "    Minimum operating Es/N0, dB -- the worst-case link point the\n"
    "    detector must still declare lock at.\n"
    "pfa : float\n"
    "    Target false-alarm probability per decision, in (0, 1).\n"
    "pd : float\n"
    "    Target detection probability per decision, in (0, 1); must exceed\n"
    "    pfa.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import SymbolSync\n"
    ">>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)\n"
    ">>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)\n"
    ">>> ss.locked\n"
    "False\n"
    ">>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.9, pd=0.9)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: configure_lock failed (rc=-4)\n" },
  { "configure_lock_raw",
    (PyCFunction)(void *)SymbolSyncObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(avgs, up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Escape hatch under configure_lock() for direct control of the lock "
    "detector's geometry: an explicit non-coherent block size (avgs), a split "
    "declare/drop threshold pair on lock_stat (level hysteresis), and both "
    "verify counts (time hysteresis) independently. Re-tuning clears the "
    "in-flight block sum and drops the lock so the next decision uses only "
    "looks gathered under the new config.\n"
    "\n"
    "The escape hatch under symsync_configure_lock() for a caller that\n"
    "derives its own averaging/threshold geometry: the block size (avgs), a\n"
    "split declare/drop threshold pair on lock_stat (level hysteresis), and\n"
    "both verify counts (time hysteresis). Re-tuning clears the in-flight\n"
    "block sum and drops the lock so the next decision uses only looks\n"
    "gathered under the new config.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "avgs : int\n"
    "    Non-coherent block size (looks/decision); clamped >= 1.\n"
    "up_thresh : float\n"
    "    Declare threshold on lock_stat.\n"
    "down_thresh : float\n"
    "    Drop threshold; choose <= up_thresh for level hysteresis.\n"
    "n_up : int\n"
    "    Consecutive above-threshold decisions to declare; clamped >= 1.\n"
    "n_down : int\n"
    "    Consecutive below-threshold decisions to drop; clamped >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import SymbolSync\n"
    ">>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)\n"
    ">>> ss.configure_lock_raw(64, 0.3, 0.3, 1, 8)   # 64-look block, 8-drop\n"
    ">>> ss.locked\n"
    "False\n"
    ">>> round(ss.lock_stat, 3)\n"
    "0.0\n" },
  { "reset", (PyCFunction)SymbolSyncObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the timing loop to its nominal rate and zero phase.\n"
    "\n"
    "Restores the object to its post-create state: the timing NCO is zeroed\n"
    "to the nominal one-wrap-per-symbol rate, the Farrow history and TED\n"
    "state are cleared, the loop-filter integrator is emptied and the lock\n"
    "detector is dropped. The configured (bn, zeta), TED selection and any\n"
    "lock geometry are preserved, so the same object can be re-run on a "
    "fresh\n"
    "stream.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import SymbolSync\n"
    ">>> ss = SymbolSync(sps=4, bn=0.02, zeta=0.707)\n"
    ">>> _ = ss.steps(np.repeat([1.0, -1.0], 4 * 40).astype(np.complex64))\n"
    ">>> ss.reset()\n"
    ">>> round(ss.rate, 1)              # back to the nominal sps\n"
    "4.0\n"
    ">>> round(ss.timing_error, 3)      # loop stress cleared\n"
    "0.0\n" },
  { "state_bytes", (PyCFunction)SymbolSyncObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the SymbolSync has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)SymbolSyncObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the SymbolSync has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)SymbolSyncObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the SymbolSync has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)SymbolSyncObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)SymbolSyncObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a SymbolSync be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "SymbolSync\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)SymbolSyncObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the SymbolSync.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never\n"
    "suppresses one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
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
