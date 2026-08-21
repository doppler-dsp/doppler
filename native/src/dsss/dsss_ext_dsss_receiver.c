/*
 * dsss_ext_dsss_receiver.c — DsssReceiver type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* DsssReceiverObject — wraps dsss_receiver_state_t *       */
/* ======================================================== */

#include "dsss_receiver/dsss_receiver_core.h"

typedef struct
{
  PyObject_HEAD dsss_receiver_state_t *handle;
} DsssReceiverObject;

static void
DsssReceiverObj_dealloc (DsssReceiverObject *self)
{
  if (self->handle)
    dsss_receiver_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DsssReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DsssReceiverObject *self = (DsssReceiverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DsssReceiverObj_init (DsssReceiverObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]    = { "code",     "chip_rate", "symbol_rate",
                               "spc",      "m",         "cn0_dbhz",
                               "pfa",      "pd",        "doppler_uncertainty",
                               "segments", "sps",       "differential",
                               NULL };
  PyObject    *code_obj    = NULL;
  double       chip_rate   = 1000000.0;
  double       symbol_rate = 1000.0;
  unsigned long long spc_raw             = 2;
  int                m                   = 2;
  double             cn0_dbhz            = 55.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  double             doppler_uncertainty = 100.0;
  unsigned long long segments_raw        = 4;
  unsigned long long sps_raw             = 8;
  int                differential        = 0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|ddKiddddKKi", kwlist, &code_obj, &chip_rate,
          &symbol_rate, &spc_raw, &m, &cn0_dbhz, &pfa, &pd,
          &doppler_uncertainty, &segments_raw, &sps_raw, &differential))
    return -1;
  size_t         spc      = (size_t)spc_raw;
  size_t         segments = (size_t)segments_raw;
  size_t         sps      = (size_t)sps_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = dsss_receiver_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, chip_rate,
      symbol_rate, spc, m, cn0_dbhz, pfa, pd, doppler_uncertainty, segments,
      sps, differential);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "DsssReceiver: invalid parameter (need a non-empty "
                       "code, chip_rate > 0, symbol_rate > 0, spc >= 1, m in "
                       "{2,4,8}, segments >= 1, sps >= 2 -- sps = 1 cannot "
                       "carry an m_out, whose smallest legal value is 2 and "
                       "which MpskReceiver requires sps to reach)");
      return -1;
    }
  return 0;
}

static PyObject *
DsssReceiverObj_steps_max_out (DsssReceiverObject *self,
                               PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dsss_receiver_steps_max_out (self->handle));
}

