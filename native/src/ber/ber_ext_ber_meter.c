/*
 * ber_ext_ber_meter.c — BerMeter type for the ber module.
 *
 * Included by ber_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only ber_ext.c is compiled.
 */
/* ======================================================== */
/* BerMeterObject — wraps ber_meter_state_t *       */
/* ======================================================== */

#include "ber_meter/ber_meter_core.h"

typedef struct
{
  PyObject_HEAD ber_meter_state_t *handle;
} BerMeterObject;

static void
BerMeterObj_dealloc (BerMeterObject *self)
{
  if (self->handle)
    ber_meter_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
BerMeterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  BerMeterObject *self = (BerMeterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
BerMeterObj_init (BerMeterObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "m", "target_errors", "conf", NULL };
  int                m        = 4;
  unsigned long long target_errors_raw = 200;
  double             conf              = 0.99;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|iKd", kwlist, &m,
                                    &target_errors_raw, &conf))
    return -1;
  size_t target_errors = (size_t)target_errors_raw;
  self->handle         = ber_meter_create (m, target_errors, conf);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "m must be 2, 4 or 8 and conf must lie in (0, 1)");
      return -1;
    }
  return 0;
}

static PyObject *
BerMeterObj_reset (BerMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ber_meter_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
BerMeterObj_set_truth (BerMeterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "truth", NULL };
  PyObject    *truth_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &truth_obj))
    return NULL;
  PyArrayObject *truth_arr = (PyArrayObject *)PyArray_FROM_OTF (
      truth_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!truth_arr)
    {
      return NULL;
    }
  const uint8_t *truth     = (const uint8_t *)PyArray_DATA (truth_arr);
  size_t         truth_len = (size_t)PyArray_SIZE (truth_arr);
  int            _rc = ber_meter_set_truth (self->handle, truth, truth_len);
  Py_DECREF (truth_arr);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_truth failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BerMeterObj_align (BerMeterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "rx", "t0", "n_marker", "period", "lag_span", "pfa", NULL };
  PyObject          *rx_obj       = NULL;
  unsigned long long t0_raw       = 0;
  unsigned long long n_marker_raw = 0;
  unsigned long long period_raw   = 0;
  int                lag_span     = 0;
  double             pfa          = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKKid", _kwlist, &rx_obj,
                                    &t0_raw, &n_marker_raw, &period_raw,
                                    &lag_span, &pfa))
    return NULL;
  size_t         t0       = (size_t)t0_raw;
  size_t         n_marker = (size_t)n_marker_raw;
  size_t         period   = (size_t)period_raw;
  PyArrayObject *rx_arr   = (PyArrayObject *)PyArray_FROM_OTF (
      rx_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!rx_arr)
    {
      return NULL;
    }
  const float complex *rx     = (const float complex *)PyArray_DATA (rx_arr);
  size_t               rx_len = (size_t)PyArray_SIZE (rx_arr);
  int y = ber_meter_align (self->handle, rx, rx_len, t0, n_marker, period,
                           lag_span, pfa);
  Py_DECREF (rx_arr);
  return PyLong_FromLong ((long)y);
}

