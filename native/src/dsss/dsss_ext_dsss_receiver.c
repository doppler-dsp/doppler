/*
 * dsss_ext_dsss_receiver.c — DsssReceiver type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* DsssReceiverObject — wraps dsss_receiver_state_t *       */
/* ======================================================== */

#include "dsss_receiver/dsss_receiver_core.h"

typedef struct
{
  PyObject_HEAD dsss_receiver_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
  PyObject      *_steps_view_ref; /* gh-437 last returned view */
} DsssReceiverObject;

static void
DsssReceiverObj_dealloc (DsssReceiverObject *self)
{
  if (self->handle)
    dsss_receiver_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_XDECREF (self->_steps_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DsssReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DsssReceiverObject *self = (DsssReceiverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DsssReceiverObj_init (DsssReceiverObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]    = { "code",     "chip_rate", "symbol_rate",
                               "spc",      "m",         "cn0_dbhz",
                               "pfa",      "pd",        "doppler_uncertainty",
                               "segments", "sps",       "differential",
                               NULL };
  PyObject    *code_obj    = NULL;
  double       chip_rate   = 1000000.0;
  double       symbol_rate = 1000.0;
  unsigned long long spc_raw             = 2;
  int                m                   = 2;
  double             cn0_dbhz            = 55.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  double             doppler_uncertainty = 100.0;
  unsigned long long segments_raw        = 4;
  unsigned long long sps_raw             = 8;
  int                differential        = 0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|ddKiddddKKi", kwlist, &code_obj, &chip_rate,
          &symbol_rate, &spc_raw, &m, &cn0_dbhz, &pfa, &pd,
          &doppler_uncertainty, &segments_raw, &sps_raw, &differential))
    return -1;
  size_t         spc      = (size_t)spc_raw;
  size_t         segments = (size_t)segments_raw;
  size_t         sps      = (size_t)sps_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = dsss_receiver_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, chip_rate,
      symbol_rate, spc, m, cn0_dbhz, pfa, pd, doppler_uncertainty, segments,
      sps, differential);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dsss_receiver_create returned NULL");
      return -1;
    }
  {
    size_t _max = dsss_receiver_steps_max_out (self->handle);
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
  return 0;
}

static PyObject *
DsssReceiverObj_steps_max_out (DsssReceiverObject *self,
                               PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dsss_receiver_steps_max_out (self->handle));
}

