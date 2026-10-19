/* jm:generated dsss_ext_async_dsss_pool.c */
/*
 * dsss_ext_async_dsss_pool.c — AsyncDsssPool type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in dsss_ext_async_dsss_pool_extra.c.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* AsyncDsssPoolObject — wraps dp_async_dsss_pool_state_t *       */
/* ======================================================== */

#include "doppler/async_dsss_pool/async_dsss_pool_core.h"

typedef struct
{
  PyObject_HEAD dp_async_dsss_pool_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
  PyObject     *_jm_retain_set_event_log_log; /* keeps log's owner alive */
  PyObject     *_jm_retain_set_telemetry_tlm; /* keeps tlm's owner alive */
} AsyncDsssPoolObject;

static void
AsyncDsssPoolObj_dealloc (AsyncDsssPoolObject *self)
{
  if (self->handle)
    dp_async_dsss_pool_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_XDECREF (self->_jm_retain_set_event_log_log);
  Py_XDECREF (self->_jm_retain_set_telemetry_tlm);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AsyncDsssPoolObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  AsyncDsssPoolObject *self = (AsyncDsssPoolObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AsyncDsssPoolObj_init (AsyncDsssPoolObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char       *kwlist[]                 = { "code",
                                                  "chip_rate",
                                                  "symbol_rate",
                                                  "spc",
                                                  "m",
                                                  "cn0_dbhz",
                                                  "pfa",
                                                  "pd",
                                                  "doppler_uncertainty",
                                                  "code_only_epochs",
                                                  "doppler_rate",
                                                  "max_peaks",
                                                  "n_slots",
                                                  "threads",
                                                  "carrier_freq_hz",
                                                  "lost_confirm_s",
                                                  "max_emitter_on_time_secs",
                                                  "segments",
                                                  "sps",
                                                  "differential",
                                                  "gain",
                                                  "pullin_intervals",
                                                  NULL };
  PyObject          *code_obj                 = NULL;
  double             chip_rate                = 1000000.0;
  double             symbol_rate              = 1000.0;
  PyObject          *spc_obj                  = NULL;
  unsigned long long spc_raw                  = 2;
  int                m                        = 2;
  double             cn0_dbhz                 = 55.0;
  double             pfa                      = 1e-3;
  double             pd                       = 0.9;
  double             doppler_uncertainty      = 100.0;
  PyObject          *code_only_epochs_obj     = NULL;
  unsigned long long code_only_epochs_raw     = 813;
  double             doppler_rate             = 0.0;
  PyObject          *max_peaks_obj            = NULL;
  unsigned long long max_peaks_raw            = 16;
  PyObject          *n_slots_obj              = NULL;
  unsigned long long n_slots_raw              = 12;
  int                threads                  = 1;
  double             carrier_freq_hz          = 0.0;
  double             lost_confirm_s           = 2.0;
  double             max_emitter_on_time_secs = 900.0;
  PyObject          *segments_obj             = NULL;
  unsigned long long segments_raw             = 4;
  PyObject          *sps_obj                  = NULL;
  unsigned long long sps_raw                  = 8;
  int                differential             = 0;
  double             gain                     = 0.125;
  PyObject          *pullin_intervals_obj     = NULL;
  unsigned long long pullin_intervals_raw     = 4;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|ddOiddddOdOOidddOOidO", kwlist, &code_obj, &chip_rate,
          &symbol_rate, &spc_obj, &m, &cn0_dbhz, &pfa, &pd,
          &doppler_uncertainty, &code_only_epochs_obj, &doppler_rate,
          &max_peaks_obj, &n_slots_obj, &threads, &carrier_freq_hz,
          &lost_confirm_s, &max_emitter_on_time_secs, &segments_obj, &sps_obj,
          &differential, &gain, &pullin_intervals_obj))
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
  if (code_only_epochs_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (code_only_epochs_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      code_only_epochs_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((code_only_epochs_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || code_only_epochs_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "code_only_epochs: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t code_only_epochs = (size_t)code_only_epochs_raw;
  if (max_peaks_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (max_peaks_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      max_peaks_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((max_peaks_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || max_peaks_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "max_peaks: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t max_peaks = (size_t)max_peaks_raw;
  if (n_slots_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_slots_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      n_slots_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_slots_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_slots_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n_slots: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t n_slots = (size_t)n_slots_raw;
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
  if (pullin_intervals_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (pullin_intervals_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      pullin_intervals_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((pullin_intervals_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || pullin_intervals_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "pullin_intervals: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t         pullin_intervals = (size_t)pullin_intervals_raw;
  PyArrayObject *code_arr
      = jm_array_arg (code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "code");
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = dp_async_dsss_pool_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, chip_rate,
      symbol_rate, spc, m, cn0_dbhz, pfa, pd, doppler_uncertainty,
      code_only_epochs, doppler_rate, max_peaks, n_slots, threads,
      carrier_freq_hz, lost_confirm_s, max_emitter_on_time_secs, segments, sps,
      differential, gain, pullin_intervals);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "AsyncDsssPool: invalid parameter, or a searcher a "
                       "cell receiver cannot take (need a code of at least 2 "
                       "chips, a finite chip_rate > 0, a finite symbol_rate "
                       "> 0 and <= chip_rate, spc >= 1, m in {2,4,8}, "
                       "segments >= 1, sps >= 2, carrier_freq_hz 0 or finite "
                       "and above chip_rate * spc / 2, lost_confirm_s >= 0, "
                       "0 < gain <= 1, n_slots >= 1, 1 <= max_peaks <= 64, "
                       "max_emitter_on_time_secs >= 0, a symbol of at most "
                       "2^20 Dll partials, segments * chip_rate / (code_len "
                       "* symbol_rate) <= 1048576; and a searcher with a "
                       "finite cn0_dbhz, 0 < pfa < 1, 0 < pd < 1, a finite "
                       "doppler_uncertainty >= 0, a finite doppler_rate >= "
                       "0, and a depth D > 1 -- code_only_epochs > 1 -- "
                       "whose Doppler row is at most four times the carrier "
                       "loop's pull-in bound, doppler_res_hz <= 2 * 0.04 * "
                       "chip_rate / code_len)");
      return -1;
    }
  return 0;
}

static PyObject *
AsyncDsssPoolObj_reset (AsyncDsssPoolObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_async_dsss_pool_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssPoolObj_push (AsyncDsssPoolObject *self, PyObject *args,
                       PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", NULL };
  PyObject    *x_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &x_obj))
    return NULL;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  size_t y;
  void  *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    y = dp_async_dsss_pool_push (_jm_h, x, x_len);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyStructSequence_Field AsyncDsssPoolObj_status_fields[] = {
  { "slot", "The slot asked for." },
  { "assigned", "1 while a receiver holds an emitter." },
  { "state", "The receiver's ASYNC_DSSS_RX_* state; -1 for a slot that does "
             "not exist." },
  { "seed_sample", "Stream position the row was assigned at." },
  { "seed_chip_phase", "The seed's code phase, chips." },
  { "seed_doppler_hz", "The seed's Doppler, Hz." },
  { "seed_cn0_dbhz", "The seed's C/N0 estimate, dB-Hz." },
  { "doppler_hz", "Where the emitter is now, Hz." },
  { "chip_phase", "Live Dll code phase, chips." },
  { "code_rate", "The Dll's code rate, chips per nominal chip (~1.0), without "
                 "the aid; a coasting Dll's is its held rate." },
  { "cn0_dbhz_est", "C/N0 estimate, dB-Hz." },
  { "code_locked", "Presence flag." },
  { "locked", "Health flag (symbol lock)." },
  { "lock_metric", "The symbol-lock statistic." },
  { "state_samples", "Samples since the receiver's state was entered." },
  { "both_down_samples", "The release clock, samples." },
  { "assigned_samples", "Samples since the row was assigned." },
  { NULL, NULL },
};
static PyStructSequence_Desc AsyncDsssPoolObj_status_desc
    = { "doppler.dsss.PoolSlot",
        "AsyncDsssPool's slot record: whether the slot is assigned, the seed "
        "it was assigned from, the receiver's live status (state 1 refining, "
        "2 tracking, 3 idle, 4 lost), and the samples since the assignment.",
        AsyncDsssPoolObj_status_fields, 17 };
static PyTypeObject *AsyncDsssPoolObj_status_type = NULL;

static PyObject *
AsyncDsssPoolObj_status (AsyncDsssPoolObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "slot", NULL };
  PyObject          *slot_obj  = NULL;
  unsigned long long slot_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &slot_obj))
    return NULL;
  if (slot_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (slot_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      slot_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((slot_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || slot_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "slot: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t slot = (size_t)slot_raw;
  if (!AsyncDsssPoolObj_status_type)
    {
      AsyncDsssPoolObj_status_type
          = PyStructSequence_NewType (&AsyncDsssPoolObj_status_desc);
      if (!AsyncDsssPoolObj_status_type)
        {
          return NULL;
        }
    }
  async_dsss_pool_slot_t _r = dp_async_dsss_pool_status (self->handle, slot);
  PyObject *_o = PyStructSequence_New (AsyncDsssPoolObj_status_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (
      _o, 0, PyLong_FromUnsignedLongLong ((unsigned long long)_r.slot));
  PyStructSequence_SET_ITEM (_o, 1, PyLong_FromLong ((long)_r.assigned));
  PyStructSequence_SET_ITEM (_o, 2, PyLong_FromLong ((long)_r.state));
  PyStructSequence_SET_ITEM (
      _o, 3, PyLong_FromUnsignedLongLong ((unsigned long long)_r.seed_sample));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.seed_chip_phase));
  PyStructSequence_SET_ITEM (_o, 5, PyFloat_FromDouble (_r.seed_doppler_hz));
  PyStructSequence_SET_ITEM (_o, 6, PyFloat_FromDouble (_r.seed_cn0_dbhz));
  PyStructSequence_SET_ITEM (_o, 7, PyFloat_FromDouble (_r.doppler_hz));
  PyStructSequence_SET_ITEM (_o, 8, PyFloat_FromDouble (_r.chip_phase));
  PyStructSequence_SET_ITEM (_o, 9, PyFloat_FromDouble (_r.code_rate));
  PyStructSequence_SET_ITEM (_o, 10, PyFloat_FromDouble (_r.cn0_dbhz_est));
  PyStructSequence_SET_ITEM (_o, 11, PyLong_FromLong ((long)_r.code_locked));
  PyStructSequence_SET_ITEM (_o, 12, PyLong_FromLong ((long)_r.locked));
  PyStructSequence_SET_ITEM (_o, 13, PyFloat_FromDouble (_r.lock_metric));
  PyStructSequence_SET_ITEM (
      _o, 14,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.state_samples));
  PyStructSequence_SET_ITEM (
      _o, 15,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.both_down_samples));
  PyStructSequence_SET_ITEM (
      _o, 16,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.assigned_samples));
  return _o;
}

static PyObject *
AsyncDsssPoolObj_symbols_max_out (AsyncDsssPoolObject *self,
                                  PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_async_dsss_pool_symbols_max_out (self->handle));
}

static PyObject *
AsyncDsssPoolObj_symbols (AsyncDsssPoolObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "slot", "out", NULL };
  PyObject          *slot_obj  = NULL;
  unsigned long long slot_raw  = 0ULL;
  PyObject          *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &slot_obj,
                                    &out_obj))
    return NULL;
  if (slot_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (slot_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      slot_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((slot_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || slot_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "slot: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t slot = (size_t)slot_raw;
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
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = dp_async_dsss_pool_symbols_max_out (self->handle);
      size_t _min_cap
          = _omax > dp_async_dsss_pool_symbols_max_out (self->handle)
                ? _omax
                : (dp_async_dsss_pool_symbols_max_out (self->handle));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = dp_async_dsss_pool_symbols (
          self->handle, slot, (float _Complex *)PyArray_DATA (out_arr), _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "AsyncDsssPool.symbols: wrote %zu elements into a buffer of %zu",
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
  size_t _need = dp_async_dsss_pool_symbols_max_out (self->handle);
  size_t _cap  = dp_async_dsss_pool_symbols_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "AsyncDsssPool.symbols: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_async_dsss_pool_symbols (self->handle, slot, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "AsyncDsssPool.symbols: wrote %zu elements into a buffer of %zu",
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
AsyncDsssPoolObj_set_event_log (AsyncDsssPoolObject *self, PyObject *args,
                                PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "log", NULL };
  PyObject    *log_obj   = Py_None;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &log_obj))
    return NULL;
  dp_event_log_t *log = NULL;
  if (log_obj != Py_None)
    {
      PyObject *log_cap = log_obj;
      Py_INCREF (log_cap);
      if (!PyCapsule_CheckExact (log_cap))
        {
          Py_DECREF (log_cap);
          log_cap = PyObject_GetAttrString (log_obj, "_capsule");
          if (!log_cap)
            return NULL;
        }
      log = (dp_event_log_t *)PyCapsule_GetPointer (
          log_cap, "doppler.telemetry.dp_event_log");
      Py_DECREF (log_cap);
      if (!log)
        return NULL;
    }
  int _rc = dp_async_dsss_pool_set_event_log (self->handle, log);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_event_log failed",
                    (long long)_rc);
      return NULL;
    }
  /* retain: the kernel keeps `log`, so this object keeps its owner
     until the next call replaces it or None detaches it. */
  if (log_obj != Py_None)
    Py_INCREF (log_obj);
  Py_XSETREF (self->_jm_retain_set_event_log_log,
              log_obj != Py_None ? log_obj : NULL);
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssPoolObj_set_telemetry (AsyncDsssPoolObject *self, PyObject *args,
                                PyObject *kwds)
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
  int      _rc
      = dp_async_dsss_pool_set_telemetry (self->handle, tlm, prefix, decim);
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
AsyncDsssPoolObj_state_bytes (AsyncDsssPoolObject *self,
                              PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_async_dsss_pool_state_bytes (self->handle));
}

static PyObject *
AsyncDsssPoolObj_get_state (AsyncDsssPoolObject *self,
                            PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_async_dsss_pool_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_async_dsss_pool_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AsyncDsssPoolObj_set_state (AsyncDsssPoolObject *self, PyObject *arg)
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
      != dp_async_dsss_pool_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_async_dsss_pool_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
AsyncDsssPool_getprop_n_slots (AsyncDsssPoolObject *self,
                               void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_slots);
}
static PyObject *
AsyncDsssPool_getprop_n_assigned (AsyncDsssPoolObject *self,
                                  void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_assigned);
}
static PyObject *
AsyncDsssPool_getprop_dropped (AsyncDsssPoolObject *self,
                               void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->dropped);
}
static PyObject *
AsyncDsssPool_getprop_events (AsyncDsssPoolObject *self,
                              void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->events);
}
static PyObject *
AsyncDsssPool_getprop_samples_consumed (AsyncDsssPoolObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->samples_consumed);
}
static PyObject *
AsyncDsssPool_getprop_doppler_res_hz (AsyncDsssPoolObject *self,
                                      void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (((self->handle->acq->doppler_res_hz)));
}
static PyObject *
AsyncDsssPool_getprop_coherent_bins (AsyncDsssPoolObject *self,
                                     void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)((self->handle->acq->coherent_bins)));
}

