/* jm:generated spectral_ext_corr2d.c */
/*
 * spectral_ext_corr2d.c — Corr2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in spectral_ext_corr2d_extra.c.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* Corr2DObject — wraps dp_corr2d_state_t *       */
/* ======================================================== */

#include "doppler/corr2d/corr2d_core.h"

typedef struct
{
  PyObject_HEAD dp_corr2d_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
} Corr2DObject;

static void
Corr2DObj_dealloc (Corr2DObject *self)
{
  if (self->handle)
    dp_corr2d_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Corr2DObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  Corr2DObject *self = (Corr2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
Corr2DObj_init (Corr2DObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "ref", "dwell", "nthreads", "ny_out", "nx_out", "col_out", NULL };
  PyObject          *ref_obj    = NULL;
  PyObject          *dwell_obj  = NULL;
  unsigned long long dwell_raw  = 1;
  int                nthreads   = 1;
  PyObject          *ny_out_obj = NULL;
  unsigned long long ny_out_raw = 0;
  PyObject          *nx_out_obj = NULL;
  unsigned long long nx_out_raw = 0;
  int                col_out    = -1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|OiOOi", kwlist, &ref_obj,
                                    &dwell_obj, &nthreads, &ny_out_obj,
                                    &nx_out_obj, &col_out))
    return -1;
  if (dwell_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (dwell_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      dwell_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((dwell_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || dwell_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "dwell: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t dwell = (size_t)dwell_raw;
  if (ny_out_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (ny_out_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      ny_out_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((ny_out_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || ny_out_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "ny_out: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t ny_out = (size_t)ny_out_raw;
  if (nx_out_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (nx_out_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      nx_out_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((nx_out_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || nx_out_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "nx_out: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t         nx_out = (size_t)nx_out_raw;
  PyArrayObject *ref_arr
      = jm_array_arg (ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "ref");
  if (!ref_arr)
    {
      return -1;
    }
  if (PyArray_NDIM (ref_arr) != 2)
    {
      PyErr_SetString (PyExc_ValueError, "ref must be a 2-D array");
      Py_DECREF (ref_arr);
      return -1;
    }
  size_t ref_dim0 = (size_t)PyArray_DIM (ref_arr, 0);
  size_t ref_dim1 = (size_t)PyArray_DIM (ref_arr, 1);
  self->handle    = dp_corr2d_create (
      (const float _Complex *)PyArray_DATA (ref_arr), ref_dim0, ref_dim1,
      dwell, nthreads, ny_out, nx_out, col_out);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "Corr2D: invalid parameter (need dwell >= 1, and "
                       "ny_out and nx_out each 0 or at least the ref extent)");
      return -1;
    }
  return 0;
}

static PyObject *
Corr2DObj_reset (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_corr2d_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
Corr2DObj_execute_max_out (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_corr2d_execute_max_out (self->handle));
}

static PyObject *
Corr2DObj_execute (Corr2DObject *self, PyObject *args, PyObject *kwds)
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
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_corr2d_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t n_out = dp_corr2d_execute (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "input length is not ny*nx; execute takes exactly "
                           "one frame");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Corr2D.execute: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
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
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Corr2D.execute: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t n_out = dp_corr2d_execute (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "input length is not ny*nx; execute takes exactly one "
                       "frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Corr2D.execute: wrote %zu elements into a buffer of %zu",
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
Corr2DObj_execute_all_max_out (Corr2DObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n_in = 0;
  if (!PyArg_ParseTuple (args, "n", &n_in))
    return NULL;
  return PyLong_FromSize_t (
      dp_corr2d_execute_all_max_out (self->handle, (size_t)n_in));
}

static PyObject *
Corr2DObj_execute_all (Corr2DObject *self, PyObject *args, PyObject *kwds)
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
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t         n     = PyArray_SIZE (in_arr);
  dp_corr2d_state_t *state = self->handle;
  size_t             _cols = (size_t)(state->n_out);
  if (_cols == 0)
    {
      PyErr_SetString (PyExc_RuntimeError,
                       "Corr2D.execute_all: out_cols evaluated to 0");
      Py_DECREF (in_arr);
      return NULL;
    }
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      if (PyArray_NDIM (out_arr) != 2
          || (size_t)PyArray_DIM (out_arr, 1) != _cols)
        {
          PyErr_Format (
              PyExc_ValueError,
              "out must be a 2-D array with %zu columns, shape (rows, %zu)",
              _cols, _cols);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax = dp_corr2d_execute_all_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float _Complex *_ng1 = (float _Complex *)PyArray_DATA (out_arr);
      size_t          n_out;
      void           *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_corr2d_execute_all (_jm_h, _ng0, (size_t)n, _ng1, _cap);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "execute_all: the input must be a whole, nonzero "
                           "number of frames, and out= must hold every map");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Corr2D.execute_all: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp _odim = (npy_intp)n_out;
      if ((size_t)_odim % _cols != 0)
        {
          PyErr_Format (PyExc_RuntimeError,
                        "Corr2D.execute_all: kernel returned %zu elements, "
                        "not a whole number of %zu-wide rows",
                        (size_t)_odim, _cols);
          Py_DECREF (out_arr);
          return NULL;
        }
      npy_intp  _odims[2] = { _odim / (npy_intp)_cols, (npy_intp)_cols };
      PyObject *_oview = PyArray_SimpleNewFromData (2, _odims, NPY_COMPLEX64,
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
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_all_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Corr2D.execute_all: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_corr2d_execute_all (_jm_h, _ng0, (size_t)n, _d0, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "execute_all: the input must be a whole, nonzero "
                       "number of frames, and out= must hold every map");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Corr2D.execute_all: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  npy_intp _odim = (npy_intp)n_out;
  if ((size_t)_odim % _cols != 0)
    {
      PyErr_Format (PyExc_RuntimeError,
                    "Corr2D.execute_all: kernel returned %zu elements, not a "
                    "whole number of %zu-wide rows",
                    (size_t)_odim, _cols);
      Py_DECREF (arr0);
      return NULL;
    }
  npy_intp     _odims[2] = { _odim / (npy_intp)_cols, (npy_intp)_cols };
  PyArray_Dims _rs0      = { _odims, 2 };
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
Corr2DObj_execute_coherent_max_out (Corr2DObject *self,
                                    PyObject     *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_corr2d_execute_coherent_max_out (self->handle));
}

static PyObject *
Corr2DObj_execute_coherent (Corr2DObject *self, PyObject *args, PyObject *kwds)
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
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_corr2d_execute_coherent_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float _Complex *_ng1 = (float _Complex *)PyArray_DATA (out_arr);
      size_t          n_out;
      void           *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out
            = dp_corr2d_execute_coherent (_jm_h, _ng0, (size_t)n, _ng1, _cap);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "execute_coherent: the input must be a whole, "
                           "nonzero number of frames, and out= must hold one "
                           "map");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Corr2D.execute_coherent: wrote %zu elements into a "
                        "buffer of %zu",
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
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_coherent_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Corr2D.execute_coherent: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_corr2d_execute_coherent (_jm_h, _ng0, (size_t)n, _d0, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "execute_coherent: the input must be a whole, nonzero "
                       "number of frames, and out= must hold one map");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Corr2D.execute_coherent: wrote %zu elements into a buffer of %zu",
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
Corr2DObj_execute_power_sum_max_out (Corr2DObject *self,
                                     PyObject     *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_corr2d_execute_power_sum_max_out (self->handle));
}

static PyObject *
Corr2DObj_execute_power_sum (Corr2DObject *self, PyObject *args,
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
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_corr2d_execute_power_sum_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float *_ng1 = (float *)PyArray_DATA (out_arr);
      size_t n_out;
      void  *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out
            = dp_corr2d_execute_power_sum (_jm_h, _ng0, (size_t)n, _ng1, _cap);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "execute_power_sum: the input must be a whole, "
                           "nonzero number of frames, and out= must hold one "
                           "map");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Corr2D.execute_power_sum: wrote %zu elements into a "
                        "buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
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
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_power_sum_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Corr2D.execute_power_sum: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_corr2d_execute_power_sum (_jm_h, _ng0, (size_t)n, _d0, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "execute_power_sum: the input must be a whole, "
                       "nonzero number of frames, and out= must hold one map");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Corr2D.execute_power_sum: wrote %zu elements into a buffer of %zu",
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
Corr2DObj_execute_power_max (Corr2DObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_power_max_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Corr2D.execute_power_max: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0 || !arr1)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_DECREF (in_arr);
      return NULL;
    }
  float    *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  uint32_t *_d1 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr1);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out
        = dp_corr2d_execute_power_max (_jm_h, _ng0, (size_t)n, _d0, _d1, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      PyErr_SetString (PyExc_ValueError,
                       "execute_power_max: the input must be a whole, "
                       "nonzero number of frames (at most 2**32), and out= "
                       "must hold one map");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      PyErr_Format (
          PyExc_RuntimeError,
          "Corr2D.execute_power_max: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      PyObject *_exact = PyTuple_Pack (2, arr0, arr1);
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return _exact;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return NULL;
    }
  Py_DECREF (v0);
  PyArray_Dims _rs1 = { &_odim, 1 };
  PyObject *v1 = PyArray_Resize ((PyArrayObject *)arr1, &_rs1, 0, NPY_CORDER);
  if (!v1)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return NULL;
    }
  Py_DECREF (v1);
  PyObject *result = PyTuple_Pack (2, arr0, arr1);
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  return result;
}

static PyObject *
Corr2DObj_execute_peaks (Corr2DObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_peaks_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Corr2D.execute_peaks: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr2  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr3  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0 || !arr1 || !arr2 || !arr3)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_XDECREF (arr2);
      Py_XDECREF (arr3);
      Py_DECREF (in_arr);
      return NULL;
    }
  float    *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  uint32_t *_d1 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr1);
  uint32_t *_d2 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr2);
  float    *_d3 = (float *)PyArray_DATA ((PyArrayObject *)arr3);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_corr2d_execute_peaks (_jm_h, _ng0, (size_t)n, _d0, _d1, _d2,
                                     _d3, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      PyErr_SetString (PyExc_ValueError,
                       "execute_peaks: the input must be a whole, nonzero "
                       "number of frames, and out= must hold one record per "
                       "frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      PyErr_Format (
          PyExc_RuntimeError,
          "Corr2D.execute_peaks: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      PyObject *_exact = PyTuple_Pack (4, arr0, arr1, arr2, arr3);
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return _exact;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v0);
  PyArray_Dims _rs1 = { &_odim, 1 };
  PyObject *v1 = PyArray_Resize ((PyArrayObject *)arr1, &_rs1, 0, NPY_CORDER);
  if (!v1)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v1);
  PyArray_Dims _rs2 = { &_odim, 1 };
  PyObject *v2 = PyArray_Resize ((PyArrayObject *)arr2, &_rs2, 0, NPY_CORDER);
  if (!v2)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v2);
  PyArray_Dims _rs3 = { &_odim, 1 };
  PyObject *v3 = PyArray_Resize ((PyArrayObject *)arr3, &_rs3, 0, NPY_CORDER);
  if (!v3)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v3);
  PyObject *result = PyTuple_Pack (4, arr0, arr1, arr2, arr3);
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  Py_DECREF (arr2);
  Py_DECREF (arr3);
  return result;
}

