/*
 * spectral_ext_fft.c — FFT type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* FFTObject — wraps fft_state_t *       */
/* ======================================================== */

#include "fft/fft_core.h"

typedef struct
{
  PyObject_HEAD fft_state_t *handle;
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
} FFTObject;

static void
FFTObj_dealloc (FFTObject *self)
{
  if (self->handle)
    fft_destroy (self->handle);
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
FFTObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FFTObject *self = (FFTObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FFTObj_init (FFTObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "n", "sign", "nthreads", NULL };
  unsigned long long n_raw    = 1024;
  int                sign     = -1;
  int                nthreads = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kii", kwlist, &n_raw, &sign,
                                    &nthreads))
    return -1;
  size_t n     = (size_t)n_raw;
  self->handle = fft_create (n, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fft_create returned NULL");
      return -1;
    }
  {
    size_t _max = fft_execute_cf64_max_out (self->handle);
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
    size_t _max = fft_execute_cf32_max_out (self->handle);
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
    size_t _max = fft_execute_inplace_cf64_max_out (self->handle);
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
    size_t _max = fft_execute_inplace_cf32_max_out (self->handle);
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
FFTObj_reset (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  fft_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FFTObj_execute_cf64_max_out (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft_execute_cf64_max_out (self->handle));
}

static PyObject *
FFTObj_execute_cf64 (FFTObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = fft_execute_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_cf64 (
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
      size_t _max = fft_execute_cf64_max_out (self->handle);
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
  size_t n_out = fft_execute_cf64 (
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
FFTObj_execute_cf32_max_out (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft_execute_cf32_max_out (self->handle));
}

static PyObject *
FFTObj_execute_cf32 (FFTObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = fft_execute_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_cf32 (
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
      size_t _max = fft_execute_cf32_max_out (self->handle);
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
  size_t n_out = fft_execute_cf32 (
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

/* Integer-IQ executes (ci16/ci8): interleaved int16/int8 I/Q in, CF32 out.
 * The int->float convert is folded into the FFT input read (no separate cvt
 * pass).  Output reuses the same pre-allocated CF32 buffer as execute_cf32.
 * Hand-written: not manifest-declared (no params shape for a fused
 * dtype-convert-on-read execute), preserved across jm apply/regenerate. */
static PyObject *
FFTObj_execute_int (FFTObject *self, PyObject *args, int is8)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, is8 ? NPY_INT8 : NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  /* interleaved I/Q: 2 ints per complex sample */
  Py_ssize_t n = PyArray_SIZE (in_arr) / 2;
  if (!self->_execute_cf32_buf)
    {
      size_t _max = fft_execute_cf32_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_cf32_buf = malloc (_max * sizeof (float complex));
      if (!self->_execute_cf32_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out
      = is8 ? fft_execute_ci8 (self->handle,
                               (const int8_t *)PyArray_DATA (in_arr),
                               (size_t)n, self->_execute_cf32_buf)
            : fft_execute_ci16 (self->handle,
                                (const int16_t *)PyArray_DATA (in_arr),
                                (size_t)n, self->_execute_cf32_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_cf32_buf);
  if (!arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFTObj_execute_ci16 (FFTObject *self, PyObject *args)
{
  return FFTObj_execute_int (self, args, 0);
}

static PyObject *
FFTObj_execute_ci8 (FFTObject *self, PyObject *args)
{
  return FFTObj_execute_int (self, args, 1);
}

static PyObject *
FFTObj_execute_inplace_cf64_max_out (FFTObject *self,
                                     PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft_execute_inplace_cf64_max_out (self->handle));
}

static PyObject *
FFTObj_execute_inplace_cf64 (FFTObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = fft_execute_inplace_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_inplace_cf64 (
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
      size_t _max = fft_execute_inplace_cf64_max_out (self->handle);
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
  size_t n_out = fft_execute_inplace_cf64 (
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
FFTObj_execute_inplace_cf32_max_out (FFTObject *self,
                                     PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft_execute_inplace_cf32_max_out (self->handle));
}

static PyObject *
FFTObj_execute_inplace_cf32 (FFTObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = fft_execute_inplace_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_inplace_cf32 (
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
      size_t _max = fft_execute_inplace_cf32_max_out (self->handle);
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
  size_t n_out = fft_execute_inplace_cf32 (
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
FFT_getprop_n (FFTObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
FFT_getprop_sign (FFTObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->sign);
}

static PyGetSetDef FFT_getset[]
    = { { "n", (getter)FFT_getprop_n, NULL, "N.\n", NULL },
        { "sign", (getter)FFT_getprop_sign, NULL, "Sign.\n", NULL },
        { NULL } };

static PyObject *
FFTObj_destroy (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FFTObj_enter (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FFTObj_exit (FFTObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFTObj_methods[] = {
  { "reset", (PyCFunction)FFTObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "execute_cf64", (PyCFunction)FFTObj_execute_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf64(x) -> ndarray\n"
    "\n"
    "Compute an out-of-place 1-D DFT on a double-precision complex input. The "
    "output is written to a fresh caller-supplied buffer; in and out must not "
    "alias.  The transform is unnormalised: the inverse DFT (sign=+1) does "
    "NOT divide by n.  Both buffers must be exactly state->n elements long.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_cf64_max_out", (PyCFunction)FFTObj_execute_cf64_max_out,
    METH_NOARGS,
    "execute_cf64_max_out() -> int\n\nMax output length execute_cf64() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_cf32", (PyCFunction)FFTObj_execute_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf32(x) -> ndarray\n"
    "\n"
    "Compute an out-of-place 1-D DFT on a single-precision complex input. "
    "Identical to fft_execute_cf64() but operates on float complex (CF32) "
    "buffers, halving memory bandwidth relative to the double-precision "
    "variant. Output is unnormalised; in and out must not alias.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_cf32_max_out", (PyCFunction)FFTObj_execute_cf32_max_out,
    METH_NOARGS,
    "execute_cf32_max_out() -> int\n\nMax output length execute_cf32() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_ci16", (PyCFunction)FFTObj_execute_ci16, METH_VARARGS,
    "execute_ci16(iq) -> ndarray\n"
    "\n"
    "Out-of-place 1-D FFT directly on interleaved int16 I/Q (CF32 out).\n"
    "The int16->float convert (v/32768, full-scale +/-1.0) is fused into\n"
    "the transform, so it is faster than i16_to_f32 then execute_cf32.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_ci16(np.zeros(2048, dtype=np.int16))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_ci8", (PyCFunction)FFTObj_execute_ci8, METH_VARARGS,
    "execute_ci8(iq) -> ndarray\n"
    "\n"
    "Out-of-place 1-D FFT directly on interleaved int8 I/Q (CF32 out).\n"
    "As execute_ci16 but int8 input (v/128, full-scale +/-1.0).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_ci8(np.zeros(2048, dtype=np.int8))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_inplace_cf64", (PyCFunction)FFTObj_execute_inplace_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf64(x) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF64). The copy step lets "
    "callers preserve their input while keeping the output buffer hot in "
    "cache.  Semantically identical to fft_execute_cf64() for separate in / "
    "out pointers; use this variant when the caller already owns out and "
    "wants the result there without a second allocation.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_inplace_cf64_max_out",
    (PyCFunction)FFTObj_execute_inplace_cf64_max_out, METH_NOARGS,
    "execute_inplace_cf64_max_out() -> int\n\nMax output length "
    "execute_inplace_cf64() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "execute_inplace_cf32", (PyCFunction)FFTObj_execute_inplace_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf32(x) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF32). Single-precision "
    "variant of fft_execute_inplace_cf64().  Copies state->n CF32 samples "
    "from in to out, then transforms out with the CF32 pocketfft plan.  in is "
    "left unmodified.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_inplace_cf32_max_out",
    (PyCFunction)FFTObj_execute_inplace_cf32_max_out, METH_NOARGS,
    "execute_inplace_cf32_max_out() -> int\n\nMax output length "
    "execute_inplace_cf32() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "destroy", (PyCFunction)FFTObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)FFTObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)FFTObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject FFTObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.FFT",
  .tp_basicsize                           = sizeof (FFTObject),
  .tp_dealloc                             = (destructor)FFTObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a reusable 1-D FFT engine for a fixed length and sign. Two "
    "pocketfft plans are created at construction time — one for CF64 and one "
    "for CF32 — so execute calls carry no plan-setup overhead.  The same "
    "instance may be called repeatedly for independent input vectors of the "
    "same length.  nthreads is accepted for API parity but is ignored; "
    "pocketfft plans are single-threaded.\n",
  .tp_methods = FFTObj_methods,
  .tp_getset  = FFT_getset,
  .tp_new     = FFTObj_new,
  .tp_init    = (initproc)FFTObj_init,
};