static PyGetSetDef AsyncDsssPool_getset[] = {
  { "n_slots", (getter)AsyncDsssPool_getprop_n_slots, NULL,
    "Receivers held; never exceeded.\n", NULL },
  { "n_assigned", (getter)AsyncDsssPool_getprop_n_assigned, NULL,
    "Slots assigned right now.\n", NULL },
  { "dropped", (getter)AsyncDsssPool_getprop_dropped, NULL,
    "Detections dropped for want of a free slot since create or reset.\n",
    NULL },
  { "events", (getter)AsyncDsssPool_getprop_events, NULL,
    "Transitions since create/reset, logged or not.\n", NULL },
  { "samples_consumed", (getter)AsyncDsssPool_getprop_samples_consumed, NULL,
    "Input samples pushed since create or reset: the stream position every "
    "event is stamped at.\n",
    NULL },
  { "doppler_res_hz", (getter)AsyncDsssPool_getprop_doppler_res_hz, NULL,
    "The searcher's Doppler row, Hz -- the resolution a seed's Doppler is "
    "reported at.\n",
    NULL },
  { "coherent_bins", (getter)AsyncDsssPool_getprop_coherent_bins, NULL,
    "The searcher's block-coherent depth D, from code_only_epochs and "
    "doppler_rate (docs/design/async-dsss-receiver.md §2.3).\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
AsyncDsssPoolObj_destroy (AsyncDsssPoolObject *self,
                          PyObject            *Py_UNUSED (ignored))
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
      dp_async_dsss_pool_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
AsyncDsssPoolObj_enter (AsyncDsssPoolObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AsyncDsssPoolObj_exit (AsyncDsssPoolObject *self, PyObject *args)
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
      dp_async_dsss_pool_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef AsyncDsssPoolObj_methods[] = {
  { "reset", (PyCFunction)AsyncDsssPoolObj_reset, METH_NOARGS,
    "Release every slot and start over: the searcher reset, every\n"
    "receiver back to idle, the table cleared, the counters zeroed.\n"
    "\n"
    "The attached log stays attached and nothing is logged -- a reset is the\n"
    "holder's decision, not an emitter's transition.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, n_slots=2)\n"
    ">>> _ = pool.push(np.zeros(2046, np.complex64))\n"
    ">>> pool.samples_consumed\n"
    "2046\n"
    ">>> pool.reset()\n"
    ">>> (pool.samples_consumed, pool.n_assigned, pool.events)\n"
    "(0, 0, 0)\n" },

  { "push", (PyCFunction)(void *)AsyncDsssPoolObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x) -> int\n"
    "\n"
    "One block of raw cf32 samples through the population.\n"
    "\n"
    "In order: the table refreshed from every live receiver's status(); the\n"
    "searcher, whose every peak within one chip of a live row's code phase,\n"
    "at any Doppler, is discarded silently as that emitter's own (the zone\n"
    "is the code axis alone: a tracked emitter's data blocks put smeared\n"
    "copies of it at its own phase rows away,\n"
    "docs/design/async-dsss-receiver-measurements.md §12.14), and each\n"
    "survivor seeded into a free slot or counted dropped when there is none\n"
    "(a hit outside the seed domain is logged refused, not dropped); every\n"
    "receiver fed, across the pool's threads (an idle or lost one consumes\n"
    "and discards); then every receiver that reports lost, or has held its\n"
    "slot past `max_emitter_on_time_secs`, released -- the row cleared, the\n"
    "receiver reset to idle. Every transition goes to the attached log (the\n"
    "seven labels are set_event_log()'s): a seed, a drop and a refusal at\n"
    "the hit's sample, a receiver's transitions at the end of this push.\n"
    "Accepts any block size: a hit decided inside the block is referred to\n"
    "the block's start before it seeds (the receiver is fed the whole\n"
    "block), on the dilated clock when the carrier is known; a dwell is\n"
    "decided when its samples arrive.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Receivers assigned after this push.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, n_slots=2)\n"
    ">>> int(pool.push(np.zeros(4 * 2046, np.complex64)))  # silence: no one\n"
    "0\n"
    ">>> pool.samples_consumed                  # the stream position\n"
    "8184\n" },
  { "status", (PyCFunction)(void *)AsyncDsssPoolObj_status,
    METH_VARARGS | METH_KEYWORDS,
    "status(slot) -> PoolSlot record (slot, assigned, state, seed_sample, "
    "seed_chip_phase, seed_doppler_hz, seed_cn0_dbhz, doppler_hz, chip_phase, "
    "code_rate, cn0_dbhz_est, code_locked, locked, lock_metric, "
    "state_samples, both_down_samples, assigned_samples)\n"
    "\n"
    "One slot's picture, by value: whether it is assigned, the seed it\n"
    "was assigned from (sample, chip phase, Doppler, C/N0), the receiver's\n"
    "own status record, and the samples since the assignment.\n"
    "\n"
    "Allocation-free: the row plus the receiver's own status record. A slot\n"
    "outside `[0, n_slots)` returns a zero record with `state` -1.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "slot : int\n"
    "    The slot.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "PoolSlot\n"
    "    The record.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, n_slots=2)\n"
    ">>> r = pool.status(1)\n"
    ">>> (r.slot, r.assigned, r.state)         # idle: 3, nothing assigned\n"
    "(1, 0, 3)\n"
    ">>> pool.status(2).state                  # no such slot\n"
    "-1\n" },
  { "symbols", (PyCFunction)(void *)AsyncDsssPoolObj_symbols,
    METH_VARARGS | METH_KEYWORDS,
    "symbols(slot, out) -> ndarray\n"
    "\n"
    "The symbols the receiver in slot decided on the last push().\n"
    "\n"
    "Copied from the pool's own buffer, which the next push() overwrites.\n"
    "Empty while the slot is idle or lost, and for a slot outside `[0,\n"
    "n_slots)`; a cell receiver's pull-in (refining) emits.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "slot : int\n"
    "    The slot.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Caller buffer.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Symbols written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, n_slots=2)\n"
    ">>> _ = pool.push(np.zeros(2046, np.complex64))\n"
    ">>> pool.symbols(0).shape                 # idle: nothing decided\n"
    "(0,)\n" },
  { "symbols_max_out", (PyCFunction)AsyncDsssPoolObj_symbols_max_out,
    METH_NOARGS,
    "symbols_max_out() -> int\n"
    "\n"
    "The per-slot symbol capacity `symbols()` can return -- grown with\n"
    "the largest block pushed so far (0 before the first push).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "set_event_log", (PyCFunction)(void *)AsyncDsssPoolObj_set_event_log,
    METH_VARARGS | METH_KEYWORDS,
    "set_event_log(log) -> None\n"
    "\n"
    "Attach the run's event log (docs/design/async-dsss-receiver.md\n"
    "§8.1); NULL (None) detaches.\n"
    "\n"
    "Borrowed, never owned: the holder opens, finalizes and closes it. From\n"
    "now on every transition is appended, labelled `seeded`, `tracking`,\n"
    "`degrade`, `lost`, `released`, `dropped` or `refused` -- a seed, a drop\n"
    "and a refusal at the hit's sample, a receiver's transitions at the end\n"
    "of the push that saw them -- with `slot`, `state`, `doppler_hz`,\n"
    "`chip_phase` and `cn0_dbhz` staged as `doppler:<name>` fields beside\n"
    "the label (`core:label`) and, on `released`, `reason` (`lost` or\n"
    "`on_time`). A log that has already failed keeps failing (its error is\n"
    "sticky); the pool counts the transition either way.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "log : object | None\n"
    "    The log, or NULL.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_event_log failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import os, tempfile\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, n_slots=2)\n"
    ">>> log = EventLog(os.path.join(tempfile.mkdtemp(), \"run.events\"))\n"
    ">>> pool.set_event_log(log)               # attached: transitions go "
    "here\n"
    ">>> _ = pool.push(np.zeros(2046, np.complex64))\n"
    ">>> pool.set_event_log(None)              # detached\n"
    ">>> log.close()\n" },
  { "set_telemetry", (PyCFunction)(void *)AsyncDsssPoolObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context across the pool: the\n"
    "searcher, every slot's receiver, and the pool's own counters.\n"
    "\n"
    "Registers, under prefix:\n"
    "\n"
    "- \"<prefix>.acq\" -- the searcher (dp_acq_set_telemetry(): its test\n"
    "  statistic, gate, CFAR reference, strongest cell and its coordinates,\n"
    "  picks and held twins, concentration and hit flag), once per decided\n"
    "  dwell;\n"
    "- \"<prefix>.s<i>\" -- slot i's receiver, every loop it holds\n"
    "  (dp_async_dsss_receiver_set_telemetry()), at each loop's own rate,\n"
    "  and continuous across the seeds and releases that reuse the slot;\n"
    "- \"<prefix>.assigned\", \"<prefix>.dropped\", \"<prefix>.events\" -- "
    "the\n"
    "  pool's `n_assigned` and its running `dropped` and `events` counts,\n"
    "  once per push(), after its releases.\n"
    "\n"
    "The probes come to 13 plus each slot's (a cell receiver's: its loops,\n"
    "its symbol-lock pair and its lifecycle probes, 30 in all), so a large\n"
    "pool needs a context with room: an attach that cannot register every\n"
    "probe fails whole. One context's DP_TLM_MAX_PROBES (512) holds up to 16\n"
    "slots, 13 + 16 * 30 = 493.\n"
    "\n"
    "A context is single-producer (dp_tlm/dp_tlm_core.h), and a pool created\n"
    "with `threads != 1` (more than one, or `<= 0` for the core count) may\n"
    "step its receivers on several, so it is refused: observe a pool on one\n"
    "thread.\n"
    "\n"
    "Setup path, never hot; the context is borrowed and must outlive the\n"
    "attachment. Passing NULL detaches everything.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix; NULL means \"pool\".\n"
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
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, n_slots=2)\n"
    ">>> tlm = Telemetry(1 << 16)\n"
    ">>> pool.set_telemetry(tlm, \"pool\")\n"
    ">>> sorted(p for p in tlm.probe_names if p.count(\".\") == 1)\n"
    "['pool.assigned', 'pool.dropped', 'pool.events']\n"
    ">>> sorted({p.split(\".\")[1] for p in tlm.probe_names})\n"
    "['acq', 'assigned', 'dropped', 'events', 's0', 's1']\n"
    ">>> _ = pool.push(np.zeros(2046, np.complex64))\n"
    ">>> pool.set_telemetry(None, \"pool\")      # detach\n" },
  { "state_bytes", (PyCFunction)AsyncDsssPoolObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AsyncDsssPool has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AsyncDsssPoolObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AsyncDsssPool has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AsyncDsssPoolObj_set_state, METH_O,
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
    "``RuntimeError`` if the AsyncDsssPool has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AsyncDsssPoolObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)AsyncDsssPoolObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AsyncDsssPool be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AsyncDsssPool\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AsyncDsssPoolObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the AsyncDsssPool.\n"
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

static PyTypeObject AsyncDsssPoolObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.dsss.AsyncDsssPool",
  .tp_basicsize                           = sizeof (AsyncDsssPoolObject),
  .tp_dealloc = (destructor)AsyncDsssPoolObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a async_dsss_pool instance: the population on cell receivers, "
    "the\n"
    "searcher's timing driving every slot\n"
    "(docs/design/async-dsss-receiver-measurements.md §12.22-12.28, #1283).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : NDArray[np.uint8]\n"
    "    Spreading code, one 0/1 chip per element.\n"
    "chip_rate : float, default 1000000.0\n"
    "    Chip rate, Hz (default: 1000000.0).\n"
    "symbol_rate : float, default 1000.0\n"
    "    Data-symbol rate, Hz (default: 1000.0).\n"
    "spc : int, default 2\n"
    "    Samples per chip (default: 2).\n"
    "m : int, default 2\n"
    "    PSK order of the receivers (default: 2).\n"
    "cn0_dbhz : float, default 55.0\n"
    "    Design C/N0 for the searcher's sizing and the receivers' (default:\n"
    "    55.0).\n"
    "pfa : float, default 1e-3\n"
    "    False-alarm target: the searcher's, and the receivers' pull-in\n"
    "    estimator's (default: 1e-3).\n"
    "pd : float, default 0.9\n"
    "    Detection-probability target, both as for pfa (default: 0.9).\n"
    "doppler_uncertainty : float, default 100.0\n"
    "    The searcher's one-sided span, Hz (default: 100.0).\n"
    "code_only_epochs : int, default 813\n"
    "    Whole code-only epochs the waveform's window holds at any chip phase "
    "--\n"
    "    the block depth of docs/design/async-dsss-receiver.md §2.3; must "
    "give D\n"
    "    > 1 (default: 813).\n"
    "doppler_rate : float, default 0.0\n"
    "    Doppler rate the depth is bounded against, Hz/s; 0 leaves the window "
    "as\n"
    "    the only bound (default: 0.0).\n"
    "max_peaks : int, default 16\n"
    "    The searcher's list capacity per dwell (default: 16).\n"
    "n_slots : int, default 12\n"
    "    Receivers held (default: 12).\n"
    "threads : int, default 1\n"
    "    Threads the receivers and the searcher's fan run across; <= 0 picks "
    "the\n"
    "    online core count, 1 is serial (default: 1).\n"
    "carrier_freq_hz : float, default 0.0\n"
    "    RF carrier the Doppler is physically coupled to, Hz, told to the\n"
    "    searcher and every receiver; 0.0 = uncoupled (default: 0.0). "
    "Coupled,\n"
    "    it is above half the sample rate (dp_acq_carrier_freq_ok()).\n"
    "lost_confirm_s : float, default 2.0\n"
    "    The release rule's interval, seconds\n"
    "    (docs/design/async-dsss-receiver.md §10) (default: 2.0).\n"
    "max_emitter_on_time_secs : float, default 900.0\n"
    "    Maximum on-air time of one emitter, seconds; 0 = never (default:\n"
    "    900.0).\n"
    "segments : int, default 4\n"
    "    The receivers' live Dll segments (default: 4).\n"
    "sps : int, default 8\n"
    "    The receivers' samples per symbol (default: 8).\n"
    "differential : int, default 0\n"
    "    The receivers' differential demap (default: 0).\n"
    "gain : float, default 0.125\n"
    "    The receivers' correction gain, chips per chip of the interval-mean\n"
    "    read, (0, 1] (default: 0.125).\n"
    "pullin_intervals : int, default 4\n"
    "    Intervals at gain 1 before `gain` applies (default: 4).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``AsyncDsssPool:\n"
    "    invalid parameter, or a searcher a cell receiver cannot take (need "
    "a\n"
    "    code of at least 2 chips, a finite chip_rate > 0, a finite "
    "symbol_rate\n"
    "    > 0 and <= chip_rate, spc >= 1, m in {2,4,8}, segments >= 1, sps >= "
    "2,\n"
    "    carrier_freq_hz 0 or finite and above chip_rate * spc / 2,\n"
    "    lost_confirm_s >= 0, 0 < gain <= 1, n_slots >= 1, 1 <= max_peaks <= "
    "64,\n"
    "    max_emitter_on_time_secs >= 0, a symbol of at most 2^20 Dll "
    "partials,\n"
    "    segments * chip_rate / (code_len * symbol_rate) <= 1048576; and a\n"
    "    searcher with a finite cn0_dbhz, 0 < pfa < 1, 0 < pd < 1, a finite\n"
    "    doppler_uncertainty >= 0, a finite doppler_rate >= 0, and a depth D "
    "> 1\n"
    "    -- code_only_epochs > 1 -- whose Doppler row is at most four times "
    "the\n"
    "    carrier loop's pull-in bound, doppler_res_hz <= 2 * 0.04 * chip_rate "
    "/\n"
    "    code_len)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                      spc=2, cn0_dbhz=45.0, doppler_uncertainty=5e3,\n"
    "...                      code_only_epochs=813, doppler_rate=500.0,\n"
    "...                      n_slots=4)\n"
    ">>> (pool.n_slots, pool.n_assigned, pool.coherent_bins)\n"
    "(4, 0, 154)\n"
    ">>> round(pool.doppler_res_hz, 1)       # the row a seed comes from\n"
    "31.7\n",
  .tp_methods = AsyncDsssPoolObj_methods,
  .tp_getset  = AsyncDsssPool_getset,
  .tp_new     = AsyncDsssPoolObj_new,
  .tp_init    = (initproc)AsyncDsssPoolObj_init,
};
