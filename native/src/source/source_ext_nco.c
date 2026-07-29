/*
 * source_ext_nco.c — NCO type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* NCOObject — wraps nco_state_t *       */
/* ======================================================== */

#include "nco/nco_core.h"

typedef struct
{
  PyObject_HEAD nco_state_t *handle;
} NCOObject;

static void
NCOObj_dealloc (NCOObject *self)
{
  if (self->handle)
    nco_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
NCOObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  NCOObject *self = (NCOObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
NCOObj_init (NCOObject *self, PyObject *args, PyObject *kwds)
{
  static char  *kwlist[]  = { "norm_freq", "nmax", NULL };
  double        norm_freq = 0.0;
  unsigned long nmax_raw  = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dk", kwlist, &norm_freq,
                                    &nmax_raw))
    return -1;
  uint32_t nmax = (uint32_t)nmax_raw;
  self->handle  = nco_create (norm_freq, nmax);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "nco_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
NCOObj_reset (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  nco_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
NCOObj_steps_u32_max_out (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_max_out (self->handle));
}

static PyObject *
NCOObj_steps_u32 (NCOObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT32
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = nco_steps_u32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = nco_steps_u32 (self->handle, (size_t)n,
                                    (uint32_t *)PyArray_DATA (out_arr), _cap);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT32,
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
  size_t _cap  = nco_steps_u32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0)
    {
      return NULL;
    }
  uint32_t *_d0   = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t    n_out = nco_steps_u32 (self->handle, (size_t)n, _d0, _cap);
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
NCOObj_steps_u32_scaled_max_out (NCOObject *self,
                                 PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_scaled_max_out (self->handle));
}

static PyObject *
NCOObj_steps_u32_scaled (NCOObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT32
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = nco_steps_u32_scaled_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = nco_steps_u32_scaled (
          self->handle, (size_t)n, (uint32_t *)PyArray_DATA (out_arr), _cap);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT32,
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
  size_t _cap  = nco_steps_u32_scaled_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0)
    {
      return NULL;
    }
  uint32_t *_d0   = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t    n_out = nco_steps_u32_scaled (self->handle, (size_t)n, _d0, _cap);
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
NCOObj_steps_u32_ovf (NCOObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  size_t _need = (size_t)n;
  size_t _cap  = nco_steps_u32_ovf_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0 || !arr1)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      return NULL;
    }
  uint32_t *_d0 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0);
  uint8_t  *_d1 = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr1);
  size_t n_out  = nco_steps_u32_ovf (self->handle, (size_t)n, _d0, _d1, _cap);
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
NCOObj_steps_u32_ctrl_max_out (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_ctrl_max_out (self->handle));
}

static PyObject *
NCOObj_steps_u32_ctrl (NCOObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "ctrl", "out", NULL };
  PyObject      *ctrl_obj  = NULL;
  PyArrayObject *ctrl_arr  = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &ctrl_obj,
                                    &out_obj))
    return NULL;
  ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF (ctrl_obj, NPY_FLOAT,
                                                NPY_ARRAY_C_CONTIGUOUS);
  if (!ctrl_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT32
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = nco_steps_u32_ctrl_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (ctrl_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (ctrl_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t n_out = nco_steps_u32_ctrl (
          self->handle, (const float *)PyArray_DATA (ctrl_arr),
          (size_t)PyArray_SIZE (ctrl_arr), (uint32_t *)PyArray_DATA (out_arr),
          _cap);
      Py_DECREF (ctrl_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT32,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (ctrl_arr);
  size_t _cap  = nco_steps_u32_ctrl_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0)
    {
      Py_DECREF (ctrl_arr);
      return NULL;
    }
  uint32_t *_d0   = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t    n_out = nco_steps_u32_ctrl (
      self->handle, (const float *)PyArray_DATA (ctrl_arr),
      (size_t)PyArray_SIZE (ctrl_arr), _d0, _cap);
  Py_DECREF (ctrl_arr);
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
NCOObj_steps_u32_scaled_ctrl_max_out (NCOObject *self,
                                      PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_scaled_ctrl_max_out (self->handle));
}

static PyObject *
NCOObj_steps_u32_scaled_ctrl (NCOObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "ctrl", "out", NULL };
  PyObject      *ctrl_obj  = NULL;
  PyArrayObject *ctrl_arr  = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &ctrl_obj,
                                    &out_obj))
    return NULL;
  ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF (ctrl_obj, NPY_FLOAT,
                                                NPY_ARRAY_C_CONTIGUOUS);
  if (!ctrl_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT32
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = nco_steps_u32_scaled_ctrl_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (ctrl_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (ctrl_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t n_out = nco_steps_u32_scaled_ctrl (
          self->handle, (const float *)PyArray_DATA (ctrl_arr),
          (size_t)PyArray_SIZE (ctrl_arr), (uint32_t *)PyArray_DATA (out_arr),
          _cap);
      Py_DECREF (ctrl_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT32,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (ctrl_arr);
  size_t _cap  = nco_steps_u32_scaled_ctrl_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0)
    {
      Py_DECREF (ctrl_arr);
      return NULL;
    }
  uint32_t *_d0   = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t    n_out = nco_steps_u32_scaled_ctrl (
      self->handle, (const float *)PyArray_DATA (ctrl_arr),
      (size_t)PyArray_SIZE (ctrl_arr), _d0, _cap);
  Py_DECREF (ctrl_arr);
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
NCOObj_steps_u32_ovf_ctrl (NCOObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "ctrl", NULL };
  PyObject      *ctrl_obj  = NULL;
  PyArrayObject *ctrl_arr  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &ctrl_obj))
    return NULL;
  ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF (ctrl_obj, NPY_FLOAT,
                                                NPY_ARRAY_C_CONTIGUOUS);
  if (!ctrl_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (ctrl_arr);
  size_t _cap  = nco_steps_u32_ovf_ctrl_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0 || !arr1)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_DECREF (ctrl_arr);
      return NULL;
    }
  uint32_t *_d0   = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0);
  uint8_t  *_d1   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr1);
  size_t    n_out = nco_steps_u32_ovf_ctrl (
      self->handle, (const float *)PyArray_DATA (ctrl_arr),
      (size_t)PyArray_SIZE (ctrl_arr), _d0, _d1, _cap);
  Py_DECREF (ctrl_arr);
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
NCOObj_state_bytes (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_state_bytes (self->handle));
}

