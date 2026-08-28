/*
 * coding_ext_interleaver.c — Interleaver type for the coding module.
 *
 * Included by coding_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only coding_ext.c is compiled.
 */
/* ======================================================== */
/* InterleaverObject — wraps interleaver_state_t *       */
/* ======================================================== */

#include "interleaver/interleaver_core.h"

typedef struct
{
  PyObject_HEAD interleaver_state_t *handle;
} InterleaverObject;

static void
InterleaverObj_dealloc (InterleaverObject *self)
{
  if (self->handle)
    interleaver_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
InterleaverObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  InterleaverObject *self = (InterleaverObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
InterleaverObj_init (InterleaverObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]      = { "rows", "cols", "unit_bits", NULL };
  unsigned long long rows_raw      = 0ULL;
  unsigned long long cols_raw      = 0ULL;
  unsigned long long unit_bits_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKK", kwlist, &rows_raw,
                                    &cols_raw, &unit_bits_raw))
    return -1;
  size_t rows      = (size_t)rows_raw;
  size_t cols      = (size_t)cols_raw;
  size_t unit_bits = (size_t)unit_bits_raw;
  self->handle     = interleaver_create (rows, cols, unit_bits);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "Interleaver: rows, cols and unit_bits must all be "
                       "non-zero, and their product must fit a size_t");
      return -1;
    }
  return 0;
}

static PyObject *
InterleaverObj_reset (InterleaverObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  interleaver_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
InterleaverObj_interleave_max_out (InterleaverObject *self, PyObject *args)
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
      interleaver_interleave_max_out (self->handle, (size_t)n_in));
}

static PyObject *
InterleaverObj_interleave (InterleaverObject *self, PyObject *args,
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
      in_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = interleaver_interleave_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = interleaver_interleave (
          self->handle, (const uint8_t *)PyArray_DATA (in_arr), (size_t)n,
          (uint8_t *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      /* A kernel refusal is 0, and 0 is not a length here: every method below
         refuses n_in == 0 too, so an empty result can only mean the input was
         not a whole number of blocks. Returning an empty array would be a
         silent wrong answer -- the frame comes back short and nothing says
         why. jm has no declarative hook for this: `error`/`status_return`
         apply to an int-returning method and a variable_output one has no
         status to carry (just-makeit#1159). */
      if (n_out == 0)
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_ValueError,
              "interleave: length %zd is not a whole number of blocks "
              "of block_bits = %zu",
              (Py_ssize_t)n, interleaver_get_block_bits (self->handle));
          return NULL;
        }
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
  size_t _need = (size_t)n;
  size_t _cap  = interleaver_interleave_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  uint8_t *_d0   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = interleaver_interleave (
      self->handle, (const uint8_t *)PyArray_DATA (in_arr), (size_t)n, _d0,
      _cap);
  Py_DECREF (in_arr);
  /* A kernel refusal is 0, and 0 is not a length here: every method below
     refuses n_in == 0 too, so an empty result can only mean the input was
     not a whole number of blocks. Returning an empty array would be a
     silent wrong answer -- the frame comes back short and nothing says
     why. jm has no declarative hook for this: `error`/`status_return`
     apply to an int-returning method and a variable_output one has no
     status to carry (just-makeit#1159). */
  if (n_out == 0)
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_ValueError,
                    "interleave: length %zd is not a whole number of blocks "
                    "of block_bits = %zu",
                    (Py_ssize_t)n, interleaver_get_block_bits (self->handle));
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
InterleaverObj_deinterleave_max_out (InterleaverObject *self, PyObject *args)
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
      interleaver_deinterleave_max_out (self->handle, (size_t)n_in));
}

