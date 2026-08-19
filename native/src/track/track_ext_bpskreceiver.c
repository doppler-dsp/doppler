/*
 * track_ext_bpskreceiver.c — BpskReceiver type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* BpskReceiverObject — wraps mpsk_receiver_state_t *       */
/* ======================================================== */

#include "mpsk_receiver/mpsk_receiver_core.h"

typedef struct
{
  PyObject_HEAD mpsk_receiver_state_t *handle;
} BpskReceiverObject;

static void
BpskReceiverObj_dealloc (BpskReceiverObject *self)
{
  if (self->handle)
    mpsk_receiver_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
BpskReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  BpskReceiverObject *self = (BpskReceiverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
BpskReceiverObj_init (BpskReceiverObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]        = { "sample_rate_hz",
                                   "symbol_rate_hz",
                                   "carrier_freq_hz",
                                   "pulse",
                                   "rrc_beta",
                                   "rrc_span",
                                   "bn_carrier",
                                   "bn_timing",
                                   "differential",
                                   "agc",
                                   NULL };
  double       sample_rate_hz  = 0.0;
  double       symbol_rate_hz  = 0.0;
  double       carrier_freq_hz = 0.0;
  const char  *pulse_str       = "iandd";
  double       rrc_beta        = 0.35;
  int          rrc_span        = 8;
  double       bn_carrier      = 0.01;
  double       bn_timing       = 0.01;
  int          differential    = 0;
  int          agc             = 1;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "dd|dsdiddii", kwlist, &sample_rate_hz, &symbol_rate_hz,
          &carrier_freq_hz, &pulse_str, &rrc_beta, &rrc_span, &bn_carrier,
          &bn_timing, &differential, &agc))
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
  self->handle = mpsk_receiver_create_bpsk (
      sample_rate_hz, symbol_rate_hz, carrier_freq_hz, pulse, rrc_beta,
      rrc_span, bn_carrier, bn_timing, differential, agc);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "MpskReceiver: invalid parameter (need m in {2,4,8}, "
                       "sps >= m_out -- sps > 2*m_out on the real-input "
                       "MpskReceiverR, whose cascade runs behind a 2:1 "
                       "halfband, m_out even in [2, 8], 0 <= rrc_beta <= 1, "
                       "rrc_span >= 1, num_phases a power of two >= 2, bn >= "
                       "0, zeta > 0, 0 < bn_agc_ratio < 1)");
      return -1;
    }
  return 0;
}

