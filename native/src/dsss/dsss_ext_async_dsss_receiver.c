/* jm:generated dsss_ext_async_dsss_receiver.c */
/*
 * dsss_ext_async_dsss_receiver.c — AsyncDsssReceiver type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in dsss_ext_async_dsss_receiver_extra.c.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* AsyncDsssReceiverObject — wraps dp_async_dsss_receiver_state_t *       */
/* ======================================================== */

#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"

typedef struct
{
  PyObject_HEAD dp_async_dsss_receiver_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
  PyObject     *_jm_retain_set_telemetry_tlm; /* keeps tlm's owner alive */
} AsyncDsssReceiverObject;

static void
AsyncDsssReceiverObj_dealloc (AsyncDsssReceiverObject *self)
{
  if (self->handle)
    dp_async_dsss_receiver_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_XDECREF (self->_jm_retain_set_telemetry_tlm);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AsyncDsssReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
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
                                             "lost_confirm_s",
                                             "reacquire",
                                             "code_only_s",
                                             "doppler_rate",
                                             NULL };
  PyObject          *code_obj            = NULL;
  double             chip_rate           = 1000000.0;
  double             symbol_rate         = 1000.0;
  PyObject          *spc_obj             = NULL;
  unsigned long long spc_raw             = 2;
  int                m                   = 2;
  double             cn0_dbhz            = 55.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  double             doppler_uncertainty = 100.0;
  PyObject          *segments_obj        = NULL;
  unsigned long long segments_raw        = 4;
  PyObject          *sps_obj             = NULL;
  unsigned long long sps_raw             = 8;
  int                differential        = 0;
  double             refine_max_error_db = 0.5;
  PyObject          *refine_samples_per_symbol_obj = NULL;
  unsigned long long refine_samples_per_symbol_raw = 4;
  double             refine_design_margin_db       = 14.0;
  PyObject          *refine_n_fft_obj              = NULL;
  unsigned long long refine_n_fft_raw              = 64;
  PyObject          *refine_zero_pad_obj           = NULL;
  unsigned long long refine_zero_pad_raw           = 8;
  int                refine_sequential_raw         = false;
  PyObject          *refine_max_n_blocks_obj       = NULL;
  unsigned long long refine_max_n_blocks_raw       = 100000;
  double             carrier_freq_hz               = 0.0;
  double             lost_confirm_s                = 0.0;
  int                reacquire_raw                 = false;
  double             code_only_s                   = 0.0;
  double             doppler_rate                  = 0.0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|ddOiddddOOidOdOOpOddpdd", kwlist, &code_obj,
          &chip_rate, &symbol_rate, &spc_obj, &m, &cn0_dbhz, &pfa, &pd,
          &doppler_uncertainty, &segments_obj, &sps_obj, &differential,
          &refine_max_error_db, &refine_samples_per_symbol_obj,
          &refine_design_margin_db, &refine_n_fft_obj, &refine_zero_pad_obj,
          &refine_sequential_raw, &refine_max_n_blocks_obj, &carrier_freq_hz,
          &lost_confirm_s, &reacquire_raw, &code_only_s, &doppler_rate))
    return -1;
  if (spc_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (spc_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      spc_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((spc_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || spc_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "spc: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t spc = (size_t)spc_raw;
  if (segments_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (segments_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      segments_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((segments_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || segments_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "segments: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t segments = (size_t)segments_raw;
  if (sps_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (sps_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      sps_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((sps_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || sps_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "sps: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t sps = (size_t)sps_raw;
  if (refine_samples_per_symbol_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (refine_samples_per_symbol_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      refine_samples_per_symbol_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((refine_samples_per_symbol_raw == (unsigned long long)-1
           && PyErr_Occurred ())
          || refine_samples_per_symbol_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "refine_samples_per_symbol: %S is out of range for "
                        "size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t refine_samples_per_symbol = (size_t)refine_samples_per_symbol_raw;
  if (refine_n_fft_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (refine_n_fft_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      refine_n_fft_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((refine_n_fft_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || refine_n_fft_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "refine_n_fft: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t refine_n_fft = (size_t)refine_n_fft_raw;
  if (refine_zero_pad_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (refine_zero_pad_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      refine_zero_pad_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((refine_zero_pad_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || refine_zero_pad_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "refine_zero_pad: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t refine_zero_pad   = (size_t)refine_zero_pad_raw;
  bool   refine_sequential = (int)refine_sequential_raw;
  if (refine_max_n_blocks_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (refine_max_n_blocks_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      refine_max_n_blocks_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((refine_max_n_blocks_raw == (unsigned long long)-1
           && PyErr_Occurred ())
          || refine_max_n_blocks_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "refine_max_n_blocks: %S is out of range for size_t [0, %llu]",
              _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t         refine_max_n_blocks = (size_t)refine_max_n_blocks_raw;
  bool           reacquire           = (int)reacquire_raw;
  PyArrayObject *code_arr
      = jm_array_arg (code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "code");
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = dp_async_dsss_receiver_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, chip_rate,
      symbol_rate, spc, m, cn0_dbhz, pfa, pd, doppler_uncertainty, segments,
      sps, differential, refine_max_error_db, refine_samples_per_symbol,
      refine_design_margin_db, refine_n_fft, refine_zero_pad,
      refine_sequential, refine_max_n_blocks, carrier_freq_hz, lost_confirm_s,
      reacquire, code_only_s, doppler_rate);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "AsyncDsssReceiver: invalid parameter (need a code of "
                       "at least 2 chips, a finite chip_rate > 0, a finite "
                       "symbol_rate > 0 and <= chip_rate, spc >= 1, m in "
                       "{2,4,8}, segments >= 1, sps >= 2, carrier_freq_hz 0 "
                       "or finite and above chip_rate * spc / 2, "
                       "lost_confirm_s >= 0 (and at least one input sample "
                       "with reacquire), refine_samples_per_symbol >= 1, "
                       "refine_n_fft >= 1, refine_zero_pad >= 1, a symbol of "
                       "at most 2^20 Dll partials, segments * chip_rate / "
                       "(code_len * symbol_rate) <= 1048576; and for the "
                       "searcher a finite cn0_dbhz, 0 < pfa < 1, 0 < pd < 1, "
                       "a finite doppler_uncertainty >= 0)");
      return -1;
    }
  return 0;
}

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static const char *const _enum_AsyncDsssReceiver_async_dsss_rx_state[] = {
  "searching", "refining", "tracking", "reserved", "lost", NULL,
};
static const int _enum_AsyncDsssReceiver_async_dsss_rx_state_c[] = {
  ASYNC_DSSS_RX_SEARCHING, ASYNC_DSSS_RX_REFINING, ASYNC_DSSS_RX_TRACKING,
  ASYNC_DSSS_RX_RESERVED,  ASYNC_DSSS_RX_LOST,
};
static inline const char *
_enum_AsyncDsssReceiver_async_dsss_rx_state_name (long v)
{
  for (int j = 0; j < 5; j++)
    if (_enum_AsyncDsssReceiver_async_dsss_rx_state_c[j] == v)
      return _enum_AsyncDsssReceiver_async_dsss_rx_state[j];
  return NULL;
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
  return PyLong_FromSize_t (
      dp_async_dsss_receiver_steps_max_out (self->handle));
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
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *x_obj     = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
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
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_async_dsss_receiver_steps_max_out (self->handle);
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
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      float _Complex *_ng0 = (float _Complex *)PyArray_DATA (out_arr);
      size_t          n_out;
      void           *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_async_dsss_receiver_steps (_jm_h, x, x_len, _ng0, _cap);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (x_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "AsyncDsssReceiver.steps: wrote %zu elements into a "
                        "buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_async_dsss_receiver_steps_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "AsyncDsssReceiver.steps: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  size_t n_out;
  void  *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_async_dsss_receiver_steps (_jm_h, x, x_len, _d0, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "AsyncDsssReceiver.steps: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
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
AsyncDsssReceiverObj_seed (AsyncDsssReceiverObject *self, PyObject *args,
                           PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "chip_phase", "doppler_hz_est", "cn0_dbhz_est", NULL };
  double chip_phase     = 0.0;
  double doppler_hz_est = 0.0;
  double cn0_dbhz_est   = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddd", _kwlist, &chip_phase,
                                    &doppler_hz_est, &cn0_dbhz_est))
    return NULL;
  int _rc = dp_async_dsss_receiver_seed (self->handle, chip_phase,
                                         doppler_hz_est, cn0_dbhz_est);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "seed refused: the receiver already holds an assignment "
                    "(refining, tracking or lost -- reset() releases it), or "
                    "chip_phase is outside [0, code_len), or doppler_hz_est "
                    "is not finite and below chip_rate * spc / 2 in "
                    "magnitude",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyStructSequence_Field AsyncDsssReceiverObj_status_fields[] = {
  { "state", "ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is." },
  { "doppler_hz", "Where the emitter is NOW: the whole carrier estimate, Hz "
                  "-- loop 1's plus what loop 2 has taken up beyond it (the "
                  "seed while refining, 0 while searching, frozen where it "
                  "was when lost). Only as good as `locked`: with the carrier "
                  "unlocked, loop 1 free-runs and this wanders." },
  { "chip_phase",
    "Code phase, chips: the live Dll's; while refining, the seed's advanced "
    "by the clock dilation since (what the hand-over starts from), so a "
    "refining receiver's signal can be excluded too." },
  { "code_rate",
    "The Dll's code rate, chips per nominal chip (~1.0 at lock), without the "
    "carrier aid; a coasting Dll's is its held rate." },
  { "cn0_dbhz_est", "C/N0 estimate, dB-Hz (the seed's)." },
  { "code_locked", "Presence flag: the Dll's lock detector." },
  { "locked", "Health flag: the symbol-lock detector." },
  { "lock_metric", "cos(2*phi) over the symbols, drives `locked`." },
  { "lock_threshold", "`locked` declares strictly above this." },
  { "car_last_error", "Pre-despread Costas residual, rad." },
  { "mpsk_last_error", "Post-despread carrier residual, rad." },
  { "state_samples", "Input samples since `state` was entered." },
  { "both_down_samples",
    "Input samples both flags have been down without a break (the release "
    "clock); in lost it keeps counting -- samples since the flags dropped. "
    "With reacquire, the symbol lock flag's clock alone." },
  { "refused_hits",
    "Searching hits refused as seeds by the seed domain (a Doppler at or past "
    "fs/2), since create or reset. The search went on over each one's tail." },
  { NULL, NULL },
};
static PyStructSequence_Desc AsyncDsssReceiverObj_status_desc
    = { "doppler.dsss.ReceiverStatus",
        "AsyncDsssReceiver's status record: state (0 searching, 1 refining, 2 "
        "tracking, 4 lost), the live estimates, both lock flags, and the two "
        "clocks in input samples.",
        AsyncDsssReceiverObj_status_fields, 14 };
static PyTypeObject *AsyncDsssReceiverObj_status_type = NULL;

static PyObject *
AsyncDsssReceiverObj_status (AsyncDsssReceiverObject *self,
                             PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!AsyncDsssReceiverObj_status_type)
    {
      AsyncDsssReceiverObj_status_type
          = PyStructSequence_NewType (&AsyncDsssReceiverObj_status_desc);
      if (!AsyncDsssReceiverObj_status_type)
        return NULL;
    }
  async_dsss_receiver_status_t _r
      = dp_async_dsss_receiver_status (self->handle);
  PyObject *_o = PyStructSequence_New (AsyncDsssReceiverObj_status_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyLong_FromLong ((long)_r.state));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.doppler_hz));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.chip_phase));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.code_rate));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.cn0_dbhz_est));
  PyStructSequence_SET_ITEM (_o, 5, PyLong_FromLong ((long)_r.code_locked));
  PyStructSequence_SET_ITEM (_o, 6, PyLong_FromLong ((long)_r.locked));
  PyStructSequence_SET_ITEM (_o, 7, PyFloat_FromDouble (_r.lock_metric));
  PyStructSequence_SET_ITEM (_o, 8, PyFloat_FromDouble (_r.lock_threshold));
  PyStructSequence_SET_ITEM (_o, 9, PyFloat_FromDouble (_r.car_last_error));
  PyStructSequence_SET_ITEM (_o, 10, PyFloat_FromDouble (_r.mpsk_last_error));
  PyStructSequence_SET_ITEM (
      _o, 11,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.state_samples));
  PyStructSequence_SET_ITEM (
      _o, 12,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.both_down_samples));
  PyStructSequence_SET_ITEM (
      _o, 13,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.refused_hits));
  return _o;
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
  PyObject          *doppler_bins_obj = NULL;
  unsigned long long doppler_bins_raw = 0ULL;
  PyObject          *n_noncoh_obj     = NULL;
  unsigned long long n_noncoh_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist,
                                    &doppler_bins_obj, &n_noncoh_obj))
    return NULL;
  if (doppler_bins_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (doppler_bins_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      doppler_bins_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((doppler_bins_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || doppler_bins_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "doppler_bins: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t doppler_bins = (size_t)doppler_bins_raw;
  if (n_noncoh_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_noncoh_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_noncoh_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_noncoh_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_noncoh_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n_noncoh: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t n_noncoh = (size_t)n_noncoh_raw;
  int    _rc      = dp_async_dsss_receiver_configure_search_raw (
      self->handle, doppler_bins, n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_search_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_set_refine_min_blocks (AsyncDsssReceiverObject *self,
                                            PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "n_blocks", NULL };
  PyObject          *n_blocks_obj = NULL;
  unsigned long long n_blocks_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &n_blocks_obj))
    return NULL;
  if (n_blocks_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_blocks_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_blocks_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_blocks_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_blocks_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n_blocks: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t n_blocks = (size_t)n_blocks_raw;
  int    _rc
      = dp_async_dsss_receiver_set_refine_min_blocks (self->handle, n_blocks);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "set_refine_min_blocks failed", (long long)_rc);
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
  PyObject    *n_looks_obj = NULL;
  unsigned long long n_looks_raw = 0ULL;
  double             alpha       = 0.0;
  long long          n_up_raw    = 0LL;
  long long          n_down_raw  = 0LL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddOdLL", _kwlist, &up_thresh,
                                    &down_thresh, &n_looks_obj, &alpha,
                                    &n_up_raw, &n_down_raw))
    return NULL;
  if (n_looks_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_looks_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_looks_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_looks_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_looks_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n_looks: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t n_looks = (size_t)n_looks_raw;
  if (n_up_raw < 0 || n_up_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "n_up: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)n_up_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t n_up = (uint32_t)n_up_raw;
  if (n_down_raw < 0 || n_down_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "n_down: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)n_down_raw, (long long)0,
                    (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t n_down = (uint32_t)n_down_raw;
  dp_async_dsss_receiver_configure_lock_raw (
      self->handle, up_thresh, down_thresh, n_looks, alpha, n_up, n_down);
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
  PyObject          *segments_obj = NULL;
  unsigned long long segments_raw = 0ULL;
  PyObject          *sps_obj      = NULL;
  unsigned long long sps_raw      = 0ULL;
  int                n            = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOi", _kwlist, &segments_obj,
                                    &sps_obj, &n))
    return NULL;
  if (segments_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (segments_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      segments_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((segments_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || segments_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "segments: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t segments = (size_t)segments_raw;
  if (sps_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (sps_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      sps_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((sps_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || sps_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "sps: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t sps = (size_t)sps_raw;
  int _rc = dp_async_dsss_receiver_configure_chain_raw (self->handle, segments,
                                                        sps, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_chain_raw failed", (long long)_rc);
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
  dp_async_dsss_receiver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_set_telemetry (AsyncDsssReceiverObject *self,
                                    PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "tlm", "prefix", "decim", NULL };
  PyObject    *tlm_obj   = Py_None;
  const char  *prefix    = NULL;
  long long    decim_raw = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Os|L", _kwlist, &tlm_obj,
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
  if (decim_raw < 0 || decim_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "decim: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)decim_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t decim = (uint32_t)decim_raw;
  int _rc = dp_async_dsss_receiver_set_telemetry (self->handle, tlm, prefix,
                                                  decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_telemetry failed",
                    (long long)_rc);
      return NULL;
    }
  /* retain: the kernel keeps `tlm`, so this object keeps its owner
     until the next call replaces it or None detaches it. */
  if (tlm_obj != Py_None)
    Py_INCREF (tlm_obj);
  Py_XSETREF (self->_jm_retain_set_telemetry_tlm,
              tlm_obj != Py_None ? tlm_obj : NULL);
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_set_threads (AsyncDsssReceiverObject *self,
                                  PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "n", NULL };
  int          n         = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "i", _kwlist, &n))
    return NULL;
  int _rc = dp_async_dsss_receiver_set_threads (self->handle, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_threads failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_set_rank (AsyncDsssReceiverObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "rank", NULL };
  PyObject          *rank_obj  = NULL;
  unsigned long long rank_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &rank_obj))
    return NULL;
  if (rank_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (rank_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      rank_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((rank_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || rank_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "rank: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t rank = (size_t)rank_raw;
  int    _rc  = dp_async_dsss_receiver_set_rank (self->handle, rank);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "rank refused: it must be below ASYNC_DSSS_RX_MAX_PEAKS "
                    "(16), the peaks a dwell reports",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssReceiverObj_set_exclusions (AsyncDsssReceiverObject *self,
                                     PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]  = { "phases", NULL };
  PyObject    *phases_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &phases_obj))
    return NULL;
  PyArrayObject *phases_arr = jm_array_arg (phases_obj, NPY_DOUBLE,
                                            NPY_ARRAY_C_CONTIGUOUS, "phases");
  if (!phases_arr)
    {
      return NULL;
    }
  const double *phases     = (const double *)PyArray_DATA (phases_arr);
  size_t        phases_len = (size_t)PyArray_SIZE (phases_arr);
  int _rc = dp_async_dsss_receiver_set_exclusions (self->handle, phases,
                                                   phases_len);
  Py_DECREF (phases_arr);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "exclusions refused: every phase must be finite, and at "
                    "most ASYNC_DSSS_RX_MAX_EXCLUSIONS (64) of them",
                    (long long)_rc);
      return NULL;
    }
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
  return PyLong_FromSize_t (dp_async_dsss_receiver_state_bytes (self->handle));
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
  size_t    _n = dp_async_dsss_receiver_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_async_dsss_receiver_get_state (self->handle, PyBytes_AS_STRING (_b));
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
      != dp_async_dsss_receiver_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_async_dsss_receiver_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
AsyncDsssReceiver_getprop_refine_min_blocks (AsyncDsssReceiverObject *self,
                                             void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->refine_min_blocks);
}
static PyObject *
AsyncDsssReceiver_getprop_threads (AsyncDsssReceiverObject *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong (
      (long)dp_async_dsss_receiver_get_threads (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_rank (AsyncDsssReceiverObject *self,
                                void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_async_dsss_receiver_get_rank (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_n_exclusions (AsyncDsssReceiverObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_async_dsss_receiver_get_n_exclusions (
          self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_coherent_bins (AsyncDsssReceiverObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_async_dsss_receiver_get_coherent_bins (
          self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_lifecycle (AsyncDsssReceiverObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long        _v = (long)(self->handle->state);
  const char *_s = _enum_AsyncDsssReceiver_async_dsss_rx_state_name (_v);
  if (!_s)
    {
      PyErr_Format (
          PyExc_ValueError,
          "lifecycle holds async_dsss_rx_state value %ld, which is none of"
          " its choices",
          _v);
      return NULL;
    }
  return PyUnicode_FromString (_s);
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
  return PyLong_FromLong (
      (long)dp_async_dsss_receiver_get_tracking (self->handle));
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
  return PyLong_FromLong (
      (long)dp_async_dsss_receiver_get_refining (self->handle));
}
static PyObject *
AsyncDsssReceiver_getprop_lost (AsyncDsssReceiverObject *self,
                                void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong (
      (long)dp_async_dsss_receiver_get_lost (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_doppler_hz (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_cn0_dbhz_est (self->handle));
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
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_async_dsss_receiver_get_segments (self->handle));
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
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_async_dsss_receiver_get_sps (self->handle));
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
  return PyLong_FromLong ((long)dp_async_dsss_receiver_get_n (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_chip_phase (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_code_rate (self->handle));
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
  return PyFloat_FromDouble (dp_async_dsss_receiver_get_lock (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_norm_freq (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_nco_freq (self->handle));
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
  return PyLong_FromLong (
      (long)dp_async_dsss_receiver_get_locked (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_lock_metric (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_lock_threshold (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_car_last_error (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_car_nco_freq (self->handle));
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
  return PyFloat_FromDouble (
      dp_async_dsss_receiver_get_mpsk_last_error (self->handle));
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
  return PyLong_FromLong (
      (long)dp_async_dsss_receiver_get_code_locked (self->handle));
}

static PyGetSetDef AsyncDsssReceiver_getset[] = {
  { "refine_min_blocks", (getter)AsyncDsssReceiver_getprop_refine_min_blocks,
    NULL,
    "Floor on the refine's dwell, in blocks (default 7). The refine's "
    "detection sizing needs fewer blocks the higher the C/N0, but the "
    "estimate the carrier chain must pull in from does not sharpen with it "
    "(#1265). Set with set_refine_min_blocks(), which carries the "
    "measurement.\n",
    NULL },
  { "threads", (getter)AsyncDsssReceiver_getprop_threads, NULL,
    "Workers the searcher runs, the calling thread included; 1 = serial. Set "
    "with dp_async_dsss_receiver_set_threads().\n",
    NULL },
  { "rank", (getter)AsyncDsssReceiver_getprop_rank, NULL,
    "Which surviving peak of a dwell the search takes; 0 = the strongest. Set "
    "with dp_async_dsss_receiver_set_rank().\n",
    NULL },
  { "n_exclusions", (getter)AsyncDsssReceiver_getprop_n_exclusions, NULL,
    "Code phases set_exclusions() holds.\n", NULL },
  { "coherent_bins", (getter)AsyncDsssReceiver_getprop_coherent_bins, NULL,
    "The searcher's slow-time depth D: epochs per coherent block.\n", NULL },
  { "lifecycle", (getter)AsyncDsssReceiver_getprop_lifecycle, NULL,
    "Where the receiver is in its lifecycle, by name: `\"searching\"`, "
    "`\"refining\"`, `\"tracking\"` or `\"lost\"` (released, waiting for "
    "`reset()`). The same state as `status().state`, which carries it as the "
    "C integer. With `reacquire` a release goes back to `\"searching\"` and "
    "never reads `\"lost\"`.\n",
    NULL },
  { "tracking", (getter)AsyncDsssReceiver_getprop_tracking, NULL,
    "1 once the live tracking chain is built and demodulating; 0 in every "
    "other state.\n",
    NULL },
  { "refining", (getter)AsyncDsssReceiver_getprop_refining, NULL,
    "1 while refining: the CarrierAcquisition dwell; 0 in every other "
    "state.\n",
    NULL },
  { "lost", (getter)AsyncDsssReceiver_getprop_lost, NULL,
    "1 once the release rule has fired: both lock flags were down, "
    "continuously, for longer than `lost_confirm_s` while tracking -- an "
    "emitter that left, or a seed that never locked. The loops have stopped "
    "and samples are consumed and discarded; the holder releases the "
    "assignment and calls reset(). 0 in every other state, and always 0 with "
    "`lost_confirm_s = 0`.\n",
    NULL },
  { "doppler_hz", (getter)AsyncDsssReceiver_getprop_doppler_hz, NULL,
    "The Doppler estimate the chain was built from, Hz: the seed's while "
    "refining, the CarrierAcquisition-refined value once tracking. The live "
    "carrier is status().doppler_hz.\n",
    NULL },
  { "cn0_dbhz_est", (getter)AsyncDsssReceiver_getprop_cn0_dbhz_est, NULL,
    "The seed's C/N0 estimate, dB-Hz, unchanged until reset().\n", NULL },
  { "segments", (getter)AsyncDsssReceiver_getprop_segments, NULL,
    "The live-tracking Dll's partial correlations per code period "
    "(refine_segments is the refine collector's).\n",
    NULL },
  { "sps", (getter)AsyncDsssReceiver_getprop_sps, NULL,
    "MpskReceiver's samples per symbol.\n", NULL },
  { "n", (getter)AsyncDsssReceiver_getprop_n, NULL,
    "MpskReceiver's terminal outputs per symbol (`m_out`: even, 2..8).\n",
    NULL },
  { "chip_phase", (getter)AsyncDsssReceiver_getprop_chip_phase, NULL,
    "The live Dll's code phase, chips, in the Dll's instantaneous convention "
    "-- the mirror of acq_result_t::code_phase's correlation-lag convention "
    "(dp_acq_build_handoff()). While refining there is no live Dll: it is the "
    "seed's phase advanced by the clock dilation since, the phase the "
    "hand-over starts the live chain from.\n",
    NULL },
  { "code_rate", (getter)AsyncDsssReceiver_getprop_code_rate, NULL,
    "The Dll's code rate, chips per nominal chip (~1.0 at lock), without the "
    "carrier aid; a coasting Dll reports its held rate.\n",
    NULL },
  { "lock", (getter)AsyncDsssReceiver_getprop_lock, NULL,
    "The carrier lock statistic of the MpskReceiver this receiver tracks "
    "with: the EMA of its M-th-power lock signal, near 1 when locked and near "
    "0 on noise.\n",
    NULL },
  { "norm_freq", (getter)AsyncDsssReceiver_getprop_norm_freq, NULL,
    "The post-despread carrier the MpskReceiver tracks: its centre plus its "
    "loop's integrator, cycles/sample at its input rate (`sps * "
    "symbol_rate`). Lags a Doppler ramp by the Type-II ramp error.\n",
    NULL },
  { "nco_freq", (getter)AsyncDsssReceiver_getprop_nco_freq, NULL,
    "The MpskReceiver's carrier NCO command, its loop filter's whole output, "
    "cycles/sample at its input rate (`sps * symbol_rate`). Its mean tracks a "
    "Doppler ramp with no lag (unlike get_norm_freq()); its variance is the "
    "carrier loop's stress.\n",
    NULL },
  { "locked", (getter)AsyncDsssReceiver_getprop_locked, NULL,
    "The receiver's symbol-lock flag: a hysteretic detector over "
    "get_lock_metric(), declared after 30 consecutive symbols strictly above "
    "get_lock_threshold() (0.5) and dropped after 15 consecutive symbols "
    "strictly below 0.3 (ASYNC_DSSS_RX_LOCK_N_UP, _UP, _N_DOWN and _DOWN, "
    "fixed at create). The flag the release rule and a pool's holder act "
    "on.\n",
    NULL },
  { "lock_metric", (getter)AsyncDsssReceiver_getprop_lock_metric, NULL,
    "Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) = "
    "cos(2*phi) over the emitted symbols (locked -> ~+1). Drives `locked`.\n",
    NULL },
  { "lock_threshold", (getter)AsyncDsssReceiver_getprop_lock_threshold, NULL,
    "The lock-metric threshold `locked` is declared strictly above (0.5, "
    "ASYNC_DSSS_RX_LOCK_UP); exposed alongside lock_metric for engineering "
    "debug.\n",
    NULL },
  { "car_last_error", (getter)AsyncDsssReceiver_getprop_car_last_error, NULL,
    "Pre-despread Costas phase discriminator (rad): the residual carrier "
    "phase LOOP 1 (which de-rotates before the Dll) is not nulling.\n",
    NULL },
  { "car_nco_freq", (getter)AsyncDsssReceiver_getprop_car_nco_freq, NULL,
    "LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency command, "
    "cycles/sample of the front-end (chip_rate*spc) rate.\n",
    NULL },
  { "mpsk_last_error", (getter)AsyncDsssReceiver_getprop_mpsk_last_error, NULL,
    "MpskReceiver carrier phase discriminator (rad): the residual carrier "
    "phase LOOP 2 (post-despread) is not nulling.\n",
    NULL },
  { "code_locked", (getter)AsyncDsssReceiver_getprop_code_locked, NULL,
    "Binary code-lock flag from the live tracking Dll's own verify-counted "
    "(pfa-tuned) lock detector — the fundamental DSSS \"am I despreading\" "
    "lock, de-chattered by up/down hysteresis.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
AsyncDsssReceiverObj_destroy (AsyncDsssReceiverObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_async_dsss_receiver_destroy (_jm_st);
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
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_async_dsss_receiver_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef AsyncDsssReceiverObj_methods[] = {

  { "steps", (PyCFunction)(void *)AsyncDsssReceiverObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x, out) -> ndarray\n"
    "\n"
    "Stream raw cf32 samples; emit demodulated symbols once tracking.\n"
    "\n"
    "Drives the state machine. It emits nothing while searching or refining\n"
    "(an empty return is normal, not an error): a hit seeds the\n"
    "frozen-carrier refine chain, `CarrierAcquisition` sharpens the coarse\n"
    "Doppler estimate, and only once it is ready (or gives up) is the live\n"
    "tracking chain built and demodulation begins. A hit inside a call hands\n"
    "the rest of the call straight to the refine chain, so no sample is\n"
    "dropped there; at the refine's end, what the refine did not consume of\n"
    "the last call goes on to the live chain. Accepts any block size; state\n"
    "carries across calls, so a capture can be fed in frames of any length\n"
    "with no seam. Lost (after the release rule fires) consumes the samples\n"
    "and emit nothing, so the feeding loop is the same in every state; while\n"
    "tracking, the release clock runs on the two lock flags after every\n"
    "call, or on the symbol lock flag alone with `reacquire` (see\n"
    "`lost_confirm_s`). Tracking emits symbols whether or not they are data:\n"
    "a noise false alarm can reach tracking and emit until the release rule\n"
    "lets it go -- or, at the default lost_confirm_s of 0, until reset() --\n"
    "and symbol lock (`locked`) is what says the symbols are a signal's, so\n"
    "read it rather than the count (#2439). Under SPEC's coupled offset +\n"
    "500 Hz/s Doppler ramp the pre-despread Costas removes the full carrier\n"
    "dynamics before the code loop, so the recovered constellation lands\n"
    "cleanly on the BPSK real axis.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input cf32 samples.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output symbols; caller provides max_out capacity.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of symbols written: 0 while searching, refining or lost, or\n"
    "    before a full symbol's worth of input.\n"
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
    "steps_max_out() -> int\n"
    "\n"
    "The output capacity steps() needs beyond the input's.\n"
    "\n"
    "Returns 0, which is NOT a bound. A call that completes a code period\n"
    "carried over from earlier calls emits all of that period's symbols\n"
    "(code_len times symbol_rate over chip_rate of them), even if it brings\n"
    "one sample. At more than one symbol per period a call can therefore\n"
    "emit more symbols than its `x_len`, and those past `max_out` are\n"
    "dropped (see dp_async_dsss_receiver_steps()). A true bound is #2469.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    0.\n" },
  { "seed", (PyCFunction)(void *)AsyncDsssReceiverObj_seed,
    METH_VARARGS | METH_KEYWORDS,
    "seed(chip_phase, doppler_hz_est, cn0_dbhz_est) -> None\n"
    "\n"
    "Take a detection from outside and start refining from it.\n"
    "\n"
    "The hand-off of docs/design/async-dsss-receiver.md §2.2: the three\n"
    "numbers a searcher's hit carries that this receiver uses --\n"
    "`acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est` --\n"
    "exactly as its own hit would have produced them (`steps()` calls this\n"
    "on its own hit). `chip_phase` is the code's instantaneous phase in\n"
    "chips, Dll's convention, at the FIRST sample of the next `steps()`\n"
    "call; dp_acq_build_handoff() makes it from a hit's lag. The refine\n"
    "chain is rebuilt from the seed and the state becomes refining; the\n"
    "unconsumed tail is the caller's to feed.\n"
    "\n"
    "Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not\n"
    "waiting for one -- refining, tracking or lost -- because \"assigned\n"
    "once\" is a property of the object, not of the caller's bookkeeping;\n"
    "`reset()` releases it. Accepted while searching: an outside hit simply\n"
    "beats the receiver's own. Also refused outside the seed domain,\n"
    "dp_async_dsss_receiver_seed_ok().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "chip_phase : float\n"
    "    Code phase at the next sample, chips, in `[0, code_len)`.\n"
    "doppler_hz_est : float\n"
    "    Coarse Doppler estimate, Hz (the refine stage sharpens it).\n"
    "cn0_dbhz_est : float\n"
    "    The hit's C/N0 estimate, dB-Hz; reported back by\n"
    "    get_cn0_dbhz_est(), unchanged until reset().\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``seed refused: the receiver already holds an assignment (refining,\n"
    "    tracking or lost -- reset() releases it), or chip_phase is outside\n"
    "    [0, code_len), or doppler_hz_est is not finite and below chip_rate\n"
    "    * spc / 2 in magnitude``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,\n"
    "...                        symbol_rate=2700.0, spc=2)\n"
    ">>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,\n"
    "...         cn0_dbhz_est=48.0)\n"
    ">>> (rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)\n"
    "(1, -1500.0, 48.0)\n"
    "\n"
    "Already assigned -- refused until reset():\n"
    "\n"
    ">>> rx.seed(0.0, 0.0, 48.0)      # doctest: +ELLIPSIS\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: seed refused: ...\n"
    ">>> rx.reset()\n"
    "\n"
    "A chip phase must be inside the code, `[0, code_len)`:\n"
    "\n"
    ">>> rx.seed(1023.0, 0.0, 48.0)   # doctest: +ELLIPSIS\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: seed refused: ...\n" },
  { "status", (PyCFunction)AsyncDsssReceiverObj_status, METH_NOARGS,
    "status() -> ReceiverStatus record (state, doppler_hz, chip_phase, "
    "code_rate, cn0_dbhz_est, code_locked, locked, lock_metric, "
    "lock_threshold, car_last_error, mpsk_last_error, state_samples, "
    "both_down_samples, refused_hits)\n"
    "\n"
    "Read the status record, by value: the state; where the emitter is\n"
    "now (the live carrier), its chip phase, code rate and C/N0; both lock\n"
    "flags with the symbol-lock metric and threshold; both carrier\n"
    "residuals; and the two clocks in input samples (since the state was\n"
    "entered; both flags down without a break).\n"
    "\n"
    "Cheap and allocation-free: every field is a read of live state. The\n"
    "one-at-a-time getters report the same fields, `doppler_hz` apart (see\n"
    "the record); this is the face a controller reads. No timestamp: the\n"
    "clocks are in input samples, and the holder, which owns the sample\n"
    "clock, stamps the record.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "ReceiverStatus\n"
    "    The record, by value.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,\n"
    "...                        symbol_rate=2700.0, spc=2)\n"
    ">>> st = rx.status()\n"
    ">>> (st.state, st.doppler_hz, st.code_locked, st.locked)   # searching\n"
    "(0, 0.0, 0, 0)\n"
    ">>> rx.seed(chip_phase=100.0, doppler_hz_est=-250.0, cn0_dbhz_est=50.0)\n"
    ">>> st = rx.status()\n"
    ">>> (st.state, round(st.doppler_hz, 6), st.cn0_dbhz_est)  # refining\n"
    "(1, -250.0, 50.0)\n"
    ">>> _ = rx.steps(np.zeros(2046, np.complex64))\n"
    ">>> rx.status().state_samples                             # since seed\n"
    "2046\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)AsyncDsssReceiverObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> None\n"
    "\n"
    "Pin the embedded Acquisition's search grid directly. Forwards to\n"
    "`dp_acq_configure_search_raw()` -- the escape hatch under this object's\n"
    "`symbol_rate`-driven auto-sizing, for a power user who wants a specific\n"
    "`(doppler_bins, n_noncoh)`. The pin replaces the whole auto-sized grid,\n"
    "the window-tiled search included. Only meaningful while searching; the\n"
    "search does not run again until the next `reset()`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    The coherent depth to pin, in `[1, reps]`: 1, as the receiver's\n"
    "    continuous engine has `reps = 1`.\n"
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
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it\n"
    ">>> rx.refining                # still searching, on the pinned grid\n"
    "0\n" },
  { "set_refine_min_blocks",
    (PyCFunction)(void *)AsyncDsssReceiverObj_set_refine_min_blocks,
    METH_VARARGS | METH_KEYWORDS,
    "set_refine_min_blocks(n_blocks) -> None\n"
    "\n"
    "Floor the refine's dwell at n_blocks, whatever the detection sizing\n"
    "asks (docs/design/async-dsss-receiver-measurements.md §12.16, #1265).\n"
    "\n"
    "CarrierAcquisition's dwell is sized for DETECTION at the derated C/N0\n"
    "(`cn0_dbhz - refine_design_margin_db`), so it shortens as the C/N0\n"
    "rises -- two blocks at 45 dB-Hz with the shipped margin -- while the\n"
    "noise of the estimate it hands the tracking chain does not shorten with\n"
    "it: 210 Hz at two blocks against a chain that pulls in from a few\n"
    "hundred, so one hand-over in sixty landed outside and tracked the code\n"
    "with the carrier never locked. Seven blocks (42 ms, the default and the\n"
    "floor dwell of docs/design/async-dsss-receiver-measurements.md §12.10)\n"
    "hold the estimate to 77 Hz. Applied to the next refine chain built -- a\n"
    "receiver already refining keeps its dwell. A mutator's value, so it\n"
    "travels in the state blob (#2022). `n_blocks` of 0 removes the floor;\n"
    "the value is clamped by `refine_max_n_blocks` where that cap is lower.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_blocks : int\n"
    "    The floor, blocks.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_refine_min_blocks failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,\n"
    "...                        symbol_rate=1e6 / 28.0, spc=4, "
    "cn0_dbhz=60.0)\n"
    ">>> rx.refine_min_blocks                     # the default floor\n"
    "7\n"
    ">>> rx.set_refine_min_blocks(12)\n"
    ">>> rx.refine_min_blocks\n"
    "12\n" },
  { "configure_lock_raw",
    (PyCFunction)(void *)AsyncDsssReceiverObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(up_thresh, down_thresh, n_looks, alpha, n_up, n_down) "
    "-> None\n"
    "\n"
    "Re-tune the live-tracking Dll's code-lock detector directly.\n"
    "Forwards to `dp_dll_configure_lock_raw()` on the live tracking Dll (the\n"
    "one behind `get_code_locked()`), NOT the refine-stage collection Dll.\n"
    "It retunes the Dll the receiver holds now; every chain build (a seed,\n"
    "the hand-over, reset() or configure_chain_raw()) replaces that Dll with\n"
    "a freshly sized one, so call it once tracking. The detector is the\n"
    "hysteretic lockdet over the DLL's per-N-look CFAR statistic — the\n"
    "levels and verify counts trade declare latency against false-alarm\n"
    "rate.\n"
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
    ">>> rx.tracking                       # replaced at the next chain "
    "build\n"
    "0\n" },
  { "configure_chain_raw",
    (PyCFunction)(void *)AsyncDsssReceiverObj_configure_chain_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_chain_raw(segments, sps, n) -> None\n"
    "\n"
    "Pin the live-tracking despread/resample/demod grid directly. The\n"
    "escape hatch for the composition-specific knob: `segments` (the live\n"
    "Dll's tracking parameter) and `sps`/`n` (MpskReceiver's samples and\n"
    "terminal outputs per symbol) are independently overridable, still\n"
    "bridged by a freshly-sized `RateConverter` and never coupled to each\n"
    "other. While searching/refining it re-pins the grid used to build the\n"
    "next tracking chain; once tracking it rebuilds `dll`/`rc`/`rx` in\n"
    "place, allocating every replacement before adopting it, and re-seeds\n"
    "the rebuilt chain from the whole carrier -- loop 1 plus loop 2's\n"
    "integrator, the sum status() reports -- so a retune keeps the signal it\n"
    "was tracking.\n"
    "\n"
    "The new grid is checked as create() checked the first, before anything\n"
    "is rebuilt, and refused with the receiver left on its prior grid: the\n"
    "Dll's domain at segments, sps >= 2, a symbol period of at most 2^20 Dll\n"
    "partials, which the Dll's aid would otherwise refuse mid-build\n"
    "(doppler#2103), and an n MpskReceiver would refuse (#2421). Still an\n"
    "abort: an allocation failure in the rebuild (doppler#2112).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "segments : int\n"
    "    Live-tracking Dll segments per code period.\n"
    "sps : int\n"
    "    MpskReceiver samples per symbol (the resample target), >= 2.\n"
    "n : int\n"
    "    MpskReceiver's terminal outputs per symbol (`m_out`): inside\n"
    "    dp_ratesync_m_ok() (even, 2..8), and dividing sps.\n"
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
    "Return to the searching state. Resets the embedded Acquisition and\n"
    "rebuilds the refine-stage chain and the live tracking chain back to\n"
    "their placeholder seed (phase 0, no Doppler). A receiver that has\n"
    "locked cannot be \"reset back to tracking the same signal,\" only back "
    "to\n"
    "searching -- every object's reset() semantics in this codebase.\n"
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
  { "set_telemetry", (PyCFunction)(void *)AsyncDsssReceiverObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context across the receiver's loops,\n"
    "and register the receiver's own lifecycle and symbol-lock decision.\n"
    "\n"
    "The receiver reports under the prefix, once per steps() call that\n"
    "carries samples, after the call, so on the time axis at the stamp the\n"
    "caller set for that block (dp_tlm_set_now()):\n"
    "\n"
    "- \"<prefix>.state\" -- where the call left the receiver, the\n"
    "  ASYNC_DSSS_RX_* value: 0 searching, 1 refining, 2 tracking, 4 lost\n"
    "  (status().state);\n"
    "- \"<prefix>.handover_hz\" -- the Doppler the chain was built from, Hz\n"
    "  (dp_async_dsss_receiver_get_doppler_hz()): 0 before a seed, the\n"
    "  seed's while refining, the refined value from the hand-over on, and 0\n"
    "  again after a refused hand-over or a reset(), which return the\n"
    "  receiver to searching. A record is a float, so it resolves about 0.5\n"
    "  Hz near 5 MHz. Not the live carrier: status().doppler_hz reads that,\n"
    "  and \".car.freq\" and \".car.nco\" are the carrier loop's own view of "
    "it,\n"
    "  in cycles a sample at the input rate, so times chip_rate * spc for\n"
    "  Hz;\n"
    "- \"<prefix>.refused\" -- the hits refused as seeds so far\n"
    "  (status().refused_hits).\n"
    "\n"
    "Feed blocks of about a code period, as the examples do, and these read\n"
    "as a time series of the lifecycle: when the search hit, how long the\n"
    "refine took, what it handed over, when the release came.\n"
    "\n"
    "Each loop reports under the prefix, at its own update rate:\n"
    "\n"
    "- \"<prefix>.car\" -- the chip-domain carrier loop\n"
    "  (dp_costas_set_telemetry(): \".lock\", \".e\", \".freq\", "
    "\".locked\",\n"
    "  \".nco\"), once per carrier update: once a code period, except while "
    "it\n"
    "  coasts, when loop 1 is held;\n"
    "- \"<prefix>.code\" -- the code loop (dp_dll_set_telemetry(): \".e\",\n"
    "  \".rate\", \".lock\", \".locked\", \".nco\"), once per discriminator "
    "update;\n"
    "- \"<prefix>.demod\" -- the symbol-domain demodulator\n"
    "  (dp_mpsk_receiver_set_telemetry(): its lock, its carrier and timing\n"
    "  loops, the symbols, its AGC), once per recovered symbol (the AGC's\n"
    "  pair once per gain update);\n"
    "- \"<prefix>.acq\" -- the acquisition engine (dp_acq_set_telemetry()),\n"
    "  once per decided dwell;\n"
    "- \"<prefix>.rc\" -- the rate converter between the code loop and the\n"
    "  demodulator (dp_RateConverter_set_telemetry()). It reports only an\n"
    "  AGC, and this receiver builds it without one, so it registers no\n"
    "  probes;\n"
    "- \"<prefix>.sym.lock\" and \"<prefix>.sym.locked\" -- the receiver's "
    "own\n"
    "  symbol-lock metric (dp_async_dsss_receiver_get_lock_metric()) and its\n"
    "  decision (dp_async_dsss_receiver_get_locked()), the one its users act\n"
    "  on, once per emitted symbol.\n"
    "\n"
    "The carrier and code loops report their filters two ways: \".car.freq\"\n"
    "and \".code.rate\" (and the demodulator's \".demod.car.freq\") are the\n"
    "integrators, each loop's frequency memory, which a coast holds;\n"
    "\".car.nco\", \".code.nco\" and \".demod.car.nco\" are the complete "
    "commands\n"
    "the NCOs run at, the proportional term (and the code loop's carrier\n"
    "aid) included. \".demod.sync.rate\" is the timing loop's output, and "
    "\".e\"\n"
    "is each discriminator's input to its filter.\n"
    "\n"
    "The attachment is a REQUEST kept on the receiver: every seed rebuilds\n"
    "the tracking chain, and the rebuilt loops are attached again under the\n"
    "same names (so the same probe ids). Records are therefore continuous\n"
    "across re-seeds, and a reset keeps the attachment too.\n"
    "\n"
    "Setup path, never hot; the context is borrowed and must outlive the\n"
    "attachment, and the receiver must step on one thread (SPSC rules in\n"
    "dp_tlm/dp_tlm_core.h). Passing NULL detaches every loop.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix; NULL means \"rx\".\n"
    "decim : int\n"
    "    Emit every decim-th event on each probe; >= 1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_telemetry failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> rx.set_telemetry(tlm, \"rx\")\n"
    ">>> sorted(p for p in tlm.probe_names if p.startswith(\"rx.c\"))\n"
    "['rx.car.e', 'rx.car.freq', 'rx.car.lock', 'rx.car.locked', "
    "'rx.car.nco', 'rx.code.e', 'rx.code.lock', 'rx.code.locked', "
    "'rx.code.nco', 'rx.code.rate']\n"
    ">>> sorted(p for p in tlm.probe_names if p.startswith((\"rx.s\", "
    "\"rx.h\",\n"
    "...                                                    \"rx.r\")))\n"
    "['rx.handover_hz', 'rx.refused', 'rx.state', 'rx.sym.lock', "
    "'rx.sym.locked']\n"
    ">>> rx.set_telemetry(None, \"rx\")      # detach\n" },
  { "set_threads", (PyCFunction)(void *)AsyncDsssReceiverObj_set_threads,
    METH_VARARGS | METH_KEYWORDS,
    "set_threads(n) -> None\n"
    "\n"
    "Set the workers the searcher fans its Doppler tiles across.\n"
    "\n"
    "Forwards to dp_acq_set_threads() on the receiver's searcher. The search\n"
    "is bit-identical at every count, so this changes the cost of a steps()\n"
    "call and nothing about its output. A pool that steps its receivers in\n"
    "parallel sets each one to 1. A mutator's value, so it travels in the\n"
    "state blob (#2022). The blob carries the count that RAN, not the\n"
    "request: set_threads(0) on a 20-core writer restores as 20 workers on\n"
    "any reader, by design -- a resumed receiver runs as the one that was\n"
    "checkpointed, and a pool's 1 is 1 everywhere. Setup path; not while\n"
    "another thread is inside steps().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Thread count; 0 = online cores, 1 = serial.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_threads failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,\n"
    "...                        symbol_rate=1e6 / 28.0, spc=4, "
    "cn0_dbhz=60.0)\n"
    ">>> rx.set_threads(1)\n"
    ">>> rx.threads\n"
    "1\n" },
  { "set_rank", (PyCFunction)(void *)AsyncDsssReceiverObj_set_rank,
    METH_VARARGS | METH_KEYWORDS,
    "set_rank(rank) -> None\n"
    "\n"
    "Take the rank-th surviving peak of a dwell, not the strongest.\n"
    "\n"
    "The searcher reports up to ASYNC_DSSS_RX_MAX_PEAKS peaks per dwell,\n"
    "strongest first. The receiver drops every peak within one chip\n"
    "(circularly) of a stronger one, so one signal's neighbouring cells hold\n"
    "one place; then every peak a stronger signal's own sidelobes could\n"
    "explain; then every peak within one chip of an excluded code phase\n"
    "(set_exclusions()). The survivors are ranked from 0. A dwell with no\n"
    "peak at this rank hands nothing off and the search goes on.\n"
    "\n"
    "The sidelobe rule: a strong signal's list also holds its code's\n"
    "Doppler-offset sidelobes, up to dp_acq_dsl_db() below it and hundreds\n"
    "of chips from its phase (design acq-surface.md §8). A peak counts as a\n"
    "second signal only if its test_stat exceeds the stronger signals'\n"
    "bound, times that level, by more than the detector's own gate\n"
    "(dp_acq_gate_stat()). The bound is the strongest signal's test_stat; on\n"
    "a deep block (get_coherent_bins() > 1) it is the sum of the stronger\n"
    "signals on the peak's own Doppler row, modulo the epoch rate, when that\n"
    "is larger, since their fixed sidelobes can stack on one cell. So the\n"
    "range does not narrow with the number of signals on the air\n"
    "(doppler#2554). A sidelobe then passes only when the noise on it alone\n"
    "would have cleared the gate, the rate the search already runs at. What\n"
    "it costs is near-far: a second signal much more than dp_acq_dsl_db()\n"
    "below a stronger one (-16.95 dB on the SPEC Gold-1023 search) is not\n"
    "taken while the stronger one is on the air (async-dsss-parallel.md §3).\n"
    "Searchers that share a stream and its block grid (get_coherent_bins())\n"
    "and hold ranks 0..k-1 therefore take k different signals in one dwell.\n"
    "Takes effect at the next dwell. A mutator's value, so it travels in the\n"
    "state blob (#2022).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rank : int\n"
    "    0 (the strongest, the default) to ASYNC_DSSS_RX_MAX_PEAKS - 1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``rank refused: it must be below ASYNC_DSSS_RX_MAX_PEAKS (16), the\n"
    "    peaks a dwell reports``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,\n"
    "...                        symbol_rate=1e6 / 28.0, spc=4, "
    "cn0_dbhz=60.0)\n"
    ">>> rx.set_rank(1)\n"
    ">>> rx.rank\n"
    "1\n" },
  { "set_exclusions", (PyCFunction)(void *)AsyncDsssReceiverObj_set_exclusions,
    METH_VARARGS | METH_KEYWORDS,
    "set_exclusions(phases) -> None\n"
    "\n"
    "Code phases the search must not take: signals another receiver\n"
    "already tracks.\n"
    "\n"
    "Replaces the list. A peak within one chip, circularly, of any of them\n"
    "is dropped from a dwell's list at any Doppler, before ranking\n"
    "(set_rank()). Phases are chips in status().chip_phase's convention and\n"
    "are folded into [0, code_len). An empty list (the default) excludes\n"
    "nothing. A tracking receiver is not affected: the list applies to its\n"
    "next search. A mutator's value, so it travels in the state blob\n"
    "(#2022).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "phases : npt.NDArray[np.float64]\n"
    "    Code phases, chips; finite. May be NULL when phases_len is 0.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``exclusions refused: every phase must be finite, and at most\n"
    "    ASYNC_DSSS_RX_MAX_EXCLUSIONS (64) of them``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,\n"
    "...                        symbol_rate=1e6 / 28.0, spc=4, "
    "cn0_dbhz=60.0)\n"
    ">>> rx.set_exclusions(np.array([2.5, 9.0]))   # 9 folds to 2\n"
    ">>> rx.n_exclusions\n"
    "2\n" },
  { "state_bytes", (PyCFunction)AsyncDsssReceiverObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AsyncDsssReceiver has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AsyncDsssReceiverObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures the state that evolves as the object runs, including any value\n"
    "a setter changed after construction, so a blob taken now and restored\n"
    "later resumes from this point. Construction parameters are not\n"
    "restored: restore into an object built the same way. A core may check\n"
    "some of them and refuse a blob built differently.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the AsyncDsssReceiver has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AsyncDsssReceiverObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place, including any value a setter\n"
    "changed after construction. Construction parameters are not restored:\n"
    "restore into an object built the same way. Length is validated against\n"
    "`state_bytes()` before the blob is handed to the C core, which may\n"
    "reject it too: a core may check some construction parameters and refuse\n"
    "a blob built differently.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AsyncDsssReceiver has already been destroyed.\n"
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
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
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
  { NULL, NULL, 0, NULL }
};

static PyTypeObject AsyncDsssReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.dsss.AsyncDsssReceiver",
  .tp_basicsize                           = sizeof (AsyncDsssReceiverObject),
  .tp_dealloc = (destructor)AsyncDsssReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create an AsyncDsssReceiver in the searching state.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : NDArray[np.uint8]\n"
    "    Spreading code, one 0/1 chip per element (0 -> +1, 1 -> -1 BPSK; "
    "only\n"
    "    the low bit is used, so pass 0/1, not +/-1).\n"
    "chip_rate : float, default 1000000.0\n"
    "    Chip rate, Hz.\n"
    "symbol_rate : float, default 1000.0\n"
    "    Data-symbol rate, Hz: finite, > 0, at most `chip_rate`, and at most\n"
    "    2^20 Dll partials per symbol at `segments`.\n"
    "spc : int, default 2\n"
    "    Samples/chip; default 2.\n"
    "m : int, default 2\n"
    "    PSK order, 2/4/8; default 2 (BPSK).\n"
    "cn0_dbhz : float, default 55.0\n"
    "    Design C/N0, dB-Hz; default 55.0 -- feeds BOTH the embedded\n"
    "    Acquisition's own sizing AND (derated by `refine_design_margin_db`)\n"
    "    CarrierAcquisition's `design_snr`.\n"
    "pfa : float, default 1e-3\n"
    "    Acquisition false-alarm target; default 1e-3. Also "
    "CarrierAcquisition's\n"
    "    own `pfa`.\n"
    "pd : float, default 0.9\n"
    "    Acquisition detection-probability target; default 0.9. Also\n"
    "    CarrierAcquisition's own `pd`.\n"
    "doppler_uncertainty : float, default 100.0\n"
    "    One-sided Doppler search half-range, Hz; default 100.0.\n"
    "segments : int, default 4\n"
    "    Live-tracking Dll's own segments; default 4.\n"
    "sps : int, default 8\n"
    "    MpskReceiver's samples/symbol; default 8.\n"
    "differential : int, default 0\n"
    "    MpskReceiver's differential demap; default 0 (coherent). It acts in\n"
    "    MpskReceiver's bits(), which this object does not offer, so "
    "steps()'\n"
    "    symbols are the same either way (#2471).\n"
    "refine_max_error_db : float, default 0.5\n"
    "    Max tolerable async-lookback correlation-power loss driving the\n"
    "    refine-stage collection Dll's coherent-I&D window count via\n"
    "    dp_dll_lookback_segments(). Oversampling the epoch is required for "
    "the\n"
    "    asynchronous data: the residual carrier rides a ~symbol_rate-wide\n"
    "    data-modulated spectrum, so segments>1 (default yields 11 at\n"
    "    tsamps=2046) samples it above Nyquist; segments=1 undersamples and\n"
    "    aliases it. Default 0.5.\n"
    "refine_samples_per_symbol : int, default 4\n"
    "    CarrierAcquisition's own operating rate = this * symbol_rate; "
    "default\n"
    "    4.\n"
    "refine_design_margin_db : float, default 14.0\n"
    "    Empirical derating of cn0_dbhz before CarrierAcquisition's "
    "design_snr;\n"
    "    default 14.0.\n"
    "refine_n_fft : int, default 64\n"
    "    CarrierAcquisition's own block size; default 64.\n"
    "refine_zero_pad : int, default 8\n"
    "    CarrierAcquisition's own zero_pad; default 8.\n"
    "refine_sequential : bool, default False\n"
    "    CarrierAcquisition's sequential mode; default false -- its early\n"
    "    per-block test fires on far too little averaging at SPEC's Es/N0 "
    "floor\n"
    "    (as few as 4 blocks, 150-200+ Hz off), so false waits the full\n"
    "    design_snr-derived dwell.\n"
    "refine_max_n_blocks : int, default 100000\n"
    "    CarrierAcquisition's give-up cap in sequential mode, and the cap on "
    "the\n"
    "    dwell in either mode; default 100000.\n"
    "carrier_freq_hz : float, default 0.0\n"
    "    Nominal RF carrier frequency, Hz, enabling carrier->code aiding; "
    "0.0\n"
    "    (default) = off. When > 0, the coupled code-rate Doppler\n"
    "    (carrier_offset/carrier_freq) is fed to the tracking Dll via\n"
    "    dp_dll_set_rate_aid() so the code loop rides a dilated clock the\n"
    "    discriminator alone can't pull in at low SNR. Set to the receiver's "
    "own\n"
    "    downlink RF frequency for a physically-coupled Doppler capture.\n"
    "    Coupled, it is above half the sample rate "
    "(dp_acq_carrier_freq_ok()).\n"
    "lost_confirm_s : float, default 0.0\n"
    "    Release rule: both lock flags down, continuously, for longer than "
    "this\n"
    "    many seconds puts the receiver in the lost state (see get_lost()). "
    "Size\n"
    "    it past the longest fade the link must ride. The clock also runs "
    "from\n"
    "    the first tracking sample, when neither flag is up yet, so a "
    "hand-off\n"
    "    that never locks within the interval is released the same way as an\n"
    "    emitter that leaves. Default 0.0 = never -- the exit is then "
    "reset().\n"
    "reacquire : bool, default False\n"
    "    Release on symbol lock alone, and search again by itself. With it "
    "set,\n"
    "    the release clock counts samples of tracking with the SYMBOL lock "
    "flag\n"
    "    down (code lock is not consulted), and when it passes "
    "lost_confirm_s\n"
    "    the receiver resets itself to searching instead of entering lost: "
    "it\n"
    "    never waits for reset(). A pool of receivers that are always online\n"
    "    uses it. The clock runs from the first tracking sample, as above, so "
    "a\n"
    "    track that never locks (a noise false alarm) is released too; size\n"
    "    lost_confirm_s past the pull-in time to symbol lock, or a slow lock "
    "is\n"
    "    released before it comes. Requires a lost_confirm_s of at least one\n"
    "    input sample, since a release that never fires would never search\n"
    "    again; a blob in the lost state is refused by set_state(). Default\n"
    "    false: the release rule above.\n"
    "code_only_s : float, default 0.0\n"
    "    Seconds of code-only signal the waveform holds at any chip phase (a\n"
    "    symbol-free window; design async-dsss-parallel.md §4), >= 0 and "
    "finite.\n"
    "    The receiver hands its searcher `code_only_epochs = "
    "floor(code_only_s *\n"
    "    chip_rate / code_len) - 1` (at least 1), and the searcher's "
    "slow-time\n"
    "    depth follows: `coherent_bins = D`, at most `(code_only_epochs + "
    "1)/2`,\n"
    "    rounded down to a 7-smooth length (dp_acq_create_continuous()). "
    "Default\n"
    "    0: no window, D = 1.\n"
    "doppler_rate : float, default 0.0\n"
    "    Doppler rate in Hz/s, >= 0 and finite, that the slow-time depth is\n"
    "    bounded against: at most `f_epoch / sqrt(2 * doppler_rate)` epochs\n"
    "    (dp_acq_create_continuous()). Default 0: the window is the only "
    "bound.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is "
    "``AsyncDsssReceiver:\n"
    "    invalid parameter (need a code of at least 2 chips, a finite "
    "chip_rate\n"
    "    > 0, a finite symbol_rate > 0 and <= chip_rate, spc >= 1, m in "
    "{2,4,8},\n"
    "    segments >= 1, sps >= 2, carrier_freq_hz 0 or finite and above\n"
    "    chip_rate * spc / 2, lost_confirm_s >= 0 (and at least one input "
    "sample\n"
    "    with reacquire), refine_samples_per_symbol >= 1, refine_n_fft >= 1,\n"
    "    refine_zero_pad >= 1, a symbol of at most 2^20 Dll partials, "
    "segments *\n"
    "    chip_rate / (code_len * symbol_rate) <= 1048576; and for the "
    "searcher a\n"
    "    finite cn0_dbhz, 0 < pfa < 1, 0 < pd < 1, a finite "
    "doppler_uncertainty\n"
    "    >= 0)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssReceiver\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                        spc=2, doppler_uncertainty=500.0)\n"
    ">>> (rx.tracking, rx.refining, rx.lost)       # searching\n"
    "(0, 0, 0)\n",
  .tp_methods = AsyncDsssReceiverObj_methods,
  .tp_getset  = AsyncDsssReceiver_getset,
  .tp_new     = AsyncDsssReceiverObj_new,
  .tp_init    = (initproc)AsyncDsssReceiverObj_init,
};
