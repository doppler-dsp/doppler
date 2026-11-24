/* jm:generated dsss_ext_ccsds415pool.c */
/*
 * dsss_ext_ccsds415pool.c — Ccsds415Pool type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in dsss_ext_ccsds415pool_extra.c.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* Ccsds415PoolObject — wraps dp_async_dsss_pool_state_t *       */
/* ======================================================== */

#include "doppler/async_dsss_pool/async_dsss_pool_core.h"

typedef struct
{
  PyObject_HEAD dp_async_dsss_pool_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
  PyObject     *_jm_retain_set_telemetry_tlm; /* keeps tlm's owner alive */
} Ccsds415PoolObject;

static void
Ccsds415PoolObj_dealloc (Ccsds415PoolObject *self)
{
  if (self->handle)
    dp_async_dsss_pool_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_XDECREF (self->_jm_retain_set_telemetry_tlm);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Ccsds415PoolObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  Ccsds415PoolObject *self = (Ccsds415PoolObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
Ccsds415PoolObj_init (Ccsds415PoolObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {
    "code",   "n_receivers", "carrier_freq_hz", "lost_confirm_s", "threads",
    "refine", NULL
  };
  PyObject          *code_obj        = NULL;
  PyObject          *n_receivers_obj = NULL;
  unsigned long long n_receivers_raw = 3;
  double             carrier_freq_hz = DP_CCSDS_415_FC_LO;
  double             lost_confirm_s  = 2.0;
  int                threads         = 0;
  int                refine_raw      = true;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|Oddip", kwlist, &code_obj,
                                    &n_receivers_obj, &carrier_freq_hz,
                                    &lost_confirm_s, &threads, &refine_raw))
    return -1;
  if (n_receivers_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_receivers_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      n_receivers_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_receivers_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_receivers_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n_receivers: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t         n_receivers = (size_t)n_receivers_raw;
  bool           refine      = (int)refine_raw;
  PyArrayObject *code_arr
      = jm_array_arg (code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "code");
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = dp_async_dsss_pool_create_ccsds415 (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, n_receivers,
      carrier_freq_hz, lost_confirm_s, threads, refine);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "Ccsds415Pool: refused: the code must be 1,023 chips "
                       "(the point's Gold code length; the code is the "
                       "mission's), n_receivers >= 1, carrier_freq_hz finite "
                       "and above 5.5 MHz (the point's 11 MSa/s over two), "
                       "lost_confirm_s >= 0 and threads >= 0");
      return -1;
    }
  return 0;
}

