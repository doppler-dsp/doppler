/* jm:generated spectral_ext_psd.c */
/*
 * spectral_ext_psd.c — PSD type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in spectral_ext_psd_extra.c.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* PSDObject — wraps dp_psd_state_t *       */
/* ======================================================== */

#include "doppler/psd/psd_core.h"

typedef struct
{
  PyObject_HEAD dp_psd_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
} PSDObject;

static void
PSDObj_dealloc (PSDObject *self)
{
  if (self->handle)
    dp_psd_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PSDObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  PSDObject *self = (PSDObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PSDObj_init (PSDObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "n",    "fs",   "window", "beta", "pad", "full_scale",
          "bits", "mode", "alpha",  "hop",  NULL };
  PyObject          *n_obj      = NULL;
  unsigned long long n_raw      = 1024;
  double             fs         = 1.0;
  const char        *window_str = "hann";
  float              beta       = 0.0f;
  PyObject          *pad_obj    = NULL;
  unsigned long long pad_raw    = 1;
  double             full_scale = 1.0;
  PyObject          *bits_obj   = NULL;
  unsigned long long bits_raw   = 0;
  const char        *mode_str   = "mean";
  double             alpha      = 0.1;
  PyObject          *hop_obj    = NULL;
  unsigned long long hop_raw    = 0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "|OdsfOdOsdO", kwlist, &n_obj, &fs, &window_str, &beta,
          &pad_obj, &full_scale, &bits_obj, &mode_str, &alpha, &hop_obj))
    return -1;
  if (n_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      n_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t n      = (size_t)n_raw;
  int    window = 0;
  if (strcmp (window_str, "hann") == 0)
    window = DP_PSD_WINDOW_HANN;
  else if (strcmp (window_str, "kaiser") == 0)
    window = DP_PSD_WINDOW_KAISER;
  else if (strcmp (window_str, "blackman-harris") == 0)
    window = DP_PSD_WINDOW_BLACKMAN_HARRIS;
  else if (strcmp (window_str, "rect") == 0)
    window = DP_PSD_WINDOW_RECT;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", "
                    "\"blackman-harris\", \"rect\", got '%s'",
                    window_str);
      return -1;
    }
  if (pad_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (pad_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      pad_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((pad_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || pad_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "pad: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t pad = (size_t)pad_raw;
  if (bits_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (bits_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      bits_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((bits_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || bits_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "bits: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t bits = (size_t)bits_raw;
  int    mode = 0;
  if (strcmp (mode_str, "mean") == 0)
    mode = ACC_TRACE_MEAN;
  else if (strcmp (mode_str, "exp") == 0)
    mode = ACC_TRACE_EXP;
  else if (strcmp (mode_str, "maxhold") == 0)
    mode = ACC_TRACE_MAXHOLD;
  else if (strcmp (mode_str, "minhold") == 0)
    mode = ACC_TRACE_MINHOLD;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "mode must be one of \"mean\", \"exp\", \"maxhold\", "
                    "\"minhold\", got '%s'",
                    mode_str);
      return -1;
    }
  if (hop_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (hop_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      hop_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((hop_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || hop_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "hop: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t hop   = (size_t)hop_raw;
  self->handle = dp_psd_create_hop (n, fs, window, beta, pad, full_scale, bits,
                                    mode, alpha, hop);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "PSD: invalid parameter (need n >= 2, pad >= 1 with n "
                       "* pad within the largest FFT, finite fs > 0, bits <= "
                       "64, hop in 1..n (0 = n), a finite full_scale > 0 "
                       "when bits = 0, a window whose taps sum to a finite "
                       "nonzero value (not hann at n = 2, nor a NaN or "
                       "overflowing kaiser beta), and in exp mode 0 < alpha "
                       "<= 1)");
      return -1;
    }
  return 0;
}

static PyObject *
PSDObj_accumulate (PSDObject *self, PyObject *args, PyObject *kwds)
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
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  void *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    dp_psd_accumulate (_jm_h, x, x_len);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  Py_RETURN_NONE;
}

static PyObject *
PSDObj_accumulate_real (PSDObject *self, PyObject *args, PyObject *kwds)
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
      = jm_array_arg (x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float *x     = (const float *)PyArray_DATA (x_arr);
  size_t       x_len = (size_t)PyArray_SIZE (x_arr);
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  void *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    dp_psd_accumulate_real (_jm_h, x, x_len);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  Py_RETURN_NONE;
}

static PyObject *
PSDObj_reset (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_psd_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
PSDObj_psd_db_max_out (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_psd_psd_db_max_out (self->handle));
}

static PyObject *
PSDObj_psd_db (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|O", _kwlist, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_psd_psd_db_max_out (self->handle);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = dp_psd_psd_db (self->handle,
                                    (float *)PyArray_DATA (out_arr), _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "PSD.psd_db: wrote %zu elements into a buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _need = dp_psd_psd_db_max_out (self->handle);
  size_t _cap  = dp_psd_psd_db_max_out (self->handle);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "PSD.psd_db: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_psd_psd_db (self->handle, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "PSD.psd_db: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
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
PSDObj_psd_dbhz_max_out (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_psd_psd_dbhz_max_out (self->handle));
}

static PyObject *
PSDObj_psd_dbhz (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|O", _kwlist, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_psd_psd_dbhz_max_out (self->handle);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = dp_psd_psd_dbhz (self->handle,
                                      (float *)PyArray_DATA (out_arr), _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "PSD.psd_dbhz: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _need = dp_psd_psd_dbhz_max_out (self->handle);
  size_t _cap  = dp_psd_psd_dbhz_max_out (self->handle);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "PSD.psd_dbhz: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_psd_psd_dbhz (self->handle, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "PSD.psd_dbhz: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
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
PSDObj_power_twosided_max_out (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_psd_power_twosided_max_out (self->handle));
}

static PyObject *
PSDObj_power_twosided (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|O", _kwlist, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_psd_power_twosided_max_out (self->handle);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = dp_psd_power_twosided (
          self->handle, (float *)PyArray_DATA (out_arr), _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "PSD.power_twosided: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _need = dp_psd_power_twosided_max_out (self->handle);
  size_t _cap  = dp_psd_power_twosided_max_out (self->handle);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "PSD.power_twosided: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_psd_power_twosided (self->handle, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "PSD.power_twosided: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
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
PSDObj_power_onesided_max_out (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_psd_power_onesided_max_out (self->handle));
}

static PyObject *
PSDObj_power_onesided (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|O", _kwlist, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_psd_power_onesided_max_out (self->handle);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = dp_psd_power_onesided (
          self->handle, (float *)PyArray_DATA (out_arr), _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "PSD.power_onesided: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _need = dp_psd_power_onesided_max_out (self->handle);
  size_t _cap  = dp_psd_power_onesided_max_out (self->handle);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "PSD.power_onesided: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_psd_power_onesided (self->handle, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "PSD.power_onesided: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
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
PSDObj_band_power_max_out (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_psd_band_power_max_out (self->handle));
}

static PyObject *
PSDObj_band_power (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bands", "out", NULL };
  PyObject    *bands_obj = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &bands_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *bands_arr
      = jm_array_arg (bands_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS, "bands");
  if (!bands_arr)
    {
      return NULL;
    }
  const double *bands     = (const double *)PyArray_DATA (bands_arr);
  size_t        bands_len = (size_t)PyArray_SIZE (bands_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (bands_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (bands_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_psd_band_power_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (bands_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (bands_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (bands_arr);
          return NULL;
        }
      size_t n_out = dp_psd_band_power (self->handle, bands, bands_len,
                                        (float *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (bands_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "PSD.band_power: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _need = (size_t)PyArray_SIZE (bands_arr);
  size_t _cap  = dp_psd_band_power_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (bands_arr);
      PyErr_Format (PyExc_OverflowError,
                    "PSD.band_power: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (bands_arr);
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_psd_band_power (self->handle, bands, bands_len, _d0, _cap);
  Py_DECREF (bands_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "PSD.band_power: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
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
PSDObj_total_band_power (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bands", NULL };
  PyObject    *bands_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &bands_obj))
    return NULL;
  PyArrayObject *bands_arr
      = jm_array_arg (bands_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS, "bands");
  if (!bands_arr)
    {
      return NULL;
    }
  const double *bands     = (const double *)PyArray_DATA (bands_arr);
  size_t        bands_len = (size_t)PyArray_SIZE (bands_arr);
  double        y = dp_psd_total_band_power (self->handle, bands, bands_len);
  Py_DECREF (bands_arr);
  return PyFloat_FromDouble (y);
}

static PyObject *
PSDObj_occupied_bw (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "fraction", NULL };
  double       fraction  = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &fraction))
    return NULL;
  double y = dp_psd_occupied_bw (self->handle, fraction);
  return PyFloat_FromDouble (y);
}

static PyObject *
PSDObj_noise_floor (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double y = dp_psd_noise_floor (self->handle);
  return PyFloat_FromDouble (y);
}

static PyObject *
PSDObj_snr (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "lo_hz", "hi_hz", NULL };
  double       lo_hz     = 0.0;
  double       hi_hz     = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", _kwlist, &lo_hz, &hi_hz))
    return NULL;
  double y = dp_psd_snr (self->handle, lo_hz, hi_hz);
  return PyFloat_FromDouble (y);
}

static PyObject *
PSDObj_sfdr (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "min_db", NULL };
  float        min_db    = 0.0f;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "f", _kwlist, &min_db))
    return NULL;
  double y = dp_psd_sfdr (self->handle, min_db);
  return PyFloat_FromDouble (y);
}

static PyObject *
PSDObj_state_bytes (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_psd_state_bytes (self->handle));
}

static PyObject *
PSDObj_get_state (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_psd_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_psd_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
PSDObj_set_state (PSDObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != dp_psd_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_psd_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
PSD_getprop_n (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)(self->handle->k.n));
}
static PyObject *
PSD_getprop_nfft (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(self->handle->k.nfft));
}
static PyObject *
PSD_getprop_fs (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
PSD_getprop_full_scale (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->k.full_scale));
}
static PyObject *
PSD_getprop_bits (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->bits);
}
static PyObject *
PSD_getprop_enbw (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->enbw);
}
static PyObject *
PSD_getprop_rbw (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (
      (self->handle->enbw * self->handle->fs / (double)self->handle->k.n));
}
static PyObject *
PSD_getprop_count (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)((size_t)self->handle->avg->count));
}
static PyObject *
PSD_getprop_mode (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)((int)self->handle->avg->mode));
}