static PyObject *
BerMeterObj_score (BerMeterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "rx", "lo", "hi", NULL };
  PyObject          *rx_obj    = NULL;
  unsigned long long lo_raw    = 0;
  unsigned long long hi_raw    = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KK", _kwlist, &rx_obj,
                                    &lo_raw, &hi_raw))
    return NULL;
  size_t         lo     = (size_t)lo_raw;
  size_t         hi     = (size_t)hi_raw;
  PyArrayObject *rx_arr = (PyArrayObject *)PyArray_FROM_OTF (
      rx_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!rx_arr)
    {
      return NULL;
    }
  const float complex *rx     = (const float complex *)PyArray_DATA (rx_arr);
  size_t               rx_len = (size_t)PyArray_SIZE (rx_arr);
  size_t               y = ber_meter_score (self->handle, rx, rx_len, lo, hi);
  Py_DECREF (rx_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyStructSequence_Field BerMeterObj_ser_fields[] = {
  { "p_hat", "Unbiased point estimate `(r-1)/(N-1)`." },
  { "lo", "Lower confidence limit." },
  { "hi", "Upper confidence limit." },
  { "rel", "Relative standard error `1/sqrt(r)`." },
  { "conf", "Two-sided confidence level." },
  { "errors", "Symbol errors counted." },
  { "symbols", "Symbols scored." },
  { NULL, NULL },
};
static PyStructSequence_Desc BerMeterObj_ser_desc
    = { "doppler.ber.BerInterval",
        "Error-rate point estimate with a Gamma/chi-square confidence "
        "interval. Assert on `lo`, never `p_hat`.",
        BerMeterObj_ser_fields, 7 };
static PyTypeObject *BerMeterObj_ser_type = NULL;

static PyObject *
BerMeterObj_ser (BerMeterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!BerMeterObj_ser_type)
    {
      BerMeterObj_ser_type = PyStructSequence_NewType (&BerMeterObj_ser_desc);
      if (!BerMeterObj_ser_type)
        return NULL;
    }
  ber_interval_t _r = ber_meter_ser (self->handle);
  PyObject      *_o = PyStructSequence_New (BerMeterObj_ser_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.p_hat));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.lo));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.hi));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.rel));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.conf));
  PyStructSequence_SET_ITEM (
      _o, 5, PyLong_FromUnsignedLongLong ((unsigned long long)_r.errors));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLongLong ((unsigned long long)_r.symbols));
  return _o;
}

static PyStructSequence_Field BerMeterObj_ber_fields[] = {
  { "p_hat", "Unbiased point estimate `(r-1)/(N-1)`." },
  { "lo", "Lower confidence limit." },
  { "hi", "Upper confidence limit." },
  { "rel", "Relative standard error `1/sqrt(r)`." },
  { "conf", "Two-sided confidence level." },
  { "errors", "Symbol errors counted." },
  { "symbols", "Symbols scored." },
  { NULL, NULL },
};
static PyStructSequence_Desc BerMeterObj_ber_desc
    = { "doppler.ber.BerInterval",
        "Error-rate point estimate with a Gamma/chi-square confidence "
        "interval. Assert on `lo`, never `p_hat`.",
        BerMeterObj_ber_fields, 7 };
static PyTypeObject *BerMeterObj_ber_type = NULL;

static PyObject *
BerMeterObj_ber (BerMeterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!BerMeterObj_ber_type)
    {
      BerMeterObj_ber_type = PyStructSequence_NewType (&BerMeterObj_ber_desc);
      if (!BerMeterObj_ber_type)
        return NULL;
    }
  ber_interval_t _r = ber_meter_ber (self->handle);
  PyObject      *_o = PyStructSequence_New (BerMeterObj_ber_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.p_hat));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.lo));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.hi));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.rel));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.conf));
  PyStructSequence_SET_ITEM (
      _o, 5, PyLong_FromUnsignedLongLong ((unsigned long long)_r.errors));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLongLong ((unsigned long long)_r.symbols));
  return _o;
}

static PyStructSequence_Field BerMeterObj_interval_fields[] = {
  { "p_hat", "Unbiased point estimate `(r-1)/(N-1)`." },
  { "lo", "Lower confidence limit." },
  { "hi", "Upper confidence limit." },
  { "rel", "Relative standard error `1/sqrt(r)`." },
  { "conf", "Two-sided confidence level." },
  { "errors", "Symbol errors counted." },
  { "symbols", "Symbols scored." },
  { NULL, NULL },
};
static PyStructSequence_Desc BerMeterObj_interval_desc
    = { "doppler.ber.BerInterval",
        "Error-rate point estimate with a Gamma/chi-square confidence "
        "interval. Assert on `lo`, never `p_hat`.",
        BerMeterObj_interval_fields, 7 };
static PyTypeObject *BerMeterObj_interval_type = NULL;