static PyObject *
BpskReceiverObj_set_telemetry (BpskReceiverObject *self, PyObject *args,
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
  int _rc = mpsk_receiver_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_telemetry failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BpskReceiverObj_steps_max_out (BpskReceiverObject *self,
                               PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (mpsk_receiver_steps_max_out (self->handle));
}

static PyObject *
BpskReceiverObj_steps (BpskReceiverObject *self, PyObject *args,
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
      size_t _omax    = mpsk_receiver_steps_max_out (self->handle);
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
        n_out = mpsk_receiver_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = mpsk_receiver_steps_max_out (self->handle);
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
    n_out = mpsk_receiver_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
BpskReceiverObj_bits_max_out (BpskReceiverObject *self,
                              PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (mpsk_receiver_bits_max_out (self->handle));
}

static PyObject *
BpskReceiverObj_bits (BpskReceiverObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
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
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = mpsk_receiver_bits_max_out (self->handle);
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
      uint8_t             *_ng2 = (uint8_t *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = mpsk_receiver_bits (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT8,
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
  size_t _cap  = mpsk_receiver_bits_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  uint8_t *_d0 = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = mpsk_receiver_bits (self->handle, _ng0, _ng1, _d0, _cap);
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
BpskReceiverObj_reset (BpskReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  mpsk_receiver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
BpskReceiverObj_state_bytes (BpskReceiverObject *self,
                             PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (mpsk_receiver_state_bytes (self->handle));
}

static PyObject *
BpskReceiverObj_get_state (BpskReceiverObject *self,
                           PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = mpsk_receiver_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  mpsk_receiver_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
BpskReceiverObj_set_state (BpskReceiverObject *self, PyObject *arg)
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
      != mpsk_receiver_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (mpsk_receiver_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
BpskReceiver_getprop_agc_gain_db (BpskReceiverObject *self,
                                  void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_agc_gain_db (self->handle));
}
static PyObject *
BpskReceiver_getprop_norm_freq (BpskReceiverObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_norm_freq (self->handle));
}
static int
BpskReceiver_setprop_norm_freq (BpskReceiverObject *self, PyObject *value,
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
  mpsk_receiver_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
BpskReceiver_getprop_lock (BpskReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_lock (self->handle));
}
static PyObject *
BpskReceiver_getprop_zeta (BpskReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_zeta (self->handle));
}
static PyObject *
BpskReceiver_getprop_num_phases (BpskReceiverObject *self,
                                 void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)mpsk_receiver_get_num_phases (self->handle));
}
static PyObject *
BpskReceiver_getprop_lock_thresh (BpskReceiverObject *self,
                                  void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_lock_thresh (self->handle));
}
static PyObject *
BpskReceiver_getprop_lock_drop_thresh (BpskReceiverObject *self,
                                       void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      mpsk_receiver_get_lock_drop_thresh (self->handle));
}
static PyObject *
BpskReceiver_getprop_sync_lock_thresh (BpskReceiverObject *self,
                                       void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      mpsk_receiver_get_sync_lock_thresh (self->handle));
}
static PyObject *
BpskReceiver_getprop_sync_lock_drop_thresh (BpskReceiverObject *self,
                                            void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      mpsk_receiver_get_sync_lock_drop_thresh (self->handle));
}
static PyObject *
BpskReceiver_getprop_bn_agc_ratio (BpskReceiverObject *self,
                                   void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_bn_agc_ratio (self->handle));
}
static PyObject *
BpskReceiver_getprop_lock_time (BpskReceiverObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLongLong (
      (long long)mpsk_receiver_get_lock_time (self->handle));
}
static PyObject *
BpskReceiver_getprop_timing_rate (BpskReceiverObject *self,
                                  void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_timing_rate (self->handle));
}
static PyObject *
BpskReceiver_getprop_m (BpskReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_get_m (self->handle));
}
static PyObject *
BpskReceiver_getprop_sps (BpskReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_get_sps (self->handle));
}
static PyObject *
BpskReceiver_getprop_m_out (BpskReceiverObject *self,
                            void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)mpsk_receiver_get_m_out (self->handle));
}
static PyObject *
BpskReceiver_getprop_clipped (BpskReceiverObject *self,
                              void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_get_clipped (self->handle));
}

static PyObject *
BpskReceiver_getprop_locked (BpskReceiverObject *self,
                             void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_get_locked (self->handle));
}