static PyObject *
InterleaverObj_deinterleave (InterleaverObject *self, PyObject *args,
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
      in_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      size_t _omax
          = interleaver_deinterleave_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = interleaver_deinterleave (
          self->handle, (const uint8_t *)PyArray_DATA (in_arr), (size_t)n,
          (uint8_t *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      /* A kernel refusal is 0, and 0 is not a length here: every method below
         refuses n_in == 0 too, so an empty result can only mean the input was
         not a whole number of blocks. Returning an empty array would be a
         silent wrong answer -- the frame comes back short and nothing says
         why. jm has no declarative hook for this: `error`/`status_return`
         apply to an int-returning method and a variable_output one has no
         status to carry (just-makeit#1159). */
      if (n_out == 0)
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_ValueError,
              "deinterleave: length %zd is not a whole number of blocks "
              "of block_bits = %zu",
              (Py_ssize_t)n, interleaver_get_block_bits (self->handle));
          return NULL;
        }
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
  size_t _need = (size_t)n;
  size_t _cap  = interleaver_deinterleave_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  uint8_t *_d0   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = interleaver_deinterleave (
      self->handle, (const uint8_t *)PyArray_DATA (in_arr), (size_t)n, _d0,
      _cap);
  Py_DECREF (in_arr);
  /* A kernel refusal is 0, and 0 is not a length here: every method below
     refuses n_in == 0 too, so an empty result can only mean the input was
     not a whole number of blocks. Returning an empty array would be a
     silent wrong answer -- the frame comes back short and nothing says
     why. jm has no declarative hook for this: `error`/`status_return`
     apply to an int-returning method and a variable_output one has no
     status to carry (just-makeit#1159). */
  if (n_out == 0)
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_ValueError,
                    "deinterleave: length %zd is not a whole number of blocks "
                    "of block_bits = %zu",
                    (Py_ssize_t)n, interleaver_get_block_bits (self->handle));
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
InterleaverObj_deinterleave_soft_max_out (InterleaverObject *self,
                                          PyObject          *args)
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
      interleaver_deinterleave_soft_max_out (self->handle, (size_t)n_in));
}

static PyObject *
InterleaverObj_deinterleave_soft (InterleaverObject *self, PyObject *args,
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
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      size_t _omax
          = interleaver_deinterleave_soft_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = interleaver_deinterleave_soft (
          self->handle, (const float *)PyArray_DATA (in_arr), (size_t)n,
          (float *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      /* A kernel refusal is 0, and 0 is not a length here: every method below
         refuses n_in == 0 too, so an empty result can only mean the input was
         not a whole number of blocks. Returning an empty array would be a
         silent wrong answer -- the frame comes back short and nothing says
         why. jm has no declarative hook for this: `error`/`status_return`
         apply to an int-returning method and a variable_output one has no
         status to carry (just-makeit#1159). */
      if (n_out == 0)
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_ValueError,
              "deinterleave_soft: length %zd is not a whole number of blocks "
              "of block_bits = %zu",
              (Py_ssize_t)n, interleaver_get_block_bits (self->handle));
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap
      = interleaver_deinterleave_soft_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = interleaver_deinterleave_soft (
      self->handle, (const float *)PyArray_DATA (in_arr), (size_t)n, _d0,
      _cap);
  Py_DECREF (in_arr);
  /* A kernel refusal is 0, and 0 is not a length here: every method below
     refuses n_in == 0 too, so an empty result can only mean the input was
     not a whole number of blocks. Returning an empty array would be a
     silent wrong answer -- the frame comes back short and nothing says
     why. jm has no declarative hook for this: `error`/`status_return`
     apply to an int-returning method and a variable_output one has no
     status to carry (just-makeit#1159). */
  if (n_out == 0)
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_ValueError,
          "deinterleave_soft: length %zd is not a whole number of blocks "
          "of block_bits = %zu",
          (Py_ssize_t)n, interleaver_get_block_bits (self->handle));
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
Interleaver_getprop_rows (InterleaverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->rows);
}
static PyObject *
Interleaver_getprop_cols (InterleaverObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->cols);
}
static PyObject *
Interleaver_getprop_unit_bits (InterleaverObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->unit_bits);
}
static PyObject *
Interleaver_getprop_block_bits (InterleaverObject *self,
                                void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)interleaver_get_block_bits (self->handle));
}
static PyObject *
Interleaver_getprop_burst_len (InterleaverObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)interleaver_get_burst_len (self->handle));
}
static PyObject *
Interleaver_getprop_separation (InterleaverObject *self,
                                void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)interleaver_get_separation (self->handle));
}