static PyObject *
BerMeterObj_interval (BerMeterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]   = { "errors", "symbols", NULL };
  unsigned long long errors_raw  = 0ULL;
  unsigned long long symbols_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KK", _kwlist, &errors_raw,
                                    &symbols_raw))
    return NULL;
  size_t errors  = (size_t)errors_raw;
  size_t symbols = (size_t)symbols_raw;
  if (!BerMeterObj_interval_type)
    {
      BerMeterObj_interval_type
          = PyStructSequence_NewType (&BerMeterObj_interval_desc);
      if (!BerMeterObj_interval_type)
        {
          return NULL;
        }
    }
  ber_interval_t _r = ber_meter_interval (self->handle, errors, symbols);
  PyObject      *_o = PyStructSequence_New (BerMeterObj_interval_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.p_hat));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.lo));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.hi));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.rel));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.conf));
  PyStructSequence_SET_ITEM (
      _o, 5, PyLong_FromUnsignedLongLong ((unsigned long long)_r.errors));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLongLong ((unsigned long long)_r.symbols));
  return _o;
}

static PyObject *
BerMeterObj_state_bytes (BerMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (ber_meter_state_bytes (self->handle));
}

static PyObject *
BerMeterObj_get_state (BerMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = ber_meter_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  ber_meter_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
BerMeterObj_set_state (BerMeterObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != ber_meter_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (ber_meter_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
BerMeter_getprop_errors (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_errors (self->handle));
}
static PyObject *
BerMeter_getprop_symbols (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_symbols (self->handle));
}
static PyObject *
BerMeter_getprop_bit_errors (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_bit_errors (self->handle));
}
static PyObject *
BerMeter_getprop_bits (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_bits (self->handle));
}
static PyObject *
BerMeter_getprop_skipped (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_skipped (self->handle));
}
static PyObject *
BerMeter_getprop_m (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)ber_meter_get_m (self->handle));
}
static PyObject *
BerMeter_getprop_target_errors (BerMeterObject *self,
                                void           *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_target_errors (self->handle));
}
static PyObject *
BerMeter_getprop_conf (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ber_meter_get_conf (self->handle));
}
static PyObject *
BerMeter_getprop_enough (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)ber_meter_get_enough (self->handle));
}
static PyObject *
BerMeter_getprop_lag (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)ber_meter_get_lag (self->handle));
}
static PyObject *
BerMeter_getprop_phase (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ber_meter_get_phase (self->handle));
}
static PyObject *
BerMeter_getprop_align_stat (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ber_meter_get_align_stat (self->handle));
}
static PyObject *
BerMeter_getprop_align_margin_db (BerMeterObject *self,
                                  void           *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ber_meter_get_align_margin_db (self->handle));
}
static PyObject *
BerMeter_getprop_align_runner_db (BerMeterObject *self,
                                  void           *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ber_meter_get_align_runner_db (self->handle));
}
static PyObject *
BerMeter_getprop_align_occurrences (BerMeterObject *self,
                                    void           *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_align_occurrences (self->handle));
}
static PyObject *
BerMeter_getprop_align_slips (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)ber_meter_get_align_slips (self->handle));
}
static PyObject *
BerMeter_getprop_align_saturated (BerMeterObject *self,
                                  void           *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)ber_meter_get_align_saturated (self->handle));
}
static PyObject *
BerMeter_getprop_align_ok (BerMeterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)ber_meter_get_align_ok (self->handle));
}

