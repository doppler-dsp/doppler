/*
 * dsss_ext_async_dsss_receiver.c — AsyncDsssReceiver type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* AsyncDsssReceiverObject — wraps async_dsss_receiver_state_t *       */
/* ======================================================== */

#include "async_dsss_receiver/async_dsss_receiver_core.h"

typedef struct
{
  PyObject_HEAD async_dsss_receiver_state_t *handle;
} AsyncDsssReceiverObject;

static void
AsyncDsssReceiverObj_dealloc (AsyncDsssReceiverObject *self)
{
  if (self->handle)
    async_dsss_receiver_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AsyncDsssReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AsyncDsssReceiverObject *self
      = (AsyncDsssReceiverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AsyncDsssReceiverObj_init (AsyncDsssReceiverObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char       *kwlist[]            = { "code",
                                             "chip_rate",
                                             "symbol_rate",
                                             "spc",
                                             "m",
                                             "cn0_dbhz",
                                             "pfa",
                                             "pd",
                                             "doppler_uncertainty",
                                             "segments",
                                             "sps",
                                             "differential",
                                             "refine_max_error_db",
                                             "refine_samples_per_symbol",
                                             "refine_design_margin_db",
                                             "refine_n_fft",
                                             "refine_zero_pad",
                                             "refine_sequential",
                                             "refine_max_n_blocks",
                                             "carrier_freq_hz",
                                             NULL };
  PyObject          *code_obj            = NULL;
  double             chip_rate           = 1000000.0;
  double             symbol_rate         = 1000.0;
  unsigned long long spc_raw             = 2;
  int                m                   = 2;
  double             cn0_dbhz            = 55.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  double             doppler_uncertainty = 100.0;
  unsigned long long segments_raw        = 4;
  unsigned long long sps_raw             = 8;
  int                differential        = 0;
  double             refine_max_error_db = 0.5;
  unsigned long long refine_samples_per_symbol_raw = 4;
  double             refine_design_margin_db       = 14.0;
  unsigned long long refine_n_fft_raw              = 64;
  unsigned long long refine_zero_pad_raw           = 8;
  int                refine_sequential_raw         = false;
  unsigned long long refine_max_n_blocks_raw       = 100000;
  double             carrier_freq_hz               = 0.0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|ddKiddddKKidKdKKpKd", kwlist, &code_obj, &chip_rate,
          &symbol_rate, &spc_raw, &m, &cn0_dbhz, &pfa, &pd,
          &doppler_uncertainty, &segments_raw, &sps_raw, &differential,
          &refine_max_error_db, &refine_samples_per_symbol_raw,
          &refine_design_margin_db, &refine_n_fft_raw, &refine_zero_pad_raw,
          &refine_sequential_raw, &refine_max_n_blocks_raw, &carrier_freq_hz))
    return -1;
  size_t spc                       = (size_t)spc_raw;
  size_t segments                  = (size_t)segments_raw;
  size_t sps                       = (size_t)sps_raw;
  size_t refine_samples_per_symbol = (size_t)refine_samples_per_symbol_raw;
  size_t refine_n_fft              = (size_t)refine_n_fft_raw;
  size_t refine_zero_pad           = (size_t)refine_zero_pad_raw;
  bool   refine_sequential         = (int)refine_sequential_raw;
  size_t refine_max_n_blocks       = (size_t)refine_max_n_blocks_raw;
  PyArrayObject *code_arr          = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = async_dsss_receiver_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, chip_rate,
      symbol_rate, spc, m, cn0_dbhz, pfa, pd, doppler_uncertainty, segments,
      sps, differential, refine_max_error_db, refine_samples_per_symbol,
      refine_design_margin_db, refine_n_fft, refine_zero_pad,
      refine_sequential, refine_max_n_blocks, carrier_freq_hz);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "async_dsss_receiver_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AsyncDsssReceiverObj_steps_max_out (AsyncDsssReceiverObject *self,
                                    PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (async_dsss_receiver_steps_max_out (self->handle));
}

