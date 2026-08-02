/*
 * dsss_ext_burst_despreader.c — BurstDespreader type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* BurstDespreaderObject — wraps burst_despreader_state_t *       */
/* ======================================================== */

#include "burst_despreader/burst_despreader_core.h"

typedef struct
{
  PyObject_HEAD burst_despreader_state_t *handle;
} BurstDespreaderObject;

static void
BurstDespreaderObj_dealloc (BurstDespreaderObject *self)
{
  if (self->handle)
    burst_despreader_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
BurstDespreaderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  BurstDespreaderObject *self
      = (BurstDespreaderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
BurstDespreaderObj_init (BurstDespreaderObject *self, PyObject *args,
                         PyObject *kwds)
{
  static char *kwlist[]
      = { "code",       "sf",      "sps", "init_norm_freq", "init_chip_phase",
          "bn_carrier", "bn_code", NULL };
  PyObject          *code_obj        = NULL;
  unsigned long long sf_raw          = 1;
  unsigned long long sps_raw         = 2;
  double             init_norm_freq  = 0.0;
  double             init_chip_phase = 0.0;
  double             bn_carrier      = 0.05;
  double             bn_code         = 0.01;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKdddd", kwlist, &code_obj,
                                    &sf_raw, &sps_raw, &init_norm_freq,
                                    &init_chip_phase, &bn_carrier, &bn_code))
    return -1;
  size_t         sf       = (size_t)sf_raw;
  size_t         sps      = (size_t)sps_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = burst_despreader_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, sf, sps,
      init_norm_freq, init_chip_phase, bn_carrier, bn_code);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "burst_despreader_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
BurstDespreaderObj_steps_max_out (BurstDespreaderObject *self,
                                  PyObject              *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (burst_despreader_steps_max_out (self->handle));
}

static PyObject *
BurstDespreaderObj_steps (BurstDespreaderObject *self, PyObject *args,
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
      size_t _omax    = burst_despreader_steps_max_out (self->handle);
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
        n_out = burst_despreader_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = burst_despreader_steps_max_out (self->handle);
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
    n_out = burst_despreader_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
BurstDespreaderObj_bits_max_out (BurstDespreaderObject *self,
                                 PyObject              *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (burst_despreader_bits_max_out (self->handle));
}

static PyObject *
BurstDespreaderObj_bits (BurstDespreaderObject *self, PyObject *args,
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
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = burst_despreader_bits_max_out (self->handle);
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
        n_out = burst_despreader_bits (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = burst_despreader_bits_max_out (self->handle);
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
    n_out = burst_despreader_bits (self->handle, _ng0, _ng1, _d0, _cap);
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
BurstDespreaderObj_set_acq (BurstDespreaderObject *self, PyObject *args,
                            PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "acq_code", "acq_reps", NULL };
  PyObject          *acq_code_obj = NULL;
  unsigned long long acq_reps_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OK", _kwlist, &acq_code_obj,
                                    &acq_reps_raw))
    return NULL;
  size_t         acq_reps     = (size_t)acq_reps_raw;
  PyArrayObject *acq_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      acq_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!acq_code_arr)
    {
      return NULL;
    }
  const uint8_t *acq_code     = (const uint8_t *)PyArray_DATA (acq_code_arr);
  size_t         acq_code_len = (size_t)PyArray_SIZE (acq_code_arr);
  burst_despreader_set_acq (self->handle, acq_code, acq_code_len, acq_reps);
  Py_DECREF (acq_code_arr);
  Py_RETURN_NONE;
}