static PyGetSetDef BpskReceiver_getset[] = {
  { "agc_gain_db", (getter)BpskReceiver_getprop_agc_gain_db, NULL,
    "Gain the front-end AGC is applying, in dB; 0.0 when `agc=0`. The "
    "diagnostic for a level problem: a receiver that will not lock with a "
    "healthy `lock` statistic, or one whose timing loop behaves differently "
    "at two input levels, is asking about this number. It settles at "
    "-10*log10(P_in / P_ref), where P_ref is the power a unit-amplitude "
    "symbol stream has where the AGC sits, so a reading far from 0 dB says "
    "the input is far from the level the cascade was built for -- which is "
    "fine, and is what the AGC is for, but is worth knowing. Distinct from "
    "the cascade's filter response, which stays unity; the two multiply.\n",
    NULL },
  { "norm_freq", (getter)BpskReceiver_getprop_norm_freq,
    (setter)BpskReceiver_setprop_norm_freq,
    "Carrier frequency the receiver is tracking, cycles/sample at the input "
    "rate: the create-time centre plus the loop's own estimate.\n",
    NULL },
  { "lock", (getter)BpskReceiver_getprop_lock, NULL,
    "EMA of the carrier lock signal.\n", NULL },
  { "zeta", (getter)BpskReceiver_getprop_zeta, NULL,
    "Loop damping actually in use. Reads back the DERIVED `1/sqrt(2)` when "
    "the constructor was given 0, or whatever was pinned instead. Everything "
    "derived is reported (docs/design/mpsk.md §8.1), on the same argument as "
    "`RateConverter.stages`: without the readback, passing 0 is an "
    "instruction whose result nobody can see.\n",
    NULL },
  { "num_phases", (getter)BpskReceiver_getprop_num_phases, NULL,
    "Matched-filter bank arms actually in use. Reads back the DERIVED 64 -- "
    "the measured saturation point, against the 1024 that shipped -- when the "
    "constructor was given 0. See `zeta` for why every derived value is "
    "reported.\n",
    NULL },
  { "lock_thresh", (getter)BpskReceiver_getprop_lock_thresh, NULL,
    "Handover declare threshold actually in use. Reads back the DERIVED "
    "`sigma_H0 * eta(Pfa)` = 0.4999 at `Pfa = 5e-6` when the constructor was "
    "given 0. See `zeta` for why every derived value is reported.\n",
    NULL },
  { "lock_drop_thresh", (getter)BpskReceiver_getprop_lock_drop_thresh, NULL,
    "Carrier DROP threshold actually in use -- 0.8x `lock_thresh`, the level "
    "hysteresis the declare/drop pair is stated with. Exposed for the same "
    "reason as the declare side: anything reading `lock` against its decision "
    "needs BOTH edges, and computing `0.8 *` at the call site is a second "
    "copy of a rule this object owns. Both carrier detectors are initialised "
    "from this pair, so one number describes them both.\n",
    NULL },
  { "sync_lock_thresh", (getter)BpskReceiver_getprop_sync_lock_thresh, NULL,
    "Timing DECLARE threshold on the `sync.lock` statistic. Not the carrier's "
    "number and not derived the same way: symsync sizes its block length and "
    "threshold together from (rolloff, esno_min, pfa, pd), so this reads back "
    "that geometry's answer. A caller plotting `sync.lock` needs this rather "
    "than `lock_thresh`, which belongs to a different statistic on a "
    "different clock.\n",
    NULL },
  { "sync_lock_drop_thresh",
    (getter)BpskReceiver_getprop_sync_lock_drop_thresh, NULL,
    "Timing DROP threshold on `sync.lock`. Equal to `sync_lock_thresh` when "
    "the timing loop carries no level hysteresis (up = down = threshold, the "
    "symsync default), so the two reading the same is information, not a bug "
    "-- the timing decision's hysteresis is in its verify COUNTS rather than "
    "its levels.\n",
    NULL },
  { "bn_agc_ratio", (getter)BpskReceiver_getprop_bn_agc_ratio, NULL,
    "AGC bandwidth as a fraction of the slowest loop it feeds, actually in "
    "use. Reads back the DERIVED 0.05 when the constructor was given 0. See "
    "`zeta` for why every derived value is reported.\n",
    NULL },
  { "lock_time", (getter)BpskReceiver_getprop_lock_time, NULL,
    "Symbols from reset to the FIRST carrier-lock declaration, or -1 if the "
    "receiver has not locked yet -- the acquisition time as a number, rather "
    "than something a caller has to infer by polling `locked` in a loop. "
    "Dated by the same hysteretic detector `locked` reports, so the two "
    "cannot disagree. In SYMBOLS, not seconds: `bn_carrier` and `bn_timing` "
    "are both normalised to the symbol rate, so a settling budget quoted in "
    "symbols is comparable across every input rate, and a caller holding Rs "
    "divides once. Only the FIRST declaration is dated -- a drop and "
    "re-acquire does not restamp it, because the question this answers is "
    "'how long did this receiver take to lock', not 'when did it last hold'. "
    "`reset()` clears it to -1.\n",
    NULL },
  { "timing_rate", (getter)BpskReceiver_getprop_timing_rate, NULL,
    "Smoothed tracked samples per symbol — departs from the nominal `sps` by "
    "exactly the sample-clock offset the timing loop is tracking.\n",
    NULL },
  { "m", (getter)BpskReceiver_getprop_m, NULL,
    "constellation order M (2, 4, 8).\n", NULL },
  { "sps", (getter)BpskReceiver_getprop_sps, NULL,
    "samples per symbol at the receiver's input.\n", NULL },
  { "m_out", (getter)BpskReceiver_getprop_m_out, NULL,
    "Terminal outputs per symbol (the old `n`, now the cascade's).\n", NULL },
  { "clipped", (getter)BpskReceiver_getprop_clipped, NULL,
    "Has the cascade's CIC stage clipped its input since the last reset? A "
    "CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past that "
    "-- the output stays finite and plausible, merely distorted, at a cost of "
    "~25 dB of EVM that no lock metric reveals. Always 0 for a plan with no "
    "CIC stage.\n",
    NULL },
  { "locked", (getter)BpskReceiver_getprop_locked, NULL,
    "Binary carrier-lock flag from the hysteretic (verify-counted) detector "
    "on `lock` -- de-chattered, unlike the raw metric. It declares after 8 "
    "consecutive symbols above `lock_thresh` and withdraws after 32 below "
    "`lock_drop_thresh`, so it answers 'is this receiver locked' rather than "
    "'was the statistic above the line on this symbol'. **It is an INDICATOR "
    "and nothing else**: it steers no loop, gates no output, and the "
    "M-th-power NDA discriminator runs from the first strobe whether or not "
    "this has declared. So a caller uses it to size a measurement window, and "
    "a wrong reading costs them that window and costs the demodulator "
    "nothing. `lock_time` dates its first declaration.\n",
    NULL },
  { NULL }
};