static PyObject *
DsssReceiverObj_steps (DsssReceiverObject *self, PyObject *args,
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
      size_t _omax    = dsss_receiver_steps_max_out (self->handle);
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
        n_out = dsss_receiver_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = dsss_receiver_steps_max_out (self->handle);
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
    n_out = dsss_receiver_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
DsssReceiverObj_configure_search_raw (DsssReceiverObject *self, PyObject *args,
                                      PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]        = { "doppler_bins", "n_noncoh", NULL };
  unsigned long long doppler_bins_raw = 0ULL;
  unsigned long long n_noncoh_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KK", _kwlist,
                                    &doppler_bins_raw, &n_noncoh_raw))
    return NULL;
  size_t doppler_bins = (size_t)doppler_bins_raw;
  size_t n_noncoh     = (size_t)n_noncoh_raw;
  int    _rc = dsss_receiver_configure_search_raw (self->handle, doppler_bins,
                                                   n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_search_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_configure_lock_raw (DsssReceiverObject *self, PyObject *args,
                                    PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]   = { "up_thresh", "down_thresh", "n_looks", "alpha",
                               "n_up",      "n_down",      NULL };
  double       up_thresh   = 0.0;
  double       down_thresh = 0.0;
  unsigned long long n_looks_raw = 0ULL;
  double             alpha       = 0.0;
  unsigned long      n_up_raw    = 0UL;
  unsigned long      n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddKdkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_looks_raw, &alpha,
                                    &n_up_raw, &n_down_raw))
    return NULL;
  size_t   n_looks = (size_t)n_looks_raw;
  uint32_t n_up    = (uint32_t)n_up_raw;
  uint32_t n_down  = (uint32_t)n_down_raw;
  dsss_receiver_configure_lock_raw (self->handle, up_thresh, down_thresh,
                                    n_looks, alpha, n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_configure_chain_raw (DsssReceiverObject *self, PyObject *args,
                                     PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "segments", "sps", "n", NULL };
  unsigned long long segments_raw = 0ULL;
  unsigned long long sps_raw      = 0ULL;
  int                n            = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KKi", _kwlist, &segments_raw,
                                    &sps_raw, &n))
    return NULL;
  size_t segments = (size_t)segments_raw;
  size_t sps      = (size_t)sps_raw;
  int _rc = dsss_receiver_configure_chain_raw (self->handle, segments, sps, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_chain_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_reset (DsssReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dsss_receiver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_state_bytes (DsssReceiverObject *self,
                             PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dsss_receiver_state_bytes (self->handle));
}

static PyObject *
DsssReceiverObj_get_state (DsssReceiverObject *self,
                           PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dsss_receiver_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dsss_receiver_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DsssReceiverObj_set_state (DsssReceiverObject *self, PyObject *arg)
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
      != dsss_receiver_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dsss_receiver_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
DsssReceiver_getprop_tracking (DsssReceiverObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)dsss_receiver_get_tracking (self->handle));
}
static PyObject *
DsssReceiver_getprop_doppler_hz (DsssReceiverObject *self,
                                 void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_doppler_hz (self->handle));
}
static PyObject *
DsssReceiver_getprop_cn0_dbhz_est (DsssReceiverObject *self,
                                   void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_cn0_dbhz_est (self->handle));
}
static PyObject *
DsssReceiver_getprop_segments (DsssReceiverObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_receiver_get_segments (self->handle));
}
static PyObject *
DsssReceiver_getprop_sps (DsssReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_receiver_get_sps (self->handle));
}
static PyObject *
DsssReceiver_getprop_n (DsssReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)dsss_receiver_get_n (self->handle));
}
static PyObject *
DsssReceiver_getprop_chip_phase (DsssReceiverObject *self,
                                 void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_chip_phase (self->handle));
}
static PyObject *
DsssReceiver_getprop_code_rate (DsssReceiverObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_code_rate (self->handle));
}
static PyObject *
DsssReceiver_getprop_lock (DsssReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_lock (self->handle));
}
static PyObject *
DsssReceiver_getprop_norm_freq (DsssReceiverObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_norm_freq (self->handle));
}

static PyGetSetDef DsssReceiver_getset[] = {
  { "tracking", (getter)DsssReceiver_getprop_tracking, NULL,
    "0 = searching, 1 = locked and demodulating.\n", NULL },
  { "doppler_hz", (getter)DsssReceiver_getprop_doppler_hz, NULL,
    "Doppler hz.\n", NULL },
  { "cn0_dbhz_est", (getter)DsssReceiver_getprop_cn0_dbhz_est, NULL,
    "Cached from the winning acquisition hit.\n", NULL },
  { "segments", (getter)DsssReceiver_getprop_segments, NULL,
    "Dll's own tracking parameter.\n", NULL },
  { "sps", (getter)DsssReceiver_getprop_sps, NULL,
    "MpskReceiver's own samples/symbol.\n", NULL },
  { "n", (getter)DsssReceiver_getprop_n, NULL,
    "MpskReceiver's own carrier-arm count.\n", NULL },
  { "chip_phase", (getter)DsssReceiver_getprop_chip_phase, NULL,
    "Dll's live tracked code phase (chips); 0.0 while searching.\n", NULL },
  { "code_rate", (getter)DsssReceiver_getprop_code_rate, NULL,
    "Dll's own tracking-quality indicator; 1.0 while searching.\n", NULL },
  { "lock", (getter)DsssReceiver_getprop_lock, NULL,
    "MpskReceiver's carrier lock EMA; 0.0 while searching.\n", NULL },
  { "norm_freq", (getter)DsssReceiver_getprop_norm_freq, NULL,
    "MpskReceiver's tracked carrier frequency; 0.0 while searching.\n", NULL },
  { NULL }
};