static PyObject *
BurstDespreaderObj_reset (BurstDespreaderObject *self,
                          PyObject              *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  burst_despreader_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
BurstDespreaderObj_state_bytes (BurstDespreaderObject *self,
                                PyObject              *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (burst_despreader_state_bytes (self->handle));
}

static PyObject *
BurstDespreaderObj_get_state (BurstDespreaderObject *self,
                              PyObject              *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = burst_despreader_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  burst_despreader_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
BurstDespreaderObj_set_state (BurstDespreaderObject *self, PyObject *arg)
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
      != burst_despreader_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (burst_despreader_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
BurstDespreader_getprop_bn_carrier (BurstDespreaderObject *self,
                                    void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_bn_carrier (self->handle));
}
static int
BurstDespreader_setprop_bn_carrier (BurstDespreaderObject *self,
                                    PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  burst_despreader_set_bn_carrier (self->handle, v);
  return 0;
}
static PyObject *
BurstDespreader_getprop_bn_code (BurstDespreaderObject *self,
                                 void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_bn_code (self->handle));
}
static int
BurstDespreader_setprop_bn_code (BurstDespreaderObject *self, PyObject *value,
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
  burst_despreader_set_bn_code (self->handle, v);
  return 0;
}
static PyObject *
BurstDespreader_getprop_norm_freq (BurstDespreaderObject *self,
                                   void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_norm_freq (self->handle));
}
static int
BurstDespreader_setprop_norm_freq (BurstDespreaderObject *self,
                                   PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  burst_despreader_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
BurstDespreader_getprop_code_phase (BurstDespreaderObject *self,
                                    void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_code_phase (self->handle));
}
static PyObject *
BurstDespreader_getprop_lock_metric (BurstDespreaderObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_lock_metric (self->handle));
}
static PyObject *
BurstDespreader_getprop_snr_est (BurstDespreaderObject *self,
                                 void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_snr_est (self->handle));
}
static PyObject *
BurstDespreader_getprop_lock_stat (BurstDespreaderObject *self,
                                   void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_despreader_get_lock_stat (self->handle));
}
static PyObject *
BurstDespreader_getprop_stat_n (BurstDespreaderObject *self,
                                void                  *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_despreader_get_stat_n (self->handle));
}

static PyGetSetDef BurstDespreader_getset[] = {
  { "bn_carrier", (getter)BurstDespreader_getprop_bn_carrier,
    (setter)BurstDespreader_setprop_bn_carrier,
    "Carrier (Costas) loop noise bandwidth, normalized to the symbol rate.\n",
    NULL },
  { "bn_code", (getter)BurstDespreader_getprop_bn_code,
    (setter)BurstDespreader_setprop_bn_code,
    "Code (DLL) loop noise bandwidth, normalized to the symbol rate.\n",
    NULL },
  { "norm_freq", (getter)BurstDespreader_getprop_norm_freq,
    (setter)BurstDespreader_setprop_norm_freq,
    "Current carrier frequency estimate, cycles/sample.\n", NULL },
  { "code_phase", (getter)BurstDespreader_getprop_code_phase, NULL,
    "Current tracked code phase within the symbol, chips.\n", NULL },
  { "lock_metric", (getter)BurstDespreader_getprop_lock_metric, NULL,
    "Lock indicator in [0,1]: the mean of |Re prompt|/|prompt| over every "
    "prompt of the burst (cumulative, not EMA). ~1 when phase-locked; ~2/pi "
    "(0.637) with no carrier.\n",
    NULL },
  { "snr_est", (getter)BurstDespreader_getprop_snr_est, NULL,
    "Post-despread SNR estimate over the burst, accumulate-then-ratio: (sum "
    "Re^2 - sum Im^2)/sum Im^2, clamped >= 0. This is the effective post-loop "
    "SNR (residual tracking jitter included) - the quantity that predicts "
    "demodulation performance; it converges to the AWGN-only A^2/sigma^2 as "
    "the loop bandwidths shrink.\n",
    NULL },
  { "lock_stat", (getter)BurstDespreader_getprop_lock_stat, NULL,
    "Calibrated whole-burst lock statistic R = sqrt(stat_n * sum Re^2 / sum "
    "Im^2) — the one-shot analog of the tracking loops' verify-counted "
    "detectors. Because the noise reference is estimated from as many samples "
    "as the signal sum, the exact H0 law is R^2 = stat_n * F(stat_n, stat_n): "
    "gate with R > sqrt(stat_n * det_threshold_f(pfa, stat_n)) — exact for "
    "every stat_n (a chi-square gate would realize tens of times the priced "
    "pfa). Payload prompts only; reset() re-arms.\n",
    NULL },
  { "stat_n", (getter)BurstDespreader_getprop_stat_n, NULL,
    "Number of prompts folded into the burst statistics so far.\n", NULL },
  { NULL }
};

