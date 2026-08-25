/*
 * dsss_ext_dsss_burst_receiver.c — DsssBurstReceiver type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* DsssBurstReceiverObject — wraps dsss_burst_receiver_state_t *       */
/* ======================================================== */

#include "dsss_burst_receiver/dsss_burst_receiver_core.h"

typedef struct
{
  PyObject_HEAD dsss_burst_receiver_state_t *handle;
} DsssBurstReceiverObject;

static void
DsssBurstReceiverObj_dealloc (DsssBurstReceiverObject *self)
{
  if (self->handle)
    dsss_burst_receiver_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DsssBurstReceiverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DsssBurstReceiverObject *self
      = (DsssBurstReceiverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DsssBurstReceiverObj_init (DsssBurstReceiverObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[]
      = { "acq_code",    "data_code",    "sync",
          "reps",        "spc",          "chip_rate",
          "payload_len", "cn0_dbhz",     "doppler_uncertainty",
          "pfa",         "pd",           "carrier_hz",
          "max_rate",    "est_segments", NULL };
  PyObject          *acq_code_obj        = NULL;
  PyObject          *data_code_obj       = NULL;
  PyObject          *sync_obj            = NULL;
  unsigned long long reps_raw            = 5;
  unsigned long long spc_raw             = 4;
  double             chip_rate           = 1000000.0;
  unsigned long long payload_len_raw     = 64;
  double             cn0_dbhz            = 50.0;
  double             doppler_uncertainty = 0.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  double             carrier_hz          = 0.0;
  double             max_rate            = 0.0;
  unsigned long long est_segments_raw    = 10;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "OOO|KKdKddddddK", kwlist, &acq_code_obj, &data_code_obj,
          &sync_obj, &reps_raw, &spc_raw, &chip_rate, &payload_len_raw,
          &cn0_dbhz, &doppler_uncertainty, &pfa, &pd, &carrier_hz, &max_rate,
          &est_segments_raw))
    return -1;
  size_t         reps         = (size_t)reps_raw;
  size_t         spc          = (size_t)spc_raw;
  size_t         payload_len  = (size_t)payload_len_raw;
  size_t         est_segments = (size_t)est_segments_raw;
  PyArrayObject *acq_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      acq_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!acq_code_arr)
    {
      return -1;
    }
  size_t         acq_code_len  = (size_t)PyArray_SIZE (acq_code_arr);
  PyArrayObject *data_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      data_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!data_code_arr)
    {
      Py_DECREF (acq_code_arr);
      return -1;
    }
  size_t         data_code_len = (size_t)PyArray_SIZE (data_code_arr);
  PyArrayObject *sync_arr      = (PyArrayObject *)PyArray_FROM_OTF (
      sync_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!sync_arr)
    {
      Py_DECREF (acq_code_arr);
      Py_DECREF (data_code_arr);
      return -1;
    }
  size_t sync_len = (size_t)PyArray_SIZE (sync_arr);
  self->handle    = dsss_burst_receiver_create (
      (const uint8_t *)PyArray_DATA (acq_code_arr), acq_code_len,
      (const uint8_t *)PyArray_DATA (data_code_arr), data_code_len,
      (const uint8_t *)PyArray_DATA (sync_arr), sync_len, reps, spc, chip_rate,
      payload_len, cn0_dbhz, doppler_uncertainty, pfa, pd, carrier_hz,
      max_rate, est_segments);
  Py_DECREF (acq_code_arr);
  Py_DECREF (data_code_arr);
  Py_DECREF (sync_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "DsssBurstReceiver: invalid parameter (need non-empty "
                       "acq_code/data_code/sync, reps >= 1, spc >= 1, "
                       "chip_rate > 0, payload_len >= 1, cn0_dbhz > 0, 0 < "
                       "pfa < 1, 0 < pd < 1)");
      return -1;
    }
  return 0;
}

static PyObject *
DsssBurstReceiverObj_push_max_out (DsssBurstReceiverObject *self,
                                   PyObject                *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t x_len = 0;
  if (!PyArg_ParseTuple (args, "n", &x_len))
    return NULL;
  return PyLong_FromSize_t (
      dsss_burst_receiver_push_max_out (self->handle, (size_t)x_len));
}