static PyObject *
Ccsds415PoolObj_push (Ccsds415PoolObject *self, PyObject *args, PyObject *kwds)
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
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  if (!self->handle)
    {
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  size_t y;
  void  *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    y = dp_async_dsss_pool_push (_jm_h, x, x_len);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyStructSequence_Field Ccsds415PoolObj_status_fields[] = {
  { "state", "ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is." },
  { "doppler_hz", "Where the emitter is NOW: the whole carrier estimate, Hz "
                  "-- loop 1's plus what loop 2 has taken up beyond it (the "
                  "seed while refining, 0 while searching, frozen where it "
                  "was when lost). Only as good as `locked`: with the carrier "
                  "unlocked, loop 1 free-runs and this wanders." },
  { "chip_phase",
    "Code phase, chips, at the sample after the last one fed: the live Dll's, "
    "carried over the partial period the chain holds; while refining, the "
    "seed's advanced by the clock dilation since (what the hand-over starts "
    "from), so a refining receiver's signal can be excluded too." },
  { "code_rate",
    "The Dll's code rate, chips per nominal chip (~1.0 at lock), without the "
    "carrier aid; a coasting Dll's is its held rate." },
  { "cn0_dbhz_est", "C/N0 estimate, dB-Hz (the seed's)." },
  { "code_locked", "Presence flag: the Dll's lock detector." },
  { "locked", "Health flag: the symbol-lock detector." },
  { "lock_metric", "cos(2*phi) over the symbols, drives `locked`." },
  { "lock_threshold", "`locked` declares strictly above this." },
  { "car_last_error", "Pre-despread loop's residual, rad (slope 1)." },
  { "mpsk_last_error", "Post-despread carrier residual, rad." },
  { "state_samples", "Input samples since `state` was entered." },
  { "both_down_samples",
    "Input samples both flags have been down without a break (the release "
    "clock); in lost it keeps counting -- samples since the flags dropped. "
    "With reacquire, the symbol lock flag's clock alone." },
  { "refused_hits",
    "Searching hits refused as seeds by the seed domain (a Doppler at or past "
    "fs/2), since create or reset. The search went on over each one's tail." },
  { "carrier_lock_n_noncoh",
    "The carrier lock's non-coherent count, the updates one decision sums "
    "(dp_async_dsss_receiver_get_carrier_lock_n_noncoh()): fixed at create." },
  { NULL, NULL },
};
static PyStructSequence_Desc Ccsds415PoolObj_status_desc
    = { "doppler.dsss.PoolReceiverStatus",
        "One AsyncDsssPool receiver's status record, read after the "
        "controller's step: state (1 refining, 2 tracking, 3 idle, 4 lost; a "
        "pooled receiver never searches), the live estimates, both lock "
        "flags, and the two clocks in input samples.",
        Ccsds415PoolObj_status_fields, 15 };
static PyTypeObject *Ccsds415PoolObj_status_type = NULL;

static PyObject *
Ccsds415PoolObj_status (Ccsds415PoolObject *self, PyObject *args,
                        PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "receiver", NULL };
  PyObject          *receiver_obj = NULL;
  unsigned long long receiver_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &receiver_obj))
    return NULL;
  if (receiver_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (receiver_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      receiver_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((receiver_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || receiver_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "receiver: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t receiver = (size_t)receiver_raw;
  if (!Ccsds415PoolObj_status_type)
    {
      Ccsds415PoolObj_status_type
          = PyStructSequence_NewType (&Ccsds415PoolObj_status_desc);
      if (!Ccsds415PoolObj_status_type)
        {
          return NULL;
        }
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  async_dsss_receiver_status_t _r
      = dp_async_dsss_pool_status (self->handle, receiver);
  PyObject *_o = PyStructSequence_New (Ccsds415PoolObj_status_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyLong_FromLong ((long)_r.state));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.doppler_hz));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.chip_phase));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.code_rate));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.cn0_dbhz_est));
  PyStructSequence_SET_ITEM (_o, 5, PyLong_FromLong ((long)_r.code_locked));
  PyStructSequence_SET_ITEM (_o, 6, PyLong_FromLong ((long)_r.locked));
  PyStructSequence_SET_ITEM (_o, 7, PyFloat_FromDouble (_r.lock_metric));
  PyStructSequence_SET_ITEM (_o, 8, PyFloat_FromDouble (_r.lock_threshold));
  PyStructSequence_SET_ITEM (_o, 9, PyFloat_FromDouble (_r.car_last_error));
  PyStructSequence_SET_ITEM (_o, 10, PyFloat_FromDouble (_r.mpsk_last_error));
  PyStructSequence_SET_ITEM (
      _o, 11,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.state_samples));
  PyStructSequence_SET_ITEM (
      _o, 12,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.both_down_samples));
  PyStructSequence_SET_ITEM (
      _o, 13,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.refused_hits));
  PyStructSequence_SET_ITEM (
      _o, 14,
      PyLong_FromUnsignedLongLong (
          (unsigned long long)_r.carrier_lock_n_noncoh));
  return _o;
}

static PyObject *
Ccsds415PoolObj_symbols_max_out (Ccsds415PoolObject *self,
                                 PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_async_dsss_pool_symbols_max_out (self->handle));
}