static PyObject *
BurstDespreaderObj_destroy (BurstDespreaderObject *self,
                            PyObject              *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      burst_despreader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BurstDespreaderObj_enter (BurstDespreaderObject *self,
                          PyObject              *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
BurstDespreaderObj_exit (BurstDespreaderObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      burst_despreader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef BurstDespreaderObj_methods[] = {

  { "steps", (PyCFunction)(void *)BurstDespreaderObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Despread a cf32 block; emit one complex prompt symbol per code period.\n"
    "\n"
    "Streams: a partial symbol is carried in state across calls. Each "
    "emitted\n"
    "symbol is the complex prompt integrate-and-dump (carrier-wiped,\n"
    "code-stripped) — its sign is the BPSK decision, its phase/magnitude the\n"
    "soft information. During a `burst_despreader_set_acq` preamble no\n"
    "symbols are emitted (the loops are pulling in); payload symbols follow.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input CF32 samples, length x_len.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of prompt symbols written into out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDespreader\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)   # length-31 chip "
    "code\n"
    ">>> bits = rng.integers(0, 2, 30).astype(np.uint8)   # payload bits\n"
    ">>> chips = np.where(code & 1, -1.0, 1.0)             # 0 -> +1, 1 -> "
    "-1\n"
    ">>> syms = np.where(bits == 1, -1.0, 1.0)             # BPSK symbols\n"
    ">>> tx = np.concatenate(\n"
    "...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)\n"
    ">>> d = BurstDespreader(code, sf=31, sps=4)\n"
    ">>> sym = d.steps(tx)                                 # one prompt per "
    "symbol\n"
    ">>> sym.shape\n"
    "(30,)\n"
    ">>> hard = (sym.real < 0).astype(np.uint8)            # BPSK decision\n"
    ">>> float(np.mean(hard != bits))                      # payload "
    "recovered\n"
    "0.0\n" },
  { "steps_max_out", (PyCFunction)BurstDespreaderObj_steps_max_out,
    METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "bits", (PyCFunction)(void *)BurstDespreaderObj_bits,
    METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Despread a cf32 block; emit one hard BPSK bit per code period.\n"
    "\n"
    "Same streaming kernel as burst_despreader_steps(), but emits the hard\n"
    "decision `crealf(prompt) >= 0` instead of the complex symbol.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input CF32 samples, length x_len.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Number of hard bits written into out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDespreader\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> bits = rng.integers(0, 2, 30).astype(np.uint8)\n"
    ">>> chips = np.where(code & 1, -1.0, 1.0)\n"
    ">>> syms = np.where(bits == 1, -1.0, 1.0)\n"
    ">>> tx = np.concatenate(\n"
    "...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)\n"
    ">>> d = BurstDespreader(code, sf=31, sps=4)\n"
    ">>> rec = d.bits(tx)                             # hard 0/1 per symbol\n"
    ">>> rec.shape\n"
    "(30,)\n"
    ">>> e = np.mean(rec != bits)                     # up to a BPSK sign "
    "flip\n"
    ">>> round(float(min(e, 1.0 - e)), 4)\n"
    "0.0\n"
    ">>> round(d.lock_metric, 3)\n"
    "1.0\n" },
  { "bits_max_out", (PyCFunction)BurstDespreaderObj_bits_max_out, METH_NOARGS,
    "bits_max_out() -> int\n\nMax output length bits() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_acq", (PyCFunction)(void *)BurstDespreaderObj_set_acq,
    METH_VARARGS | METH_KEYWORDS,
    "set_acq(acq_code, acq_reps) -> None\n"
    "\n"
    "Enable preamble-aided pull-in: track acq_reps periods of the (distinct) "
    "acq_code coherently before despreading the payload with the data code. "
    "Call before feeding the burst; clears when the preamble is consumed.\n"
    "\n"
    "Track acq_reps periods of acq_code coherently (the unmodulated, "
    "repeated\n"
    "acquisition preamble — a full ±pi phase discriminator, so the loops "
    "pull\n"
    "in even a wide residual) before switching to the data code for the\n"
    "payload. Call before feeding the burst; the acq mode clears\n"
    "automatically once the preamble is consumed, and re-arms on\n"
    "burst_despreader_reset(). NB: set_acq re-arms the PREAMBLE only — the\n"
    "cumulative burst statistics (lock_metric / snr_est / lock_stat / "
    "stat_n)\n"
    "are re-armed by burst_despreader_reset(); call it between bursts.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "acq_code : NDArray[np.uint8]\n"
    "    Acquisition code (0/1), length acq_code_len; copied.\n"
    "acq_reps : int\n"
    "    Number of acq-code periods in the preamble.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDespreader\n"
    ">>> rng = np.random.default_rng(5)\n"
    ">>> acq = rng.integers(0, 2, 128).astype(np.uint8)    # long acq code\n"
    ">>> data_code = rng.integers(0, 2, 32).astype(np.uint8)\n"
    ">>> pbits = rng.integers(0, 2, 40).astype(np.uint8)\n"
    ">>> asig = np.where(acq & 1, -1.0, 1.0)\n"
    ">>> dch = np.where(data_code & 1, -1.0, 1.0)\n"
    ">>> psyms = np.where(pbits == 1, -1.0, 1.0)\n"
    ">>> pre = np.concatenate([np.repeat(asig, 4) for _ in range(4)])\n"
    ">>> pay = np.concatenate([np.repeat(s * dch, 4) for s in psyms])\n"
    ">>> burst = np.concatenate([pre, pay]).astype(np.complex64)\n"
    ">>> d = BurstDespreader(data_code, sf=32, sps=4)\n"
    ">>> d.set_acq(acq, 4)                    # 4 preamble reps, pulls loops "
    "in\n"
    ">>> out = d.bits(burst)                  # preamble emits nothing\n"
    ">>> out.shape                            # only the payload symbols come "
    "out\n"
    "(40,)\n"
    ">>> e = np.mean(out != pbits)\n"
    ">>> round(float(min(e, 1.0 - e)), 4)\n"
    "0.0\n" },
  { "reset", (PyCFunction)BurstDespreaderObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loops to the create-time phase/frequency; preserve config.\n"
    "\n"
    "Restores the carrier NCO to the seed frequency and the code phase to "
    "the\n"
    "seed chip, zeroes the loop accumulators, and clears the cumulative "
    "burst\n"
    "read-backs (lock_metric / snr_est / lock_stat / stat_n) — the spreading\n"
    "code and bandwidths are kept. Call it between bursts so each burst's\n"
    "statistics start clean; a prior burst_despreader_set_acq() preamble is\n"
    "also re-armed.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDespreader\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> chips = np.where(code & 1, -1.0, 1.0)\n"
    ">>> syms = np.where(rng.integers(0, 2, 30) == 1, -1.0, 1.0)\n"
    ">>> tx = np.concatenate(\n"
    "...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)\n"
    ">>> d = BurstDespreader(code, sf=31, sps=4)\n"
    ">>> first = d.bits(tx)\n"
    ">>> d.reset()                              # re-arm for a new burst\n"
    ">>> np.array_equal(first, d.bits(tx))      # same result as a fresh "
    "object\n"
    "True\n" },
  { "state_bytes", (PyCFunction)BurstDespreaderObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the BurstDespreaderObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)BurstDespreaderObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the BurstDespreaderObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)BurstDespreaderObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the BurstDespreaderObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)BurstDespreaderObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)BurstDespreaderObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a BurstDespreader be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BurstDespreader\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)BurstDespreaderObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the BurstDespreader.\n"
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

static PyTypeObject BurstDespreaderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.BurstDespreader",
  .tp_basicsize                           = sizeof (BurstDespreaderObject),
  .tp_dealloc = (destructor)BurstDespreaderObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a burst despreader instance.\n",
  .tp_methods = BurstDespreaderObj_methods,
  .tp_getset  = BurstDespreader_getset,
  .tp_new     = BurstDespreaderObj_new,
  .tp_init    = (initproc)BurstDespreaderObj_init,
};
