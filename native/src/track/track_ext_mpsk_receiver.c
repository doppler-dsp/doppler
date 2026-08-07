/*
 * track_ext_mpsk_receiver.c — MpskReceiver type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* MpskReceiverObject — wraps mpsk_receiver_state_t *       */
/* ======================================================== */

#include "mpsk_receiver/mpsk_receiver_core.h"

typedef struct
{
  PyObject_HEAD mpsk_receiver_state_t *handle;
} MpskReceiverObject;

static void
MpskReceiverObj_dealloc (MpskReceiverObject *self)
{
  if (self->handle)
    mpsk_receiver_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MpskReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  MpskReceiverObject *self = (MpskReceiverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MpskReceiverObj_init (MpskReceiverObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "m",           "sps",          "m_out",       "pulse",
          "rrc_beta",    "rrc_span",     "bn_carrier",  "zeta",
          "bn_timing",   "acq_to_track", "lock_thresh", "init_norm_freq",
          "warmup_syms", "differential", "num_phases",  "nda_tap",
          NULL };
  int                m               = 4;
  double             sps             = 8.0;
  unsigned long long m_out_raw       = 8;
  const char        *pulse_str       = "iandd";
  double             rrc_beta        = 0.35;
  int                rrc_span        = 8;
  double             bn_carrier      = 0.01;
  double             zeta            = 0.707;
  double             bn_timing       = 0.01;
  int                acq_to_track    = 0;
  double             lock_thresh     = 0.5;
  double             init_norm_freq  = 0.0;
  unsigned long long warmup_syms_raw = 100;
  int                differential    = 0;
  unsigned long long num_phases_raw  = 1024;
  const char        *nda_tap_str     = "strobe";

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "|idKsdidddiddKiKs", kwlist, &m, &sps, &m_out_raw,
          &pulse_str, &rrc_beta, &rrc_span, &bn_carrier, &zeta, &bn_timing,
          &acq_to_track, &lock_thresh, &init_norm_freq, &warmup_syms_raw,
          &differential, &num_phases_raw, &nda_tap_str))
    return -1;
  size_t m_out = (size_t)m_out_raw;
  int    pulse = 0;
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
  size_t warmup_syms = (size_t)warmup_syms_raw;
  size_t num_phases  = (size_t)num_phases_raw;
  int    nda_tap     = 0;
  if (strcmp (nda_tap_str, "strobe") == 0)
    nda_tap = 0;
  else if (strcmp (nda_tap_str, "mf_all") == 0)
    nda_tap = 1;
  else if (strcmp (nda_tap_str, "lo_arm") == 0)
    nda_tap = 2;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "nda_tap must be one of \"strobe\", \"mf_all\", "
                    "\"lo_arm\", got '%s'",
                    nda_tap_str);
      return -1;
    }
  self->handle = mpsk_receiver_create (
      m, sps, m_out, pulse, rrc_beta, rrc_span, bn_carrier, zeta, bn_timing,
      acq_to_track, lock_thresh, init_norm_freq, warmup_syms, differential,
      num_phases, nda_tap);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "MpskReceiver: invalid parameter (need m in {2,4,8}, "
                       "sps >= m_out, m_out even in [2, 8], 0 <= rrc_beta <= "
                       "1, rrc_span >= 1, num_phases a power of two >= 2, bn "
                       ">= 0, zeta > 0)");
      return -1;
    }
  return 0;
}