static PyGetSetDef Interleaver_getset[]
    = { { "rows", (getter)Interleaver_getprop_rows, NULL,
          "Interleaving depth -- codewords interleaved.\n", NULL },
        { "cols", (getter)Interleaver_getprop_cols, NULL,
          "Units per codeword.\n", NULL },
        { "unit_bits", (getter)Interleaver_getprop_unit_bits, NULL,
          "Bits per interleaved unit; 1 interleaves bits, 8 octets.\n", NULL },
        { "block_bits", (getter)Interleaver_getprop_block_bits, NULL,
          "Bits in one block -- `rows * cols * unit_bits`. Every call takes a "
          "whole multiple of this.\n",
          NULL },
        { "burst_len", (getter)Interleaver_getprop_burst_len, NULL,
          "The longest burst this geometry fully spreads: a burst of up to "
          "this many consecutive units on the wire touches each codeword at "
          "most once. An outer code correcting `t` units per codeword "
          "survives a burst of `t * burst_len`.\n",
          NULL },
        { "separation", (getter)Interleaver_getprop_separation, NULL,
          "The other half of the link budget -- what `burst_len` spreads a "
          "burst ACROSS. Equal to `cols`, and named for what it buys rather "
          "than for the matrix it comes from.\n",
          NULL },
        { NULL } };

