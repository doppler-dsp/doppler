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
  int            y = ber_meter_set_truth (self->handle, truth, truth_len);
  Py_DECREF (truth_arr);
  return PyLong_FromLong ((long)y);
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
  { "p_hat", NULL }, { "lo", NULL },     { "hi", NULL },      { "rel", NULL },
  { "conf", NULL },  { "errors", NULL }, { "symbols", NULL }, { NULL, NULL },
};
static PyStructSequence_Desc BerMeterObj_ser_desc
    = { "doppler.ber.BerInterval", NULL, BerMeterObj_ser_fields, 7 };
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
  { "p_hat", NULL }, { "lo", NULL },     { "hi", NULL },      { "rel", NULL },
  { "conf", NULL },  { "errors", NULL }, { "symbols", NULL }, { NULL, NULL },
};
static PyStructSequence_Desc BerMeterObj_ber_desc
    = { "doppler.ber.BerInterval", NULL, BerMeterObj_ber_fields, 7 };
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
  { "p_hat", NULL }, { "lo", NULL },     { "hi", NULL },      { "rel", NULL },
  { "conf", NULL },  { "errors", NULL }, { "symbols", NULL }, { NULL, NULL },
};
static PyStructSequence_Desc BerMeterObj_interval_desc
    = { "doppler.ber.BerInterval", NULL, BerMeterObj_interval_fields, 7 };
static PyTypeObject *BerMeterObj_interval_type = NULL;

static PyObject *
BerMeterObj_interval (BerMeterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t       errors    = 0;
  size_t       symbols   = 0;
  static char *_kwlist[] = { "errors", "symbols", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KK", _kwlist, &errors,
                                    &symbols))
    return NULL;
  if (!BerMeterObj_interval_type)
    {
      BerMeterObj_interval_type
          = PyStructSequence_NewType (&BerMeterObj_interval_desc);
      if (!BerMeterObj_interval_type)
        return NULL;
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
    "Reset state to post-create defaults." },

  { "set_truth", (PyCFunction)(void *)BerMeterObj_set_truth,
    METH_VARARGS | METH_KEYWORDS,
    "set_truth(truth) -> int\n"
    "\n"
    "Install the transmitted symbol INDICES (0..m-1, not Gray labels) this "
    "meter scores against. Copied, so the caller's buffer need not outlive "
    "the call, and reused across every burst. Raises ValueError if any index "
    "is outside 0..m-1.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import BerMeter\n"
    "    >>> obj = BerMeter(4, 200, 0.99)\n"
    "    >>> obj.set_truth(np.zeros(4, dtype=np.uint8))\n"
    "    0\n" },
  { "align", (PyCFunction)(void *)BerMeterObj_align,
    METH_VARARGS | METH_KEYWORDS,
    "align(rx, t0, n_marker, period, lag_span, pfa) -> int\n"
    "\n"
    "Detect where the recovered symbols sit against truth, returning a "
    "BerAlign. The alignment is DETECTED by correlating a known marker -- "
    "truth[t0 : t0+n_marker], optionally repeating every `period` symbols -- "
    "and gated by a false-alarm probability, NOT searched by minimising the "
    "error count. That distinction is the whole point: a min-over-(lag, "
    "rotation) search is an optimisation over the answer, and it both "
    "false-passes on a lucky alignment and false-floors when the true lag "
    "falls outside the span. A marker too short to identify an alignment "
    "returns ok=False rather than a plausible wrong lag. Repeats are combined "
    "non-coherently, which raises the processing gain and exposes cycle "
    "slips.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import BerMeter\n"
    "    >>> obj = BerMeter(4, 200, 0.99)\n"
    "    >>> obj.align(np.zeros(4, dtype=np.complex64), 0, 0, 0, 0, 0.0)\n"
    "    0\n" },
  { "score", (PyCFunction)(void *)BerMeterObj_score,
    METH_VARARGS | METH_KEYWORDS,
    "score(rx, lo, hi) -> int\n"
    "\n"
    "Score rx[lo:hi] against the truth and accumulate; returns the symbols "
    "scored. Uses the supplied alignment VERBATIM -- no lag search, no "
    "rotation search, no minimisation of any kind over the answer. Uses the "
    "alignment align() last detected, together with the marker geometry that "
    "found it, so a measurement cannot be handed an alignment belonging to a "
    "different burst. Symbols covered by a marker occurrence are excluded, as "
    "are any whose truth index falls outside the installed sequence; both "
    "land in `skipped`.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import BerMeter\n"
    "    >>> obj = BerMeter(4, 200, 0.99)\n"
    "    >>> obj.score(np.zeros(4, dtype=np.complex64), 0, 0)\n"
    "    0\n" },
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
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)BerMeterObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)BerMeterObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)BerMeterObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)BerMeterObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)BerMeterObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject BerMeterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ber.BerMeter",
  .tp_basicsize                           = sizeof (BerMeterObject),
  .tp_dealloc                             = (destructor)BerMeterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Create a meter for constellation m stopping at target_errors.\n",
  .tp_methods = BerMeterObj_methods,
  .tp_getset  = BerMeter_getset,
  .tp_new     = BerMeterObj_new,
  .tp_init    = (initproc)BerMeterObj_init,
};
