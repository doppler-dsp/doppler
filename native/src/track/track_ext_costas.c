/*
 * track_ext_costas.c — Costas type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* CostasObject — wraps costas_state_t *       */
/* ======================================================== */

#include "costas/costas_core.h"

typedef struct
{
  PyObject_HEAD costas_state_t *handle;
} CostasObject;

static void
CostasObj_dealloc (CostasObject *self)
{
  if (self->handle)
    costas_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CostasObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CostasObject *self = (CostasObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CostasObj_init (CostasObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "bn", "zeta", "init_norm_freq", "tsamps", "bn_fll", NULL };
  double             bn             = 0.05;
  double             zeta           = 0.707;
  double             init_norm_freq = 0.0;
  unsigned long long tsamps_raw     = 64;
  double             bn_fll         = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddKd", kwlist, &bn, &zeta,
                                    &init_norm_freq, &tsamps_raw, &bn_fll))
    return -1;
  size_t tsamps = (size_t)tsamps_raw;
  self->handle  = costas_create (bn, zeta, init_norm_freq, tsamps, bn_fll);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "costas_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CostasObj_steps_max_out (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (costas_steps_max_out (self->handle));
}

static PyObject *
CostasObj_steps (CostasObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = costas_steps_max_out (self->handle);
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
        n_out = costas_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = costas_steps_max_out (self->handle);
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
    n_out = costas_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
CostasObj_set_telemetry (CostasObject *self, PyObject *args, PyObject *kwds)
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
  int      _rc   = costas_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_configure (CostasObject *self, PyObject *args, PyObject *kwds)
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
  costas_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_configure_lock (CostasObject *self, PyObject *args, PyObject *kwds)
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
  costas_configure_lock (self->handle, up_thresh, down_thresh, n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_reset (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  costas_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_state_bytes (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (costas_state_bytes (self->handle));
}

static PyObject *
CostasObj_get_state (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = costas_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  costas_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CostasObj_set_state (CostasObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != costas_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (costas_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Costas_getprop_bn (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_bn (self->handle));
}
static int
Costas_setprop_bn (CostasObject *self, PyObject *value,
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
  costas_set_bn (self->handle, v);
  return 0;
}
static PyObject *
Costas_getprop_norm_freq (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_norm_freq (self->handle));
}
static int
Costas_setprop_norm_freq (CostasObject *self, PyObject *value,
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
  costas_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
Costas_getprop_lock_metric (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_lock_metric (self->handle));
}
static PyObject *
Costas_getprop_locked (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(costas_get_locked (self->handle)));
}
static PyObject *
Costas_getprop_last_error (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_last_error (self->handle));
}
static PyObject *
Costas_getprop_bn_fll (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_bn_fll (self->handle));
}
static int
Costas_setprop_bn_fll (CostasObject *self, PyObject *value,
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
  costas_set_bn_fll (self->handle, v);
  return 0;
}

static PyGetSetDef Costas_getset[]
    = { { "bn", (getter)Costas_getprop_bn, (setter)Costas_setprop_bn,
          "PLL loop noise bandwidth (retained).\n", NULL },
        { "norm_freq", (getter)Costas_getprop_norm_freq,
          (setter)Costas_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock_metric", (getter)Costas_getprop_lock_metric, NULL,
          "EMA of |Re P|/|P| (1 = locked).\n", NULL },
        { "locked", (getter)Costas_getprop_locked, NULL,
          "Current carrier lock decision: True after the verify count of "
          "consecutive above-threshold symbols, False again after the drop "
          "count of consecutive below-threshold ones (see configure_lock).\n",
          NULL },
        { "last_error", (getter)Costas_getprop_last_error, NULL,
          "last PLL discriminator (loop stress).\n", NULL },
        { "bn_fll", (getter)Costas_getprop_bn_fll,
          (setter)Costas_setprop_bn_fll,
          "FLL-assist bandwidth (0 = pure PLL).\n", NULL },
        { NULL } };

static PyObject *
CostasObj_destroy (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      costas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_enter (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CostasObj_exit (CostasObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      costas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CostasObj_methods[] = {

  { "steps", (PyCFunction)(void *)CostasObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "De-rotate a cf32 block with the integer-NCO carrier, coherently "
    "integrate over each tsamps-sample symbol, run the decision-directed "
    "Costas discriminator, and emit one complex prompt symbol per symbol.\n"
    "\n"
    "The streaming Python face of the loop. For every input sample it wipes\n"
    "the (tracked) carrier off x with the integer-phase NCO, sums the result\n"
    "into the coherent integrate-and-dump accumulator, and on each symbol\n"
    "boundary (one every tsamps samples) dumps the accumulator as the "
    "prompt,\n"
    "runs the BPSK Costas discriminator to steer the NCO frequency and "
    "phase,\n"
    "and appends the mean-scaled prompt to the output. Loop state carries\n"
    "across calls, so a long capture can be fed block by block; exactly one\n"
    "prompt symbol comes out per tsamps input samples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input samples, one complex baseband sample each.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of prompt symbols written to out (one per tsamps input\n"
    "    samples). On the Python face this is the recovered-symbol array.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Costas\n"
    ">>> tsamps = 16\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> bits = rng.integers(0, 2, 4000) * 2 - 1\n"
    ">>> sig = np.repeat(bits.astype(np.complex64), tsamps)\n"
    ">>> k = np.arange(len(sig))\n"
    ">>> rx = (sig * np.exp(2j * np.pi * 0.003 * k)).astype(np.complex64)\n"
    ">>> c = Costas(bn=0.05, zeta=0.707, tsamps=tsamps)\n"
    ">>> sym = c.steps(rx)             # one prompt symbol per tsamps "
    "samples\n"
    ">>> sym.shape\n"
    "(4000,)\n"
    ">>> round(c.norm_freq, 4)         # pulled onto the 0.003 cyc/sample "
    "residual\n"
    "0.003\n"
    ">>> c.lock_metric > 0.9\n"
    "True\n" },
  { "steps_max_out", (PyCFunction)CostasObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)CostasObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the carrier loop's "
    "probes on it. Registers four probes, emitted once per dumped symbol and "
    "further thinned by decim: \"<prefix>.lock\" (the |Re P|/|P| lock-metric "
    "EMA, 1 = phase-locked), \"<prefix>.e\" (the PLL discriminator output — "
    "the loop stress), \"<prefix>.freq\" (the tracked NCO frequency, "
    "cycles/sample) and \"<prefix>.locked\" (the verify-counted lock "
    "decision, 0/1 — see costas_configure_lock). Passing NULL detaches.  "
    "Setup path, never hot: call before the producer thread starts stepping; "
    "the context is borrowed and must outlive the attachment (SPSC rules in "
    "telemetry/telemetry.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"car\" or \"ch0.car\".\n"
    "decim : int\n"
    "    Emit every decim-th symbol; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Costas\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)\n"
    ">>> c.set_telemetry(tlm, \"car\")\n"
    ">>> sorted(tlm.probe_names())\n"
    "['car.e', 'car.freq', 'car.lock', 'car.locked']\n"
    ">>> x = np.ones(64 * 100, dtype=np.complex64)\n"
    ">>> _ = c.steps(x)\n"
    ">>> recs = tlm.read()   # four records per dumped symbol\n"
    ">>> len(recs) == 4 * 100\n"
    "True\n" },
  { "configure", (PyCFunction)(void *)CostasObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the "
    "frequency/phase estimate.\n"
    "\n"
    "Re-derives the PI coefficients from the loop bandwidth and damping and\n"
    "installs them live. The NCO frequency, phase and loop integrator are\n"
    "left untouched, so a converged loop keeps tracking straight through the\n"
    "re-tune — narrow the bandwidth once pulled in for lower phase jitter, "
    "or\n"
    "widen it to chase a faster-moving residual.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float\n"
    "    Loop noise bandwidth, normalised to the symbol rate.\n"
    "zeta : float\n"
    "    Damping factor (0.707 = critically damped).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import Costas\n"
    ">>> c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=16)\n"
    ">>> c.configure(0.02, 1.0)                    # narrow the loop, "
    "over-damp\n"
    ">>> (round(c.bn, 3), round(c.norm_freq, 3))   # new gains, estimate "
    "kept\n"
    "(0.02, 0.01)\n" },
  { "configure_lock", (PyCFunction)(void *)CostasObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune the carrier lock detector: locked flips up after n_up "
    "consecutive dumped symbols with the lock-metric EMA above up_thresh, and "
    "drops after n_down consecutive symbols below down_thresh (level + time "
    "hysteresis; see detection.LockDet). The defaults (0.85/0.78, 8 up / 32 "
    "down) derive from the metric's no-carrier statistics: |Re P|/|P| "
    "averages 2/pi (~0.64) under H0 with an EMA-smoothed std of ~0.07, so the "
    "declare threshold sits ~3 sigma above the no-carrier mean. A live lock "
    "survives the re-tune; the in-flight verify run restarts.\n"
    "\n"
    "The always-on lock decision steps a verify-counted detector\n"
    "(lockdet_core.h) on the |Re P|/|P| lock-metric EMA once per dumped\n"
    "symbol: `locked` flips up after n_up consecutive symbols with the "
    "metric\n"
    "above up_thresh and drops after n_down consecutive symbols below\n"
    "down_thresh. The defaults derive from the metric's own H0 statistics —\n"
    "with no carrier, |Re P|/|P| = |cos(theta)| for a uniform theta, whose\n"
    "mean is 2/pi (~0.637) and per-symbol std ~0.31; the COSTAS_LOCK_ALPHA =\n"
    "0.1 EMA reduces that to ~0.071, so the default declare threshold 0.85\n"
    "sits ~3 sigma above the no-carrier mean, with the drop threshold at "
    "0.78\n"
    "for level hysteresis and 8-up/32-down verify counts for time hysteresis\n"
    "(declare fast, drop reluctantly — the EMA already correlates adjacent\n"
    "looks, so the counts guard against band-edge dwell rather than\n"
    "compounding i.i.d. probabilities). A live lock survives the re-tune; "
    "the\n"
    "in-flight verify run restarts.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    Declare threshold on the lock-metric EMA.\n"
    "down_thresh : float\n"
    "    Drop threshold (<= up_thresh for level hysteresis).\n"
    "n_up : int\n"
    "    Consecutive above-threshold symbols to declare; clamped to >= 1.\n"
    "n_down : int\n"
    "    Consecutive below-threshold symbols to drop; clamped to >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import Costas\n"
    ">>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)\n"
    ">>> c.locked\n"
    "False\n"
    ">>> c.configure_lock(0.9, 0.8, 4, 16)   # tighter declare, faster "
    "drop\n" },
  { "reset", (PyCFunction)CostasObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time frequency/phase; preserve config.\n"
    "\n"
    "Drops the lock and rewinds the NCO, loop integrator and\n"
    "integrate-and-dump accumulators to the create-time seed frequency, "
    "while\n"
    "retaining the configured loop bandwidth, damping and lock-detector\n"
    "thresholds. Reprocess the same input after a reset and the output is\n"
    "bit-identical.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Costas\n"
    ">>> tsamps = 16\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> bits = rng.integers(0, 2, 1500) * 2 - 1\n"
    ">>> sig = np.repeat(bits.astype(np.complex64), tsamps)\n"
    ">>> k = np.arange(len(sig))\n"
    ">>> rx = (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)\n"
    ">>> c = Costas(bn=0.05, zeta=0.707, tsamps=tsamps)\n"
    ">>> _ = c.steps(rx)\n"
    ">>> round(c.norm_freq, 4) != 0.0     # loop pulled onto the residual\n"
    "True\n"
    ">>> c.reset()\n"
    ">>> c.norm_freq                       # back to the create-time seed\n"
    "0.0\n"
    ">>> c.lock_metric\n"
    "0.0\n" },
  { "state_bytes", (PyCFunction)CostasObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the CostasObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CostasObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CostasObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CostasObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the CostasObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)CostasObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)CostasObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Costas be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Costas\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CostasObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Costas.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
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

static PyTypeObject CostasObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.Costas",
  .tp_basicsize                           = sizeof (CostasObject),
  .tp_dealloc                             = (destructor)CostasObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Costas type.\n",
  .tp_methods                             = CostasObj_methods,
  .tp_getset                              = Costas_getset,
  .tp_new                                 = CostasObj_new,
  .tp_init                                = (initproc)CostasObj_init,
};
