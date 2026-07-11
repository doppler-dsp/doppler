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
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
  PyObject      *_steps_view_ref; /* gh-437 last returned view */
  uint8_t       *_bits_buf;       /* pre-allocated output for bits */
  size_t         _bits_buf_cap;   /* allocated capacity for bits */
  void         **_bits_retired;   /* gh-219 deferred free */
  size_t         _bits_retired_n;
  size_t         _bits_retired_cap;
  PyObject      *_bits_view_ref; /* gh-437 last returned view */
} MpskReceiverObject;

static void
MpskReceiverObj_dealloc (MpskReceiverObject *self)
{
  if (self->handle)
    mpsk_receiver_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_XDECREF (self->_steps_view_ref);
  free (self->_bits_buf);
  for (size_t _i = 0; _i < self->_bits_retired_n; _i++)
    free (self->_bits_retired[_i]);
  free (self->_bits_retired);
  Py_XDECREF (self->_bits_view_ref);
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
      = { "m",           "sps",          "n",           "pulse",
          "rrc_beta",    "rrc_span",     "bn_carrier",  "zeta",
          "bn_timing",   "acq_to_track", "lock_thresh", "init_norm_freq",
          "warmup_syms", "differential", NULL };
  int                m               = 4;
  unsigned long long sps_raw         = 8;
  int                n               = 4;
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

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "|iKisdidddiddKi", kwlist, &m, &sps_raw, &n, &pulse_str,
          &rrc_beta, &rrc_span, &bn_carrier, &zeta, &bn_timing, &acq_to_track,
          &lock_thresh, &init_norm_freq, &warmup_syms_raw, &differential))
    return -1;
  size_t sps   = (size_t)sps_raw;
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
  self->handle       = mpsk_receiver_create (
      m, sps, n, pulse, rrc_beta, rrc_span, bn_carrier, zeta, bn_timing,
      acq_to_track, lock_thresh, init_norm_freq, warmup_syms, differential);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "mpsk_receiver_create returned NULL");
      return -1;
    }
  {
    size_t _max = mpsk_receiver_steps_max_out (self->handle);
    if (_max)
      {
        self->_steps_buf = malloc (_max * sizeof (float complex));
        if (!self->_steps_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_steps_buf_cap = _max;
      }
  }
  {
    size_t _max = mpsk_receiver_bits_max_out (self->handle);
    if (_max)
      {
        self->_bits_buf = malloc (_max * sizeof (uint8_t));
        if (!self->_bits_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_bits_buf_cap = _max;
      }
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
  size_t _need      = (size_t)PyArray_SIZE (x_arr);
  int    _view_live = 0;
  if (self->_steps_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_steps_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_steps_view_ref) != Py_None;
#endif
    }
  if (!self->_steps_buf || self->_steps_buf_cap < _need || _view_live)
    {
      size_t _max = mpsk_receiver_steps_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_steps_buf
          && self->_steps_retired_n == self->_steps_retired_cap)
        {
          size_t _rcap
              = self->_steps_retired_cap ? self->_steps_retired_cap * 2 : 4;
          void **_rt = realloc (self->_steps_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_steps_retired     = _rt;
          self->_steps_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_steps_buf)
        self->_steps_retired[self->_steps_retired_n++] = self->_steps_buf;
      self->_steps_buf     = _tmp;
      self->_steps_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = mpsk_receiver_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                                 self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_steps_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_steps_view_ref);
  self->_steps_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_steps_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
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
  size_t _need      = (size_t)PyArray_SIZE (x_arr);
  int    _view_live = 0;
  if (self->_bits_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_bits_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_bits_view_ref) != Py_None;
#endif
    }
  if (!self->_bits_buf || self->_bits_buf_cap < _need || _view_live)
    {
      size_t _max = mpsk_receiver_bits_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_bits_buf && self->_bits_retired_n == self->_bits_retired_cap)
        {
          size_t _rcap
              = self->_bits_retired_cap ? self->_bits_retired_cap * 2 : 4;
          void **_rt = realloc (self->_bits_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_bits_retired     = _rt;
          self->_bits_retired_cap = _rcap;
        }
      uint8_t *_tmp = malloc (_max * sizeof (uint8_t));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_bits_buf)
        self->_bits_retired[self->_bits_retired_n++] = self->_bits_buf;
      self->_bits_buf     = _tmp;
      self->_bits_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = mpsk_receiver_bits (self->handle, _ng0, _ng1, self->_bits_buf,
                                self->_bits_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_UINT8, self->_bits_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_bits_view_ref);
  self->_bits_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_bits_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
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
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)mpsk_receiver_get_sps (self->handle));
}
static PyObject *
MpskReceiver_getprop_n (MpskReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)mpsk_receiver_get_n (self->handle));
}