static PyGetSetDef PSD_getset[] = {
  { "n", (getter)PSD_getprop_n, NULL, "Window / frame length (samples).\n",
    NULL },
  { "nfft", (getter)PSD_getprop_nfft, NULL, "Zero-padded transform length.\n",
    NULL },
  { "fs", (getter)PSD_getprop_fs, NULL, "Sample rate, Hz.\n", NULL },
  { "full_scale", (getter)PSD_getprop_full_scale, NULL,
    "Amplitude that reads 0 dBFS.\n", NULL },
  { "bits", (getter)PSD_getprop_bits, NULL,
    "ADC depth that set full_scale, else 0.\n", NULL },
  { "enbw", (getter)PSD_getprop_enbw, NULL,
    "Equivalent noise bandwidth, bins (the kernel's, copied at create).\n",
    NULL },
  { "rbw", (getter)PSD_getprop_rbw, NULL, "Rbw.\n", NULL },
  { "count", (getter)PSD_getprop_count, NULL,
    "Frames folded into the averaged trace so far.\n", NULL },
  { "mode", (getter)PSD_getprop_mode, NULL,
    "Trace reduction mode, as the enum index: 0 mean, 1 exp, 2 maxhold, 3 "
    "minhold.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
PSDObj_destroy (PSDObject *self, PyObject *Py_UNUSED (ignored))
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
      dp_psd_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
PSDObj_enter (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PSDObj_exit (PSDObject *self, PyObject *args)
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
      dp_psd_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef PSDObj_methods[] = {

  { "accumulate", (PyCFunction)(void *)PSDObj_accumulate,
    METH_VARARGS | METH_KEYWORDS,
    "accumulate(x) -> None\n"
    "\n"
    "Window, FFT and fold every cf32 frame the stream completes into the "
    "average; a partial frame is carried to the next call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Complex baseband samples (cf32).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import PSD\n"
    ">>> n = 64\n"
    ">>> w = PSD(n=n, fs=1.0, window=\"hann\", mode=\"mean\")\n"
    ">>> k = 8\n"
    ">>> x = np.exp(2j*np.pi*k*np.arange(n)/n).astype(np.complex64)\n"
    ">>> for _ in range(4):\n"
    "...     w.accumulate(x)\n"
    ">>> psd = w.psd_db()\n"
    ">>> psd.shape\n"
    "(64,)\n"
    ">>> int(np.argmax(psd)) == n // 2 + k\n"
    "True\n"
    ">>> w.count\n"
    "4\n" },
  { "accumulate_real", (PyCFunction)(void *)PSDObj_accumulate_real,
    METH_VARARGS | METH_KEYWORDS,
    "accumulate_real(x) -> None\n"
    "\n"
    "Window, zero-pad, FFT and fold the whole real frames of a block into the "
    "average, at hop = n. A trailing partial frame is dropped. It neither "
    "reads nor writes the carry of the complex accumulate, so mixing the two "
    "on one object mixes two streams.\n"
    "\n"
    "This call stays on whole frames at hop = n: it folds floor(n_in / n)\n"
    "frames and drops a trailing partial frame. It neither reads nor writes\n"
    "the carry of the complex accumulate call, so mixing the two on one\n"
    "object mixes two streams, and a hop below n is not used by this call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.float32]\n"
    "    Real samples (f32).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> obj.accumulate_real(np.zeros(4, dtype=np.float32))\n" },
  { "reset", (PyCFunction)PSDObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Discard the running average; counters return to zero.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> obj.reset()\n" },
  { "psd_db", (PyCFunction)(void *)PSDObj_psd_db, METH_VARARGS | METH_KEYWORDS,
    "psd_db(out) -> ndarray | None\n"
    "\n"
    "Averaged power spectrum in dB (None before any accumulate).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Destination, at least nfft float32 elements.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    min(nfft, max_out), or 0 if empty. None when the C call writes no\n"
    "    output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> y = obj.psd_db()\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "psd_db_max_out", (PyCFunction)PSDObj_psd_db_max_out, METH_NOARGS,
    "psd_db_max_out() -> int\n"
    "\n"
    "Output capacity hint for psd_db(); equals nfft.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "psd_dbhz", (PyCFunction)(void *)PSDObj_psd_dbhz,
    METH_VARARGS | METH_KEYWORDS,
    "psd_dbhz(out) -> ndarray | None\n"
    "\n"
    "Averaged power spectral density in dB/Hz (None before any accumulate).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Optional pre-allocated output buffer. When given, the result is\n"
    "    written into it and the returned array is a view of exactly the\n"
    "    samples produced; when omitted, a fresh array is allocated.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    Output. None when the C call writes no output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import PSD\n"
    ">>> w = PSD(n=32, fs=2.0, window=\"hann\", mode=\"mean\")\n"
    ">>> w.accumulate(np.ones(32, dtype=np.complex64))\n"
    ">>> a = w.psd_db(); b = w.psd_dbhz()\n"
    ">>> bool(np.allclose(a - b, (a - b)[0]))   # offset is a constant\n"
    "True\n" },
  { "psd_dbhz_max_out", (PyCFunction)PSDObj_psd_dbhz_max_out, METH_NOARGS,
    "psd_dbhz_max_out() -> int\n"
    "\n"
    "Output capacity hint for psd_dbhz(); equals nfft.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "power_twosided", (PyCFunction)(void *)PSDObj_power_twosided,
    METH_VARARGS | METH_KEYWORDS,
    "power_twosided(out) -> ndarray | None\n"
    "\n"
    "Averaged linear power, DC-centred two-sided (length nfft); "
    "cg^2-normalised.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Destination, at least nfft float32 elements.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    min(nfft, max_out), or 0 if empty. None when the C call writes no\n"
    "    output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> y = obj.power_twosided()\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "power_twosided_max_out", (PyCFunction)PSDObj_power_twosided_max_out,
    METH_NOARGS,
    "power_twosided_max_out() -> int\n"
    "\n"
    "Output capacity hint for dp_psd_power_twosided(); equals nfft.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "power_onesided", (PyCFunction)(void *)PSDObj_power_onesided,
    METH_VARARGS | METH_KEYWORDS,
    "power_onesided(out) -> ndarray | None\n"
    "\n"
    "Averaged linear power, one-sided fold (length nfft/2+1); "
    "cg^2-normalised.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Destination, at least nfft/2 + 1 float32 elements.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    min(nfft/2 + 1, max_out), or 0 if empty. None when the C call\n"
    "    writes no output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> y = obj.power_onesided()\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "power_onesided_max_out", (PyCFunction)PSDObj_power_onesided_max_out,
    METH_NOARGS,
    "power_onesided_max_out() -> int\n"
    "\n"
    "Output capacity hint for dp_psd_power_onesided(); equals nfft/2+1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "band_power", (PyCFunction)(void *)PSDObj_band_power,
    METH_VARARGS | METH_KEYWORDS,
    "band_power(bands, out) -> ndarray | None\n"
    "\n"
    "Integrated power per band in dB; bands = [lo0,hi0,lo1,hi1,...] Hz. None "
    "when there is nothing to report: before any accumulate, or no complete "
    "lo/hi pair.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bands : npt.NDArray[np.float64]\n"
    "    Flat `[lo,hi,...]` band edges, Hz.\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Destination, at least n_bands float32 elements.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    The number of bands written, min(n_bands, max_out); 0 (Python None)\n"
    "    when there is nothing to report. None when the C call writes no\n"
    "    output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import PSD\n"
    ">>> w = PSD(n=64, fs=1.0, window=\"hann\", mode=\"mean\")\n"
    ">>> w.accumulate(np.ones(64, dtype=np.complex64))\n"
    ">>> pb = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))\n"
    ">>> pb.shape\n"
    "(2,)\n" },
  { "band_power_max_out", (PyCFunction)PSDObj_band_power_max_out, METH_NOARGS,
    "band_power_max_out() -> int\n"
    "\n"
    "Output capacity hint for band_power(); 0 (binding sizes from bands).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "total_band_power", (PyCFunction)(void *)PSDObj_total_band_power,
    METH_VARARGS | METH_KEYWORDS,
    "total_band_power(bands) -> float\n"
    "\n"
    "Total integrated power across all bands in dB.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bands : npt.NDArray[np.float64]\n"
    "    Flat `[lo,hi,...]` band edges, Hz.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Total band power in dB (dB floor if empty).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> obj.total_band_power(np.zeros(4, dtype=np.float64))\n"
    "    0.0\n" },
  { "occupied_bw", (PyCFunction)(void *)PSDObj_occupied_bw,
    METH_VARARGS | METH_KEYWORDS,
    "occupied_bw(fraction) -> float\n"
    "\n"
    "Occupied bandwidth in Hz holding the given fraction of total power.\n"
    "\n"
    "Calls obw_from_power on the averaged spectrum, so the search and its\n"
    "domain are that function's and the two cannot disagree.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "fraction : float\n"
    "    Power fraction in the OPEN interval (0, 1), e.g. 0.99. 1 is\n"
    "    excluded: all of the power in float is every bin that rounding\n"
    "    residue reached, which has no stable answer.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Occupied bandwidth in Hz (0 if empty or no power); NaN when\n"
    "    fraction is outside (0, 1), before any frame too, rather than a\n"
    "    silently clamped answer.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import PSD\n"
    ">>> w = PSD(n=64, fs=64.0, window=\"rect\")\n"
    ">>> w.accumulate(np.ones(64, dtype=np.complex64))  # all power at DC\n"
    ">>> w.occupied_bw(0.99)                            # one bin, fs / 64\n"
    "1.0\n"
    ">>> w.occupied_bw(1.0)                             # outside (0, 1)\n"
    "nan\n" },
  { "noise_floor", (PyCFunction)PSDObj_noise_floor, METH_NOARGS,
    "noise_floor() -> float\n"
    "\n"
    "Median of the averaged dB trace (noise-floor estimate).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Median dB level (0 if empty).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> obj.noise_floor()\n"
    "    0.0\n" },
  { "snr", (PyCFunction)(void *)PSDObj_snr, METH_VARARGS | METH_KEYWORDS,
    "snr(lo_hz, hi_hz) -> float\n"
    "\n"
    "Peak-in-band level minus noise floor, in dB.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "lo_hz : float\n"
    "    Band lower edge, Hz.\n"
    "hi_hz : float\n"
    "    Band upper edge, Hz.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    SNR in dB (0 if empty).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> obj.snr(0.0, 0.0)\n"
    "    0.0\n" },
  { "sfdr", (PyCFunction)(void *)PSDObj_sfdr, METH_VARARGS | METH_KEYWORDS,
    "sfdr(min_db) -> float\n"
    "\n"
    "Spurious-free dynamic range in dB from the top two peaks.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "min_db : float\n"
    "    Minimum peak level considered, dB.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Carrier-minus-highest-spur level in dB (0 if fewer than two peaks).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1, hop=0)\n"
    "    >>> obj.sfdr(0.0)\n"
    "    0.0\n" },
  { "state_bytes", (PyCFunction)PSDObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the PSD has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)PSDObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the PSD has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)PSDObj_set_state, METH_O,
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
    "``RuntimeError`` if the PSD has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)PSDObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)PSDObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a PSD be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "PSD\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)PSDObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the PSD.\n"
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

static PyTypeObject PSDObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.spectral.PSD",
  .tp_basicsize                           = sizeof (PSDObject),
  .tp_dealloc                             = (destructor)PSDObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create an averaging PSD estimator whose frames are spaced by a hop.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int, default 1024\n"
    "    Window / frame length in samples. Must be >= 2.\n"
    "fs : float, default 1.0\n"
    "    Sample rate in Hz (used for dB/Hz and band frequencies); finite and "
    ">\n"
    "    0.\n"
    "window : Literal[\"hann\", \"kaiser\", \"blackman-harris\", \"rect\"], "
    "default \"hann\"\n"
    "    The window: \"hann\" (the default), \"kaiser\", \"blackman-harris\" "
    "or \"rect\"\n"
    "    (no taper: best resolution, worst leakage).\n"
    "\n"
    "    - ``\"hann\"`` — Hann: the default.\n"
    "    - ``\"kaiser\"`` — Kaiser, shaped by `beta`.\n"
    "    - ``\"blackman-harris\"`` — 4-term Blackman-Harris.\n"
    "    - ``\"rect\"`` — Rectangular: no taper, best resolution, worst "
    "leakage.\n"
    "beta : float, default 0.0\n"
    "    Kaiser beta (ignored by every other window). One that makes the "
    "window\n"
    "    non-finite -- NaN, or large enough to overflow it -- is refused.\n"
    "pad : int, default 1\n"
    "    Zero-pad factor (>= 1; 0 is refused); nfft = next_pow_two(n * pad).\n"
    "full_scale : float, default 1.0\n"
    "    Amplitude that reads 0 dBFS in the dB getters; finite and > 0. "
    "Ignored\n"
    "    when bits > 0.\n"
    "bits : int, default 0\n"
    "    ADC depth: when > 0, sets full_scale = 2^(bits-1) (the single\n"
    "    definition of the dBFS reference); 0 = use full_scale directly. At "
    "most\n"
    "    64.\n"
    "mode : Literal[\"mean\", \"exp\", \"maxhold\", \"minhold\"], default "
    "\"mean\"\n"
    "    Averaging mode: \"mean\", \"exp\", \"maxhold\" or \"minhold\".\n"
    "\n"
    "    - ``\"mean\"`` — Linear running mean of all frames.\n"
    "    - ``\"exp\"`` — Exponential moving average (alpha).\n"
    "    - ``\"maxhold\"`` — Per-bin running maximum.\n"
    "    - ``\"minhold\"`` — Per-bin running minimum.\n"
    "alpha : float, default 0.1\n"
    "    EMA smoothing factor, exp mode only, where it must lie in (0, 1]: "
    "the\n"
    "    rule is the AccTrace averager's, whose create refuses anything else, "
    "so\n"
    "    this does too. Ignored, and so not checked, in the other modes.\n"
    "hop : int, default 0\n"
    "    Samples between frame starts, 1..n. 0 means n, which gives\n"
    "    non-overlapping frames; the default. A hop above n would leave "
    "samples\n"
    "    between frames that are never kept, so it is refused.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``PSD: invalid\n"
    "    parameter (need n >= 2, pad >= 1 with n * pad within the largest "
    "FFT,\n"
    "    finite fs > 0, bits <= 64, hop in 1..n (0 = n), a finite full_scale "
    "> 0\n"
    "    when bits = 0, a window whose taps sum to a finite nonzero value "
    "(not\n"
    "    hann at n = 2, nor a NaN or overflowing kaiser beta), and in exp "
    "mode 0\n"
    "    < alpha <= 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import PSD\n"
    ">>> w = PSD(n=1024, fs=1.0e6, window=\"kaiser\", beta=8.0, "
    "mode=\"mean\", hop=512)\n"
    ">>> w.n, w.fs\n"
    "(1024, 1000000.0)\n"
    ">>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)\n"
    "True\n",
  .tp_methods = PSDObj_methods,
  .tp_getset  = PSD_getset,
  .tp_new     = PSDObj_new,
  .tp_init    = (initproc)PSDObj_init,
};