static PyObject *
Ccsds415PoolObj_symbols (Ccsds415PoolObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "receiver", "out", NULL };
  PyObject          *receiver_obj = NULL;
  unsigned long long receiver_raw = 0ULL;
  PyObject          *out_obj      = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &receiver_obj,
                                    &out_obj))
    return NULL;
  if (receiver_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (receiver_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      receiver_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((receiver_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || receiver_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "receiver: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t receiver = (size_t)receiver_raw;
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
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax = dp_async_dsss_pool_symbols_max_out (self->handle);
      size_t _min_cap
          = _omax > dp_async_dsss_pool_symbols_max_out (self->handle)
                ? _omax
                : (dp_async_dsss_pool_symbols_max_out (self->handle));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t n_out = dp_async_dsss_pool_symbols (
          self->handle, receiver, (float _Complex *)PyArray_DATA (out_arr),
          _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Ccsds415Pool.symbols: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = dp_async_dsss_pool_symbols_max_out (self->handle);
  size_t _cap  = dp_async_dsss_pool_symbols_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "Ccsds415Pool.symbols: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t n_out
      = dp_async_dsss_pool_symbols (self->handle, receiver, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Ccsds415Pool.symbols: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
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
Ccsds415PoolObj_reset (Ccsds415PoolObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_async_dsss_pool_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
Ccsds415PoolObj_set_threads (Ccsds415PoolObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "n", NULL };
  int          n         = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "i", _kwlist, &n))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_async_dsss_pool_set_threads (self->handle, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "threads refused: it must be >= 0 (0 = the online "
                    "cores), and 1 while a telemetry context is attached",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Ccsds415PoolObj_set_telemetry (Ccsds415PoolObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "tlm", "prefix", "decim", NULL };
  PyObject    *tlm_obj   = Py_None;
  const char  *prefix    = NULL;
  long long    decim_raw = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Os|L", _kwlist, &tlm_obj,
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
            {
              if (!PyErr_ExceptionMatches (PyExc_AttributeError))
                return NULL;
              PyErr_Clear ();
              PyErr_Format (PyExc_TypeError,
                            "tlm must be the doppler.telemetry.dp_tlm capsule"
                            " or an object exposing it as ._capsule,"
                            " not %s",
                            Py_TYPE (tlm_obj)->tp_name);
              return NULL;
            }
        }
      tlm = (dp_tlm_t *)PyCapsule_GetPointer (tlm_cap,
                                              "doppler.telemetry.dp_tlm");
      Py_DECREF (tlm_cap);
      if (!tlm)
        return NULL;
    }
  if (decim_raw < 0 || decim_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "decim: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)decim_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t decim = (uint32_t)decim_raw;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc
      = dp_async_dsss_pool_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "set_telemetry failed: the pool must run on one worker "
                    "(threads 1), decim must be >= 1, and every receiver's "
                    "names must fit the prefix and the context's table",
                    (long long)_rc);
      return NULL;
    }
  /* retain: the kernel keeps `tlm`, so this object keeps its owner
     until the next call replaces it or None detaches it. */
  if (tlm_obj != Py_None)
    Py_INCREF (tlm_obj);
  Py_XSETREF (self->_jm_retain_set_telemetry_tlm,
              tlm_obj != Py_None ? tlm_obj : NULL);
  Py_RETURN_NONE;
}

static PyObject *
Ccsds415PoolObj_state_bytes (Ccsds415PoolObject *self,
                             PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_async_dsss_pool_state_bytes (self->handle));
}

static PyObject *
Ccsds415PoolObj_get_state (Ccsds415PoolObject *self,
                           PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_async_dsss_pool_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_async_dsss_pool_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
Ccsds415PoolObj_set_state (Ccsds415PoolObject *self, PyObject *arg)
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
      != dp_async_dsss_pool_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_async_dsss_pool_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Ccsds415Pool_getprop_threads (Ccsds415PoolObject *self,
                              void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)dp_async_dsss_pool_get_threads (self->handle));
}
static PyObject *
Ccsds415Pool_getprop_n_receivers (Ccsds415PoolObject *self,
                                  void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_receivers);
}
static PyObject *
Ccsds415Pool_getprop_samples (Ccsds415PoolObject *self,
                              void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->samples);
}
static PyObject *
Ccsds415Pool_getprop_released_lost (Ccsds415PoolObject *self,
                                    void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->released_lost);
}
static PyObject *
Ccsds415Pool_getprop_released_duplicate (Ccsds415PoolObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->released_duplicate);
}
static PyObject *
Ccsds415Pool_getprop_released_ghost (Ccsds415PoolObject *self,
                                     void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->released_ghost);
}
static PyObject *
Ccsds415Pool_getprop_coherent_bins (Ccsds415PoolObject *self,
                                    void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_async_dsss_pool_get_coherent_bins (self->handle));
}