static PyObject *
BpskReceiverObj_destroy (BpskReceiverObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      mpsk_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BpskReceiverObj_enter (BpskReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
BpskReceiverObj_exit (BpskReceiverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      mpsk_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef BpskReceiverObj_methods[] = {

  { "set_telemetry", (PyCFunction)(void *)BpskReceiverObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context across the receiver.\n"
    "Registers the receiver's own \"<prefix>.lock\" probe (the carrier lock\n"
    "EMA), then the carrier loop's \"<prefix>.car.e\" / \".freq\" / "
    "\".locked\"\n"
    "and the symbol-timing loop's \"<prefix>.sync.e\" / \".ctrl\" / \".rate\" "
    "/\n"
    "\".lock\" / \".locked\" / \".mu\" -- ten probes emitted once per "
    "recovered\n"
    "symbol -- then the front end's AGC under \"<prefix>.agc\"\n"
    "(\"<prefix>.agc.gain_db\" and \"<prefix>.agc.level_db\"; see\n"
    "agc_set_telemetry()). Twelve probes total, all thinned by decim.\n"
    "Passing NULL detaches everything.\n"
    "\n"
    "Instrumenting it matters because it is FIRST in the chain, and a level\n"
    "error is the one kind no downstream loop can correct for itself: a TED\n"
    "normalises by its own construct-time slope, so it reads a level error\n"
    "as a loop-gain error (A^2 Gardner, A DTTL) with no other reference to\n"
    "catch it. This receiver also makes the AGC the slowest of its three\n"
    "loops by construction -- mpsk_rx_agc_bn() derives its bandwidth as a\n"
    "fraction of the slowest loop it feeds, and bn_agc_ratio is validated to\n"
    "(0, 1) -- but that is a choice of THIS composition, and slowest does\n"
    "not by itself mean longest: settling is set by the bandwidth AND by how\n"
    "far the level starts from the reference, which is unknown at\n"
    "construction. Which is exactly why it has to be measured rather than\n"
    "inferred; the zero-referenced \"<prefix>.agc.level_db\" is what makes\n"
    "that possible.\n"
    "\n"
    "With agc = 0 at construction there is no AGC to attach and the two\n"
    "probes are simply absent (fourteen, not sixteen); this still returns\n"
    "DP_OK.\n"
    "\n"
    "Setup path, never hot; the context is borrowed and must outlive the\n"
    "attachment (SPSC rules in dp_tlm/dp_tlm_core.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"rx\".\n"
    "decim : int\n"
    "    Emit every decim-th symbol (every decim-th gain update for the two\n"
    "    AGC probes); >= 1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_telemetry failed``, with the return code appended (gh-869).\n"
    "\n"
    "Warnings\n"
    "--------\n"
    "The two AGC probes are NOT at the symbol rate the other ten are. That\n"
    "AGC sits pre-terminal in the cascade (RateConverter's tap, ahead of the\n"
    "stage the timing loop steers) and emits once per gain-update event,\n"
    "i.e. every AGC_DECIM_DEFAULT samples of that fixed-rate stream -- so it\n"
    "reports on a grid that depends on the planned cascade, not on recovered\n"
    "symbols, and a run yields a different number of AGC records than\n"
    "carrier records. Compare the two by TIME, never by record index. This\n"
    "is deliberate: the AGC's bandwidth is quoted in the pre-terminal\n"
    "stream's units precisely so it is not coupled to the loop that is\n"
    "stretching the symbol grid (see RateConverter_enable_agc()).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiver\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 14)   # 15 probes x ~512 syms + headroom\n"
    ">>> rx = MpskReceiver(m=4, sps=4, m_out=2)\n"
    ">>> rx.set_telemetry(tlm, \"rx\")\n"
    ">>> len(tlm.probe_names)\n"
    "15\n"
    ">>> rng = np.random.default_rng(7)\n"
    ">>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)\n"
    ">>> x = np.repeat(syms, 4)\n"
    ">>> _ = rx.steps(x)\n"
    ">>> recs = tlm.read()\n"
    ">>> tlm.dropped        # size the ring, or the counts below diverge\n"
    "0\n"
    ">>> n_sync = len(recs[recs[\"probe\"] == tlm.probe_id(\"rx.sync.e\")])\n"
    ">>> n_car = len(recs[recs[\"probe\"] == tlm.probe_id(\"rx.car.e\")])\n"
    ">>> n_sync > 0 and n_sync == n_car\n"
    "True\n"
    ">>> n_agc = len(recs[recs[\"probe\"] == "
    "tlm.probe_id(\"rx.agc.gain_db\")])\n"
    ">>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid\n"
    "True\n" },
  { "steps", (PyCFunction)(void *)BpskReceiverObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Demodulate a cf32 block and return the recovered M-PSK symbols (one\n"
    "cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the\n"
    "receiver pushes x through the matched DDC -- LO mix, decimating\n"
    "cascade, and a terminal polyphase stage whose bank IS the matched\n"
    "filter and whose selected arm IS the fractional symbol-timing delay --\n"
    "then folds every output that stage produced into two loops: a Gardner\n"
    "symbol-timing loop steering the cascade's rate_ctrl port, and a carrier\n"
    "loop steering the LO's freq_ctrl port. The carrier discriminator runs\n"
    "on the on-time strobe only -- a non-strobe output straddles two\n"
    "symbols, so its M-th power is intersymbol interference rather than\n"
    "carrier phase -- and it is the non-data-aided M-th-power error\n"
    "throughout, needing no data and no symbol timing: there is one\n"
    "discriminator, running from the first symbol to the last. The loop\n"
    "locks to one of m phases (M-fold ambiguity); resolve it with\n"
    "bits(differential) or a sync word. Read norm_freq for the tracked\n"
    "carrier and lock for the carrier lock metric.\n"
    "\n"
    "Runs the per-sample loop (mix + cascade + matched filter, then the\n"
    "carrier and timing loops) over x and writes one cf32 symbol per\n"
    "recovered symbol period — roughly `x_len / sps` outputs. Read norm_freq\n"
    "for the tracked carrier and lock for the carrier lock metric.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input cf32 samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of symbols written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiver\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> idx = rng.integers(0, 4, 3000)                  # QPSK symbols\n"
    ">>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)\n"
    ">>> tx = tx.astype(np.complex64)                    # 8 samples/symbol\n"
    ">>> rx = MpskReceiver(m=4, sps=8, m_out=4, bn_carrier=0.02)\n"
    ">>> sym = rx.steps(tx)                              # blind NDA acquire\n"
    ">>> sym.size                                        # ~ x_len / sps\n"
    "2998\n"
    ">>> rx.lock > 0.8                                   # carrier locked\n"
    "True\n" },
  { "steps_max_out", (PyCFunction)BpskReceiverObj_steps_max_out, METH_NOARGS,
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
  { "bits", (PyCFunction)(void *)BpskReceiverObj_bits,
    METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Demodulate a cf32 block and return hard Gray-coded bits (log2(m)\n"
    "bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if\n"
    "the receiver was created with differential=1, each symbol's bits come\n"
    "from the phase DIFFERENCE between consecutive symbols\n"
    "(rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the\n"
    "symbol-error rate). Same per-sample carrier/timing recovery as steps().\n"
    "\n"
    "Like mpsk_receiver_steps(), but each recovered symbol is sliced to its\n"
    "nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With\n"
    "the differential option set at create time, the Gray label is taken\n"
    "from the phase *difference* between consecutive symbols\n"
    "(rotation-invariant — it resolves the M-fold carrier ambiguity), else\n"
    "from the absolute (coherent) decision.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input cf32 samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Number of bits written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiver\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> idx = rng.integers(0, 2, 3000)                  # BPSK payload bits\n"
    ">>> tx = np.repeat(np.exp(1j * np.pi * idx), 8).astype(np.complex64)\n"
    ">>> rx = MpskReceiver(m=2, sps=8, m_out=4, bn_carrier=0.005)\n"
    ">>> b = rx.bits(tx)                                 # 1 hard bit/symbol\n"
    ">>> b.size\n"
    "2998\n"
    ">>> # settled tail matches the payload, up to the BPSK\n"
    ">>> # inversion ambiguity and the pipeline's one-symbol lead\n"
    ">>> tail = np.mean(b[1001:2001] != idx[1000:2000])\n"
    ">>> round(float(min(tail, 1 - tail)), 3)\n"
    "0.0\n" },
  { "bits_max_out", (PyCFunction)BpskReceiverObj_bits_max_out, METH_NOARGS,
    "bits_max_out() -> int\n"
    "\n"
    "Largest number of samples bits() can return in the current state.\n"
    "\n"
    "Size an `out=` buffer with this before calling bits(), or use it to\n"
    "allocate one up front. The bound is this object's own: what it depends\n"
    "on is a property of the algorithm, so a header block on bits_max_out()\n"
    "replaces this text.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Upper bound on the output length; the actual call may return "
    "fewer.\n" },
  { "reset", (PyCFunction)BpskReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the carrier and symbol-timing loops to their create-time\n"
    "state; preserve configuration.\n"
    "\n"
    "Clears the cascade's filter memory, the carrier and timing NCOs, the\n"
    "loop-filter integrators and the lock detectors, and returns the carrier\n"
    "estimate to init_norm_freq. The configuration (order, rate, pulse,\n"
    "bandwidths) is untouched, so the same input fed twice around a reset\n"
    "reproduces the same output bit-for-bit.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiver\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> idx = rng.integers(0, 4, 300)\n"
    ">>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)\n"
    ">>> tx = tx.astype(np.complex64)\n"
    ">>> rx = MpskReceiver(m=4, sps=8, m_out=4)\n"
    ">>> first = rx.steps(tx)\n"
    ">>> rx.reset()                                # back to the cold state\n"
    ">>> np.array_equal(first, rx.steps(tx))       # same input, same output\n"
    "True\n" },
  { "state_bytes", (PyCFunction)BpskReceiverObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the BpskReceiver has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)BpskReceiverObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the BpskReceiver has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)BpskReceiverObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the BpskReceiver has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)BpskReceiverObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)BpskReceiverObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a MpskReceiver be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "MpskReceiver\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)BpskReceiverObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the MpskReceiver.\n"
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

static PyTypeObject BpskReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.BpskReceiver",
  .tp_basicsize                           = sizeof (BpskReceiverObject),
  .tp_dealloc = (destructor)BpskReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "BpskReceiver type.\n",
  .tp_methods = BpskReceiverObj_methods,
  .tp_getset  = BpskReceiver_getset,
  .tp_new     = BpskReceiverObj_new,
  .tp_init    = (initproc)BpskReceiverObj_init,
};