static PyObject *
Corr2DObj_state_bytes (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_corr2d_state_bytes (self->handle));
}

static PyObject *
Corr2DObj_get_state (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_corr2d_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_corr2d_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
Corr2DObj_set_state (Corr2DObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != dp_corr2d_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_corr2d_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Corr2D_getprop_ny (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
Corr2D_getprop_nx (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
Corr2D_getprop_ny_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->ny_out);
}
static PyObject *
Corr2D_getprop_nx_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->nx_out);
}
static PyObject *
Corr2D_getprop_n_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n_out);
}
static PyObject *
Corr2D_getprop_col_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->col_out);
}
static PyObject *
Corr2D_getprop_dwell (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
Corr2D_getprop_count (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->count);
}

static PyGetSetDef Corr2D_getset[]
    = { { "ny", (getter)Corr2D_getprop_ny, NULL, "Row count.\n", NULL },
        { "nx", (getter)Corr2D_getprop_nx, NULL, "Column count.\n", NULL },
        { "ny_out", (getter)Corr2D_getprop_ny_out, NULL,
          "Output rows (== ny unless decoupled).\n", NULL },
        { "nx_out", (getter)Corr2D_getprop_nx_out, NULL,
          "Output columns (== nx unless decoupled).\n", NULL },
        { "n_out", (getter)Corr2D_getprop_n_out, NULL,
          "ny_out * nx_out — output element count.\n", NULL },
        { "col_out", (getter)Corr2D_getprop_col_out, NULL,
          "Output column, or < 0 for the full map.\n", NULL },
        { "dwell", (getter)Corr2D_getprop_dwell, NULL, "Integration depth.\n",
          NULL },
        { "count", (getter)Corr2D_getprop_count, NULL,
          "Frames accumulated (0 … dwell-1).\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
Corr2DObj_destroy (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
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
      dp_corr2d_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyObject *
Corr2DObj_enter (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Corr2DObj_exit (Corr2DObject *self, PyObject *args)
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
      dp_corr2d_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef Corr2DObj_methods[] = {
  { "reset", (PyCFunction)Corr2DObj_reset, METH_NOARGS,
    "Zero the accumulator and reset the integration counter to 0.\n"
    "Equivalent to starting a fresh dwell cycle without rebuilding FFT plans\n"
    "or recomputing ref_spec.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Corr2D(ref=ref, dwell=3)\n"
    ">>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))\n"
    ">>> c.count\n"
    "1\n"
    ">>> c.reset()\n"
    ">>> c.count\n"
    "0\n" },

  { "execute", (PyCFunction)(void *)Corr2DObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray | None\n"
    "\n"
    "Correlate one 2-D frame and optionally dump the coherent\n"
    "accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with\n"
    "ref_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷\n"
    "ny*nx). Accumulating in the frequency domain and inverting once is\n"
    "exactly the per-frame inverse summed, by linearity of the IFFT — valid\n"
    "because the dwell is **coherent** (a complex sum); a non-coherent\n"
    "(magnitude) integration could not defer the inverse. The Python wrapper\n"
    "accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny*nx\n"
    "ndarray, a no-dump returns None.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer for the correlation map (CF32, length n_out:\n"
    "    ny_out*nx_out, or ny with col_out); written only on a dump call.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64] | None\n"
    "    n_out on a dump (or max_out if smaller), 0 otherwise (None in\n"
    "    Python). If n_in is not the required length the call is REFUSED\n"
    "    with nothing read, written or counted: SIZE_MAX in C, ValueError in\n"
    "    Python (#1925). None when the C call writes no output.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``input length is not ny*nx;\n"
    "    execute takes exactly one frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Corr2D(ref=ref, dwell=2)\n"
    ">>> x = np.ones((2, 2), dtype=np.complex64)\n"
    ">>> c.execute(x) is None   # frame 1 — no dump\n"
    "True\n"
    ">>> c.execute(x).tolist()  # frame 2 — dump\n"
    "[(2+0j), (2+0j), (2+0j), (2+0j)]\n" },
  { "execute_max_out", (PyCFunction)Corr2DObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Maximum output samples per execute call (dp_corr2d_state_t::n_out --\n"
    "ny_out*nx_out normally, or ny when a single dp_corr2d_state_t::col_out\n"
    "was selected).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n" },
  { "execute_all", (PyCFunction)(void *)Corr2DObj_execute_all,
    METH_VARARGS | METH_KEYWORDS,
    "execute_all(x, out) -> ndarray\n"
    "\n"
    "Every map of a batch, unreduced: map h at `out + h * n_out`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    H * n_out values (CF32).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    H * n_out, or SIZE_MAX (nothing written) for a refused batch. From\n"
    "    Python, an (H, n_out) array, a view of `out=` when given.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``execute_all: the input must be\n"
    "    a whole, nonzero number of frames, and out= must hold every map``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> maps = dd.execute_all(frames)\n"
    ">>> maps.shape\n"
    "(2, 256)\n"
    ">>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay\n"
    "[3, 11]\n"
    ">>> dd.count                    # the dwell never moved\n"
    "0\n" },
  { "execute_all_max_out", (PyCFunction)Corr2DObj_execute_all_max_out,
    METH_VARARGS,
    "execute_all_max_out(n_in) -> int\n"
    "\n"
    "Samples dp_corr2d_execute_all() writes for n_in input samples: H *\n"
    "n_out, H = n_in / (ny * nx).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input samples about to be passed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    H * n_out, or SIZE_MAX if that overflows.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32\n"
    "512\n" },
  { "execute_coherent", (PyCFunction)(void *)Corr2DObj_execute_coherent,
    METH_VARARGS | METH_KEYWORDS,
    "execute_coherent(x, out) -> ndarray\n"
    "\n"
    "The coherent sum of a batch's maps, inverted once.\n"
    "\n"
    "The frames' product spectra are summed and the sum is inverted, as a\n"
    "dwell does: the result is bit-identical to what a fresh object with\n"
    "dwell = H dumps for the same frames, and equal to the sum of the H maps\n"
    "up to rounding. The object's own dwell is untouched.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    n_out values (CF32).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    n_out, or SIZE_MAX (nothing written) for a refused batch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``execute_coherent: the input\n"
    "    must be a whole, nonzero number of frames, and out= must hold one\n"
    "    map``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> x = np.tile(code, ny)\n"
    ">>> total = dd.execute_coherent(np.concatenate([x, -x]))\n"
    ">>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel\n"
    "True\n" },
  { "execute_coherent_max_out",
    (PyCFunction)Corr2DObj_execute_coherent_max_out, METH_NOARGS,
    "execute_coherent_max_out() -> int\n"
    "\n"
    "Samples dp_corr2d_execute_coherent() writes: n_out.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> dd.execute_coherent_max_out()\n"
    "256\n" },
  { "execute_power_sum", (PyCFunction)(void *)Corr2DObj_execute_power_sum,
    METH_VARARGS | METH_KEYWORDS,
    "execute_power_sum(x, out) -> ndarray\n"
    "\n"
    "The power of a batch's maps, summed cell by cell: each output cell\n"
    "is the sum over h of that cell's `|map_h|^2`, summed in h order in\n"
    "float.\n"
    "\n"
    "Unlike the coherent sum, phase does not cancel: frames x and -x sum to\n"
    "twice one map's power, where their coherent sum is zero.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    n_out powers (float).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    n_out, or SIZE_MAX (nothing written) for a refused batch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``execute_power_sum: the input\n"
    "    must be a whole, nonzero number of frames, and out= must hold one\n"
    "    map``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> x = np.tile(code, ny)\n"
    ">>> power = dd.execute_power_sum(np.concatenate([x, -x]))\n"
    ">>> one = np.abs(dd.execute_all(x)[0]) ** 2\n"
    ">>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel\n"
    "True\n" },
  { "execute_power_sum_max_out",
    (PyCFunction)Corr2DObj_execute_power_sum_max_out, METH_NOARGS,
    "execute_power_sum_max_out() -> int\n"
    "\n"
    "Powers dp_corr2d_execute_power_sum() writes: n_out.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> dd.execute_power_sum_max_out()\n"
    "256\n" },
  { "execute_power_max", (PyCFunction)Corr2DObj_execute_power_max,
    METH_VARARGS,
    "execute_power_max(x) -> tuple[ndarray, ndarray]\n"
    "\n"
    "The largest power over a batch's maps, cell by cell, and the map it\n"
    "came from.\n"
    "\n"
    "Each cell of out is the largest `|map_h|^2` that cell takes over the\n"
    "maps, and the same cell of out1 is the h it came from. A tie goes to\n"
    "the lowest h: a later map replaces the running maximum only when it is\n"
    "strictly greater.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "tuple[NDArray[np.float32], NDArray[np.uint32]]\n"
    "    n_out, or SIZE_MAX (nothing written) for a refused batch or H above\n"
    "    2^32. From Python, the tuple (power, index).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``execute_power_max: the input\n"
    "    must be a whole, nonzero number of frames (at most 2**32), and out=\n"
    "    must hold one map``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> power, index = dd.execute_power_max(frames)\n"
    ">>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks\n"
    "(0, 1)\n" },
  { "execute_peaks", (PyCFunction)Corr2DObj_execute_peaks, METH_VARARGS,
    "execute_peaks(x) -> tuple[ndarray, ndarray, ndarray, ndarray]\n"
    "\n"
    "One record per map of a batch: its peak power, where the peak is,\n"
    "and its mean power.\n"
    "\n"
    "Entry h of each output describes map h: out its largest `|map|^2`, out1\n"
    "and out2 the row and column of that cell (the first in row-major order\n"
    "on a tie), and out3 the mean `|map|^2` over its n_out cells. A map is\n"
    "ny_out x nx_out; with col_out it is ny x 1, and the column reported is\n"
    "col_out. The peak-to-mean of each map is out over out3, entry by entry:\n"
    "a statistic of each map, which the same statistic of a pointwise\n"
    "maximum over maps does not give.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "tuple[NDArray[np.float32], NDArray[np.uint32], NDArray[np.uint32], "
    "NDArray[np.float32]]\n"
    "    H, or SIZE_MAX (nothing written) for a refused batch. From Python,\n"
    "    the tuple (peak, row, col, mean).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``execute_peaks: the input must\n"
    "    be a whole, nonzero number of frames, and out= must hold one record\n"
    "    per frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> peak, row, col, mean = dd.execute_peaks(frames)\n"
    ">>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2\n"
    "([4, 4], [3, 11])\n"
    ">>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean\n"
    "True\n" },
  { "state_bytes", (PyCFunction)Corr2DObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Corr2D has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)Corr2DObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the Corr2D has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)Corr2DObj_set_state, METH_O,
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
    "``RuntimeError`` if the Corr2D has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)Corr2DObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)Corr2DObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Corr2D be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Corr2D\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)Corr2DObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Corr2D.\n"
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

static PyTypeObject Corr2DObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.spectral.Corr2D",
  .tp_basicsize                           = sizeof (Corr2DObject),
  .tp_dealloc                             = (destructor)Corr2DObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a 2-D FFT correlator with coherent integrate-and-dump.\n"
    "Two-dimensional extension of dp_corr_create(). The reference is a flat\n"
    "row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once "
    "so\n"
    "each execute() call costs two 2-D FFTs plus ny*nx complex multiplies. "
    "The\n"
    "Python wrapper requires ref to be a 2-D ndarray with shape (ny, nx); it\n"
    "passes a flat view to C.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ref : NDArray[np.complex64]\n"
    "    Reference image, 2-D (ny, nx) CF32 ndarray in Python.\n"
    "dwell : int, default 1\n"
    "    Integration depth; must be >= 1.\n"
    "nthreads : int, default 1\n"
    "    Workers for the single-row-reference fast path's per-row stage: 1 "
    "runs\n"
    "    serially (no threads), N runs N workers (the caller included), <= 0\n"
    "    uses every online core. The count is capped at ny, one row per "
    "worker\n"
    "    at most. The output is bit-identical whatever the count. The "
    "general\n"
    "    2-D path and the col_out path run serially and ignore it.\n"
    "ny_out : int, default 0\n"
    "    Inverse/output rows; 0 => native (ny). Must be >= ny. A larger "
    "output\n"
    "    zero-pads the cross-spectrum before the inverse, returning the\n"
    "    band-limited (Dirichlet) interpolation of the correlation on a "
    "finer\n"
    "    (ny_out, nx_out) grid — same peak, sub-bin resolution. Native is\n"
    "    bit-exact and allocates no extra buffers.\n"
    "nx_out : int, default 0\n"
    "    Inverse/output columns; 0 => native (nx). Must be >= nx.\n"
    "col_out : int, default -1\n"
    "    Emit ONLY this correlation lag, or < 0 for the whole map. When set, "
    "a\n"
    "    dump writes ny values -- one per row -- instead of ny*nx_out, "
    "computed\n"
    "    as the time-domain sum over p of `conj(ref(p)) * row((p + col_out) "
    "mod\n"
    "    nx)`, which is what R(i, col_out) expands to once the 1/nx cancels. "
    "No\n"
    "    transform runs in either direction, so the cost per row is O(nx) "
    "rather\n"
    "    than O(nx log nx). Requires the single-row-reference fast path (a\n"
    "    caller that knows its lag is by construction correlating against a "
    "code\n"
    "    replica) and the NATIVE output grid: an interpolated column is a\n"
    "    fractional lag, which no time-domain sum produces. create() returns\n"
    "    NULL if col_out >= 0 with a reference the fast path rejects, with\n"
    "    ny_out or nx_out decoupled, or with a lag outside 0 .. nx-1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``Corr2D: invalid\n"
    "    parameter (need dwell >= 1, and ny_out and nx_out each 0 or at "
    "least\n"
    "    the ref extent)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Corr2D(ref=ref, dwell=1, nthreads=1)\n"
    ">>> c.ny, c.nx, c.dwell, c.count\n"
    "(4, 4, 1, 0)\n",
  .tp_methods = Corr2DObj_methods,
  .tp_getset  = Corr2D_getset,
  .tp_new     = Corr2DObj_new,
  .tp_init    = (initproc)Corr2DObj_init,
};