static PyGetSetDef MpskReceiver_getset[]
    = { { "norm_freq", (getter)MpskReceiver_getprop_norm_freq,
          (setter)MpskReceiver_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock", (getter)MpskReceiver_getprop_lock, NULL, "Lock.\n", NULL },
        { "timing_rate", (getter)MpskReceiver_getprop_timing_rate, NULL,
          "Timing rate.\n", NULL },
        { "tracking", (getter)MpskReceiver_getprop_tracking, NULL,
          "Tracking.\n", NULL },
        { "m", (getter)MpskReceiver_getprop_m, NULL, "M.\n", NULL },
        { "sps", (getter)MpskReceiver_getprop_sps, NULL, "Sps.\n", NULL },
        { "n", (getter)MpskReceiver_getprop_n, NULL, "N.\n", NULL },
        { NULL } };

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
    "\"<prefix>.tracking\" (the two-way handover decision, 0/1 — the lockdet "
    "output, so a consumer sees exactly when the carrier was handed to the "
    "decision-directed discriminator or dropped back to NDA), then forwards "
    "the attach to both embedded loops: the carrier loop registers "
    "\"<prefix>.car.lock\" / \".e\" / \".freq\" / \".locked\" (plus its arm "
    "AGC's \"<prefix>.car.agc.gain_db\") and the symbol-timing loop registers "
    "\"<prefix>.sync.e\" / \".freq\" / \".rate\" — ten probes total, all "
    "thinned by decim.  Every probe except the AGC's emits once per recovered "
    "symbol (the receiver flushes both loops at the symbol strobe, not at the "
    "carrier loop's sample rate); the AGC's emits at its own amortized "
    "gain-update rate.  Passing NULL detaches the receiver and both loops.  "
    "Setup path, never hot; the context is borrowed and must outlive the "
    "attachment (SPSC rules in telemetry/telemetry.h).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiver\n"
    "    >>> obj = MpskReceiver(4, 8, 4, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0)\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "steps", (PyCFunction)MpskReceiverObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Demodulate a cf32 block and return the recovered M-PSK symbols (one cf32 "
    "per recovered symbol period, ~ len(x)/sps outputs). Per sample the "
    "receiver de-rotates with the integer-NCO carrier (predetection "
    "wipe-off), accumulates a non-data-aided M-th-power I/Q arm at n "
    "dumps/symbol to acquire the carrier with no data and no symbol timing, "
    "matched-filters the de-rotated stream (integrate-and-dump or RRC), and "
    "runs a Gardner symbol-timing loop. With acq_to_track enabled a "
    "verify-counted two-way handover steps on the carrier lock metric each "
    "symbol: it switches to a lower-jitter decision-directed carrier loop "
    "after 8 consecutive above-lock_thresh symbols, and on a sustained lock "
    "loss (32 consecutive symbols below 0.8*lock_thresh) drops back to the "
    "NDA acquisition steer, the shared NCO carrying the frequency estimate "
    "both ways. The loop locks to one of m phases (M-fold ambiguity); resolve "
    "it with bits(differential) or a sync word. Read norm_freq for the "
    "tracked carrier and lock for the carrier lock metric.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiver\n"
    "    >>> obj = MpskReceiver(4, 8, 4, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)MpskReceiverObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "bits", (PyCFunction)MpskReceiverObj_bits, METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Demodulate a cf32 block and return hard Gray-coded bits (log2(m) bytes "
    "of 0/1 per recovered symbol, LSB-first). Coherent by default; if the "
    "receiver was created with differential=1, each symbol's bits come from "
    "the phase DIFFERENCE between consecutive symbols (rotation-invariant — "
    "resolves the m-fold carrier ambiguity at ~2x the symbol-error rate). "
    "Same per-sample carrier/timing recovery as steps().\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiver\n"
    "    >>> obj = MpskReceiver(4, 8, 4, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0)\n"
    "    >>> y = obj.bits(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
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
    "    >>> import numpy as np\n"
    "    >>> from doppler import MpskReceiver\n"
    "    >>> obj = MpskReceiver(4, 8, 4, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0)\n"
    "    >>> obj.configure_lock(0.0, 0.0, 0, 0)\n" },
  { "reset", (PyCFunction)MpskReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the carrier and symbol-timing loops to their create-time state; "
    "preserve configuration.\n"
    "\n"
    "    >>> from doppler import MpskReceiver\n"
    "    >>> obj = MpskReceiver(4, 8, 4, \"iandd\", 0.35, 8, 0.01, 0.707, "
    "0.01, 0, 0.5, 0.0, 100, 0)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)MpskReceiverObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)MpskReceiverObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)MpskReceiverObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)MpskReceiverObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)MpskReceiverObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)MpskReceiverObj_exit, METH_VARARGS, NULL },
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
