/*
 * track_ext_mpsk_receiver_r.c — MpskReceiverR type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* MpskReceiverRObject — wraps mpsk_receiver_r_state_t *       */
/* ======================================================== */

#include "mpsk_receiver_r/mpsk_receiver_r_core.h"

typedef struct
{
  PyObject_HEAD mpsk_receiver_r_state_t *handle;
} MpskReceiverRObject;

static void
MpskReceiverRObj_dealloc (MpskReceiverRObject *self)
{
  if (self->handle)
    mpsk_receiver_r_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MpskReceiverRObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  MpskReceiverRObject *self = (MpskReceiverRObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MpskReceiverRObj_init (MpskReceiverRObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char       *kwlist[]       = { "m",
                                        "sps",
                                        "m_out",
                                        "pulse",
                                        "rrc_beta",
                                        "rrc_span",
                                        "bn_carrier",
                                        "zeta",
                                        "bn_timing",
                                        "acq_to_track",
                                        "lock_thresh",
                                        "init_norm_freq",
                                        "differential",
                                        "num_phases",
                                        "nda_tap",
                                        "agc",
                                        "bn_agc_ratio",
                                        NULL };
  int                m              = 4;
  double             sps            = 32.0;
  unsigned long long m_out_raw      = 8;
  const char        *pulse_str      = "iandd";
  double             rrc_beta       = 0.35;
  int                rrc_span       = 8;
  double             bn_carrier     = 0.01;
  double             zeta           = 0.707;
  double             bn_timing      = 0.01;
  int                acq_to_track   = 0;
  double             lock_thresh    = 0.5;
  double             init_norm_freq = 0.0;
  int                differential   = 0;
  unsigned long long num_phases_raw = 1024;
  const char        *nda_tap_str    = "strobe";
  int                agc            = 1;
  double             bn_agc_ratio   = 0.05;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "|idKsdidddiddiKsid", kwlist, &m, &sps, &m_out_raw,
          &pulse_str, &rrc_beta, &rrc_span, &bn_carrier, &zeta, &bn_timing,
          &acq_to_track, &lock_thresh, &init_norm_freq, &differential,
          &num_phases_raw, &nda_tap_str, &agc, &bn_agc_ratio))
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
  size_t num_phases = (size_t)num_phases_raw;
  int    nda_tap    = 0;
  if (strcmp (nda_tap_str, "strobe") == 0)
    nda_tap = 0;
  else if (strcmp (nda_tap_str, "mf_out") == 0)
    nda_tap = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "nda_tap must be one of \"strobe\", \"mf_out\", got '%s'",
                    nda_tap_str);
      return -1;
    }
  self->handle = mpsk_receiver_r_create (
      m, sps, m_out, pulse, rrc_beta, rrc_span, bn_carrier, zeta, bn_timing,
      acq_to_track, lock_thresh, init_norm_freq, differential, num_phases,
      nda_tap, agc, bn_agc_ratio);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "MpskReceiverR: invalid parameter (need m in {2,4,8}, "
                       "sps > 2*m_out, m_out even in [2, 8], 0 <= rrc_beta "
                       "<= 1, rrc_span >= 1, num_phases a power of two >= 2, "
                       "bn >= 0, zeta > 0, 0 < bn_agc_ratio < 1)");
      return -1;
    }
  return 0;
}

