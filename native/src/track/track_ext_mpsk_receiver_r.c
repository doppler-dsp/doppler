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
  static char *kwlist[]
      = { "m",           "sps",          "m_out",       "pulse",
          "rrc_beta",    "rrc_span",     "bn_carrier",  "zeta",
          "bn_timing",   "acq_to_track", "lock_thresh", "init_norm_freq",
          "warmup_syms", "differential", "num_phases",  "nda_tap",
          NULL };
  int                m               = 4;
  double             sps             = 32.0;
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
  self->handle = mpsk_receiver_r_create (
      m, sps, m_out, pulse, rrc_beta, rrc_span, bn_carrier, zeta, bn_timing,
      acq_to_track, lock_thresh, init_norm_freq, warmup_syms, differential,
      num_phases, nda_tap);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "MpskReceiverR: invalid parameter (need m in {2,4,8}, "
                       "sps > 2*m_out, m_out even in [2, 8], 0 <= rrc_beta "
                       "<= 1, rrc_span >= 1, num_phases a power of two >= 2, "
                       "bn >= 0, zeta > 0)");
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
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
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

static PyGetSetDef MpskReceiverR_getset[]
    = { { "norm_freq", (getter)MpskReceiverR_getprop_norm_freq,
          (setter)MpskReceiverR_setprop_norm_freq,
          "Tracked carrier, cycles/sample at the REAL input rate.\n", NULL },
        { "lock", (getter)MpskReceiverR_getprop_lock, NULL, "Lock.\n", NULL },
        { "timing_rate", (getter)MpskReceiverR_getprop_timing_rate, NULL,
          "Timing rate.\n", NULL },
        { "tracking", (getter)MpskReceiverR_getprop_tracking, NULL,
          "Tracking.\n", NULL },
        { "m", (getter)MpskReceiverR_getprop_m, NULL, "M.\n", NULL },
        { "sps", (getter)MpskReceiverR_getprop_sps, NULL, "Sps.\n", NULL },
        { "m_out", (getter)MpskReceiverR_getprop_m_out, NULL, "M out.\n",
          NULL },
        { "clipped", (getter)MpskReceiverR_getprop_clipped, NULL,
          "Has the cascade's CIC stage clipped its input since the last "
          "reset? A CIC bounds its input to |Re|, |Im| <= 1.0 and clips "
          "silently past that -- the output stays finite and plausible, "
          "merely distorted, at a cost of ~25 dB of EVM that no lock metric "
          "reveals. Always 0 for a plan with no CIC stage.\n",
          NULL },
        { NULL } };

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
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) telemetry; registers the same eleven probes as "
    "mpsk_receiver_set_telemetry(), whose contract this shares.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiverR\n"
    "    >>> obj = MpskReceiverR(4, 32.0, 8, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0, 1024, \"strobe\")\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "steps", (PyCFunction)(void *)MpskReceiverRObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Demodulate a real f32 block and return the recovered M-PSK symbols (one "
    "cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the "
    "receiver pushes x through the matched DDCR -- LO mix, decimating "
    "cascade, and a terminal polyphase stage whose bank IS the matched filter "
    "and whose selected arm IS the fractional symbol-timing delay -- then "
    "folds every output that stage produced into two loops: a Gardner "
    "symbol-timing loop steering the cascade's rate_ctrl port, and a carrier "
    "loop steering the LO's freq_ctrl port. The carrier discriminator runs on "
    "the on-time strobe only -- a non-strobe output straddles two symbols, so "
    "its M-th power is intersymbol interference rather than carrier phase -- "
    "and while acquiring it is the non-data-aided M-th-power error, needing "
    "no data and no symbol timing. With acq_to_track enabled a verify-counted "
    "two-way handover steps on the carrier lock metric each symbol: it "
    "switches to a lower-jitter decision-directed carrier loop after 8 "
    "consecutive above-lock_thresh symbols, and on a sustained lock loss (32 "
    "consecutive symbols below 0.8*lock_thresh) drops back to the NDA "
    "acquisition steer, the shared loop filter carrying the frequency "
    "estimate both ways. The loop locks to one of m phases (M-fold "
    "ambiguity); resolve it with bits(differential) or a sync word. Read "
    "norm_freq for the tracked carrier and lock for the carrier lock metric.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiverR\n"
    "    >>> obj = MpskReceiverR(4, 32.0, 8, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0, 1024, \"strobe\")\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)MpskReceiverRObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "bits", (PyCFunction)(void *)MpskReceiverRObj_bits,
    METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Demodulate a real f32 block and return hard Gray-coded bits (log2(m) "
    "bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if "
    "the receiver was created with differential=1, each symbol's bits come "
    "from the phase DIFFERENCE between consecutive symbols "
    "(rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the "
    "symbol-error rate). Same per-sample carrier/timing recovery as steps().\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiverR\n"
    "    >>> obj = MpskReceiverR(4, 32.0, 8, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0, 1024, \"strobe\")\n"
    "    >>> y = obj.bits(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "bits_max_out", (PyCFunction)MpskReceiverRObj_bits_max_out, METH_NOARGS,
    "bits_max_out() -> int\n\nMax output length bits() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "configure_lock", (PyCFunction)(void *)MpskReceiverRObj_configure_lock,
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
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiverR\n"
    "    >>> obj = MpskReceiverR(4, 32.0, 8, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0, 1024, \"strobe\")\n"
    "    >>> obj.configure_lock(0.0, 0.0, 0, 0)\n" },
  { "reset", (PyCFunction)MpskReceiverRObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the carrier and symbol-timing loops to their create-time state; "
    "preserve configuration.\n"
    "\n"
    "    >>> from doppler import MpskReceiverR\n"
    "    >>> obj = MpskReceiverR(4, 32.0, 8, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0, 1024, \"strobe\")\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)MpskReceiverRObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)MpskReceiverRObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)MpskReceiverRObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)MpskReceiverRObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)MpskReceiverRObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)MpskReceiverRObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject MpskReceiverRObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.MpskReceiverR",
  .tp_basicsize                           = sizeof (MpskReceiverRObject),
  .tp_dealloc = (destructor)MpskReceiverRObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a real-input M-PSK receiver.\n",
  .tp_methods = MpskReceiverRObj_methods,
  .tp_getset  = MpskReceiverR_getset,
  .tp_new     = MpskReceiverRObj_new,
  .tp_init    = (initproc)MpskReceiverRObj_init,
};