static PyObject *
DsssBurstReceiverObj_push (DsssBurstReceiverObject *self, PyObject *args,
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
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = dsss_burst_receiver_push_max_out (
          self->handle, (size_t)PyArray_SIZE (x_arr));
      size_t _min_cap = _omax;
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
        n_out
            = dsss_burst_receiver_push (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = dsss_burst_receiver_push_max_out (
      self->handle, (size_t)PyArray_SIZE (x_arr));
  (void)_need;
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
    n_out = dsss_burst_receiver_push (self->handle, _ng0, _ng1, _d0, _cap);
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
DsssBurstReceiverObj_configure_search_raw (DsssBurstReceiverObject *self,
                                           PyObject *args, PyObject *kwds)
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
  int _rc = dsss_burst_receiver_configure_search_raw (self->handle,
                                                      doppler_bins, n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_search_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssBurstReceiverObj_reset (DsssBurstReceiverObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dsss_burst_receiver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DsssBurstReceiverObj_state_bytes (DsssBurstReceiverObject *self,
                                  PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dsss_burst_receiver_state_bytes (self->handle));
}

static PyObject *
DsssBurstReceiverObj_get_state (DsssBurstReceiverObject *self,
                                PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dsss_burst_receiver_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dsss_burst_receiver_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DsssBurstReceiverObj_set_state (DsssBurstReceiverObject *self, PyObject *arg)
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
      != dsss_burst_receiver_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dsss_burst_receiver_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
DsssBurstReceiver_getprop_preamble_start (DsssBurstReceiverObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_burst_receiver_get_preamble_start (
          self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_frame_valid (DsssBurstReceiverObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong (
      (long)dsss_burst_receiver_get_frame_valid (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_doppler_hz_est (DsssBurstReceiverObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_doppler_hz_est (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_doppler_res_hz (DsssBurstReceiverObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_doppler_res_hz (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_cn0_dbhz_est (DsssBurstReceiverObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_cn0_dbhz_est (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_est_freq_hz (DsssBurstReceiverObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_est_freq_hz (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_est_rate_hz (DsssBurstReceiverObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_est_rate_hz (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_est_snr_db (DsssBurstReceiverObject *self,
                                      void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_est_snr_db (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_refine_margin (DsssBurstReceiverObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (
      dsss_burst_receiver_get_refine_margin (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_pending (DsssBurstReceiverObject *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_burst_receiver_get_pending (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_dropped (DsssBurstReceiverObject *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_burst_receiver_get_dropped (self->handle));
}
static PyObject *
DsssBurstReceiver_getprop_n_bursts (DsssBurstReceiverObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dsss_burst_receiver_get_n_bursts (self->handle));
}

static PyGetSetDef DsssBurstReceiver_getset[]
    = { { "preamble_start", (getter)DsssBurstReceiver_getprop_preamble_start,
          NULL, "Stream-absolute preamble start. Never late.\n", NULL },
        { "frame_valid", (getter)DsssBurstReceiver_getprop_frame_valid, NULL,
          "1 if the CRC-16 trailer matched.\n", NULL },
        { "doppler_hz_est", (getter)DsssBurstReceiver_getprop_doppler_hz_est,
          NULL, "Folded/signed Doppler estimate, Hz.\n", NULL },
        { "doppler_res_hz", (getter)DsssBurstReceiver_getprop_doppler_res_hz,
          NULL, "Doppler bin width = chip_rate/(sf*coherent_bins).\n", NULL },
        { "cn0_dbhz_est", (getter)DsssBurstReceiver_getprop_cn0_dbhz_est, NULL,
          "Estimated carrier-to-noise density (dB-Hz), backed out of "
          "test_stat via the same C/N0 <-> per-sample-amplitude-SNR "
          "relationship used to size the engine (see acq_create_burst()/ "
          "acq_create_continuous()). Tracks the true C/N0 while receiver AWGN "
          "dominates the CFAR noise estimate; saturates at the code's own "
          "autocorrelation-sidelobe floor once the true C/N0 exceeds what "
          "this code/geometry can resolve — a real ceiling, not a fault.\n",
          NULL },
        { "est_freq_hz", (getter)DsssBurstReceiver_getprop_est_freq_hz, NULL,
          "estimated residual Doppler (Hz).\n", NULL },
        { "est_rate_hz", (getter)DsssBurstReceiver_getprop_est_rate_hz, NULL,
          "estimated Doppler rate (Hz/s).\n", NULL },
        { "est_snr_db", (getter)DsssBurstReceiver_getprop_est_snr_db, NULL,
          "estimator confidence (dB).\n", NULL },
        { "refine_margin", (getter)DsssBurstReceiver_getprop_refine_margin,
          NULL,
          "Winning preamble correlation over its nearest whole-period "
          "competitor. Near 1 means the period was NOT resolved.\n",
          NULL },
        { "pending", (getter)DsssBurstReceiver_getprop_pending, NULL,
          "Bursts demodulated, awaiting return by push().\n", NULL },
        { "dropped", (getter)DsssBurstReceiver_getprop_dropped, NULL,
          "Overrun ctr.\n", NULL },
        { "n_bursts", (getter)DsssBurstReceiver_getprop_n_bursts, NULL,
          "Bursts demodulated, lifetime.\n", NULL },
        { NULL } };

static PyObject *
DsssBurstReceiverObj_destroy (DsssBurstReceiverObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dsss_burst_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DsssBurstReceiverObj_enter (DsssBurstReceiverObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DsssBurstReceiverObj_exit (DsssBurstReceiverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dsss_burst_receiver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DsssBurstReceiverObj_methods[] = {

  { "push", (PyCFunction)(void *)DsssBurstReceiverObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x, out) -> ndarray\n"
    "\n"
    "Stream raw cf32 samples. Samples feed the embedded BurstAcquisition\n"
    "and are retained in a history ring; when a detection fires, the refine\n"
    "stage correlates the whole preamble to recover the exact preamble start\n"
    "-- the one quantity acquisition structurally cannot report, since its\n"
    "code_phase is a lag modulo one code period -- and the burst is\n"
    "demodulated the moment its last sample has arrived. Returns the payload\n"
    "bits of AT MOST ONE completed burst per call, so the read-back fields\n"
    "always describe the burst whose bits were just returned; if more are\n"
    "ready, `pending` is non-zero and a further push (an empty array will\n"
    "do) drains the next. An empty return is normal, not an error: it means\n"
    "no burst completed in this call. Accepts any block size; state carries\n"
    "across calls.\n"
    "\n"
    "Retains x in the history ring and feeds the embedded acquisition. When\n"
    "a detection fires, the refine stage correlates the whole preamble to\n"
    "recover the exact preamble start -- the quantity acquisition\n"
    "structurally cannot report, its code phase being a lag modulo one code\n"
    "period -- and the burst is demodulated once its last sample has\n"
    "arrived.\n"
    "\n"
    "Writes the payload of AT MOST ONE completed burst per call, so the\n"
    "read-back fields always describe the burst whose bits were just\n"
    "returned. If more are ready, `pending` is non-zero and a further push\n"
    "drains the next. Returning 0 is normal, not an error: it means no burst\n"
    "completed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input samples (cf32), x_len long.\n"
    "out : NDArray[np.uint8] | None\n"
    "    Payload bits, caller-owned, max_out long.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Bits written to out (0 or payload_len).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssBurstReceiver\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> rx = DsssBurstReceiver(\n"
    "...     rng.integers(0, 2, 31).astype(np.uint8),\n"
    "...     rng.integers(0, 2, 8).astype(np.uint8),\n"
    "...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, payload_len=32)\n"
    ">>> bits = rx.push(np.zeros(4096, dtype=np.complex64))\n"
    ">>> bits.size            # silence carries no burst\n"
    "0\n" },
  { "push_max_out", (PyCFunction)DsssBurstReceiverObj_push_max_out,
    METH_VARARGS,
    "push_max_out(x_len) -> int\n"
    "\n"
    "Max bits push() writes: the payload length.\n"
    "\n"
    "Independent of x_len, because at most one burst is returned per call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x_len : int\n"
    "    Input length (ignored).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    payload_len -- the buffer a caller must provide.\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)DsssBurstReceiverObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> None\n"
    "\n"
    "Pin the embedded BurstAcquisition's search grid directly, bypassing\n"
    "the auto-sizing -- the escape hatch for a caller who wants a specific\n"
    "(doppler_bins, n_noncoh). Forwards to the engine unchanged.\n"
    "\n"
    "The escape hatch for a caller who wants a specific (doppler_bins,\n"
    "n_noncoh) rather than the grid the cn0_dbhz/pfa/pd sizing chooses.\n"
    "Forwards to the embedded engine unchanged.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    Coherent depth to pin, in [1, reps].\n"
    "n_noncoh : int\n"
    "    Non-coherent looks to combine.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``configure_search_raw failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssBurstReceiver\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> rx = DsssBurstReceiver(\n"
    "...     rng.integers(0, 2, 31).astype(np.uint8),\n"
    "...     rng.integers(0, 2, 8).astype(np.uint8),\n"
    "...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, payload_len=32)\n"
    ">>> rx.configure_search_raw(doppler_bins=1, n_noncoh=1)\n" },
  { "reset", (PyCFunction)DsssBurstReceiverObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Return to the searching state: resets the embedded acquisition,\n"
    "drops the history ring's contents and clears every read-back, so a\n"
    "fresh stream cannot inherit the previous burst's verdict. Construction\n"
    "parameters are untouched.\n"
    "\n"
    "Resets the embedded acquisition, discards the retained look-back, and\n"
    "clears all the event fields, so a fresh stream cannot inherit the\n"
    "previous burst's verdict. The lifetime counters (`n_bursts`, `dropped`)\n"
    "deliberately survive -- a reset that zeroed them could hide that this\n"
    "receiver had already lost samples. Construction parameters are\n"
    "untouched.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssBurstReceiver\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> rx = DsssBurstReceiver(\n"
    "...     rng.integers(0, 2, 31).astype(np.uint8),\n"
    "...     rng.integers(0, 2, 8).astype(np.uint8),\n"
    "...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, payload_len=32)\n"
    ">>> _ = rx.push(np.zeros(1024, dtype=np.complex64))\n"
    ">>> rx.reset()\n"
    ">>> rx.frame_valid\n"
    "0\n" },
  { "state_bytes", (PyCFunction)DsssBurstReceiverObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DsssBurstReceiver has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DsssBurstReceiverObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DsssBurstReceiver has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DsssBurstReceiverObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the DsssBurstReceiver has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)DsssBurstReceiverObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)DsssBurstReceiverObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a DsssBurstReceiver be used in a `with` statement so its C\n"
    "resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "DsssBurstReceiver\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DsssBurstReceiverObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the DsssBurstReceiver.\n"
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

static PyTypeObject DsssBurstReceiverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.DsssBurstReceiver",
  .tp_basicsize                           = sizeof (DsssBurstReceiverObject),
  .tp_dealloc = (destructor)DsssBurstReceiverObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a burst receiver: acquisition, refine and demodulation composed\n"
    "behind one push().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "acq_code : NDArray[np.uint8]\n"
    "    Preamble PN chips (0/1), length acq_code_len.\n"
    "data_code : NDArray[np.uint8]\n"
    "    Payload spreading chips (0/1), data_code_len long.\n"
    "sync : NDArray[np.uint8]\n"
    "    Frame sync word (0/1 symbols), sync_len long.\n"
    "reps : int, default 5\n"
    "    Preamble code repetitions (>= 1).\n"
    "spc : int, default 4\n"
    "    Samples per chip (>= 1).\n"
    "chip_rate : float, default 1000000.0\n"
    "    Chip rate in Hz (> 0).\n"
    "payload_len : int, default 64\n"
    "    Payload bits per burst (>= 1).\n"
    "cn0_dbhz : float, default 50.0\n"
    "    Carrier-to-noise density in dB-Hz (> 0), sizing the acquisition "
    "search.\n"
    "doppler_uncertainty : float, default 0.0\n"
    "    One-sided Doppler half-range, Hz.\n"
    "pfa : float, default 1e-3\n"
    "    Target false-alarm probability, in (0, 1).\n"
    "pd : float, default 0.9\n"
    "    Target detection probability, in (0, 1).\n"
    "carrier_hz : float, default 0.0\n"
    "    RF carrier (Hz) for code-Doppler; 0 = ignore.\n"
    "max_rate : float, default 0.0\n"
    "    Chirp-rate search half-span (cycles/sample^2).\n"
    "est_segments : int, default 10\n"
    "    Segments the feedforward estimator fits over.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is "
    "``DsssBurstReceiver:\n"
    "    invalid parameter (need non-empty acq_code/data_code/sync, reps >= "
    "1,\n"
    "    spc >= 1, chip_rate > 0, payload_len >= 1, cn0_dbhz > 0, 0 < pfa < "
    "1, 0\n"
    "    < pd < 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import DsssBurstReceiver\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> acq = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> dat = rng.integers(0, 2, 8).astype(np.uint8)\n"
    ">>> syn = np.zeros(13, dtype=np.uint8)\n"
    ">>> rx = DsssBurstReceiver(acq, dat, syn, reps=4, spc=4,\n"
    "...                        payload_len=32)\n"
    ">>> rx.n_bursts\n"
    "0\n",
  .tp_methods = DsssBurstReceiverObj_methods,
  .tp_getset  = DsssBurstReceiver_getset,
  .tp_new     = DsssBurstReceiverObj_new,
  .tp_init    = (initproc)DsssBurstReceiverObj_init,
};
