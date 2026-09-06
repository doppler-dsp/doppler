/*
 * dsss_ext_async_dsss_pool.c — AsyncDsssPool type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* AsyncDsssPoolObject — wraps async_dsss_pool_state_t *       */
/* ======================================================== */

#include "async_dsss_pool/async_dsss_pool_core.h"

typedef struct
{
  PyObject_HEAD async_dsss_pool_state_t *handle;
} AsyncDsssPoolObject;

static void
AsyncDsssPoolObj_dealloc (AsyncDsssPoolObject *self)
{
  if (self->handle)
    async_dsss_pool_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AsyncDsssPoolObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
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
                                                  "refine_max_error_db",
                                                  "refine_samples_per_symbol",
                                                  "refine_design_margin_db",
                                                  "refine_n_fft",
                                                  "refine_zero_pad",
                                                  "refine_sequential",
                                                  "refine_max_n_blocks",
                                                  NULL };
  PyObject          *code_obj                 = NULL;
  double             chip_rate                = 1000000.0;
  double             symbol_rate              = 1000.0;
  unsigned long long spc_raw                  = 2;
  int                m                        = 2;
  double             cn0_dbhz                 = 55.0;
  double             pfa                      = 1e-3;
  double             pd                       = 0.9;
  double             doppler_uncertainty      = 100.0;
  unsigned long long code_only_epochs_raw     = 1;
  double             doppler_rate             = 0.0;
  unsigned long long max_peaks_raw            = 16;
  unsigned long long n_slots_raw              = 12;
  int                threads                  = 1;
  double             carrier_freq_hz          = 0.0;
  double             lost_confirm_s           = 2.0;
  double             max_emitter_on_time_secs = 900.0;
  unsigned long long segments_raw             = 4;
  unsigned long long sps_raw                  = 8;
  int                differential             = 0;
  double             refine_max_error_db      = 0.5;
  unsigned long long refine_samples_per_symbol_raw = 4;
  double             refine_design_margin_db       = 14.0;
  unsigned long long refine_n_fft_raw              = 64;
  unsigned long long refine_zero_pad_raw           = 8;
  int                refine_sequential_raw         = false;
  unsigned long long refine_max_n_blocks_raw       = 100000;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|ddKiddddKdKKidddKKidKdKKpK", kwlist, &code_obj,
          &chip_rate, &symbol_rate, &spc_raw, &m, &cn0_dbhz, &pfa, &pd,
          &doppler_uncertainty, &code_only_epochs_raw, &doppler_rate,
          &max_peaks_raw, &n_slots_raw, &threads, &carrier_freq_hz,
          &lost_confirm_s, &max_emitter_on_time_secs, &segments_raw, &sps_raw,
          &differential, &refine_max_error_db, &refine_samples_per_symbol_raw,
          &refine_design_margin_db, &refine_n_fft_raw, &refine_zero_pad_raw,
          &refine_sequential_raw, &refine_max_n_blocks_raw))
    return -1;
  size_t spc                       = (size_t)spc_raw;
  size_t code_only_epochs          = (size_t)code_only_epochs_raw;
  size_t max_peaks                 = (size_t)max_peaks_raw;
  size_t n_slots                   = (size_t)n_slots_raw;
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
  self->handle    = async_dsss_pool_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, chip_rate,
      symbol_rate, spc, m, cn0_dbhz, pfa, pd, doppler_uncertainty,
      code_only_epochs, doppler_rate, max_peaks, n_slots, threads,
      carrier_freq_hz, lost_confirm_s, max_emitter_on_time_secs, segments, sps,
      differential, refine_max_error_db, refine_samples_per_symbol,
      refine_design_margin_db, refine_n_fft, refine_zero_pad,
      refine_sequential, refine_max_n_blocks);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "async_dsss_pool_create returned NULL");
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
  async_dsss_pool_reset (self->handle);
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
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  size_t                y     = async_dsss_pool_push (self->handle, x, x_len);
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
  { "code_rate", "Live Dll code rate, chips per sample." },
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
  unsigned long long slot_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &slot_raw))
    return NULL;
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
  async_dsss_pool_slot_t _r = async_dsss_pool_status (self->handle, slot);
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
  return PyLong_FromSize_t (async_dsss_pool_symbols_max_out (self->handle));
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
  unsigned long long slot_raw  = 0ULL;
  PyObject          *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K|O", _kwlist, &slot_raw,
                                    &out_obj))
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = async_dsss_pool_symbols_max_out (self->handle);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = async_dsss_pool_symbols (
          self->handle, slot, (float _Complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _need = async_dsss_pool_symbols_max_out (self->handle);
  size_t _cap  = async_dsss_pool_symbols_max_out (self->handle);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = async_dsss_pool_symbols (self->handle, slot, _d0, _cap);
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
  int _rc = async_dsss_pool_set_event_log (self->handle, log);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_event_log failed",
                    (long long)_rc);
      return NULL;
    }
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
  return PyLong_FromSize_t (async_dsss_pool_state_bytes (self->handle));
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
  size_t    _n = async_dsss_pool_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  async_dsss_pool_get_state (self->handle, PyBytes_AS_STRING (_b));
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
      != async_dsss_pool_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (async_dsss_pool_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
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
    "Receivers the pool holds; it never exceeds this.\n", NULL },
  { "n_assigned", (getter)AsyncDsssPool_getprop_n_assigned, NULL,
    "Slots assigned right now.\n", NULL },
  { "dropped", (getter)AsyncDsssPool_getprop_dropped, NULL,
    "Detections dropped for want of a free slot since create or reset.\n",
    NULL },
  { "events", (getter)AsyncDsssPool_getprop_events, NULL,
    "Transitions since create or reset, logged or not.\n", NULL },
  { "samples_consumed", (getter)AsyncDsssPool_getprop_samples_consumed, NULL,
    "Input samples pushed since create or reset -- the stream position every "
    "event is stamped at.\n",
    NULL },
  { "doppler_res_hz", (getter)AsyncDsssPool_getprop_doppler_res_hz, NULL,
    "The searcher's Doppler row, Hz -- one side of the exclusion zone.\n",
    NULL },
  { "coherent_bins", (getter)AsyncDsssPool_getprop_coherent_bins, NULL,
    "The searcher's block-coherent depth D, from code_only_epochs and "
    "doppler_rate (section 2.3).\n",
    NULL },
  { NULL }
};