static PyObject *
DsssReceiverObj_steps (DsssReceiverObject *self, PyObject *args,
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
      size_t _omax    = dsss_receiver_steps_max_out (self->handle);
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
        n_out = dsss_receiver_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
      size_t _max = dsss_receiver_steps_max_out (self->handle);
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
    n_out = dsss_receiver_steps (self->handle, _ng0, _ng1, self->_steps_buf,
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
DsssReceiverObj_configure_search_raw (DsssReceiverObject *self, PyObject *args,
                                      PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]        = { "doppler_bins", "n_noncoh", NULL };
  unsigned long long doppler_bins_raw = 0ULL;
  unsigned long long n_noncoh_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KK", _kwlist,
                                    &doppler_bins_raw, &n_noncoh_raw))
    return NULL;
  size_t doppler_bins = (size_t)doppler_bins_raw;
  size_t n_noncoh     = (size_t)n_noncoh_raw;
  int    _rc = dsss_receiver_configure_search_raw (self->handle, doppler_bins,
                                                   n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_search_raw failed (rc=%d)",
                    _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_configure_lock_raw (DsssReceiverObject *self, PyObject *args,
                                    PyObject *kwds)
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
  unsigned long long n_looks_raw = 0ULL;
  double             alpha       = 0.0;
  unsigned long      n_up_raw    = 0UL;
  unsigned long      n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddKdkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_looks_raw, &alpha,
                                    &n_up_raw, &n_down_raw))
    return NULL;
  size_t   n_looks = (size_t)n_looks_raw;
  uint32_t n_up    = (uint32_t)n_up_raw;
  uint32_t n_down  = (uint32_t)n_down_raw;
  dsss_receiver_configure_lock_raw (self->handle, up_thresh, down_thresh,
                                    n_looks, alpha, n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_configure_chain_raw (DsssReceiverObject *self, PyObject *args,
                                     PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "segments", "sps", "n", NULL };
  unsigned long long segments_raw = 0ULL;
  unsigned long long sps_raw      = 0ULL;
  int                n            = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KKi", _kwlist, &segments_raw,
                                    &sps_raw, &n))
    return NULL;
  size_t segments = (size_t)segments_raw;
  size_t sps      = (size_t)sps_raw;
  int _rc = dsss_receiver_configure_chain_raw (self->handle, segments, sps, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_chain_raw failed (rc=%d)",
                    _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_reset (DsssReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dsss_receiver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_state_bytes (DsssReceiverObject *self,
                             PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dsss_receiver_state_bytes (self->handle));
}

static PyObject *
DsssReceiverObj_get_state (DsssReceiverObject *self,
                           PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dsss_receiver_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dsss_receiver_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DsssReceiverObj_set_state (DsssReceiverObject *self, PyObject *arg)
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
      != dsss_receiver_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dsss_receiver_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
DsssReceiver_getprop_tracking (DsssReceiverObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)dsss_receiver_get_tracking (self->handle));
}
static PyObject *
DsssReceiver_getprop_doppler_hz (DsssReceiverObject *self,
                                 void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_doppler_hz (self->handle));
}
static PyObject *
DsssReceiver_getprop_cn0_dbhz_est (DsssReceiverObject *self,
                                   void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_cn0_dbhz_est (self->handle));
}
static PyObject *
DsssReceiver_getprop_segments (DsssReceiverObject *self,
                               void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_receiver_get_segments (self->handle));
}
static PyObject *
DsssReceiver_getprop_sps (DsssReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_receiver_get_sps (self->handle));
}
static PyObject *
DsssReceiver_getprop_n (DsssReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)dsss_receiver_get_n (self->handle));
}
static PyObject *
DsssReceiver_getprop_chip_phase (DsssReceiverObject *self,
                                 void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_chip_phase (self->handle));
}
static PyObject *
DsssReceiver_getprop_code_rate (DsssReceiverObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_code_rate (self->handle));
}
static PyObject *
DsssReceiver_getprop_lock (DsssReceiverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_lock (self->handle));
}
static PyObject *
DsssReceiver_getprop_norm_freq (DsssReceiverObject *self,
                                void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dsss_receiver_get_norm_freq (self->handle));
}

static PyGetSetDef DsssReceiver_getset[] = {
  { "tracking", (getter)DsssReceiver_getprop_tracking, NULL, "Tracking.\n",
    NULL },
  { "doppler_hz", (getter)DsssReceiver_getprop_doppler_hz, NULL,
    "Doppler hz.\n", NULL },
  { "cn0_dbhz_est", (getter)DsssReceiver_getprop_cn0_dbhz_est, NULL,
    "Cn0 dbhz est.\n", NULL },
  { "segments", (getter)DsssReceiver_getprop_segments, NULL, "Segments.\n",
    NULL },
  { "sps", (getter)DsssReceiver_getprop_sps, NULL, "Sps.\n", NULL },
  { "n", (getter)DsssReceiver_getprop_n, NULL, "N.\n", NULL },
  { "chip_phase", (getter)DsssReceiver_getprop_chip_phase, NULL,
    "Dll's live tracked code phase (chips); 0.0 while searching.\n", NULL },
  { "code_rate", (getter)DsssReceiver_getprop_code_rate, NULL,
    "Dll's own tracking-quality indicator; 1.0 while searching.\n", NULL },
  { "lock", (getter)DsssReceiver_getprop_lock, NULL,
    "MpskReceiver's carrier lock EMA; 0.0 while searching.\n", NULL },
  { "norm_freq", (getter)DsssReceiver_getprop_norm_freq, NULL,
    "MpskReceiver's tracked carrier frequency; 0.0 while searching.\n", NULL },
  { NULL }
};