static PyGetSetDef Ccsds415Pool_getset[] = {
  { "threads", (getter)Ccsds415Pool_getprop_threads, NULL,
    "Workers push() steps the receivers on, the calling thread included: what "
    "set_threads() asked for, at most n_receivers, and the online cores for "
    "0.\n",
    NULL },
  { "n_receivers", (getter)Ccsds415Pool_getprop_n_receivers, NULL,
    "Receivers, fixed at create.\n", NULL },
  { "samples", (getter)Ccsds415Pool_getprop_samples, NULL,
    "Input samples pushed since create or reset: the pool's shared clock.\n",
    NULL },
  { "released_lost", (getter)Ccsds415Pool_getprop_released_lost, NULL,
    "Lost receivers the controller reset.\n", NULL },
  { "released_duplicate", (getter)Ccsds415Pool_getprop_released_duplicate,
    NULL, "Duplicate holders it reset.\n", NULL },
  { "released_ghost", (getter)Ccsds415Pool_getprop_released_ghost, NULL,
    "Sidelobe ghosts it reset.\n", NULL },
  { "coherent_bins", (getter)Ccsds415Pool_getprop_coherent_bins, NULL,
    "The receivers' slow-time depth D, epochs per coherent block "
    "(dp_async_dsss_receiver_get_coherent_bins()); alike for all.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
Ccsds415PoolObj_destroy (Ccsds415PoolObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_async_dsss_pool_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
Ccsds415PoolObj_enter (Ccsds415PoolObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Ccsds415PoolObj_exit (Ccsds415PoolObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_async_dsss_pool_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef Ccsds415PoolObj_methods[] = {

  { "push", (PyCFunction)(void *)Ccsds415PoolObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x) -> int\n"
    "\n"
    "Step every receiver on one block, running the controller at every\n"
    "group of code periods it completes.\n"
    "\n"
    "Hands every whole group of ASYNC_DSSS_POOL_PERIODS_PER_JOIN code\n"
    "periods it completes to the search and every receiver, one period a\n"
    "call (header steps 1 and 2), keeps each one's symbols for symbols(),\n"
    "and after each group releases the lost, resolves duplicates, releases\n"
    "ghosts, seeds the idle receivers and sets the search's levers (step 3).\n"
    "The samples past the last whole group are kept for the next push.\n"
    "Accepts any block size, and state carries across calls: the result is a\n"
    "function of the stream, not of its blocking.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input cf32 samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The receivers holding a signal (refining or tracking) at the end of\n"
    "    the block.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,\n"
    "...                      symbol_rate=2700.0, sample_rate=6.138e6,\n"
    "...                      doppler_uncertainty=500.0, threads=1)\n"
    ">>> pool.push(np.zeros(2046, dtype=np.complex64))\n"
    "0\n" },
  { "status", (PyCFunction)(void *)Ccsds415PoolObj_status,
    METH_VARARGS | METH_KEYWORDS,
    "status(receiver) -> PoolReceiverStatus record (state, doppler_hz, "
    "chip_phase, code_rate, cn0_dbhz_est, code_locked, locked, lock_metric, "
    "lock_threshold, car_last_error, mpsk_last_error, state_samples, "
    "both_down_samples, refused_hits, carrier_lock_n_noncoh)\n"
    "\n"
    "Receiver receiver's status, now.\n"
    "\n"
    "dp_async_dsss_receiver_status() of that receiver, as of the last whole\n"
    "code period fed, after the controller's step on it: a receiver the\n"
    "controller released reads idle. A receiver past the pool returns a\n"
    "zeroed record with `state` ASYNC_DSSS_POOL_NO_RECEIVER.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "receiver : int\n"
    "    0 to n_receivers - 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "PoolReceiverStatus\n"
    "    The status record.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,\n"
    "...                      symbol_rate=2700.0, sample_rate=6.138e6,\n"
    "...                      doppler_uncertainty=500.0, threads=1)\n"
    ">>> (pool.status(0).state, pool.status(2).state)  # 3 idle; 2 is past\n"
    "(3, -1)\n" },
  { "symbols", (PyCFunction)(void *)Ccsds415PoolObj_symbols,
    METH_VARARGS | METH_KEYWORDS,
    "symbols(receiver, out) -> ndarray\n"
    "\n"
    "The symbols receiver receiver emitted during the last push().\n"
    "\n"
    "Empty for a receiver that was idle, refining or lost throughout the\n"
    "push, and for a receiver past the pool. Tracking emits symbols whether\n"
    "or not they are a signal's (dp_async_dsss_receiver_steps()): read\n"
    "status().locked for that. A push replaces them, and set_state() and\n"
    "reset() clear them.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "receiver : int\n"
    "    0 to n_receivers - 1.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output symbols.\n"
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
    ">>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,\n"
    "...                      symbol_rate=2700.0, sample_rate=6.138e6,\n"
    "...                      doppler_uncertainty=500.0, threads=1)\n"
    ">>> _ = pool.push(np.zeros(2046, dtype=np.complex64))\n"
    ">>> pool.symbols(0).size                    # idle: nothing yet\n"
    "0\n" },
  { "symbols_max_out", (PyCFunction)Ccsds415PoolObj_symbols_max_out,
    METH_NOARGS,
    "symbols_max_out() -> int\n"
    "\n"
    "The most symbols symbols() can return: the largest count any\n"
    "receiver emitted during the last push().\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The largest per-receiver symbol count from the last push(); 0\n"
    "    before the first push and after reset() or set_state().\n" },
  { "reset", (PyCFunction)Ccsds415PoolObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Every receiver back to idle and the search to no exclusions; the\n"
    "clock, the release counts, the search's period count and the ghost\n"
    "history back to 0.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, chip_rate=3.069e6, symbol_rate=2700.0,\n"
    "...                      sample_rate=6.138e6, doppler_uncertainty=500.0, "
    "threads=1)\n"
    ">>> _ = pool.push(np.zeros(2046, dtype=np.complex64))\n"
    ">>> pool.samples\n"
    "2046\n"
    ">>> pool.reset()\n"
    ">>> pool.samples\n"
    "0\n" },
  { "set_threads", (PyCFunction)(void *)Ccsds415PoolObj_set_threads,
    METH_VARARGS | METH_KEYWORDS,
    "set_threads(n) -> None\n"
    "\n"
    "Set the workers push() steps the receivers on.\n"
    "\n"
    "The receivers' results do not change with it (header step 2); only the\n"
    "time a push takes does. A mutator's value, so it travels in the state\n"
    "blob (#2022).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    1 = serial, 0 = the online cores; at most n_receivers run.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``threads refused: it must be >= 0 (0 = the online cores), and 1\n"
    "    while a telemetry context is attached``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,\n"
    "...                      symbol_rate=2700.0, sample_rate=6.138e6,\n"
    "...                      doppler_uncertainty=500.0)\n"
    ">>> pool.set_threads(1)\n"
    ">>> pool.threads\n"
    "1\n" },
  { "set_telemetry", (PyCFunction)(void *)Ccsds415PoolObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context across every receiver.\n"
    "\n"
    "Receiver i reports under \"<prefix>.r<i>\" exactly as\n"
    "dp_async_dsss_receiver_set_telemetry() describes, on the one context. A\n"
    "context is single-producer, so this needs one producer: it is refused\n"
    "unless push() runs on one worker (get_threads() 1), and while a context\n"
    "is attached set_threads() refuses more. Parallel telemetry, one context\n"
    "per receiver, is doppler#2547.\n"
    "\n"
    "Whole or nothing: every receiver's names must fit before any registers\n"
    "on tlm (they are first attached to a scratch context), and a refusal\n"
    "leaves the pool on its previous context, or detached. The attachment is\n"
    "a request kept on the pool, so a receiver the controller resets keeps\n"
    "reporting under the same names.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Context to attach, or NULL to detach every receiver.\n"
    "prefix : str\n"
    "    Probe-name prefix; NULL means \"pool\".\n"
    "decim : int, default 1\n"
    "    Emit every decim-th event on each probe; >= 1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_telemetry failed: the pool must run on one worker (threads\n"
    "    1), decim must be >= 1, and every receiver's names must fit the\n"
    "    prefix and the context's table``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import AsyncDsssPool\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,\n"
    "...                      symbol_rate=2700.0, sample_rate=6.138e6,\n"
    "...                      doppler_uncertainty=500.0, threads=1)\n"
    ">>> tlm = Telemetry(1 << 16)\n"
    ">>> pool.set_telemetry(tlm, \"pool\")\n"
    ">>> sorted(p for p in tlm.probe_names if p.endswith(\".state\"))\n"
    "['pool.r0.state', 'pool.r1.state']\n" },
  { "state_bytes", (PyCFunction)Ccsds415PoolObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Ccsds415Pool has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)Ccsds415PoolObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures the state that evolves as the object runs, including any value\n"
    "a setter changed after construction, so a blob taken now and restored\n"
    "later resumes from this point. Construction parameters are not\n"
    "restored: restore into an object built the same way. A core may check\n"
    "some of them and refuse a blob built differently.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the Ccsds415Pool has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)Ccsds415PoolObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place, including any value a setter\n"
    "changed after construction. Construction parameters are not restored:\n"
    "restore into an object built the same way. Length is validated against\n"
    "`state_bytes()` before the blob is handed to the C core, which may\n"
    "reject it too: a core may check some construction parameters and refuse\n"
    "a blob built differently.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Ccsds415Pool has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)Ccsds415PoolObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)Ccsds415PoolObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Ccsds415Pool be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Ccsds415Pool\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)Ccsds415PoolObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Ccsds415Pool.\n"
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
  { NULL, NULL, 0, NULL }
};

static PyTypeObject Ccsds415PoolObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.dsss.Ccsds415Pool",
  .tp_basicsize                           = sizeof (Ccsds415PoolObject),
  .tp_dealloc = (destructor)Ccsds415PoolObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "A pool of async DSSS receivers at the CCSDS 415.1-B-1 specification "
    "point, each as Ccsds415Receiver builds one; the numbers are "
    "doppler/ccsds/ccsds_415.h's. Every method is AsyncDsssPool's.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : NDArray[np.uint8]\n"
    "    The mission's spreading code, one chip a byte (0/1): 1,023 chips.\n"
    "n_receivers : int, default 3\n"
    "    Receivers in the pool.\n"
    "carrier_freq_hz : float, default DP_CCSDS_415_FC_LO\n"
    "    The carrier, F_fwd, Hz: 2.2e9 to 2.8e9 at the point.\n"
    "lost_confirm_s : float, default 2.0\n"
    "    A receiver whose flags are down longer than this is released.\n"
    "threads : int, default 0\n"
    "    Worker threads; 0 is the pool's own choice.\n"
    "refine : bool, default True\n"
    "    The refine stage on (true) or off.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``Ccsds415Pool:\n"
    "    refused: the code must be 1,023 chips (the point's Gold code "
    "length;\n"
    "    the code is the mission's), n_receivers >= 1, carrier_freq_hz "
    "finite\n"
    "    and above 5.5 MHz (the point's 11 MSa/s over two), lost_confirm_s >= "
    "0\n"
    "    and threads >= 0``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Ccsds415Pool\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023), np.uint8)\n"
    ">>> pool = Ccsds415Pool(code, n_receivers=2, carrier_freq_hz=2.8e9)\n"
    ">>> pool.n_receivers\n"
    "2\n",
  .tp_methods = Ccsds415PoolObj_methods,
  .tp_getset  = Ccsds415Pool_getset,
  .tp_new     = Ccsds415PoolObj_new,
  .tp_init    = (initproc)Ccsds415PoolObj_init,
};
