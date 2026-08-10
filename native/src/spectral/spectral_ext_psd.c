/*
 * spectral_ext_psd.c — PSD type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* PSDObject — wraps psd_state_t *       */
/* ======================================================== */

#include "psd/psd_core.h"

typedef struct
{
  PyObject_HEAD psd_state_t *handle;
} PSDObject;

static void
PSDObj_dealloc (PSDObject *self)
{
  if (self->handle)
    psd_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PSDObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PSDObject *self = (PSDObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PSDObj_init (PSDObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]    = { "n",          "fs",   "window", "beta",  "pad",
                               "full_scale", "bits", "mode",   "alpha", NULL };
  unsigned long long n_raw = 1024;
  double             fs    = 1.0;
  const char        *window_str = "hann";
  float              beta       = 0.0f;
  unsigned long long pad_raw    = 1;
  double             full_scale = 1.0;
  unsigned long long bits_raw   = 0;
  const char        *mode_str   = "mean";
  double             alpha      = 0.1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KdsfKdKsd", kwlist, &n_raw,
                                    &fs, &window_str, &beta, &pad_raw,
                                    &full_scale, &bits_raw, &mode_str, &alpha))
    return -1;
  size_t n      = (size_t)n_raw;
  int    window = 0;
  if (strcmp (window_str, "hann") == 0)
    window = 0;
  else if (strcmp (window_str, "kaiser") == 0)
    window = 1;
  else if (strcmp (window_str, "blackman-harris") == 0)
    window = 2;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", "
                    "\"blackman-harris\", got '%s'",
                    window_str);
      return -1;
    }
  size_t pad  = (size_t)pad_raw;
  size_t bits = (size_t)bits_raw;
  int    mode = 0;
  if (strcmp (mode_str, "mean") == 0)
    mode = 0;
  else if (strcmp (mode_str, "exp") == 0)
    mode = 1;
  else if (strcmp (mode_str, "maxhold") == 0)
    mode = 2;
  else if (strcmp (mode_str, "minhold") == 0)
    mode = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "mode must be one of \"mean\", \"exp\", \"maxhold\", "
                    "\"minhold\", got '%s'",
                    mode_str);
      return -1;
    }
  self->handle
      = psd_create (n, fs, window, beta, pad, full_scale, bits, mode, alpha);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "psd_create returned NULL");
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
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float complex *x     = (const float complex *)PyArray_DATA (x_arr);
  size_t               x_len = (size_t)PyArray_SIZE (x_arr);
  psd_accumulate (self->handle, x, x_len);
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
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float *x     = (const float *)PyArray_DATA (x_arr);
  size_t       x_len = (size_t)PyArray_SIZE (x_arr);
  psd_accumulate_real (self->handle, x, x_len);
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
  psd_reset (self->handle);
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
  return PyLong_FromSize_t (psd_psd_db_max_out (self->handle));
}

static PyObject *
PSDObj_psd_db (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = psd_psd_db_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = psd_psd_db (self->handle, (size_t)n,
                                 (float *)PyArray_DATA (out_arr), _cap);
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = psd_psd_db_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = psd_psd_db (self->handle, (size_t)n, _d0, _cap);
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
  return PyLong_FromSize_t (psd_psd_dbhz_max_out (self->handle));
}

static PyObject *
PSDObj_psd_dbhz (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = psd_psd_dbhz_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = psd_psd_dbhz (self->handle, (size_t)n,
                                   (float *)PyArray_DATA (out_arr), _cap);
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = psd_psd_dbhz_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = psd_psd_dbhz (self->handle, (size_t)n, _d0, _cap);
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
  return PyLong_FromSize_t (psd_power_twosided_max_out (self->handle));
}

static PyObject *
PSDObj_power_twosided (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = psd_power_twosided_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = psd_power_twosided (
          self->handle, (size_t)n, (float *)PyArray_DATA (out_arr), _cap);
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = psd_power_twosided_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = psd_power_twosided (self->handle, (size_t)n, _d0, _cap);
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
  return PyLong_FromSize_t (psd_power_onesided_max_out (self->handle));
}

static PyObject *
PSDObj_power_onesided (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = psd_power_onesided_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = psd_power_onesided (
          self->handle, (size_t)n, (float *)PyArray_DATA (out_arr), _cap);
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = psd_power_onesided_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = psd_power_onesided (self->handle, (size_t)n, _d0, _cap);
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
  return PyLong_FromSize_t (psd_band_power_max_out (self->handle));
}