static PyObject *
AsyncDsssReceiverObj_steps (AsyncDsssReceiverObject *self, PyObject *args,
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
      size_t _omax    = async_dsss_receiver_steps_max_out (self->handle);
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
        n_out
            = async_dsss_receiver_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = async_dsss_receiver_steps_max_out (self->handle);
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
    n_out = async_dsss_receiver_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
AsyncDsssReceiverObj_configure_search_raw (AsyncDsssReceiverObject *self,
                                           PyObject *args, PyObject *kwds)
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
  int _rc = async_dsss_receiver_configure_search_raw (self->handle,
                                                      doppler_bins, n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_search_raw failed (rc=%d)",
                    _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_configure_lock_raw (AsyncDsssReceiverObject *self,
                                         PyObject *args, PyObject *kwds)
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
  async_dsss_receiver_configure_lock_raw (self->handle, up_thresh, down_thresh,
                                          n_looks, alpha, n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_configure_chain_raw (AsyncDsssReceiverObject *self,
                                          PyObject *args, PyObject *kwds)
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
  int    _rc = async_dsss_receiver_configure_chain_raw (self->handle, segments,
                                                        sps, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_chain_raw failed (rc=%d)",
                    _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_reset (AsyncDsssReceiverObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  async_dsss_receiver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_state_bytes (AsyncDsssReceiverObject *self,
                                  PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (async_dsss_receiver_state_bytes (self->handle));
}

static PyObject *
AsyncDsssReceiverObj_get_state (AsyncDsssReceiverObject *self,
                                PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = async_dsss_receiver_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  async_dsss_receiver_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AsyncDsssReceiverObj_set_state (AsyncDsssReceiverObject *self, PyObject *arg)
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
      != async_dsss_receiver_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (async_dsss_receiver_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
AsyncDsssReceiver_getprop_tracking (AsyncDsssReceiverObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong (
      (long)async_dsss_receiver_get_tracking (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_refining (AsyncDsssReceiverObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong (
      (long)async_dsss_receiver_get_refining (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_doppler_hz (AsyncDsssReceiverObject *self,
                                      void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_doppler_hz (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_cn0_dbhz_est (AsyncDsssReceiverObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_cn0_dbhz_est (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_segments (AsyncDsssReceiverObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)async_dsss_receiver_get_segments (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_sps (AsyncDsssReceiverObject *self,
                               void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)async_dsss_receiver_get_sps (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_n (AsyncDsssReceiverObject *self,
                             void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)async_dsss_receiver_get_n (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_chip_phase (AsyncDsssReceiverObject *self,
                                      void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_chip_phase (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_code_rate (AsyncDsssReceiverObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (async_dsss_receiver_get_code_rate (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_lock (AsyncDsssReceiverObject *self,
                                void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (async_dsss_receiver_get_lock (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_norm_freq (AsyncDsssReceiverObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (async_dsss_receiver_get_norm_freq (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_nco_freq (AsyncDsssReceiverObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (async_dsss_receiver_get_nco_freq (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_locked (AsyncDsssReceiverObject *self,
                                  void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)async_dsss_receiver_get_locked (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_lock_metric (AsyncDsssReceiverObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_lock_metric (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_lock_threshold (AsyncDsssReceiverObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_lock_threshold (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_car_last_error (AsyncDsssReceiverObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_car_last_error (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_car_nco_freq (AsyncDsssReceiverObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_car_nco_freq (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_mpsk_last_error (AsyncDsssReceiverObject *self,
                                           void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      async_dsss_receiver_get_mpsk_last_error (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_code_locked (AsyncDsssReceiverObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong (
      (long)async_dsss_receiver_get_code_locked (self->handle));
}

static PyGetSetDef AsyncDsssReceiver_getset[] = {
  { "tracking", (getter)AsyncDsssReceiver_getprop_tracking, NULL,
    "1 once the live tracking chain is built and demodulating; 0 while "
    "searching or refining.\n",
    NULL },
  { "refining", (getter)AsyncDsssReceiver_getprop_refining, NULL,
    "1 while the refine stage (CarrierAcquisition collection) is active; 0 "
    "while searching or tracking.\n",
    NULL },
  { "doppler_hz", (getter)AsyncDsssReceiver_getprop_doppler_hz, NULL,
    "The current best Doppler estimate: the coarse handoff value while "
    "refining, the CarrierAcquisition-refined value once tracking.\n",
    NULL },
  { "cn0_dbhz_est", (getter)AsyncDsssReceiver_getprop_cn0_dbhz_est, NULL,
    "Cached from the winning acquisition hit.\n", NULL },
  { "segments", (getter)AsyncDsssReceiver_getprop_segments, NULL,
    "Live-tracking Dll's own segments -- distinct from refine_segments above "
    "(see the module docstring / dll_lookback_segments()'s own doc on the "
    "WINDOWS vs TRACK_WINDOWS split).\n",
    NULL },
  { "sps", (getter)AsyncDsssReceiver_getprop_sps, NULL,
    "MpskReceiver's own samples/symbol.\n", NULL },
  { "n", (getter)AsyncDsssReceiver_getprop_n, NULL,
    "MpskReceiver's own carrier-arm count.\n", NULL },
  { "chip_phase", (getter)AsyncDsssReceiver_getprop_chip_phase, NULL,
    "Chips, Dll's own instantaneous-phase convention (the mirror image of "
    "acq_result_t::code_phase's correlation-lag convention -- see "
    "acq_build_handoff()'s doc comment).\n",
    NULL },
  { "code_rate", (getter)AsyncDsssReceiver_getprop_code_rate, NULL,
    "chips advanced per nominal chip (~1.0).\n", NULL },
  { "lock", (getter)AsyncDsssReceiver_getprop_lock, NULL,
    "decision rule on lock_metric: thresholds + verify counters, stepped per "
    "symbol.\n",
    NULL },
  { "norm_freq", (getter)AsyncDsssReceiver_getprop_norm_freq, NULL,
    "Smoothed carrier estimate (integrator only, cycles/sample of the "
    "MpskReceiver output rate); lags a Doppler ramp by the constant Type-II "
    "ramp error.\n",
    NULL },
  { "nco_freq", (getter)AsyncDsssReceiver_getprop_nco_freq, NULL,
    "Live carrier loop-filter output = NCO frequency command (cycles/sample "
    "of the MpskReceiver output rate): its mean tracks a Doppler ramp with no "
    "lag, its variance is the carrier loop stress.\n",
    NULL },
  { "locked", (getter)AsyncDsssReceiver_getprop_locked, NULL,
    "Binary receiver lock: the hysteretic (up/down verify-counted) lock "
    "detector on the emitted symbols -- declared when lock_metric stays >= "
    "lock_threshold for the up-count and dropped below it for the "
    "down-count.\n",
    NULL },
  { "lock_metric", (getter)AsyncDsssReceiver_getprop_lock_metric, NULL,
    "Symbol-lock metric: SNR-weighted running mean of the BPSK lock signal "
    "(I^2-Q^2)/(I^2+Q^2) = cos(2*phi) over the emitted symbols (locked -> "
    "~+1). Drives `locked`; exposed for engineering debug.\n",
    NULL },
  { "lock_threshold", (getter)AsyncDsssReceiver_getprop_lock_threshold, NULL,
    "The lock_metric declare threshold `locked` latches above (the lockdet "
    "up_thresh); exposed alongside lock_metric for engineering debug.\n",
    NULL },
  { "car_last_error", (getter)AsyncDsssReceiver_getprop_car_last_error, NULL,
    "Pre-despread Costas phase discriminator (rad): the residual carrier "
    "phase loop 1 (de-rotates before the Dll) is not nulling. Engineering "
    "debug.\n",
    NULL },
  { "car_nco_freq", (getter)AsyncDsssReceiver_getprop_car_nco_freq, NULL,
    "Loop 1 (pre-despread Costas) loop-filter output = NCO frequency command, "
    "cycles/sample of the front-end (chip_rate*spc) rate. Engineering "
    "debug.\n",
    NULL },
  { "mpsk_last_error", (getter)AsyncDsssReceiver_getprop_mpsk_last_error, NULL,
    "MpskReceiver carrier phase discriminator (rad): the residual carrier "
    "phase loop 2 (post-despread) is not nulling. Engineering debug.\n",
    NULL },
  { "code_locked", (getter)AsyncDsssReceiver_getprop_code_locked, NULL,
    "Binary code-lock flag from the live tracking Dll's own verify-counted "
    "(pfa-tuned) lock detector -- the fundamental DSSS \"am I despreading\" "
    "lock, de-chattered by up/down hysteresis.\n",
    NULL },
  { NULL }
};

static PyObject *
AsyncDsssReceiverObj_destroy (AsyncDsssReceiverObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      async_dsss_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_enter (AsyncDsssReceiverObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AsyncDsssReceiverObj_exit (AsyncDsssReceiverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      async_dsss_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AsyncDsssReceiverObj_methods[] = {

  { "steps", (PyCFunction)(void *)AsyncDsssReceiverObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Stream raw cf32 samples through the receiver. While searching, samples "
    "feed the embedded Acquisition and nothing is emitted. On a hit, the "
    "refine stage (a frozen-carrier Dll collection feeding "
    "CarrierAcquisition) is built and seeded from it, and the unconsumed tail "
    "of this call is handed straight to it -- no samples dropped. Once "
    "CarrierAcquisition reports ready (or its own give-up cap is reached), "
    "the live tracking chain (Dll + per-partial Costas + RateConverter + "
    "MpskReceiver) is built fresh, seeded from the ORIGINAL handoff chip "
    "phase and the refined-or-unrefined Doppler estimate, and demodulated "
    "symbols are returned from then on. Accepts any block size; state carries "
    "across calls.\n"
    "\n"
    "Drives the search -> refine -> track state machine. While searching or\n"
    "refining, nothing is emitted (an empty return is normal, not an error):\n"
    "a hit seeds the frozen-carrier refine chain, `CarrierAcquisition`\n"
    "sharpens the coarse Doppler estimate, and only once it is ready (or\n"
    "gives up) is the live tracking chain built and demodulation begins.\n"
    "Accepts any block size; state carries across calls, so a capture can be\n"
    "fed in frames of any length with no seam. Under SPEC's coupled offset +\n"
    "500 Hz/s Doppler ramp the pre-despread Costas removes the full carrier\n"
    "dynamics before the code loop, so the recovered constellation lands\n"
    "cleanly on the BPSK real axis.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input cf32 samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of symbols written (0 while searching/refining, or while\n"
    "    tracking with not yet a full symbol's worth of input).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> sf, chip, sym, spc = 1023, 3.069e6, 2700.0, 2\n"
    ">>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym\n"
    ">>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)\n"
    ">>> csign = np.where(code & 1, -1.0, 1.0)\n"
    ">>> rng = np.random.default_rng(21)\n"
    ">>> n = int(600 * tsym) + 4 * te            # 600 async BPSK symbols\n"
    ">>> idx = np.arange(n)\n"
    ">>> data = (rng.integers(0, 2, 604) * 2 - 1).astype(float)\n"
    ">>> si = np.clip((idx / tsym).astype(int), 0, 603)\n"
    ">>> t = idx / fs\n"
    "\n"
    "DSSS chips on a carrier sweeping at 500 Hz/s — the ramp the async\n"
    "receiver has to track:\n"
    "\n"
    ">>> sig = (data[si] * csign[(idx // spc) % sf]\n"
    "...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))\n"
    ">>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB\n"
    ">>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))\n"
    ">>> pre = 5 * te                            # noise-only lead-in\n"
    ">>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)\n"
    "...          + 1j * rng.standard_normal(pre + n))\n"
    ">>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)\n"
    "...      + noise.astype(np.complex64))\n"
    ">>> rx = AsyncDsssReceiver(\n"
    "...     code, chip_rate=chip, symbol_rate=sym, spc=spc,\n"
    "...     cn0_dbhz=cn0, doppler_uncertainty=500.0)\n"
    ">>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]\n"
    ">>> syms = np.concatenate([s for s in syms if len(s)])\n"
    ">>> rx.tracking                  # searched, refined, now tracking\n"
    "1\n"
    ">>> len(syms) > 300              # symbols recovered under the ramp\n"
    "True\n"
    "\n"
    "Nearly all the energy lands on I, so the BPSK phase is resolved:\n"
    "\n"
    ">>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))\n"
    "True\n" },
  { "steps_max_out", (PyCFunction)AsyncDsssReceiverObj_steps_max_out,
    METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "configure_search_raw",
    (PyCFunction)(void *)AsyncDsssReceiverObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> int\n"
    "\n"
    "Pin the embedded Acquisition's search grid directly, bypassing the "
    "symbol_rate-driven auto-sizing. Only meaningful while searching.\n"
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
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it\n"
    ">>> rx.refining                # still searching, on the pinned grid\n"
    "0\n" },
  { "configure_lock_raw",
    (PyCFunction)(void *)AsyncDsssReceiverObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(up_thresh, down_thresh, n_looks, alpha, n_up, n_down) "
    "-> None\n"
    "\n"
    "Re-tune the live-tracking Dll's code-lock detector directly. Only "
    "meaningful once tracking has begun; a no-op while searching or "
    "refining.\n"
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
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,\n"
    "...                       alpha=0.1, n_up=5, n_down=3)\n"
    ">>> rx.tracking                       # a no-op until tracking begins\n"
    "0\n" },
  { "configure_chain_raw",
    (PyCFunction)(void *)AsyncDsssReceiverObj_configure_chain_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_chain_raw(segments, sps, n) -> int\n"
    "\n"
    "Pin the live-tracking despread/resample/demod grid directly, bypassing "
    "the create-time segments/sps defaults. Only meaningful once tracking; "
    "rebuilds the chain with every replacement allocated first, so a failed "
    "pin leaves the receiver on its prior grid.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "segments : int\n"
    "    Live-tracking Dll segments per code period.\n"
    "sps : int\n"
    "    MpskReceiver samples per symbol (the resample target).\n"
    "n : int\n"
    "    MpskReceiver's carrier-arm count; must divide sps.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain\n"
    ">>> rx.segments                       # tracking grid updated in place\n"
    "6\n" },
  { "reset", (PyCFunction)AsyncDsssReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Return to the searching state: resets the embedded Acquisition and frees "
    "every refine-stage/track-stage child (rebuilt from scratch on the next "
    "hit).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> rx.reset()                 # abort any lock, hunt from scratch\n"
    ">>> (rx.tracking, rx.refining, rx.chip_phase)   # all cleared\n"
    "(0, 0, 0.0)\n" },
  { "state_bytes", (PyCFunction)AsyncDsssReceiverObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AsyncDsssReceiverObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AsyncDsssReceiverObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AsyncDsssReceiverObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AsyncDsssReceiverObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AsyncDsssReceiverObj has already been "
    "destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AsyncDsssReceiverObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AsyncDsssReceiverObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AsyncDsssReceiver be used in a `with` statement so its C\n"
    "resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AsyncDsssReceiver\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AsyncDsssReceiverObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the AsyncDsssReceiver.\n"
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

static PyTypeObject AsyncDsssReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.AsyncDsssReceiver",
  .tp_basicsize                           = sizeof (AsyncDsssReceiverObject),
  .tp_dealloc = (destructor)AsyncDsssReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an AsyncDsssReceiver in the searching state.\n",
  .tp_methods = AsyncDsssReceiverObj_methods,
  .tp_getset  = AsyncDsssReceiver_getset,
  .tp_new     = AsyncDsssReceiverObj_new,
  .tp_init    = (initproc)AsyncDsssReceiverObj_init,
};