static PyObject *
MpskReceiverRObj_set_telemetry (MpskReceiverRObject *self, PyObject *args,
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
  int _rc = mpsk_receiver_r_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_telemetry failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MpskReceiverRObj_steps_max_out (MpskReceiverRObject *self,
                                PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (mpsk_receiver_r_steps_max_out (self->handle));
}

static PyObject *
MpskReceiverRObj_steps (MpskReceiverRObject *self, PyObject *args,
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
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_FLOAT,
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
      size_t _omax    = mpsk_receiver_r_steps_max_out (self->handle);
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
      const float   *_ng0 = (const float *)PyArray_DATA (x_arr);
      size_t         _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t         n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = mpsk_receiver_r_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = mpsk_receiver_r_steps_max_out (self->handle);
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
  const float *_ng0 = (const float *)PyArray_DATA (x_arr);
  size_t       _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t       n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = mpsk_receiver_r_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
MpskReceiverRObj_bits_max_out (MpskReceiverRObject *self,
                               PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (mpsk_receiver_r_bits_max_out (self->handle));
}

static PyObject *
MpskReceiverRObj_bits (MpskReceiverRObject *self, PyObject *args,
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
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_FLOAT,
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
      size_t _omax    = mpsk_receiver_r_bits_max_out (self->handle);
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
      const float *_ng0 = (const float *)PyArray_DATA (x_arr);
      size_t       _ng1 = (size_t)PyArray_SIZE (x_arr);
      uint8_t     *_ng2 = (uint8_t *)PyArray_DATA (out_arr);
      size_t       n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = mpsk_receiver_r_bits (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = mpsk_receiver_r_bits_max_out (self->handle);
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
  const float *_ng0 = (const float *)PyArray_DATA (x_arr);
  size_t       _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t       n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = mpsk_receiver_r_bits (self->handle, _ng0, _ng1, _d0, _cap);
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
MpskReceiverRObj_configure_lock (MpskReceiverRObject *self, PyObject *args,
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
  mpsk_receiver_r_configure_lock (self->handle, up_thresh, down_thresh, n_up,
                                  n_down);
  Py_RETURN_NONE;
}

static PyObject *
MpskReceiverRObj_reset (MpskReceiverRObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  mpsk_receiver_r_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
MpskReceiverRObj_state_bytes (MpskReceiverRObject *self,
                              PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (mpsk_receiver_r_state_bytes (self->handle));
}

static PyObject *
MpskReceiverRObj_get_state (MpskReceiverRObject *self,
                            PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = mpsk_receiver_r_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  mpsk_receiver_r_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
MpskReceiverRObj_set_state (MpskReceiverRObject *self, PyObject *arg)
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
      != mpsk_receiver_r_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (mpsk_receiver_r_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
MpskReceiverR_getprop_agc_gain_db (MpskReceiverRObject *self,
                                   void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_r_get_agc_gain_db (self->handle));
}
static PyObject *
MpskReceiverR_getprop_norm_freq (MpskReceiverRObject *self,
                                 void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_r_get_norm_freq (self->handle));
}
static int
MpskReceiverR_setprop_norm_freq (MpskReceiverRObject *self, PyObject *value,
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
  mpsk_receiver_r_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
MpskReceiverR_getprop_lock (MpskReceiverRObject *self,
                            void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_r_get_lock (self->handle));
}
static PyObject *
MpskReceiverR_getprop_lock_time (MpskReceiverRObject *self,
                                 void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLongLong (
      (long long)mpsk_receiver_r_get_lock_time (self->handle));
}
static PyObject *
MpskReceiverR_getprop_timing_rate (MpskReceiverRObject *self,
                                   void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_r_get_timing_rate (self->handle));
}
static PyObject *
MpskReceiverR_getprop_tracking (MpskReceiverRObject *self,
                                void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_r_get_tracking (self->handle));
}
static PyObject *
MpskReceiverR_getprop_m (MpskReceiverRObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_r_get_m (self->handle));
}
static PyObject *
MpskReceiverR_getprop_sps (MpskReceiverRObject *self,
                           void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (mpsk_receiver_r_get_sps (self->handle));
}
static PyObject *
MpskReceiverR_getprop_m_out (MpskReceiverRObject *self,
                             void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)mpsk_receiver_r_get_m_out (self->handle));
}
static PyObject *
MpskReceiverR_getprop_clipped (MpskReceiverRObject *self,
                               void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_r_get_clipped (self->handle));
}