static PyObject *
MpskReceiverObj_set_telemetry (MpskReceiverObject *self, PyObject *args,
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
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MpskReceiverObj_steps_max_out (MpskReceiverObject *self,
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
MpskReceiverObj_steps (MpskReceiverObject *self, PyObject *args,
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
MpskReceiverObj_bits_max_out (MpskReceiverObject *self,
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
MpskReceiverObj_bits (MpskReceiverObject *self, PyObject *args, PyObject *kwds)
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
MpskReceiverObj_configure_lock (MpskReceiverObject *self, PyObject *args,
                                PyObject *kwds)
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
  mpsk_receiver_configure_lock (self->handle, up_thresh, down_thresh, n_up,
                                n_down);
  Py_RETURN_NONE;
}

static PyObject *
MpskReceiverObj_reset (MpskReceiverObject *self, PyObject *Py_UNUSED (ignored))
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
MpskReceiverObj_state_bytes (MpskReceiverObject *self,
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
MpskReceiverObj_get_state (MpskReceiverObject *self,
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
MpskReceiverObj_set_state (MpskReceiverObject *self, PyObject *arg)
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
MpskReceiver_getprop_norm_freq (MpskReceiverObject *self,
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
MpskReceiver_setprop_norm_freq (MpskReceiverObject *self, PyObject *value,
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
MpskReceiver_getprop_lock (MpskReceiverObject *self, void *Py_UNUSED (closure))
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
MpskReceiver_getprop_timing_rate (MpskReceiverObject *self,
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
MpskReceiver_getprop_tracking (MpskReceiverObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_get_tracking (self->handle));
}
static PyObject *
MpskReceiver_getprop_m (MpskReceiverObject *self, void *Py_UNUSED (closure))
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
MpskReceiver_getprop_sps (MpskReceiverObject *self, void *Py_UNUSED (closure))
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
MpskReceiver_getprop_m_out (MpskReceiverObject *self,
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
MpskReceiver_getprop_clipped (MpskReceiverObject *self,
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

static PyGetSetDef MpskReceiver_getset[] = {
  { "norm_freq", (getter)MpskReceiver_getprop_norm_freq,
    (setter)MpskReceiver_setprop_norm_freq,
    "Carrier frequency the receiver is tracking, cycles/sample at the input "
    "rate: the create-time centre plus the loop's own estimate.\n",
    NULL },
  { "lock", (getter)MpskReceiver_getprop_lock, NULL,
    "EMA of the carrier lock signal.\n", NULL },
  { "timing_rate", (getter)MpskReceiver_getprop_timing_rate, NULL,
    "Smoothed tracked samples per symbol — departs from the nominal `sps` by "
    "exactly the sample-clock offset the timing loop is tracking.\n",
    NULL },
  { "tracking", (getter)MpskReceiver_getprop_tracking, NULL,
    "0 = NDA acquire, 1 = decision.\n", NULL },
  { "m", (getter)MpskReceiver_getprop_m, NULL,
    "constellation order M (2, 4, 8).\n", NULL },
  { "sps", (getter)MpskReceiver_getprop_sps, NULL,
    "samples per symbol at the receiver's input.\n", NULL },
  { "m_out", (getter)MpskReceiver_getprop_m_out, NULL,
    "Terminal outputs per symbol (the old `n`, now the cascade's).\n", NULL },
  { "clipped", (getter)MpskReceiver_getprop_clipped, NULL,
    "Has the cascade's CIC stage clipped its input since the last reset? A "
    "CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past that "
    "-- the output stays finite and plausible, merely distorted, at a cost of "
    "~25 dB of EVM that no lock metric reveals. Always 0 for a plan with no "
    "CIC stage.\n",
    NULL },
  { NULL }
};

static PyObject *
MpskReceiverObj_destroy (MpskReceiverObject *self,
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
MpskReceiverObj_enter (MpskReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MpskReceiverObj_exit (MpskReceiverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      mpsk_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef MpskReceiverObj_methods[] = {

  { "set_telemetry", (PyCFunction)(void *)MpskReceiverObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context across the receiver. Registers "
    "the receiver's own \"<prefix>.lock\" probe (the carrier lock EMA) and "
    "\"<prefix>.tracking\" (the two-way handover decision, 0/1 — so a "
    "consumer sees exactly when the carrier was handed to the "
    "decision-directed discriminator or dropped back to NDA), then the "
    "carrier loop's \"<prefix>.car.e\" / \".freq\" / \".locked\" and the "
    "symbol-timing loop's \"<prefix>.sync.e\" / \".ctrl\" / \".rate\" / "
    "\".lock\" / \".locked\" / \".mu\" -- eleven probes total, all thinned by "
    "decim and all emitted once per recovered symbol.  Passing NULL detaches "
    "everything. Setup path, never hot; the context is borrowed and must "
    "outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"rx\".\n"
    "decim : int\n"
    "    Emit every decim-th symbol; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiver\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 14)   # 11 probes x ~512 syms + headroom\n"
    ">>> rx = MpskReceiver(m=4, sps=4, m_out=2)\n"
    ">>> rx.set_telemetry(tlm, \"rx\")\n"
    ">>> len(tlm.probe_names)\n"
    "11\n"
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
    "True\n" },
  { "steps", (PyCFunction)(void *)MpskReceiverObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Demodulate a cf32 block and return the recovered M-PSK symbols (one cf32 "
    "per recovered symbol period, ~ len(x)/sps outputs). Per sample the "
    "receiver pushes x through the matched DDC -- LO mix, decimating cascade, "
    "and a terminal polyphase stage whose bank IS the matched filter and "
    "whose selected arm IS the fractional symbol-timing delay -- then folds "
    "every output that stage produced into two loops: a Gardner symbol-timing "
    "loop steering the cascade's rate_ctrl port, and a carrier loop steering "
    "the LO's freq_ctrl port. The carrier discriminator runs on the on-time "
    "strobe only -- a non-strobe output straddles two symbols, so its M-th "
    "power is intersymbol interference rather than carrier phase -- and while "
    "acquiring it is the non-data-aided M-th-power error, needing no data and "
    "no symbol timing. With acq_to_track enabled a verify-counted two-way "
    "handover steps on the carrier lock metric each symbol: it switches to a "
    "lower-jitter decision-directed carrier loop after 8 consecutive "
    "above-lock_thresh symbols, and on a sustained lock loss (32 consecutive "
    "symbols below 0.8*lock_thresh) drops back to the NDA acquisition steer, "
    "the shared loop filter carrying the frequency estimate both ways. The "
    "loop locks to one of m phases (M-fold ambiguity); resolve it with "
    "bits(differential) or a sync word. Read norm_freq for the tracked "
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
    "2997\n"
    ">>> round(rx.lock, 2)                               # carrier locked\n"
    "0.91\n" },
  { "steps_max_out", (PyCFunction)MpskReceiverObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "bits", (PyCFunction)(void *)MpskReceiverObj_bits,
    METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Demodulate a cf32 block and return hard Gray-coded bits (log2(m) bytes "
    "of 0/1 per recovered symbol, LSB-first). Coherent by default; if the "
    "receiver was created with differential=1, each symbol's bits come from "
    "the phase DIFFERENCE between consecutive symbols (rotation-invariant — "
    "resolves the m-fold carrier ambiguity at ~2x the symbol-error rate). "
    "Same per-sample carrier/timing recovery as steps().\n"
    "\n"
    "Like mpsk_receiver_steps(), but each recovered symbol is sliced to its\n"
    "nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With\n"
    "the differential option set at create time, the Gray label is taken "
    "from\n"
    "the phase *difference* between consecutive symbols (rotation-invariant "
    "—\n"
    "it resolves the M-fold carrier ambiguity), else from the absolute\n"
    "(coherent) decision.\n"
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
    "2997\n"
    ">>> # settled tail matches the payload, up to the BPSK\n"
    ">>> # inversion ambiguity\n"
    ">>> tail = np.mean(b[1000:2000] != idx[1000:2000])\n"
    ">>> round(float(min(tail, 1 - tail)), 3)\n"
    "0.0\n" },
  { "bits_max_out", (PyCFunction)MpskReceiverObj_bits_max_out, METH_NOARGS,
    "bits_max_out() -> int\n\nMax output length bits() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "configure_lock", (PyCFunction)(void *)MpskReceiverObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune the acquisition<->tracking handover detector: hands the carrier "
    "to the decision-directed discriminator after n_up consecutive symbols "
    "with the carrier lock EMA above up_thresh, and falls back to NDA "
    "acquisition after n_down consecutive symbols below down_thresh (level + "
    "time hysteresis; see detection.LockDet). Previously only settable at "
    "construction (lock_thresh, with fixed 0.8x drop / 8-up / 32-down "
    "constants) -- this is the post-construction re-tune Dll and Costas both "
    "already have. A live handover survives the re-tune; the in-flight verify "
    "run restarts.\n"
    "\n"
    "Full lockdet control over the handover, mirroring\n"
    "costas_configure_lock(): a split declare/drop threshold pair on the\n"
    "carrier lock EMA (level hysteresis) and both verify counts (time\n"
    "hysteresis). A live handover survives the re-tune; the in-flight verify\n"
    "run restarts.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    Declare threshold on the carrier lock EMA.\n"
    "down_thresh : float\n"
    "    Drop threshold; choose <= up_thresh for level hysteresis.\n"
    "n_up : int\n"
    "    Consecutive above-threshold symbols to hand over to the\n"
    "    decision-directed discriminator; clamped >= 1.\n"
    "n_down : int\n"
    "    Consecutive below-threshold symbols to fall back to NDA "
    "acquisition;\n"
    "    clamped >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import MpskReceiver\n"
    ">>> rx = MpskReceiver(m=4, sps=4, m_out=2, acq_to_track=1)\n"
    ">>> rx.tracking\n"
    "0\n"
    ">>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, fast "
    "drop\n" },
  { "reset", (PyCFunction)MpskReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the carrier and symbol-timing loops to their create-time state; "
    "preserve configuration.\n"
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
  { "state_bytes", (PyCFunction)MpskReceiverObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the MpskReceiverObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)MpskReceiverObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the MpskReceiverObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)MpskReceiverObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the MpskReceiverObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)MpskReceiverObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)MpskReceiverObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a MpskReceiver be used in a `with` statement so its C resources "
    "are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "MpskReceiver\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)MpskReceiverObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the MpskReceiver.\n"
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

static PyTypeObject MpskReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.MpskReceiver",
  .tp_basicsize                           = sizeof (MpskReceiverObject),
  .tp_dealloc = (destructor)MpskReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an M-PSK receiver.\n",
  .tp_methods = MpskReceiverObj_methods,
  .tp_getset  = MpskReceiver_getset,
  .tp_new     = MpskReceiverObj_new,
  .tp_init    = (initproc)MpskReceiverObj_init,
};