static PyGetSetDef BerMeter_getset[] = {
  { "errors", (getter)BerMeter_getprop_errors, NULL,
    "Symbol errors counted so far.\n", NULL },
  { "symbols", (getter)BerMeter_getprop_symbols, NULL,
    "Symbols scored so far.\n", NULL },
  { "bit_errors", (getter)BerMeter_getprop_bit_errors, NULL,
    "Gray-coded bit errors counted so far.\n", NULL },
  { "bits", (getter)BerMeter_getprop_bits, NULL, "Bits scored so far.\n",
    NULL },
  { "skipped", (getter)BerMeter_getprop_skipped, NULL,
    "Symbols skipped: covered by a marker occurrence, or with a truth index "
    "outside the installed sequence. Marker symbols are excluded on purpose "
    "-- they are known, so scoring them would flatter the rate with symbols "
    "that had no chance of being wrong.\n",
    NULL },
  { "m", (getter)BerMeter_getprop_m, NULL, "Constellation order.\n", NULL },
  { "target_errors", (getter)BerMeter_getprop_target_errors, NULL,
    "The inverse-binomial stop condition.\n", NULL },
  { "conf", (getter)BerMeter_getprop_conf, NULL,
    "Two-sided confidence level used by ser() and ber().\n", NULL },
  { "enough", (getter)BerMeter_getprop_enough, NULL,
    "True once target_errors have been counted. THE stop condition for a "
    "measurement loop: fix the ERROR count and let the symbol count fall out "
    "(inverse binomial sampling), so the relative standard error is 1/sqrt(r) "
    "-- a function of the error count alone. Stopping on a fixed symbol count "
    "instead makes the precision depend on the very rate being measured: "
    "20000 symbols at SER 1e-3 gives ~20 errors and ~22% relative error, "
    "which reads as real seed-to-seed variation in the receiver and is not.\n",
    NULL },
  { "lag", (getter)BerMeter_getprop_lag, NULL,
    "Alignment from the last align(): rx[i] carries truth[i + lag].\n", NULL },
  { "phase", (getter)BerMeter_getprop_phase, NULL,
    "Absolute residual constellation rotation from the last align(), radians. "
    "The marker resolves this outright, so there is no M-fold ambiguity left "
    "to search over.\n",
    NULL },
  { "align_stat", (getter)BerMeter_getprop_align_stat, NULL,
    "Detection statistic at the correlation peak, in threshold units.\n",
    NULL },
  { "align_margin_db", (getter)BerMeter_getprop_align_margin_db, NULL,
    "20*log10(stat/threshold) -- headroom over the false-alarm gate. Negative "
    "means the alignment was NOT detected.\n",
    NULL },
  { "align_runner_db", (getter)BerMeter_getprop_align_runner_db, NULL,
    "Peak over the best runner-up lag, dB. Under ~3 dB the peak is ambiguous "
    "and the alignment is rejected.\n",
    NULL },
  { "align_occurrences", (getter)BerMeter_getprop_align_occurrences, NULL,
    "Marker occurrences combined non-coherently.\n", NULL },
  { "align_slips", (getter)BerMeter_getprop_align_slips, NULL,
    "Occurrences whose phase disagreed with the peak by more than half a "
    "decision sector -- cycle slips.\n",
    NULL },
  { "align_saturated", (getter)BerMeter_getprop_align_saturated, NULL,
    "True when the peak sat on an edge of the lag search: lag_span is too "
    "small and the result is not trustworthy.\n",
    NULL },
  { "align_ok", (getter)BerMeter_getprop_align_ok, NULL,
    "True when the last align() was detected, unambiguous and unsaturated. "
    "score() is meaningless unless this is True.\n",
    NULL },
  { NULL }
};