static PyGetSetDef MpskReceiverR_getset[] = {
  { "agc_gain_db", (getter)MpskReceiverR_getprop_agc_gain_db, NULL,
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
  { "norm_freq", (getter)MpskReceiverR_getprop_norm_freq,
    (setter)MpskReceiverR_setprop_norm_freq,
    "Tracked carrier, cycles/sample at the REAL input rate.\n", NULL },
  { "lock", (getter)MpskReceiverR_getprop_lock, NULL,
    "EMA of the carrier lock signal.\n", NULL },
  { "lock_time", (getter)MpskReceiverR_getprop_lock_time, NULL,
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
  { "timing_rate", (getter)MpskReceiverR_getprop_timing_rate, NULL,
    "Timing rate.\n", NULL },
  { "tracking", (getter)MpskReceiverR_getprop_tracking, NULL,
    "0 = NDA acquire, 1 = decision.\n", NULL },
  { "m", (getter)MpskReceiverR_getprop_m, NULL,
    "constellation order M (2, 4, 8).\n", NULL },
  { "sps", (getter)MpskReceiverR_getprop_sps, NULL,
    "samples per symbol at the receiver's input.\n", NULL },
  { "m_out", (getter)MpskReceiverR_getprop_m_out, NULL,
    "terminal outputs per symbol.\n", NULL },
  { "clipped", (getter)MpskReceiverR_getprop_clipped, NULL,
    "Has the cascade's CIC stage clipped its input since the last reset? A "
    "CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past that "
    "-- the output stays finite and plausible, merely distorted, at a cost of "
    "~25 dB of EVM that no lock metric reveals. Always 0 for a plan with no "
    "CIC stage.\n",
    NULL },
  { NULL }
};

static PyObject *
MpskReceiverRObj_destroy (MpskReceiverRObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      mpsk_receiver_r_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MpskReceiverRObj_enter (MpskReceiverRObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MpskReceiverRObj_exit (MpskReceiverRObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      mpsk_receiver_r_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef MpskReceiverRObj_methods[] = {

  { "set_telemetry", (PyCFunction)(void *)MpskReceiverRObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context across the receiver.\n"
    "\n"
    "Registers the same thirteen probes as mpsk_receiver_set_telemetry(),\n"
    "whose contract it shares in full: the receiver's own \"<prefix>.lock\"\n"
    "and \"<prefix>.tracking\", the carrier loop's \"<prefix>.car.e\" / "
    "\".freq\"\n"
    "/ \".locked\", and the symbol-timing loop's \"<prefix>.sync.e\" / "
    "\".ctrl\" /\n"
    "\".rate\" / \".lock\" / \".locked\" / \".mu\" — eleven emitted once per\n"
    "recovered symbol — then the front end's AGC under \"<prefix>.agc\"\n"
    "(\".gain_db\" and \".level_db\"), forwarded through "
    "ddcr_set_telemetry().\n"
    "All thinned by decim. Passing NULL detaches everything. Setup path,\n"
    "never hot; the context is borrowed and must outlive the attachment.\n"
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
    "As on the complex twin, the two AGC probes are on the cascade's\n"
    "MFR-input grid rather than the symbol grid, so their record count\n"
    "differs from the other eleven — compare by time, not by index. See\n"
    "mpsk_receiver_set_telemetry() for why, and for why that AGC is the\n"
    "slowest loop in the receiver.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiverR\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 14)\n"
    ">>> rx = MpskReceiverR(m=4, sps=10, m_out=2, init_norm_freq=0.25)\n"
    ">>> rx.set_telemetry(tlm, \"rx\")\n"
    ">>> len(tlm.probe_names)\n"
    "13\n"
    ">>> rng = np.random.default_rng(7)\n"
    ">>> idx = rng.integers(0, 4, 512)\n"
    ">>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 10)\n"
    ">>> n = np.arange(bb.size)\n"
    ">>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real\n"
    ">>> x = np.ascontiguousarray(x.astype(np.float32))\n"
    ">>> _ = rx.steps(x)\n"
    ">>> recs = tlm.read()\n"
    ">>> tlm.dropped            # size the ring, or the counts below diverge\n"
    "0\n"
    ">>> n_sync = len(recs[recs[\"probe\"] == tlm.probe_id(\"rx.sync.e\")])\n"
    ">>> n_car = len(recs[recs[\"probe\"] == tlm.probe_id(\"rx.car.e\")])\n"
    ">>> n_sync > 0 and n_sync == n_car\n"
    "True\n"
    ">>> n_agc = len(recs[recs[\"probe\"] == "
    "tlm.probe_id(\"rx.agc.gain_db\")])\n"
    ">>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid\n"
    "True\n" },
  { "steps", (PyCFunction)(void *)MpskReceiverRObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Demodulate a real f32 block and return the recovered M-PSK symbols\n"
    "(one cf32 per recovered symbol period, ~ len(x)/sps outputs). Per\n"
    "sample the receiver pushes x through the matched DDCR -- LO mix,\n"
    "decimating cascade, and a terminal polyphase stage whose bank IS the\n"
    "matched filter and whose selected arm IS the fractional symbol-timing\n"
    "delay -- then folds every output that stage produced into two loops: a\n"
    "Gardner symbol-timing loop steering the cascade's rate_ctrl port, and a\n"
    "carrier loop steering the LO's freq_ctrl port. The carrier\n"
    "discriminator runs on the on-time strobe only -- a non-strobe output\n"
    "straddles two symbols, so its M-th power is intersymbol interference\n"
    "rather than carrier phase -- and while acquiring it is the\n"
    "non-data-aided M-th-power error, needing no data and no symbol timing.\n"
    "With acq_to_track enabled a verify-counted two-way handover steps on\n"
    "the carrier lock metric each symbol: it switches to a lower-jitter\n"
    "decision-directed carrier loop after 8 consecutive above-lock_thresh\n"
    "symbols, and on a sustained lock loss (32 consecutive symbols below\n"
    "0.8*lock_thresh) drops back to the NDA acquisition steer, the shared\n"
    "loop filter carrying the frequency estimate both ways. The loop locks\n"
    "to one of m phases (M-fold ambiguity); resolve it with\n"
    "bits(differential) or a sync word. Read norm_freq for the tracked\n"
    "carrier and lock for the carrier lock metric.\n"
    "\n"
    "As mpsk_receiver_steps(), taking real samples: the R2C halfband makes\n"
    "them complex before anything else touches them.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Real f32 input samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of symbols written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiverR\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> idx = rng.integers(0, 4, 2400)                  # QPSK symbols\n"
    ">>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)  # 32 sps\n"
    ">>> n = np.arange(bb.size)\n"
    ">>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4\n"
    ">>> x = np.ascontiguousarray(x.astype(np.float32))\n"
    ">>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)\n"
    ">>> sym = rx.steps(x)\n"
    ">>> sym.size                                        # ~ x_len / sps\n"
    "2398\n"
    ">>> rx.lock > 0.8                                   # carrier locked\n"
    "True\n" },
  { "steps_max_out", (PyCFunction)MpskReceiverRObj_steps_max_out, METH_NOARGS,
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
  { "bits", (PyCFunction)(void *)MpskReceiverRObj_bits,
    METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Demodulate a real f32 block and return hard Gray-coded bits (log2(m)\n"
    "bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if\n"
    "the receiver was created with differential=1, each symbol's bits come\n"
    "from the phase DIFFERENCE between consecutive symbols\n"
    "(rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the\n"
    "symbol-error rate). Same per-sample carrier/timing recovery as steps().\n"
    "\n"
    "As mpsk_receiver_bits(), taking real samples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Real f32 input samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Number of bits written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiverR\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> idx = rng.integers(0, 2, 2400)                  # BPSK payload bits\n"
    ">>> bb = np.repeat(np.exp(1j * np.pi * idx), 32)\n"
    ">>> n = np.arange(bb.size)\n"
    ">>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4\n"
    ">>> x = np.ascontiguousarray(x.astype(np.float32))\n"
    ">>> rx = MpskReceiverR(m=2, sps=32, m_out=8, init_norm_freq=0.25,\n"
    "...                    bn_carrier=0.005)\n"
    ">>> b = rx.bits(x)                                  # 1 hard bit/symbol\n"
    ">>> b.size\n"
    "2398\n"
    ">>> # settled tail matches the payload, up to the BPSK\n"
    ">>> # inversion ambiguity\n"
    ">>> tail = np.mean(b[1500:2300] != idx[1500:2300])\n"
    ">>> round(float(min(tail, 1 - tail)), 3)\n"
    "0.0\n" },
  { "bits_max_out", (PyCFunction)MpskReceiverRObj_bits_max_out, METH_NOARGS,
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
  { "configure_lock", (PyCFunction)(void *)MpskReceiverRObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune the acquisition<->tracking handover detector: hands the\n"
    "carrier to the decision-directed discriminator after n_up consecutive\n"
    "symbols with the carrier lock EMA above up_thresh, and falls back to\n"
    "NDA acquisition after n_down consecutive symbols below down_thresh\n"
    "(level + time hysteresis; see detection.LockDet). Previously only\n"
    "settable at construction (lock_thresh, with fixed 0.8x drop / 8-up /\n"
    "32-down constants) -- this is the post-construction re-tune Dll and\n"
    "Costas both already have. A live handover survives the re-tune; the\n"
    "in-flight verify run restarts.\n"
    "\n"
    "The real-input twin of mpsk_receiver_configure_lock(), whose contract\n"
    "it shares exactly: a split declare/drop threshold pair on the carrier\n"
    "lock EMA (level hysteresis) plus both verify counts (time hysteresis).\n"
    "A live handover survives the re-tune; the in-flight verify run\n"
    "restarts.\n"
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
    "    Consecutive below-threshold symbols to fall back to NDA\n"
    "    acquisition; clamped >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import MpskReceiverR\n"
    ">>> rx = MpskReceiverR(m=4, sps=10, m_out=2, acq_to_track=1)\n"
    ">>> rx.tracking\n"
    "0\n"
    ">>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, fast "
    "drop\n" },
  { "reset", (PyCFunction)MpskReceiverRObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the carrier and symbol-timing loops to their create-time\n"
    "state; preserve configuration.\n"
    "\n"
    "Identical in effect to mpsk_receiver_reset() — clears the R2C halfband\n"
    "and cascade memory, the carrier and timing NCOs, the loop integrators\n"
    "and the lock detectors, and returns the carrier estimate to\n"
    "init_norm_freq. Configuration is untouched, so a burst fed twice around\n"
    "a reset reproduces bit-for-bit.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import MpskReceiverR\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> idx = rng.integers(0, 4, 300)\n"
    ">>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)\n"
    ">>> n = np.arange(bb.size)\n"
    ">>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4\n"
    ">>> x = np.ascontiguousarray(x.astype(np.float32))\n"
    ">>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)\n"
    ">>> first = rx.steps(x)\n"
    ">>> rx.reset()                                # back to the cold state\n"
    ">>> np.array_equal(first, rx.steps(x))        # same input, same output\n"
    "True\n" },
  { "state_bytes", (PyCFunction)MpskReceiverRObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the MpskReceiverR has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)MpskReceiverRObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the MpskReceiverR has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)MpskReceiverRObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the MpskReceiverR has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)MpskReceiverRObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)MpskReceiverRObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a MpskReceiverR be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "MpskReceiverR\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)MpskReceiverRObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the MpskReceiverR.\n"
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

static PyTypeObject MpskReceiverRObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.MpskReceiverR",
  .tp_basicsize                           = sizeof (MpskReceiverRObject),
  .tp_dealloc = (destructor)MpskReceiverRObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a real-input M-PSK receiver.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "m : int, default 4\n"
    "    Constellation order M, 2/4/8 (default 4 = QPSK).\n"
    "sps : float, default 32.0\n"
    "    Samples per symbol. Any double **strictly greater than `2 * "
    "m_out`**\n"
    "    (the cascade behind the R2C halfband runs at twice the overall "
    "rate,\n"
    "    and Ddcr needs that below 0.5) -- 33.33389 is as valid as 32, "
    "because\n"
    "    the front end plans its own cascade and the terminal stage's\n"
    "    accumulator is a double. That is the real-world case whenever the "
    "ADC\n"
    "    clock is free-running against the symbol clock. The default is 32\n"
    "    rather than the complex twin's 8 purely to clear that bound: "
    "`m_out`\n"
    "    defaults to 8, so anything at or below 16 cannot construct.\n"
    "m_out : int, default 8\n"
    "    Terminal outputs per symbol: even, 2..8. The Gardner detector takes\n"
    "    every m_out-th output as the on-time strobe and the one m_out/2 back "
    "as\n"
    "    the transition gate, so the oversampled matched-filtered stream "
    "falls\n"
    "    out for free. **The default is 8 because that is where the I&D "
    "matched\n"
    "    filter reaches the coherent bound.** The rectangle is one symbol "
    "wide,\n"
    "    so its matched filter is an m_out-tap sum spanning it, and a "
    "smaller\n"
    "    m_out samples that same integral more coarsely. Measured on the "
    "complex\n"
    "    twin at its default sps=8 against the coherent bound EVM_dB =\n"
    "    -(Es/N0)_dB: at 18 dB Es/N0, m_out=8 lands 0.41 dB off the bound "
    "where\n"
    "    m_out=4 loses 3.11 dB; at 14 dB it is 0.25 dB against 1.71 dB -- "
    "the\n"
    "    gap widens as noise stops hiding it. Because `sps` must clear `2 *\n"
    "    m_out` here, this default is also what puts the `sps` default at "
    "32.\n"
    "    **Never pair 2 with pulse=\"iandd\"**: the matched filter "
    "degenerates to\n"
    "    a two-tap sum, the eye barely opens (measured lock statistic -0.34 "
    "at 2\n"
    "    against +0.95 at 4 on the same NRZ stream) and acquisition itself "
    "fails\n"
    "    about half the time. Replaces the old `n` (NDA arm dumps per "
    "symbol):\n"
    "    the cascade's own outputs now feed the carrier discriminator, so "
    "there\n"
    "    is no separate arm to size.\n"
    "pulse : Literal[\"iandd\", \"rrc\"], default \"iandd\"\n"
    "    Matched-filter shape (default MPSK_RX_PULSE_IANDD).\n"
    "rrc_beta : float, default 0.35\n"
    "    RRC roll-off in `[0, 1]` (default 0.35; RRC only).\n"
    "rrc_span : int, default 8\n"
    "    RRC one-sided span in symbols (default 8; RRC only).\n"
    "bn_carrier : float, default 0.01\n"
    "    Carrier loop noise bandwidth, normalised to the symbol rate "
    "(default\n"
    "    0.005).\n"
    "zeta : float, default 0.707\n"
    "    Damping factor for both loops (default 0.707).\n"
    "bn_timing : float, default 0.01\n"
    "    Timing loop noise bandwidth, per symbol (0.01).\n"
    "acq_to_track : int, default 0\n"
    "    Enable the two-way handover (default 0).\n"
    "lock_thresh : float, default 0.5\n"
    "    Handover declare threshold (default 0.5).\n"
    "init_norm_freq : float, default 0.0\n"
    "    Carrier frequency to tune to, cycles/sample **at the real input "
    "rate**\n"
    "    (default 0.0). A real IF at `0.2 * fs` is `0.2`; the halved value "
    "the\n"
    "    LO actually uses is this object's business, not the caller's.\n"
    "differential : int, default 0\n"
    "    bits(): differential demap (default 0).\n"
    "num_phases : int, default 1024\n"
    "    Matched-filter bank arms; a power of two. Sets the "
    "fractional-timing\n"
    "    resolution to 1/num_phases of an output period. The bank is sized "
    "by\n"
    "    the POST-decimation rate, so this costs the same at sps=8 and "
    "sps=256.\n"
    "nda_tap : Literal[\"strobe\", \"mf_out\"], default \"strobe\"\n"
    "    Where the NDA carrier discriminator reads, which sets its pull-in "
    "range\n"
    "    and whether it needs symbol timing at all. An M-th-power detector\n"
    "    updating at rate F can only see |df| < F/(2M), so the tap point IS "
    "the\n"
    "    range. `strobe` (default) reads the on-time MFR output at the "
    "symbol\n"
    "    rate Rs: the cleanest input, the narrowest range (Rs/(2M)), and the\n"
    "    only tap whose input quality depends on the timing loop. `mf_out` "
    "reads\n"
    "    every MFR output at m_out*Rs -- m_out times the range and no timing\n"
    "    dependence, paid for with the ISI the between-symbol outputs carry.\n"
    "    `mf_in` is not offered on the real-input type: it runs at the "
    "cascade's\n"
    "    `bank_sps`, which this front end does not publish, and construction\n"
    "    refuses it rather than falling back to a rate that would mis-size "
    "the\n"
    "    loop. Fixed at construction: nothing switches underneath you. If "
    "you\n"
    "    need more range than any tap gives, put a coarse frequency estimate "
    "in\n"
    "    front and pass it as init_norm_freq.\n"
    "agc : int, default 1\n"
    "    Level the front-end cascade so the timing detector's construct-time\n"
    "    slope means what it says. The TED normalises by its OWN slope and\n"
    "    nothing else, and that slope is computed for a UNIT-amplitude "
    "symbol\n"
    "    stream -- amplitude enters the raw error as A^2 (Gardner) or A^1\n"
    "    (DTTL), so a 4x level error is a 16x loop-gain error. With this on, "
    "an\n"
    "    AGC sits inside the cascade just before the matched filter and "
    "drives\n"
    "    the average power to the level a unit-amplitude symbol stream would\n"
    "    have there, derived from the matched filter's own pulse energy "
    "rather\n"
    "    than configured. On by default because the alternative is a "
    "receiver\n"
    "    whose loop gain depends on how loud the input happened to be. Set 0 "
    "to\n"
    "    reproduce the un-levelled behaviour exactly, which is the handle "
    "for\n"
    "    attributing any measurement that moves. This is the receiver's ONLY "
    "AGC\n"
    "    -- the carrier discriminator normalises by its own |z|^M and needs\n"
    "    none. Read the applied gain back with `agc_gain_db`. On this twin "
    "the\n"
    "    AGC sits behind the R2C halfband, so it levels the analytic signal "
    "at\n"
    "    the intermediate rate rather than the real input.\n"
    "bn_agc_ratio : float, default 0.05\n"
    "    The one AGC's noise bandwidth as a fraction of the SLOWEST loop it\n"
    "    feeds -- min(bn_carrier, bn_timing), not the carrier's alone, since "
    "the\n"
    "    AGC feeds the timing loop directly. Must be in (0, 1) and "
    "construction\n"
    "    refuses otherwise, because an AGC at or above the bandwidth of a "
    "loop\n"
    "    it feeds corrects the excursions that loop is itself producing and "
    "the\n"
    "    two integrate against each other; the signal LEVEL is a slow "
    "property\n"
    "    of the channel, not a disturbance to reject at loop speed. Exposed\n"
    "    rather than fixed because the right separation depends on how fast "
    "the\n"
    "    channel's level moves against how fast its phase and timing do, "
    "which\n"
    "    is a property of the link. The cold start is not the loop's job "
    "either\n"
    "    way -- the AGC seeds its gain from a direct measurement -- so slow "
    "is\n"
    "    cheap here.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``MpskReceiverR:\n"
    "    invalid parameter (need m in {2,4,8}, sps > 2*m_out, m_out even in "
    "[2,\n"
    "    8], 0 <= rrc_beta <= 1, rrc_span >= 1, num_phases a power of two >= "
    "2,\n"
    "    bn >= 0, zeta > 0, 0 < bn_agc_ratio < 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler import MpskReceiverR\n"
    ">>> obj = MpskReceiverR(\n"
    "...     m=4,\n"
    "...     sps=32.0,\n"
    "...     m_out=8,\n"
    "...     pulse=\"iandd\",\n"
    "...     rrc_beta=0.35,\n"
    "...     rrc_span=8,\n"
    "...     bn_carrier=0.01,\n"
    "...     zeta=0.707,\n"
    "...     bn_timing=0.01,\n"
    "...     acq_to_track=0,\n"
    "...     lock_thresh=0.5,\n"
    "...     init_norm_freq=0.0,\n"
    "...     differential=0,\n"
    "...     num_phases=1024,\n"
    "...     nda_tap=\"strobe\",\n"
    "...     agc=1,\n"
    "...     bn_agc_ratio=0.05,\n"
    "... )\n",
  .tp_methods = MpskReceiverRObj_methods,
  .tp_getset  = MpskReceiverR_getset,
  .tp_new     = MpskReceiverRObj_new,
  .tp_init    = (initproc)MpskReceiverRObj_init,
};