static PyObject *
AsyncDsssPoolObj_destroy (AsyncDsssPoolObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      async_dsss_pool_destroy (self->handle);
      self->handle = NULL;
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
      async_dsss_pool_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AsyncDsssPoolObj_methods[] = {
  { "reset", (PyCFunction)AsyncDsssPoolObj_reset, METH_NOARGS,
    "Reset AsyncDsssPool to its post-create state: the searcher reset,\n"
    "every receiver back to idle, the table cleared, the counters zeroed.\n"
    "The attached log stays attached; nothing is logged.\n" },

  { "push", (PyCFunction)(void *)AsyncDsssPoolObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x) -> int\n"
    "\n"
    "One block of raw cf32 samples through the population (design section\n"
    "8.2), in order: the searcher; the table refreshed from every live\n"
    "receiver's status(); every peak inside one exclusion zone (one Doppler\n"
    "row by one chip, section 7.1) of a live row dropped as that emitter's\n"
    "own; each survivor seeded into a free slot, or counted dropped when\n"
    "there is none; every receiver fed (an idle or lost one consumes and\n"
    "discards, so the feed has no per-state branch), across the threads the\n"
    "pool was given; then every receiver that reports lost, or has held its\n"
    "slot past max_emitter_on_time_secs, released -- the row cleared, the\n"
    "receiver reset to idle. Every transition -- seeded, tracking, degrade,\n"
    "lost, released, dropped -- goes to the attached event log at the sample\n"
    "it happened. Accepts any block size; a searcher dwell is decided when\n"
    "its samples arrive. Returns the receivers assigned after this push.\n"
    "\n"
    "In order: the searcher; the table refreshed; every peak inside one\n"
    "exclusion zone of a live row dropped as that emitter's own; each\n"
    "survivor seeded into a free slot or counted dropped; every receiver\n"
    "fed, across the pool's threads; every receiver that reports lost, or\n"
    "has held its slot past the maximum on-air time, released. Every\n"
    "transition goes to the attached log at the sample it happened. Accepts\n"
    "any block size: a hit decided inside the block is referred to the\n"
    "block's start before it seeds (the receiver is fed the whole block), on\n"
    "the dilated clock when the carrier is known.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
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
    "was assigned from (sample, chip phase, Doppler, C/N0 -- the searcher's\n"
    "hand-off record, verbatim), the receiver's own status record (state,\n"
    "the live Doppler, chip phase, code rate and C/N0, both lock flags, the\n"
    "symbol-lock metric, the two clocks), and the samples since the\n"
    "assignment. Raises ValueError for a slot outside [0, n_slots).\n"
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
    "The symbols slot `slot`'s receiver decided on the last push(),\n"
    "borrowed from the pool's own buffer (sized once at create by the\n"
    "receiver's steps_max_out()): empty while the slot is idle, refining or\n"
    "lost. Valid until the next push(), reset() or set_state(). Raises\n"
    "ValueError for a slot outside [0, n_slots).\n"
    "\n"
    "Copied from the pool's own buffer, which the next push() overwrites.\n"
    "Empty while the slot is idle, refining or lost, and for a slot outside\n"
    "`[0, n_slots)`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "slot : int\n"
    "    The slot.\n"
    "out : NDArray[np.complex64] | None\n"
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
    "Attach the run's event log (design section 8.1): from now on every\n"
    "transition -- seeded, tracking, degrade, lost, released, dropped -- is\n"
    "appended at the sample it happened, with the slot, the receiver's\n"
    "state, the Doppler, the chip phase and the C/N0 staged as\n"
    "doppler:<name> fields beside the label (core:label). The pool is the\n"
    "one component that stamps; the log is borrowed, never owned. None\n"
    "detaches.\n"
    "\n"
    "Borrowed, never owned: the holder opens, finalizes and closes it. From\n"
    "now on every transition is appended at the sample it happened, with\n"
    "`slot`, `state`, `doppler_hz`, `chip_phase` and `cn0_dbhz` staged as\n"
    "`doppler:<name>` fields beside the label (`core:label`) and, on\n"
    "`released`, `reason` (`lost` or `on_time`). A log that has already\n"
    "failed keeps failing (its error is sticky); the pool counts the\n"
    "transition either way.\n"
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
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
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
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
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
  { NULL }
};

static PyTypeObject AsyncDsssPoolObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.AsyncDsssPool",
  .tp_basicsize                           = sizeof (AsyncDsssPoolObject),
  .tp_dealloc = (destructor)AsyncDsssPoolObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a async_dsss_pool instance.\n"
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
    "    False-alarm target, the searcher's and the refine's (default: "
    "1e-3).\n"
    "pd : float, default 0.9\n"
    "    Detection-probability target (default: 0.9).\n"
    "doppler_uncertainty : float, default 100.0\n"
    "    The searcher's one-sided span, Hz (default: 100.0).\n"
    "code_only_epochs : int, default 1\n"
    "    Whole code-only epochs the waveform's window holds at any chip phase "
    "--\n"
    "    the block depth of section 2.3; 1 = no window (default: 1).\n"
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
    "    searcher and every receiver; 0.0 = uncoupled (default: 0.0).\n"
    "lost_confirm_s : float, default 2.0\n"
    "    The release rule's interval, seconds (section 10) (default: 2.0).\n"
    "max_emitter_on_time_secs : float, default 900.0\n"
    "    Maximum on-air time of one emitter, seconds: a slot held longer is\n"
    "    released (`reason` on_time); 0 = never (default: 900.0,\n"
    "    ASYNC_DSSS_POOL_MAX_EMITTER_ON_ TIME_SECS).\n"
    "segments : int, default 4\n"
    "    The receivers' live Dll segments (default: 4).\n"
    "sps : int, default 8\n"
    "    The receivers' samples per symbol (default: 8).\n"
    "differential : int, default 0\n"
    "    The receivers' differential demap (default: 0).\n"
    "refine_max_error_db : float, default 0.5\n"
    "    As async_dsss_receiver_create() (default: 0.5).\n"
    "refine_samples_per_symbol : int, default 4\n"
    "    As async_dsss_receiver_create() (default: 4).\n"
    "refine_design_margin_db : float, default 14.0\n"
    "    As async_dsss_receiver_create() (default: 14.0).\n"
    "refine_n_fft : int, default 64\n"
    "    As async_dsss_receiver_create() (default: 64).\n"
    "refine_zero_pad : int, default 8\n"
    "    As async_dsss_receiver_create() (default: 8).\n"
    "refine_sequential : bool, default False\n"
    "    As async_dsss_receiver_create() (default: false).\n"
    "refine_max_n_blocks : int, default 100000\n"
    "    As async_dsss_receiver_create() (default: 100000).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> obj = AsyncDsssPool(\n"
    "...     code=np.zeros(1, dtype=np.uint8),\n"
    "...     chip_rate=1000000.0,\n"
    "...     symbol_rate=1000.0,\n"
    "...     spc=2,\n"
    "...     m=2,\n"
    "...     cn0_dbhz=55.0,\n"
    "...     pfa=1e-3,\n"
    "...     pd=0.9,\n"
    "...     doppler_uncertainty=100.0,\n"
    "...     code_only_epochs=1,\n"
    "...     doppler_rate=0.0,\n"
    "...     max_peaks=16,\n"
    "...     n_slots=12,\n"
    "...     threads=1,\n"
    "...     carrier_freq_hz=0.0,\n"
    "...     lost_confirm_s=2.0,\n"
    "...     max_emitter_on_time_secs=900.0,\n"
    "...     segments=4,\n"
    "...     sps=8,\n"
    "...     differential=0,\n"
    "...     refine_max_error_db=0.5,\n"
    "...     refine_samples_per_symbol=4,\n"
    "...     refine_design_margin_db=14.0,\n"
    "...     refine_n_fft=64,\n"
    "...     refine_zero_pad=8,\n"
    "...     refine_sequential=False,\n"
    "...     refine_max_n_blocks=100000,\n"
    "... )\n",
  .tp_methods = AsyncDsssPoolObj_methods,
  .tp_getset  = AsyncDsssPool_getset,
  .tp_new     = AsyncDsssPoolObj_new,
  .tp_init    = (initproc)AsyncDsssPoolObj_init,
};