static PyObject *
BerMeterObj_destroy (BerMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ber_meter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BerMeterObj_enter (BerMeterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
BerMeterObj_exit (BerMeterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      ber_meter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef BerMeterObj_methods[] = {
  { "reset", (PyCFunction)BerMeterObj_reset, METH_NOARGS,
    "Zero the running counters; keep the configuration and the truth.\n"
    "\n"
    "Returns the meter to a fresh count while preserving m, the error\n"
    "target, the confidence level and the installed truth sequence, so one\n"
    "meter can measure independent captures back to back without\n"
    "reinstalling truth. The last detected alignment is left untouched; call\n"
    "align() again for the next capture before scoring it.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ber import BerMeter\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> truth = rng.integers(0, 4, size=400).astype(np.uint8)\n"
    ">>> ang = 2 * np.pi * truth / 4 + np.pi / 4\n"
    ">>> rx = np.exp(1j * ang).astype(np.complex64)\n"
    ">>> met = BerMeter(m=4)\n"
    ">>> met.set_truth(truth)\n"
    ">>> met.align(rx, n_marker=64)\n"
    "1\n"
    ">>> met.score(rx, hi=truth.size)\n"
    "336\n"
    ">>> met.symbols\n"
    "336\n"
    ">>> met.reset()               # reuse the meter for the next capture\n"
    ">>> (met.errors, met.symbols)\n"
    "(0, 0)\n" },

  { "set_truth", (PyCFunction)(void *)BerMeterObj_set_truth,
    METH_VARARGS | METH_KEYWORDS,
    "set_truth(truth) -> int\n"
    "\n"
    "Install the transmitted symbol INDICES (0..m-1, not Gray labels)\n"
    "this meter scores against. Copied, so the caller's buffer need not\n"
    "outlive the call, and reused across every burst. Raises ValueError if\n"
    "any index is outside 0..m-1.\n"
    "\n"
    "Copied, so the caller's buffer need not outlive the call, and reused\n"
    "across every burst. Values are symbol INDICES in `0..m-1` (not Gray\n"
    "labels): the meter Gray-encodes each side itself when it counts bit\n"
    "errors, so handing it Gray labels would double-encode and inflate the\n"
    "rate.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "truth : NDArray[np.uint8]\n"
    "    Transmitted symbol indices, each in `0..m-1`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ber import BerMeter\n"
    ">>> met = BerMeter(m=4)\n"
    ">>> truth = np.array(\n"
    "...     [0, 3, 1, 2, 2, 0], dtype=np.uint8)  # indices, 0..3\n"
    ">>> met.set_truth(truth)\n"
    ">>> met.set_truth(np.array([9], dtype=np.uint8))  # 9 is not in 0..3\n"
    "Traceback (most recent call last):\n"
    "ValueError: set_truth failed (rc=-4)\n" },
  { "align", (PyCFunction)(void *)BerMeterObj_align,
    METH_VARARGS | METH_KEYWORDS,
    "align(rx, t0, n_marker, period, lag_span, pfa) -> int\n"
    "\n"
    "Detect where the recovered symbols sit against truth, returning a\n"
    "BerAlign. The alignment is DETECTED by correlating a known marker --\n"
    "truth[t0 : t0+n_marker], optionally repeating every `period` symbols --\n"
    "and gated by a false-alarm probability, NOT searched by minimising the\n"
    "error count. That distinction is the whole point: a min-over-(lag,\n"
    "rotation) search is an optimisation over the answer, and it both\n"
    "false-passes on a lucky alignment and false-floors when the true lag\n"
    "falls outside the span. A marker too short to identify an alignment\n"
    "returns ok=False rather than a plausible wrong lag. Repeats are\n"
    "combined non-coherently, which raises the processing gain and exposes\n"
    "cycle slips.\n"
    "\n"
    "Correlates the known marker `truth[t0 .. t0 + n_marker)` against rx\n"
    "over a span of lags, gates the peak with a false-alarm probability, and\n"
    "stores the winning lag, absolute carrier phase and marker geometry on\n"
    "the meter so score() later uses exactly this detection — never a lag\n"
    "searched to minimise the error count. The peak's phase is the ABSOLUTE\n"
    "constellation rotation, so no M-fold ambiguity is left to resolve; a\n"
    "marker too short to clear the gate reports failure rather than a\n"
    "plausible wrong lag. Read the outcome through align_ok, lag, phase and\n"
    "align_margin_db.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rx : NDArray[np.complex64]\n"
    "    Recovered symbols to align against the truth.\n"
    "t0 : int\n"
    "    Truth index of the marker's first occurrence.\n"
    "n_marker : int\n"
    "    Marker length in symbols; 0 selects BER_SYNC_SYMS.\n"
    "period : int\n"
    "    Repeat period in symbols; 0 for a single occurrence.\n"
    "lag_span : int\n"
    "    Search half-width in symbols; 0 selects BER_LAG_SPAN.\n"
    "pfa : float\n"
    "    Whole-search false-alarm probability; 0 selects 1e-6.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    1 when the detection passed its false-alarm gate, else 0.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ber import BerMeter\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> truth = rng.integers(0, 4, size=600).astype(np.uint8)\n"
    ">>> ang = 2 * np.pi * truth / 4 + np.pi / 4\n"
    ">>> rx = np.exp(1j * ang).astype(np.complex64)\n"
    ">>> met = BerMeter(m=4)\n"
    ">>> met.set_truth(truth)\n"
    ">>> met.align(rx, n_marker=64)     # correlate a 64-symbol marker\n"
    "1\n"
    ">>> met.lag, met.align_ok          # detected, so score() is valid\n"
    "(0, 1)\n" },
  { "score", (PyCFunction)(void *)BerMeterObj_score,
    METH_VARARGS | METH_KEYWORDS,
    "score(rx, lo, hi) -> int\n"
    "\n"
    "Score rx[lo:hi] against the truth and accumulate; returns the\n"
    "symbols scored. Uses the supplied alignment VERBATIM -- no lag search,\n"
    "no rotation search, no minimisation of any kind over the answer. Uses\n"
    "the alignment align() last detected, together with the marker geometry\n"
    "that found it, so a measurement cannot be handed an alignment belonging\n"
    "to a different burst. Symbols covered by a marker occurrence are\n"
    "excluded, as are any whose truth index falls outside the installed\n"
    "sequence; both land in `skipped`.\n"
    "\n"
    "Demodulates each symbol in the window under the alignment the last\n"
    "align() detected — its lag and absolute phase — and tallies symbol and\n"
    "Gray-coded bit errors against the installed truth. The alignment is\n"
    "used VERBATIM: no lag search, no rotation search, no minimisation of\n"
    "any kind over the answer. Symbols covered by a marker occurrence are\n"
    "excluded, as are any whose truth index falls outside the installed\n"
    "sequence; both land in skipped.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rx : NDArray[np.complex64]\n"
    "    Recovered symbols to score.\n"
    "lo : int\n"
    "    First symbol index to score (inclusive).\n"
    "hi : int\n"
    "    One past the last symbol index to score; clamped to rx_len. `hi =\n"
    "    0` scores nothing, so pass the window's true end.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Symbols actually scored (window length minus skipped symbols).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ber import BerMeter\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> truth = rng.integers(0, 4, size=800).astype(np.uint8)\n"
    ">>> ang = 2 * np.pi * truth / 4 + np.pi / 4\n"
    ">>> rx = np.exp(1j * ang).astype(np.complex64)\n"
    ">>> met = BerMeter(m=4)\n"
    ">>> met.set_truth(truth)\n"
    ">>> met.align(rx, n_marker=64)\n"
    "1\n"
    ">>> met.score(rx, hi=truth.size)   # the 64 marker symbols are excluded\n"
    "736\n"
    ">>> met.errors, met.skipped\n"
    "(0, 64)\n" },
  { "ser", (PyCFunction)BerMeterObj_ser, METH_VARARGS,
    "ser() -> BerInterval record (p_hat, lo, hi, rel, conf, errors, "
    "symbols)." },
  { "ber", (PyCFunction)BerMeterObj_ber, METH_VARARGS,
    "ber() -> BerInterval record (p_hat, lo, hi, rel, conf, errors, "
    "symbols)." },
  { "interval", (PyCFunction)(void *)BerMeterObj_interval,
    METH_VARARGS | METH_KEYWORDS,
    "interval(errors, symbols) -> BerInterval record (p_hat, lo, hi, rel, "
    "conf, errors, symbols)." },
  { "state_bytes", (PyCFunction)BerMeterObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the BerMeterObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)BerMeterObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the BerMeterObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)BerMeterObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the BerMeterObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)BerMeterObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)BerMeterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a BerMeter be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BerMeter\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)BerMeterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the BerMeter.\n"
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

static PyTypeObject BerMeterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ber.BerMeter",
  .tp_basicsize                           = sizeof (BerMeterObject),
  .tp_dealloc                             = (destructor)BerMeterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Create a meter for constellation m stopping at target_errors.\n"
            "\n"
            "Parameters\n"
            "----------\n"
            "m : int, default 4\n"
            "    Constellation order (2, 4, 8).\n"
            "target_errors : int, default 200\n"
            "    Inverse-binomial stop condition; 0 selects 200.\n"
            "conf : float, default 0.99\n"
            "    Two-sided confidence level; 0 selects 0.99.\n"
            "\n"
            "Examples\n"
            "--------\n"
            "Create with defaults:\n"
            "\n"
            ">>> from doppler import BerMeter\n"
            ">>> obj = BerMeter(m=4, target_errors=200, conf=0.99)\n",
  .tp_methods = BerMeterObj_methods,
  .tp_getset  = BerMeter_getset,
  .tp_new     = BerMeterObj_new,
  .tp_init    = (initproc)BerMeterObj_init,
};
