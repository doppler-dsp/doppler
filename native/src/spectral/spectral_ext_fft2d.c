/*
 * spectral_ext_fft2d.c — FFT2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* FFT2DObject — wraps fft2d_state_t *       */
/* ======================================================== */

#include "fft2d/fft2d_core.h"

typedef struct
{
  PyObject_HEAD fft2d_state_t *handle;
  double complex
        *_execute_cf64_buf;     /* pre-allocated output for execute_cf64 */
  size_t _execute_cf64_buf_cap; /* allocated capacity for execute_cf64 */
  void **_execute_cf64_retired; /* gh-219 deferred free */
  size_t _execute_cf64_retired_n;
  size_t _execute_cf64_retired_cap;
  float complex *_execute_cf32_buf; /* pre-allocated output for execute_cf32 */
  size_t _execute_cf32_buf_cap;     /* allocated capacity for execute_cf32 */
  void **_execute_cf32_retired;     /* gh-219 deferred free */
  size_t _execute_cf32_retired_n;
  size_t _execute_cf32_retired_cap;
  double complex *_execute_inplace_cf64_buf;    /* pre-allocated output for
                                                   execute_inplace_cf64 */
  size_t _execute_inplace_cf64_buf_cap;         /* allocated capacity for
                                                   execute_inplace_cf64 */
  void         **_execute_inplace_cf64_retired; /* gh-219 deferred free */
  size_t         _execute_inplace_cf64_retired_n;
  size_t         _execute_inplace_cf64_retired_cap;
  float complex *_execute_inplace_cf32_buf; /* pre-allocated output for
                                               execute_inplace_cf32 */
  size_t _execute_inplace_cf32_buf_cap;     /* allocated capacity for
                                               execute_inplace_cf32 */
  void **_execute_inplace_cf32_retired;     /* gh-219 deferred free */
  size_t _execute_inplace_cf32_retired_n;
  size_t _execute_inplace_cf32_retired_cap;
} FFT2DObject;