static PyObject *
InterleaverObj_destroy (InterleaverObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      interleaver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
InterleaverObj_enter (InterleaverObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
InterleaverObj_exit (InterleaverObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      interleaver_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef InterleaverObj_methods[] = {
  { "reset", (PyCFunction)InterleaverObj_reset, METH_NOARGS,
    "No-op; an interleaver carries nothing between calls.\n"
    "\n"
    "Present because the object surface has it, and honest about why it does\n"
    "nothing: a reset that pretended to clear something would suggest there\n"
    "was something to clear. The geometry is configuration, not state, so it\n"
    "survives — a reset that cleared THAT would leave every later call\n"
    "refusing.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> il = Interleaver(rows=2, cols=3)\n"
    ">>> il.reset()\n"
    ">>> il.block_bits\n"
    "6\n" },

  { "interleave", (PyCFunction)(void *)InterleaverObj_interleave,
    METH_VARARGS | METH_KEYWORDS,
    "interleave(x, out) -> ndarray\n"
    "\n"
    "Interleave a whole number of blocks: write the input by rows into a\n"
    "`rows` x `cols` matrix and read it back by columns. Length-preserving.\n"
    "A length that is not a whole multiple of `block_bits` is REFUSED rather\n"
    "than padded -- padding changes the length, and a receiver\n"
    "de-interleaving the padded block recovers different bits.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Input.\n"
    "out : NDArray[np.uint8] | None\n"
    "    Where to write n_in bits; must not overlap in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    n_in on success, 0 if the length is not a whole number of blocks or\n"
    "    out is too small. A partial block is REFUSED rather than padded:\n"
    "    padding changes the length, and a receiver that de-interleaved the\n"
    "    padded block would recover different bits.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> il = Interleaver(rows=3, cols=4)\n"
    ">>> x = np.arange(12, dtype=np.uint8)\n"
    ">>> np.asarray(il.interleave(x)).tolist()\n"
    "[0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]\n" },
  { "interleave_max_out", (PyCFunction)InterleaverObj_interleave_max_out,
    METH_VARARGS,
    "interleave_max_out(n_in) -> int\n"
    "\n"
    "Output bits for n_in input bits — the same number.\n"
    "\n"
    "A permutation moves bits and does not add or remove any, so this is the\n"
    "identity. It exists because the binding asks a method how much room its\n"
    "output needs, and answering \"the same\" is not something a caller "
    "should\n"
    "have to know.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input length in bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_in.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> Interleaver(rows=4, cols=8).interleave_max_out(32)\n"
    "32\n" },
  { "deinterleave", (PyCFunction)(void *)InterleaverObj_deinterleave,
    METH_VARARGS | METH_KEYWORDS,
    "deinterleave(x, out) -> ndarray\n"
    "\n"
    "Undo `interleave()` over the same geometry. De-interleaving a `rows`\n"
    "x `cols` block is interleaving a `cols` x `rows` one, so this is the\n"
    "same kernel with two arguments exchanged -- which is also why a SQUARE\n"
    "block is its own inverse.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Input.\n"
    "out : NDArray[np.uint8] | None\n"
    "    Where to write n_in bits; must not overlap in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    n_in, or 0 on a refusal.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> il = Interleaver(rows=3, cols=4)\n"
    ">>> x = np.arange(12, dtype=np.uint8)\n"
    ">>> y = np.asarray(il.interleave(x))\n"
    ">>> np.array_equal(np.asarray(il.deinterleave(y)), x)\n"
    "True\n" },
  { "deinterleave_max_out", (PyCFunction)InterleaverObj_deinterleave_max_out,
    METH_VARARGS,
    "deinterleave_max_out(n_in) -> int\n"
    "\n"
    "Output bits for n_in input bits — the same number.\n"
    "\n"
    "Identical to interleaver_interleave_max_out, and for the same reason:\n"
    "the inverse of a permutation is a permutation.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input length in bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_in.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> Interleaver(rows=4, cols=8).deinterleave_max_out(32)\n"
    "32\n" },
  { "deinterleave_soft", (PyCFunction)(void *)InterleaverObj_deinterleave_soft,
    METH_VARARGS | METH_KEYWORDS,
    "deinterleave_soft(x, out) -> ndarray\n"
    "\n"
    "Undo an interleave over SOFT values -- the receive path that\n"
    "matters. `DsssBurstReceiver.llrs` span the whole frame, and an outer\n"
    "decoder wants them de-interleaved BEFORE it runs; slicing to hard bits\n"
    "first throws away the confidence the soft output exists to carry. There\n"
    "is no `interleave_soft`, because a transmitter has bits, not LLRs.\n"
    "\n"
    "`dsss_burst_receiver`'s `llrs` span the whole frame, and an outer\n"
    "decoder wants them de-interleaved BEFORE it runs. Slicing to hard bits\n"
    "first and de-interleaving those throws away the confidence the soft\n"
    "output exists to carry, which is most of what an outer code is for.\n"
    "\n"
    "There is no `interleave_soft`: a transmitter has bits, not LLRs.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input.\n"
    "out : NDArray[np.float32] | None\n"
    "    Where to write n_in values; must not overlap in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    n_in, or 0 on a refusal.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> il = Interleaver(rows=2, cols=3)\n"
    ">>> llr = np.array([1., 2., 3., 4., 5., 6.], dtype=np.float32)\n"
    ">>> np.asarray(il.deinterleave_soft(llr)).tolist()\n"
    "[1.0, 3.0, 5.0, 2.0, 4.0, 6.0]\n" },
  { "deinterleave_soft_max_out",
    (PyCFunction)InterleaverObj_deinterleave_soft_max_out, METH_VARARGS,
    "deinterleave_soft_max_out(n_in) -> int\n"
    "\n"
    "Output values for n_in soft input values — the same number.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input length in values.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_in.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> Interleaver(rows=4, cols=8).deinterleave_soft_max_out(32)\n"
    "32\n" },
  { "destroy", (PyCFunction)InterleaverObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)InterleaverObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Interleaver be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Interleaver\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)InterleaverObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Interleaver.\n"
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

static PyTypeObject InterleaverObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "coding.Interleaver",
  .tp_basicsize                           = sizeof (InterleaverObject),
  .tp_dealloc                             = (destructor)InterleaverObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Build an interleaver over a rows x cols block of unit_bits units.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rows : int\n"
    "    Interleaving depth; the longest burst fully spread. Must be "
    "non-zero.\n"
    "cols : int\n"
    "    Units per codeword. Must be non-zero.\n"
    "unit_bits : int, default 1\n"
    "    Bits per interleaved unit. 1 interleaves bits; 8 interleaves "
    "octets,\n"
    "    which is what spreads a burst across the codewords of a "
    "symbol-oriented\n"
    "    code such as Reed-Solomon over GF(256). Must be non-zero.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``Interleaver: "
    "rows,\n"
    "    cols and unit_bits must all be non-zero, and their product must fit "
    "a\n"
    "    size_t``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Interleaver\n"
    ">>> il = Interleaver(rows=3, cols=4)\n"
    ">>> il.block_bits, il.burst_len, il.separation\n"
    "(12, 3, 4)\n",
  .tp_methods = InterleaverObj_methods,
  .tp_getset  = Interleaver_getset,
  .tp_new     = InterleaverObj_new,
  .tp_init    = (initproc)InterleaverObj_init,
};