static PyObject *
DsssReceiverObj_destroy (DsssReceiverObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dsss_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssReceiverObj_enter (DsssReceiverObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DsssReceiverObj_exit (DsssReceiverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dsss_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DsssReceiverObj_methods[] = {

  { "steps", (PyCFunction)DsssReceiverObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Stream raw cf32 samples through the receiver. While searching, samples "
    "feed the embedded Acquisition and nothing is emitted (an empty array is "
    "normal, not an error). The moment a hit fires, "
    "Dll/RateConverter/MpskReceiver are built and seeded from it -- the same "
    "phase-inversion hand-off and rate-bridging this project's "
    "async-DSSS-receiver gallery story validated by hand -- and the "
    "unconsumed tail of this same call is handed straight to them, so no "
    "samples are dropped at the transition. While tracking, samples feed Dll "
    "-> RateConverter -> MpskReceiver in sequence and demodulated symbols are "
    "returned. Accepts any block size; state carries across calls.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DsssReceiver\n"
    "    >>> obj = DsssReceiver(np.zeros(1, dtype=np.uint8), 1000000.0, "
    "1000.0, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)DsssReceiverObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "configure_search_raw",
    (PyCFunction)(void *)DsssReceiverObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> int\n"
    "\n"
    "Pin the embedded Acquisition's search grid directly, bypassing the "
    "symbol_rate-driven auto-sizing -- the escape hatch for a power user who "
    "wants a specific (doppler_bins, n_noncoh). Only meaningful while "
    "searching.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DsssReceiver\n"
    "    >>> obj = DsssReceiver(np.zeros(1, dtype=np.uint8), 1000000.0, "
    "1000.0, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0)\n"
    "    >>> obj.configure_search_raw(0, 0)\n"
    "    0\n" },
  { "configure_lock_raw",
    (PyCFunction)(void *)DsssReceiverObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(up_thresh, down_thresh, n_looks, alpha, n_up, n_down) "
    "-> None\n"
    "\n"
    "Re-tune the embedded Dll's code-lock detector directly. Only meaningful "
    "once tracking has begun; a no-op while searching.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DsssReceiver\n"
    "    >>> obj = DsssReceiver(np.zeros(1, dtype=np.uint8), 1000000.0, "
    "1000.0, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0)\n"
    "    >>> obj.configure_lock_raw(0.0, 0.0, 0, 0.0, 0, 0)\n" },
  { "configure_chain_raw",
    (PyCFunction)(void *)DsssReceiverObj_configure_chain_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_chain_raw(segments, sps, n) -> int\n"
    "\n"
    "Pin the despread/resample/demod grid directly, bypassing the create-time "
    "segments/sps defaults -- segments (Dll's tracking parameter) and sps/n "
    "(MpskReceiver's rate/carrier-arm parameters) stay independently "
    "overridable here, still bridged by a freshly-sized RateConverter, never "
    "coupled to each other. Only meaningful once tracking; rebuilds the chain "
    "with every replacement allocated first, so a failed pin leaves the "
    "receiver on its prior grid.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DsssReceiver\n"
    "    >>> obj = DsssReceiver(np.zeros(1, dtype=np.uint8), 1000000.0, "
    "1000.0, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0)\n"
    "    >>> obj.configure_chain_raw(0, 0, 0)\n"
    "    0\n" },
  { "reset", (PyCFunction)DsssReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Return to the searching state: resets the embedded Acquisition and frees "
    "Dll/RateConverter/MpskReceiver (rebuilt from scratch on the next hit).\n"
    "\n"
    "    >>> from doppler import DsssReceiver\n"
    "    >>> obj = DsssReceiver(np.zeros(1, dtype=np.uint8), 1000000.0, "
    "1000.0, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)DsssReceiverObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)DsssReceiverObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)DsssReceiverObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)DsssReceiverObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DsssReceiverObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DsssReceiverObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject DsssReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.DsssReceiver",
  .tp_basicsize                           = sizeof (DsssReceiverObject),
  .tp_dealloc = (destructor)DsssReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a DSSS receiver in the searching state.\n",
  .tp_methods = DsssReceiverObj_methods,
  .tp_getset  = DsssReceiver_getset,
  .tp_new     = DsssReceiverObj_new,
  .tp_init    = (initproc)DsssReceiverObj_init,
};