static void
FFT2DObj_dealloc (FFT2DObject *self)
{
  if (self->handle)
    fft2d_destroy (self->handle);
  free (self->_execute_cf64_buf);
  for (size_t _i = 0; _i < self->_execute_cf64_retired_n; _i++)
    free (self->_execute_cf64_retired[_i]);
  free (self->_execute_cf64_retired);
  free (self->_execute_cf32_buf);
  for (size_t _i = 0; _i < self->_execute_cf32_retired_n; _i++)
    free (self->_execute_cf32_retired[_i]);
  free (self->_execute_cf32_retired);
  free (self->_execute_inplace_cf64_buf);
  for (size_t _i = 0; _i < self->_execute_inplace_cf64_retired_n; _i++)
    free (self->_execute_inplace_cf64_retired[_i]);
  free (self->_execute_inplace_cf64_retired);
  free (self->_execute_inplace_cf32_buf);
  for (size_t _i = 0; _i < self->_execute_inplace_cf32_retired_n; _i++)
    free (self->_execute_inplace_cf32_retired[_i]);
  free (self->_execute_inplace_cf32_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FFT2DObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FFT2DObject *self = (FFT2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FFT2DObj_init (FFT2DObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "ny", "nx", "sign", "nthreads", NULL };
  unsigned long long ny_raw   = 64;
  unsigned long long nx_raw   = 64;
  int                sign     = -1;
  int                nthreads = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKii", kwlist, &ny_raw,
                                    &nx_raw, &sign, &nthreads))
    return -1;
  size_t ny    = (size_t)ny_raw;
  size_t nx    = (size_t)nx_raw;
  self->handle = fft2d_create (ny, nx, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fft2d_create returned NULL");
      return -1;
    }
  {
    size_t _max = fft2d_execute_cf64_max_out (self->handle);
    if (_max)
      {
        self->_execute_cf64_buf = malloc (_max * sizeof (double complex));
        if (!self->_execute_cf64_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_cf64_buf_cap = _max;
      }
  }
  {
    size_t _max = fft2d_execute_cf32_max_out (self->handle);
    if (_max)
      {
        self->_execute_cf32_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_cf32_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_cf32_buf_cap = _max;
      }
  }
  {
    size_t _max = fft2d_execute_inplace_cf64_max_out (self->handle);
    if (_max)
      {
        self->_execute_inplace_cf64_buf
            = malloc (_max * sizeof (double complex));
        if (!self->_execute_inplace_cf64_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_inplace_cf64_buf_cap = _max;
      }
  }
  {
    size_t _max = fft2d_execute_inplace_cf32_max_out (self->handle);
    if (_max)
      {
        self->_execute_inplace_cf32_buf
            = malloc (_max * sizeof (float complex));
        if (!self->_execute_inplace_cf32_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_inplace_cf32_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
FFT2DObj_reset (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  fft2d_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FFT2DObj_execute_cf64_max_out (FFT2DObject *self,
                               PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_cf64_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_cf64 (FFT2DObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft2d_execute_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_cf64 (
          self->handle, (const double complex *)PyArray_DATA (in_arr),
          (size_t)n, (double complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
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
  if (!self->_execute_cf64_buf || self->_execute_cf64_buf_cap < _need)
    {
      size_t _max = fft2d_execute_cf64_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_cf64_buf
          && self->_execute_cf64_retired_n == self->_execute_cf64_retired_cap)
        {
          size_t _rcap = self->_execute_cf64_retired_cap
                             ? self->_execute_cf64_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_cf64_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_cf64_retired     = _rt;
          self->_execute_cf64_retired_cap = _rcap;
        }
      double complex *_tmp = malloc (_max * sizeof (double complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_cf64_buf)
        self->_execute_cf64_retired[self->_execute_cf64_retired_n++]
            = self->_execute_cf64_buf;
      self->_execute_cf64_buf     = _tmp;
      self->_execute_cf64_buf_cap = _max;
    }
  size_t n_out = fft2d_execute_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf64_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128,
                                             self->_execute_cf64_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFT2DObj_execute_cf32_max_out (FFT2DObject *self,
                               PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_cf32_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_cf32 (FFT2DObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft2d_execute_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_cf32 (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
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
  size_t _need = (size_t)n;
  if (!self->_execute_cf32_buf || self->_execute_cf32_buf_cap < _need)
    {
      size_t _max = fft2d_execute_cf32_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_cf32_buf
          && self->_execute_cf32_retired_n == self->_execute_cf32_retired_cap)
        {
          size_t _rcap = self->_execute_cf32_retired_cap
                             ? self->_execute_cf32_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_cf32_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_cf32_retired     = _rt;
          self->_execute_cf32_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_cf32_buf)
        self->_execute_cf32_retired[self->_execute_cf32_retired_n++]
            = self->_execute_cf32_buf;
      self->_execute_cf32_buf     = _tmp;
      self->_execute_cf32_buf_cap = _max;
    }
  size_t n_out = fft2d_execute_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf32_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_cf32_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFT2DObj_execute_inplace_cf64_max_out (FFT2DObject *self,
                                       PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_inplace_cf64_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_inplace_cf64 (FFT2DObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft2d_execute_inplace_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_inplace_cf64 (
          self->handle, (const double complex *)PyArray_DATA (in_arr),
          (size_t)n, (double complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
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
  if (!self->_execute_inplace_cf64_buf
      || self->_execute_inplace_cf64_buf_cap < _need)
    {
      size_t _max = fft2d_execute_inplace_cf64_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_inplace_cf64_buf
          && self->_execute_inplace_cf64_retired_n
                 == self->_execute_inplace_cf64_retired_cap)
        {
          size_t _rcap = self->_execute_inplace_cf64_retired_cap
                             ? self->_execute_inplace_cf64_retired_cap * 2
                             : 4;
          void **_rt   = realloc (self->_execute_inplace_cf64_retired,
                                  _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_inplace_cf64_retired     = _rt;
          self->_execute_inplace_cf64_retired_cap = _rcap;
        }
      double complex *_tmp = malloc (_max * sizeof (double complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_inplace_cf64_buf)
        self->_execute_inplace_cf64_retired
            [self->_execute_inplace_cf64_retired_n++]
            = self->_execute_inplace_cf64_buf;
      self->_execute_inplace_cf64_buf     = _tmp;
      self->_execute_inplace_cf64_buf_cap = _max;
    }
  size_t n_out = fft2d_execute_inplace_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf64_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128,
                                             self->_execute_inplace_cf64_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFT2DObj_execute_inplace_cf32_max_out (FFT2DObject *self,
                                       PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_inplace_cf32_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_inplace_cf32 (FFT2DObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft2d_execute_inplace_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_inplace_cf32 (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
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
  size_t _need = (size_t)n;
  if (!self->_execute_inplace_cf32_buf
      || self->_execute_inplace_cf32_buf_cap < _need)
    {
      size_t _max = fft2d_execute_inplace_cf32_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_inplace_cf32_buf
          && self->_execute_inplace_cf32_retired_n
                 == self->_execute_inplace_cf32_retired_cap)
        {
          size_t _rcap = self->_execute_inplace_cf32_retired_cap
                             ? self->_execute_inplace_cf32_retired_cap * 2
                             : 4;
          void **_rt   = realloc (self->_execute_inplace_cf32_retired,
                                  _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_inplace_cf32_retired     = _rt;
          self->_execute_inplace_cf32_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_inplace_cf32_buf)
        self->_execute_inplace_cf32_retired
            [self->_execute_inplace_cf32_retired_n++]
            = self->_execute_inplace_cf32_buf;
      self->_execute_inplace_cf32_buf     = _tmp;
      self->_execute_inplace_cf32_buf_cap = _max;
    }
  size_t n_out = fft2d_execute_inplace_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf32_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_inplace_cf32_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}
static PyObject *
FFT2D_getprop_ny (FFT2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
FFT2D_getprop_nx (FFT2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
FFT2D_getprop_sign (FFT2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->sign);
}

static PyGetSetDef FFT2D_getset[]
    = { { "ny", (getter)FFT2D_getprop_ny, NULL, "Ny.\n", NULL },
        { "nx", (getter)FFT2D_getprop_nx, NULL, "Nx.\n", NULL },
        { "sign", (getter)FFT2D_getprop_sign, NULL, "Sign.\n", NULL },
        { NULL } };

static PyObject *
FFT2DObj_destroy (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fft2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FFT2DObj_enter (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FFT2DObj_exit (FFT2DObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fft2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFT2DObj_methods[] = {
  { "reset", (PyCFunction)FFT2DObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "execute_cf64", (PyCFunction)FFT2DObj_execute_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf64(x) -> ndarray\n"
    "\n"
    "Compute an out-of-place 2-D DFT on a double-precision complex grid. in "
    "is a flat row-major CF64 array of length ny*nx.  The output is written "
    "to the caller-supplied out buffer (also ny*nx); the two must not alias.  "
    "The transform is unnormalised.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT2D\n"
    "    >>> obj = FFT2D(64, 64, -1, 1)\n"
    "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_cf64_max_out", (PyCFunction)FFT2DObj_execute_cf64_max_out,
    METH_NOARGS,
    "execute_cf64_max_out() -> int\n\nMax output length execute_cf64() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_cf32", (PyCFunction)FFT2DObj_execute_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf32(x) -> ndarray\n"
    "\n"
    "Compute an out-of-place 2-D DFT on a single-precision complex grid. "
    "Single-precision variant of fft2d_execute_cf64().  Accepts and returns "
    "flat row-major CF32 arrays of length ny*nx.  Output is unnormalised; in "
    "and out must not alias.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT2D\n"
    "    >>> obj = FFT2D(64, 64, -1, 1)\n"
    "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_cf32_max_out", (PyCFunction)FFT2DObj_execute_cf32_max_out,
    METH_NOARGS,
    "execute_cf32_max_out() -> int\n\nMax output length execute_cf32() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_inplace_cf64", (PyCFunction)FFT2DObj_execute_inplace_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf64(x) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF64 2-D). The ny*nx CF64 "
    "samples from in are first memcpy'd to out; the 2-D DFT is then applied "
    "to out in-place.  in is left unmodified. Useful when the caller owns out "
    "and wants to preserve in.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT2D\n"
    "    >>> obj = FFT2D(64, 64, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_inplace_cf64_max_out",
    (PyCFunction)FFT2DObj_execute_inplace_cf64_max_out, METH_NOARGS,
    "execute_inplace_cf64_max_out() -> int\n\nMax output length "
    "execute_inplace_cf64() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "execute_inplace_cf32", (PyCFunction)FFT2DObj_execute_inplace_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf32(x) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF32 2-D). "
    "Single-precision variant of fft2d_execute_inplace_cf64().  Copies ny*nx "
    "CF32 samples then applies the CF32 2-D pocketfft plan to out.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT2D\n"
    "    >>> obj = FFT2D(64, 64, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_inplace_cf32_max_out",
    (PyCFunction)FFT2DObj_execute_inplace_cf32_max_out, METH_NOARGS,
    "execute_inplace_cf32_max_out() -> int\n\nMax output length "
    "execute_inplace_cf32() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "destroy", (PyCFunction)FFT2DObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)FFT2DObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)FFT2DObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject FFT2DObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.FFT2D",
  .tp_basicsize                           = sizeof (FFT2DObject),
  .tp_dealloc                             = (destructor)FFT2DObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a reusable 2-D FFT engine for a fixed ny×nx grid. Two pocketfft "
    "2-D plans are built at construction time — one CF64, one CF32.  All "
    "execute calls accept and return flat row-major arrays of length ny*nx; "
    "the Python layer may reshape them with .reshape(ny, nx). nthreads is "
    "accepted for API parity but ignored.\n",
  .tp_methods = FFT2DObj_methods,
  .tp_getset  = FFT2D_getset,
  .tp_new     = FFT2DObj_new,
  .tp_init    = (initproc)FFT2DObj_init,
};