static PyObject *
NCOObj_get_state (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = nco_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  nco_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
NCOObj_set_state (NCOObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != nco_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (nco_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
NCO_getprop_norm_freq (NCOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (nco_get_norm_freq (self->handle));
}
static int
NCO_setprop_norm_freq (NCOObject *self, PyObject *value,
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
  nco_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
NCO_getprop_phase (NCOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong ((unsigned long)nco_get_phase (self->handle));
}
static int
NCO_setprop_phase (NCOObject *self, PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  unsigned long v_raw = 0UL;
  if (!PyArg_Parse (value, "k", &v_raw))
    return -1;
  uint32_t v = (uint32_t)v_raw;
  nco_set_phase (self->handle, v);
  return 0;
}
static PyObject *
NCO_getprop_phase_inc (NCOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong (
      (unsigned long)nco_get_phase_inc (self->handle));
}

static PyGetSetDef NCO_getset[] = {
  { "norm_freq", (getter)NCO_getprop_norm_freq, (setter)NCO_setprop_norm_freq,
    "Normalised frequency (read/write). Setting norm_freq recomputes "
    "phase_inc = floor(frac(v) × 2^32) and takes effect on the next "
    "nco_steps_* call; phase is NOT reset.\n",
    NULL },
  { "phase", (getter)NCO_getprop_phase, (setter)NCO_setprop_phase,
    "Current phase accumulator value (read/write). Reading returns the "
    "current integer phase in `[0, 2^32)`.  Writing overrides the accumulator "
    "directly, allowing arbitrary phase offsets without re-creating the "
    "NCO.\n",
    NULL },
  { "phase_inc", (getter)NCO_getprop_phase_inc, NULL,
    "Per-sample phase increment (read-only). Derived from norm_freq as "
    "floor(frac(norm_freq) × 2^32).  Updated automatically whenever norm_freq "
    "is written.  A freq of 0.25 gives phase_inc = 1073741824 (0x40000000).\n",
    NULL },
  { NULL }
};

static PyObject *
NCOObj_destroy (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      nco_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
NCOObj_enter (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
NCOObj_exit (NCOObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      nco_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef NCOObj_methods[] = {
  { "reset", (PyCFunction)NCOObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "steps_u32", (PyCFunction)(void *)NCOObj_steps_u32,
    METH_VARARGS | METH_KEYWORDS,
    "steps_u32(n=1) -> ndarray\n"
    "\n"
    "Advance n samples; write raw uint32 accumulator values. Each element is "
    "the phase value BEFORE the increment fires, so `out[0]` is the phase at "
    "the moment of the call.  The accumulator wraps silently at 2^32, giving "
    "the full-resolution integer ramp that the scaled and carry variants "
    "derive from.  Returns n.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_max_out", (PyCFunction)NCOObj_steps_u32_max_out, METH_NOARGS,
    "steps_u32_max_out() -> int\n\nMax output length steps_u32() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "steps_u32_scaled", (PyCFunction)(void *)NCOObj_steps_u32_scaled,
    METH_VARARGS | METH_KEYWORDS,
    "steps_u32_scaled(n=1) -> ndarray\n"
    "\n"
    "Advance n samples; values scaled to `[0, nmax)`. Uses the branchless "
    "fixed-point identity `out[i]` = (uint64_t)phase * nmax >> 32 to map the "
    "full accumulator range uniformly onto [0, nmax) without a modulo "
    "operation.  When nmax == 0 falls back to the raw accumulator (identical "
    "to nco_steps_u32).  Useful for polyphase filter bank indexing and direct "
    "LUT addressing.  Returns n.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_scaled(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_scaled_max_out", (PyCFunction)NCOObj_steps_u32_scaled_max_out,
    METH_NOARGS,
    "steps_u32_scaled_max_out() -> int\n\nMax output length "
    "steps_u32_scaled() can produce for the current state.\nUse to size the "
    "``out=`` buffer." },
  { "steps_u32_ovf", (PyCFunction)NCOObj_steps_u32_ovf, METH_VARARGS,
    "steps_u32_ovf(n=1) -> tuple[ndarray, ndarray]\n"
    "\n"
    "Advance n samples; write raw phase values and per-sample carry. "
    "Identical to nco_steps_u32 for the phase array, but simultaneously fills "
    "a parallel uint8 carry buffer: `out1[i]` is 1 if the add that produced "
    "`out[i]`'s post-increment phase wrapped past 2^32, else 0. The carry "
    "marks the exact boundary of one input period and is the primitive for "
    "polyphase sample-clock and rational resampling engines. Returns n.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_ovf(4)\n"
    "    >>> y[0].dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_ctrl", (PyCFunction)(void *)NCOObj_steps_u32_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "steps_u32_ctrl(ctrl) -> ndarray\n"
    "\n"
    "Advance ctrl_len samples; raw phase, with a per-sample control offset "
    "added on top of the fixed phase_inc (not persisted).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_ctrl(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_ctrl_max_out", (PyCFunction)NCOObj_steps_u32_ctrl_max_out,
    METH_NOARGS,
    "steps_u32_ctrl_max_out() -> int\n\nMax output length steps_u32_ctrl() "
    "can produce for the current state.\nUse to size the ``out=`` buffer." },
  { "steps_u32_scaled_ctrl", (PyCFunction)(void *)NCOObj_steps_u32_scaled_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "steps_u32_scaled_ctrl(ctrl) -> ndarray\n"
    "\n"
    "Advance ctrl_len samples; values scaled to `[0, nmax)`, with a "
    "per-sample control offset added on top of phase_inc.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_scaled_ctrl(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_scaled_ctrl_max_out",
    (PyCFunction)NCOObj_steps_u32_scaled_ctrl_max_out, METH_NOARGS,
    "steps_u32_scaled_ctrl_max_out() -> int\n\nMax output length "
    "steps_u32_scaled_ctrl() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "steps_u32_ovf_ctrl", (PyCFunction)(void *)NCOObj_steps_u32_ovf_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "steps_u32_ovf_ctrl(ctrl) -> tuple[ndarray, ndarray]\n"
    "\n"
    "Advance ctrl_len samples; raw phase + per-sample carry, with a "
    "per-sample control offset added on top of phase_inc.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_ovf_ctrl(np.zeros(4))\n"
    "    >>> y[0].dtype\n"
    "    dtype('uint32')\n" },
  { "state_bytes", (PyCFunction)NCOObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)NCOObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)NCOObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)NCOObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)NCOObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)NCOObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject NCOObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "source.NCO",
  .tp_basicsize                           = sizeof (NCOObject),
  .tp_dealloc                             = (destructor)NCOObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an NCO instance. Allocates and initialises the phase "
                "accumulator to zero, converts norm_freq to the integer phase_inc "
                "= floor(frac(norm_freq) × 2^32), and stores nmax for scaled "
                "output.  The NCO is immediately ready to call nco_steps_u32 / "
                "nco_steps_u32_scaled / nco_steps_u32_ovf.\n",
  .tp_methods = NCOObj_methods,
  .tp_getset  = NCO_getset,
  .tp_new     = NCOObj_new,
  .tp_init    = (initproc)NCOObj_init,
};