static PyObject *
DsssReceiverObj_destroy (DsssReceiverObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dsss_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_enter (DsssReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DsssReceiverObj_exit (DsssReceiverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dsss_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DsssReceiverObj_methods[] = {

  { "steps", (PyCFunction)(void *)DsssReceiverObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x, out) -> ndarray\n"
    "\n"
    "Stream raw cf32 samples through the receiver. While searching,\n"
    "samples feed the embedded Acquisition and nothing is emitted (an empty\n"
    "array is normal, not an error). The moment a hit fires,\n"
    "Dll/RateConverter/MpskReceiver are built and seeded from it -- the same\n"
    "phase-inversion hand-off and rate-bridging this project's\n"
    "async-DSSS-receiver gallery story validated by hand -- and the\n"
    "unconsumed tail of this same call is handed straight to them, so no\n"
    "samples are dropped at the transition. While tracking, samples feed Dll\n"
    "-> RateConverter -> MpskReceiver in sequence and demodulated symbols\n"
    "are returned. Accepts any block size; state carries across calls.\n"
    "\n"
    "While searching, samples feed the embedded Acquisition and nothing is\n"
    "emitted (0 return is normal, not an error). The moment a hit fires,\n"
    "`Dll`/`RateConverter`/`MpskReceiver` are built and seeded from it, and\n"
    "the unconsumed tail of THIS call — computed exactly from\n"
    "`acq->samples_consumed`, no samples dropped or double-fed — is handed\n"
    "straight to them in the same call. While tracking, samples feed `Dll ->\n"
    "RateConverter -> MpskReceiver` in sequence. Accepts any block size;\n"
    "state carries across calls (`Acquisition`/`Dll`/\n"
    "`RateConverter`/`MpskReceiver` are all already block-size invariant, so\n"
    "this object needs no ring-buffering of its own).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input cf32 samples.\n"
    "out : NDArray[np.complex64] | None\n"
    "    Output symbols; caller provides max_out capacity.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of symbols written (0 while searching, or while tracking\n"
    "    with not yet a full symbol's worth of input).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2\n"
    ">>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym\n"
    ">>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)\n"
    ">>> csign = np.where(code & 1, -1.0, 1.0)\n"
    ">>> rng = np.random.default_rng(6)\n"
    ">>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols\n"
    ">>> idx = np.arange(n)\n"
    ">>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)\n"
    ">>> si = np.clip((idx / tsym).astype(int), 0, 403)\n"
    ">>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips\n"
    ">>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz\n"
    ">>> pre = 3 * te                     # noise-only lead-in, pre-signal\n"
    ">>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0\n"
    ">>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)\n"
    "...          + 1j * rng.standard_normal(pre + n))\n"
    ">>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)\n"
    "...      + noise.astype(np.complex64))\n"
    ">>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,\n"
    "...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)\n"
    ">>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]\n"
    ">>> syms = np.concatenate([s for s in syms if len(s)])\n"
    ">>> rx.tracking                  # acquired and now demodulating\n"
    "1\n"
    ">>> len(syms) > 300              # a few hundred symbols recovered\n"
    "True\n"
    "\n"
    "Nearly all the energy lands on I, so the BPSK phase is resolved:\n"
    "\n"
    ">>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))\n"
    "True\n" },
  { "steps_max_out", (PyCFunction)DsssReceiverObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n"
    "\n"
    "Largest number of samples steps() can return in the current state.\n"
    "\n"
    "Size an `out=` buffer with this before calling steps(), or use it to\n"
    "allocate one up front. The bound is this object's own: what it depends\n"
    "on is a property of the algorithm, so a header block on steps_max_out()\n"
    "replaces this text.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Upper bound on the output length; the actual call may return "
    "fewer.\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)DsssReceiverObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> None\n"
    "\n"
    "Pin the embedded Acquisition's search grid directly, bypassing the\n"
    "symbol_rate-driven auto-sizing -- the escape hatch for a power user who\n"
    "wants a specific (doppler_bins, n_noncoh). Only meaningful while\n"
    "searching.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    Number of Doppler window tiles to search (>= 1); capped by the\n"
    "    create-time `doppler_uncertainty` span (one tile per code-epoch\n"
    "    Doppler bin width).\n"
    "n_noncoh : int\n"
    "    Non-coherent looks accumulated per grid cell (1..256); more looks\n"
    "    buys sensitivity at the cost of dwell, replacing the auto-sized\n"
    "    count.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``configure_search_raw failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)\n"
    ">>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it\n"
    ">>> rx.tracking                # still searching, on the pinned grid\n"
    "0\n" },
  { "configure_lock_raw",
    (PyCFunction)(void *)DsssReceiverObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(up_thresh, down_thresh, n_looks, alpha, n_up, n_down) "
    "-> None\n"
    "\n"
    "Re-tune the embedded Dll's code-lock detector directly. Only\n"
    "meaningful once tracking has begun; a no-op while searching.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    CFAR-statistic level to declare code lock (hit when the statistic\n"
    "    exceeds it).\n"
    "down_thresh : float\n"
    "    Level below which a look is a miss; choose <= up_thresh for level\n"
    "    hysteresis.\n"
    "n_looks : int\n"
    "    Looks per decision — the DLL's non-coherent integration depth\n"
    "    feeding one statistic.\n"
    "alpha : float\n"
    "    EMA smoothing coefficient on the lock statistic (0..1); smaller is\n"
    "    smoother/slower.\n"
    "n_up : int\n"
    "    Consecutive hits required to declare lock.\n"
    "n_down : int\n"
    "    Consecutive misses required to drop lock.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)\n"
    ">>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,\n"
    "...                       alpha=0.1, n_up=5, n_down=3)\n"
    ">>> rx.tracking                # a no-op until a hit builds the Dll\n"
    "0\n" },
  { "configure_chain_raw",
    (PyCFunction)(void *)DsssReceiverObj_configure_chain_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_chain_raw(segments, sps, n) -> None\n"
    "\n"
    "Pin the despread/resample/demod grid directly, bypassing the\n"
    "create-time segments/sps defaults -- segments (Dll's tracking\n"
    "parameter) and sps/n (MpskReceiver's rate/carrier-arm parameters) stay\n"
    "independently overridable here, still bridged by a freshly-sized\n"
    "RateConverter, never coupled to each other. Only meaningful once\n"
    "tracking; rebuilds the chain with every replacement allocated first, so\n"
    "a failed pin leaves the receiver on its prior grid.\n"
    "\n"
    "The escape hatch for the one composition-specific knob this object adds\n"
    "beyond its children's own: `segments` (Dll's tracking parameter) and\n"
    "`sps`/`n` (MpskReceiver's sample-rate/carrier-arm parameters) are\n"
    "indepen­dently overridable here, still bridged by a freshly-sized\n"
    "`RateConverter` — never coupled to each other (see the module\n"
    "docstring). Rebuilds `dll`/`rc`/`rx` with every replacement allocated\n"
    "first, only freeing and adopting the old ones once every allocation has\n"
    "succeeded (mirrors `Acquisition`'s own `acq_regrid()` discipline) — a\n"
    "failed pin leaves the receiver tracking on its prior grid, not\n"
    "half-destroyed. Only meaningful once tracking (the grid defaults still\n"
    "apply to create-time auto-sizing for the next hit while searching; call\n"
    "`dsss_receiver_create()` with different `segments`/`sps` for that, or\n"
    "re-pin here again after the next hit).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "segments : int\n"
    "    Dll tracking segments per code period.\n"
    "sps : int\n"
    "    MpskReceiver samples per symbol (the resample target).\n"
    "n : int\n"
    "    MpskReceiver's carrier-arm count; must divide sps.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``configure_chain_raw failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)\n"
    ">>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain\n"
    ">>> rx.segments                       # tracking grid updated in place\n"
    "6\n" },
  { "reset", (PyCFunction)DsssReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Return to the searching state: resets the embedded Acquisition and\n"
    "frees Dll/RateConverter/MpskReceiver (rebuilt from scratch on the next\n"
    "hit).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)\n"
    ">>> rx.reset()                 # abort any lock, hunt from scratch\n"
    ">>> (rx.tracking, rx.chip_phase)   # back to searching, all cleared\n"
    "(0, 0.0)\n" },
  { "state_bytes", (PyCFunction)DsssReceiverObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DsssReceiver has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DsssReceiverObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DsssReceiver has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DsssReceiverObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the DsssReceiver has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)DsssReceiverObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)DsssReceiverObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a DsssReceiver be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "DsssReceiver\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DsssReceiverObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the DsssReceiver.\n"
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

static PyTypeObject DsssReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.DsssReceiver",
  .tp_basicsize                           = sizeof (DsssReceiverObject),
  .tp_dealloc = (destructor)DsssReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a DSSS receiver in the searching state.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : NDArray[np.uint8]\n"
    "    Spreading code, one 0/1 chip per element (0 -> +1, 1 -> -1 BPSK; "
    "only\n"
    "    the low bit is used, so pass 0/1, not +/-1).\n"
    "chip_rate : float, default 1000000.0\n"
    "    Chip rate, Hz. Required.\n"
    "symbol_rate : float, default 1000.0\n"
    "    Data-symbol rate, Hz. Required — passed straight to the embedded\n"
    "    Acquisition's own `symbol_rate` (diagnostic there; see\n"
    "    `acq_create_continuous()`).\n"
    "spc : int, default 2\n"
    "    Samples/chip (front-end oversample); default 2 (fs = 2x chip_rate).\n"
    "m : int, default 2\n"
    "    PSK order, 2/4/8; default 2 (BPSK).\n"
    "cn0_dbhz : float, default 55.0\n"
    "    Design C/N0 for acquisition sizing, dB-Hz; default 55.0.\n"
    "pfa : float, default 1e-3\n"
    "    Acquisition false-alarm target; default 1e-3.\n"
    "pd : float, default 0.9\n"
    "    Acquisition detection-probability target; default 0.9.\n"
    "doppler_uncertainty : float, default 100.0\n"
    "    One-sided Doppler search half-range, Hz; default 100.0.\n"
    "segments : int, default 4\n"
    "    Dll's own non-coherent partial-correlation count per code epoch — "
    "its\n"
    "    tracking- robustness parameter, independent of `sps` (see the "
    "module\n"
    "    docstring); default 4, this story's own validated sweet spot.\n"
    "sps : int, default 8\n"
    "    MpskReceiver's samples/symbol, reached by an internal RateConverter\n"
    "    bridging the despreader's own partial rate to this rate; default 8,\n"
    "    MpskReceiver's own constructor default.\n"
    "differential : int, default 0\n"
    "    MpskReceiver's differential (rotation- invariant) demap; default 0\n"
    "    (coherent).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``DsssReceiver: "
    "invalid\n"
    "    parameter (need a non-empty code, chip_rate > 0, symbol_rate > 0, "
    "spc\n"
    "    >= 1, m in {2,4,8}, segments >= 1, sps >= 2 -- sps = 1 cannot carry "
    "an\n"
    "    m_out, whose smallest legal value is 2 and which MpskReceiver "
    "requires\n"
    "    sps to reach)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2\n"
    ">>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym\n"
    ">>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)\n"
    ">>> csign = np.where(code & 1, -1.0, 1.0)\n"
    ">>> rng = np.random.default_rng(6)\n"
    ">>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols\n"
    ">>> idx = np.arange(n)\n"
    ">>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)\n"
    ">>> si = np.clip((idx / tsym).astype(int), 0, 403)\n"
    ">>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips\n"
    ">>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz\n"
    ">>> pre = 3 * te                     # noise-only lead-in, pre-signal\n"
    ">>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0\n"
    ">>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)\n"
    "...          + 1j * rng.standard_normal(pre + n))\n"
    ">>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)\n"
    "...      + noise.astype(np.complex64))\n"
    ">>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,\n"
    "...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)\n"
    ">>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]\n"
    ">>> syms = np.concatenate([s for s in syms if len(s)])\n"
    ">>> rx.tracking                  # acquired, now demodulating\n"
    "1\n"
    ">>> len(syms) > 300              # a few hundred symbols recovered\n"
    "True\n"
    "\n"
    "Nearly all the energy lands on I, so the BPSK phase is resolved:\n"
    "\n"
    ">>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))\n"
    "True\n",
  .tp_methods = DsssReceiverObj_methods,
  .tp_getset  = DsssReceiver_getset,
  .tp_new     = DsssReceiverObj_new,
  .tp_init    = (initproc)DsssReceiverObj_init,
};