static PyObject *
PSDObj_band_power (PSDObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "bands", "out", NULL };
  PyObject      *bands_obj = NULL;
  PyArrayObject *bands_arr = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &bands_obj,
                                    &out_obj))
    return NULL;
  bands_arr = (PyArrayObject *)PyArray_FROM_OTF (bands_obj, NPY_DOUBLE,
                                                 NPY_ARRAY_C_CONTIGUOUS);
  if (!bands_arr)
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
          Py_DECREF (bands_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (bands_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = psd_band_power_max_out (self->handle);
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
      size_t n_out = psd_band_power (self->handle,
                                     (const double *)PyArray_DATA (bands_arr),
                                     (size_t)PyArray_SIZE (bands_arr),
                                     (float *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (bands_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (bands_arr);
  size_t _cap  = psd_band_power_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (bands_arr);
      return NULL;
    }
  float *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out
      = psd_band_power (self->handle, (const double *)PyArray_DATA (bands_arr),
                        (size_t)PyArray_SIZE (bands_arr), _d0, _cap);
  Py_DECREF (bands_arr);
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
  PyArrayObject *bands_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bands_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
  if (!bands_arr)
    {
      return NULL;
    }
  const double *bands     = (const double *)PyArray_DATA (bands_arr);
  size_t        bands_len = (size_t)PyArray_SIZE (bands_arr);
  double        y = psd_total_band_power (self->handle, bands, bands_len);
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
  double y = psd_occupied_bw (self->handle, fraction);
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
  double y = psd_noise_floor (self->handle);
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
  double y = psd_snr (self->handle, lo_hz, hi_hz);
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
  double y = psd_sfdr (self->handle, min_db);
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
  return PyLong_FromSize_t (psd_state_bytes (self->handle));
}

static PyObject *
PSDObj_get_state (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = psd_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  psd_get_state (self->handle, PyBytes_AS_STRING (_b));
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
  if ((size_t)PyBytes_GET_SIZE (arg) != psd_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (psd_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
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
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
PSD_getprop_nfft (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
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
  return PyFloat_FromDouble (self->handle->full_scale);
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
  return PyFloat_FromDouble (self->handle->enbw * self->handle->fs
                             / (double)self->handle->n);
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
      (unsigned long long)(size_t)self->handle->avg->count);
}
static PyObject *
PSD_getprop_mode (PSDObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)(int)self->handle->avg->mode);
}

static PyGetSetDef PSD_getset[]
    = { { "n", (getter)PSD_getprop_n, NULL,
          "Window / frame length (samples).\n", NULL },
        { "nfft", (getter)PSD_getprop_nfft, NULL,
          "Zero-padded transform length.\n", NULL },
        { "fs", (getter)PSD_getprop_fs, NULL, "Sample rate, Hz.\n", NULL },
        { "full_scale", (getter)PSD_getprop_full_scale, NULL,
          "Amplitude that reads 0 dBFS.\n", NULL },
        { "bits", (getter)PSD_getprop_bits, NULL,
          "ADC depth that set full_scale, else 0.\n", NULL },
        { "enbw", (getter)PSD_getprop_enbw, NULL,
          "Equivalent noise bandwidth, bins.\n", NULL },
        { "rbw", (getter)PSD_getprop_rbw, NULL, "Rbw.\n", NULL },
        { "count", (getter)PSD_getprop_count, NULL,
          "Frames folded in so far.\n", NULL },
        { "mode", (getter)PSD_getprop_mode, NULL, "Reduction mode.\n", NULL },
        { NULL } };

static PyObject *
PSDObj_destroy (PSDObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      psd_destroy (self->handle);
      self->handle = NULL;
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
      psd_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef PSDObj_methods[] = {

  { "accumulate", (PyCFunction)(void *)PSDObj_accumulate,
    METH_VARARGS | METH_KEYWORDS,
    "accumulate(x) -> None\n"
    "\n"
    "Window, FFT and fold floor(n_in/n) cf32 frames into the average.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
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
    "Window, zero-pad, FFT and fold floor(n_in/n) real frames into the "
    "average.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Real samples (f32).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
    "    >>> obj.accumulate_real(np.zeros(4, dtype=np.float32))\n" },
  { "reset", (PyCFunction)PSDObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Discard the running average; counters return to zero.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
    "    >>> obj.reset()\n" },
  { "psd_db", (PyCFunction)(void *)PSDObj_psd_db, METH_VARARGS | METH_KEYWORDS,
    "psd_db(n=1) -> ndarray\n"
    "\n"
    "Averaged power spectrum in dB (None before any accumulate).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(1024, 1.0, \"hann\", 0.0, 1, 1.0, 0, \"mean\", 0.1)\n"
    "    >>> y = obj.psd_db(4)\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "psd_db_max_out", (PyCFunction)PSDObj_psd_db_max_out, METH_NOARGS,
    "psd_db_max_out() -> int\n\nMax output length psd_db() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "psd_dbhz", (PyCFunction)(void *)PSDObj_psd_dbhz,
    METH_VARARGS | METH_KEYWORDS,
    "psd_dbhz(n=1) -> ndarray\n"
    "\n"
    "Averaged power spectral density in dB/Hz (None before any accumulate).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(1024, 1.0, \"hann\", 0.0, 1, 1.0, 0, \"mean\", 0.1)\n"
    "    >>> y = obj.psd_dbhz(4)\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "psd_dbhz_max_out", (PyCFunction)PSDObj_psd_dbhz_max_out, METH_NOARGS,
    "psd_dbhz_max_out() -> int\n\nMax output length psd_dbhz() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "power_twosided", (PyCFunction)(void *)PSDObj_power_twosided,
    METH_VARARGS | METH_KEYWORDS,
    "power_twosided(n=1) -> ndarray\n"
    "\n"
    "Averaged linear power, DC-centred two-sided (length nfft); "
    "cg^2-normalised.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(1024, 1.0, \"hann\", 0.0, 1, 1.0, 0, \"mean\", 0.1)\n"
    "    >>> y = obj.power_twosided(4)\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "power_twosided_max_out", (PyCFunction)PSDObj_power_twosided_max_out,
    METH_NOARGS,
    "power_twosided_max_out() -> int\n\nMax output length power_twosided() "
    "can produce for the current state.\nUse to size the ``out=`` buffer." },
  { "power_onesided", (PyCFunction)(void *)PSDObj_power_onesided,
    METH_VARARGS | METH_KEYWORDS,
    "power_onesided(n=1) -> ndarray\n"
    "\n"
    "Averaged linear power, one-sided fold (length nfft/2+1); "
    "cg^2-normalised.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(1024, 1.0, \"hann\", 0.0, 1, 1.0, 0, \"mean\", 0.1)\n"
    "    >>> y = obj.power_onesided(4)\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "power_onesided_max_out", (PyCFunction)PSDObj_power_onesided_max_out,
    METH_NOARGS,
    "power_onesided_max_out() -> int\n\nMax output length power_onesided() "
    "can produce for the current state.\nUse to size the ``out=`` buffer." },
  { "band_power", (PyCFunction)(void *)PSDObj_band_power,
    METH_VARARGS | METH_KEYWORDS,
    "band_power(bands) -> ndarray\n"
    "\n"
    "Integrated power per band in dB; bands = [lo0,hi0,lo1,hi1,...] Hz.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bands : NDArray[np.float64]\n"
    "    Flat `[lo,hi,...]` band edges, Hz.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    min(n_bands, max_out), or 0 if empty.\n"
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
    "band_power_max_out() -> int\n\nMax output length band_power() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "total_band_power", (PyCFunction)(void *)PSDObj_total_band_power,
    METH_VARARGS | METH_KEYWORDS,
    "total_band_power(bands) -> float\n"
    "\n"
    "Total integrated power across all bands in dB.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bands : NDArray[np.float64]\n"
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
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
    "    >>> obj.total_band_power(np.zeros(4, dtype=np.float64))\n"
    "    0.0\n" },
  { "occupied_bw", (PyCFunction)(void *)PSDObj_occupied_bw,
    METH_VARARGS | METH_KEYWORDS,
    "occupied_bw(fraction) -> float\n"
    "\n"
    "Occupied bandwidth in Hz holding the given fraction of total power.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "fraction : float\n"
    "    Power fraction in (0, 1], e.g. 0.99.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Occupied bandwidth in Hz (0 if empty or no power).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
    "    >>> obj.occupied_bw(0.0)\n"
    "    0.0\n" },
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
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
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
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
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
    "    >>> from doppler import PSD\n"
    "    >>> obj = PSD(n=1024, fs=1.0, window=\"hann\", beta=0.0, pad=1, "
    "full_scale=1.0, bits=0, mode=\"mean\", alpha=0.1)\n"
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
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
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
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
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
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
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
  { NULL }
};

static PyTypeObject PSDObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.PSD",
  .tp_basicsize                           = sizeof (PSDObject),
  .tp_dealloc                             = (destructor)PSDObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an averaging PSD estimator.\n",
  .tp_methods = PSDObj_methods,
  .tp_getset  = PSD_getset,
  .tp_new     = PSDObj_new,
  .tp_init    = (initproc)PSDObj_init,
};
