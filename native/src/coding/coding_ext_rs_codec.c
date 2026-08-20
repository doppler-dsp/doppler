/*
 * coding_ext_rs_codec.c — ReedSolomon type for the coding module.
 *
 * Included by coding_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only coding_ext.c is compiled.
 */
/* ======================================================== */
/* ReedSolomonObject — wraps rs_codec_state_t *       */
/* ======================================================== */

#include "rs_codec/rs_codec_core.h"

typedef struct
{
  PyObject_HEAD rs_codec_state_t *handle;
} ReedSolomonObject;

static void
ReedSolomonObj_dealloc (ReedSolomonObject *self)
{
  if (self->handle)
    rs_codec_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ReedSolomonObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ReedSolomonObject *self = (ReedSolomonObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ReedSolomonObj_init (ReedSolomonObject *self, PyObject *args, PyObject *kwds)
{
  static char  *kwlist[]        = { "nroots",     "symbol_bits", "field_poly",
                                    "first_root", "root_stride", NULL };
  unsigned long nroots_raw      = 0UL;
  unsigned long symbol_bits_raw = 8;
  unsigned long field_poly_raw  = 29;
  unsigned long first_root_raw  = 1;
  unsigned long root_stride_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "k|kkkk", kwlist, &nroots_raw,
                                    &symbol_bits_raw, &field_poly_raw,
                                    &first_root_raw, &root_stride_raw))
    return -1;
  uint32_t nroots      = (uint32_t)nroots_raw;
  uint32_t symbol_bits = (uint32_t)symbol_bits_raw;
  uint32_t field_poly  = (uint32_t)field_poly_raw;
  uint32_t first_root  = (uint32_t)first_root_raw;
  uint32_t root_stride = (uint32_t)root_stride_raw;
  self->handle = rs_codec_create (nroots, symbol_bits, field_poly, first_root,
                                  root_stride);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "ReedSolomon: not a usable code — need an even nroots "
                       "in 2..64 leaving at least one information symbol, 2 "
                       "<= symbol_bits <= 8, a PRIMITIVE field_poly, and a "
                       "root_stride coprime with 2**symbol_bits - 1");
      return -1;
    }
  return 0;
}

static PyObject *
ReedSolomonObj_encode_max_out (ReedSolomonObject *self, PyObject *args)
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
      rs_codec_encode_max_out (self->handle, (size_t)n_in));
}

static PyObject *
ReedSolomonObj_encode (ReedSolomonObject *self, PyObject *args, PyObject *kwds)
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
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = rs_codec_encode_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = rs_codec_encode (
          self->handle, (const uint8_t *)PyArray_DATA (in_arr), (size_t)n,
          (uint8_t *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
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
  size_t _cap  = rs_codec_encode_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  uint8_t *_d0 = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out
      = rs_codec_encode (self->handle, (const uint8_t *)PyArray_DATA (in_arr),
                         (size_t)n, _d0, _cap);
  Py_DECREF (in_arr);
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
ReedSolomonObj_decode (ReedSolomonObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]    = { "codeword", NULL };
  PyObject    *codeword_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &codeword_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (codeword_obj)
      || PyArray_TYPE ((PyArrayObject *)codeword_obj) != NPY_UINT8
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)codeword_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)codeword_obj))
    {
      PyErr_SetString (PyExc_TypeError,
                       "codeword must be a writable, C-contiguous"
                       " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *codeword_arr = (PyArrayObject *)PyArray_FROM_OTF (
      codeword_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!codeword_arr)
    {
      return NULL;
    }
  uint8_t *codeword     = (uint8_t *)PyArray_DATA (codeword_arr);
  size_t   codeword_len = (size_t)PyArray_SIZE (codeword_arr);
  int      y = rs_codec_decode (self->handle, codeword, codeword_len);
  Py_DECREF (codeword_arr);
  return PyLong_FromLong ((long)y);
}

static PyObject *
ReedSolomonObj_syndromes_max_out (ReedSolomonObject *self, PyObject *args)
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
      rs_codec_syndromes_max_out (self->handle, (size_t)n_in));
}

static PyObject *
ReedSolomonObj_syndromes (ReedSolomonObject *self, PyObject *args,
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
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = rs_codec_syndromes_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = rs_codec_syndromes (
          self->handle, (const uint8_t *)PyArray_DATA (in_arr), (size_t)n,
          (uint8_t *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
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
  size_t _cap  = rs_codec_syndromes_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  uint8_t *_d0   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = rs_codec_syndromes (self->handle,
                                       (const uint8_t *)PyArray_DATA (in_arr),
                                       (size_t)n, _d0, _cap);
  Py_DECREF (in_arr);
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
ReedSolomonObj_codeword_ok (ReedSolomonObject *self, PyObject *args,
                            PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]    = { "codeword", NULL };
  PyObject    *codeword_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &codeword_obj))
    return NULL;
  PyArrayObject *codeword_arr = (PyArrayObject *)PyArray_FROM_OTF (
      codeword_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!codeword_arr)
    {
      return NULL;
    }
  const uint8_t *codeword     = (const uint8_t *)PyArray_DATA (codeword_arr);
  size_t         codeword_len = (size_t)PyArray_SIZE (codeword_arr);
  int y = rs_codec_codeword_ok (self->handle, codeword, codeword_len);
  Py_DECREF (codeword_arr);
  return PyLong_FromLong ((long)y);
}

static PyObject *
ReedSolomonObj_generator (ReedSolomonObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &out_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      return NULL;
    }
  uint8_t *out     = (uint8_t *)PyArray_DATA (out_arr);
  size_t   out_len = (size_t)PyArray_SIZE (out_arr);
  size_t   y       = rs_codec_generator (self->handle, out, out_len);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}
static PyObject *
ReedSolomon_getprop_n (ReedSolomonObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)rs_codec_get_n (self->handle));
}
static PyObject *
ReedSolomon_getprop_k (ReedSolomonObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)rs_codec_get_k (self->handle));
}
static PyObject *
ReedSolomon_getprop_e (ReedSolomonObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)rs_codec_get_e (self->handle));
}
static PyObject *
ReedSolomon_getprop_nroots (ReedSolomonObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)rs_codec_get_nroots (self->handle));
}
static PyObject *
ReedSolomon_getprop_symbol_bits (ReedSolomonObject *self,
                                 void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)rs_codec_get_symbol_bits (self->handle));
}

static PyGetSetDef ReedSolomon_getset[]
    = { { "n", (getter)ReedSolomon_getprop_n, NULL,
          "Symbols per codeword, `2^J - 1`.\n", NULL },
        { "k", (getter)ReedSolomon_getprop_k, NULL,
          "Information symbols per codeword, `n - nroots`.\n", NULL },
        { "e", (getter)ReedSolomon_getprop_e, NULL,
          "Correctable symbols per codeword, `nroots / 2`.\n", NULL },
        { "nroots", (getter)ReedSolomon_getprop_nroots, NULL,
          "Parity symbols per codeword, `2E`.\n", NULL },
        { "symbol_bits", (getter)ReedSolomon_getprop_symbol_bits, NULL,
          "Symbol width `J`, in bits.\n", NULL },
        { NULL } };

static PyObject *
ReedSolomonObj_destroy (ReedSolomonObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      rs_codec_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ReedSolomonObj_enter (ReedSolomonObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ReedSolomonObj_exit (ReedSolomonObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      rs_codec_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ReedSolomonObj_methods[] = {

  { "encode", (PyCFunction)(void *)ReedSolomonObj_encode,
    METH_VARARGS | METH_KEYWORDS,
    "encode(x) -> ndarray\n"
    "\n"
    "Encode `k` information symbols into a whole `n`-symbol codeword.\n"
    "\n"
    "Systematic: the information symbols are copied through untouched and\n"
    "the `nroots` parity symbols follow them, which is the order they are\n"
    "transmitted in. `rs_encode` computes the parity; this places it.\n"
    "\n"
    "The WHOLE codeword rather than the parity alone, because that is the\n"
    "unit every other method here takes — rs_codec_decode,\n"
    "rs_codec_syndromes and rs_codec_codeword_ok all read `n` symbols, and a\n"
    "caller who wants the parity by itself can take the last `nroots` of the\n"
    "answer. (`rs_encode` is the other split, and is still there for a frame\n"
    "assembler that has already placed the information.)\n"
    "\n"
    "out may alias in — `rs_codec_encode (rs, buf, k, buf, n)` appends the\n"
    "parity to a buffer that already holds the information, which is the\n"
    "call a frame assembler makes and the one `rs_encode` exists for.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    `n` on success, or 0 if n_in is not exactly `k` or out is too small\n"
    "    — refusing rather than truncating, since a short codeword is not a\n"
    "    codeword.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ReedSolomon\n"
    ">>> rs = ReedSolomon(nroots=32)\n"
    ">>> info = np.arange(rs.k, dtype=np.uint8)\n"
    ">>> word = rs.encode(info)\n"
    ">>> word.size, bool(np.array_equal(word[: rs.k], info))\n"
    "(255, True)\n"
    ">>> rs.codeword_ok(word)\n"
    "1\n" },
  { "encode_max_out", (PyCFunction)ReedSolomonObj_encode_max_out, METH_VARARGS,
    "encode_max_out(n_in) -> int\n"
    "\n"
    "Symbols rs_codec_encode writes for n_in information symbols: a whole\n"
    "codeword, `n`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "decode", (PyCFunction)(void *)ReedSolomonObj_decode,
    METH_VARARGS | METH_KEYWORDS,
    "decode(codeword) -> int\n"
    "\n"
    "Correct up to `E` symbol errors, IN PLACE.\n"
    "\n"
    "`rs_decode`, over the caller's own buffer: the corrected symbols land\n"
    "in codeword itself, which is why the binding demands a writable array\n"
    "rather than quietly working on a copy the caller would then discard.\n"
    "\n"
    "**It either refuses or leaves a codeword.** On success the key equation\n"
    "has zeroed every syndrome by construction, so the result passes\n"
    "rs_codec_codeword_ok. On refusal codeword is untouched.\n"
    "\n"
    "A refusal is not the same claim as \"more than `E` errors\". Beyond `E` "
    "a\n"
    "bounded-distance decoder can land inside another codeword's sphere and\n"
    "miscorrect — a property of the code, not of this implementation — which\n"
    "is why this reports a COUNT rather than a verdict, and why frame-level\n"
    "accounting is the protection.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "codeword : NDArray[np.uint8]\n"
    "    `n` symbols, corrected in place.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Symbols corrected, 0 for an already-valid codeword, **-1** when the\n"
    "    word is too far from every codeword to name one, or **-2** when\n"
    "    codeword_len is not `n`. Two negative codes rather than one because\n"
    "    they are different kinds of fact: -1 is the channel's answer and -2\n"
    "    is the caller's mistake.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ReedSolomon\n"
    ">>> rs = ReedSolomon(nroots=32)\n"
    ">>> word = rs.encode(np.arange(rs.k, dtype=np.uint8))\n"
    ">>> word[3] ^= 0xFF          # one symbol, however many bits it moved\n"
    ">>> word[40] ^= 0x01\n"
    ">>> rs.decode(word)          # corrected in place\n"
    "2\n"
    ">>> bool(np.array_equal(word[: rs.k], np.arange(rs.k, dtype=np.uint8)))\n"
    "True\n" },
  { "syndromes", (PyCFunction)(void *)ReedSolomonObj_syndromes,
    METH_VARARGS | METH_KEYWORDS,
    "syndromes(x) -> ndarray\n"
    "\n"
    "The `nroots` syndromes of an `n`-symbol word.\n"
    "\n"
    "All zero is the DEFINING property of the code: it needs no encoder and\n"
    "no decoder to check, which is what makes it usable both as a test\n"
    "oracle and as a receiver's error detector. rs_codec_codeword_ok is this\n"
    "reduced to the one bit most callers want.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    `nroots`, or 0 if n_in is not `n` or out is too small.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ReedSolomon\n"
    ">>> rs = ReedSolomon(nroots=32)\n"
    ">>> word = rs.encode(np.zeros(rs.k, dtype=np.uint8))\n"
    ">>> bool(rs.syndromes(word).any())      # a codeword has none\n"
    "False\n"
    ">>> word[7] ^= 0x20\n"
    ">>> bool(rs.syndromes(word).any())\n"
    "True\n" },
  { "syndromes_max_out", (PyCFunction)ReedSolomonObj_syndromes_max_out,
    METH_VARARGS,
    "syndromes_max_out(n_in) -> int\n"
    "\n"
    "Syndromes rs_codec_syndromes writes: `nroots`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "codeword_ok", (PyCFunction)(void *)ReedSolomonObj_codeword_ok,
    METH_VARARGS | METH_KEYWORDS,
    "codeword_ok(codeword) -> int\n"
    "\n"
    "Is this a valid codeword? — every syndrome zero.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "codeword : NDArray[np.uint8]\n"
    "    `n` symbols.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    1 when every syndrome is zero, 0 otherwise — including when\n"
    "    codeword_len is not `n`, since a word of the wrong length is not a\n"
    "    codeword of this code.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ReedSolomon\n"
    ">>> rs = ReedSolomon(nroots=32)\n"
    ">>> rs.codeword_ok(np.zeros(rs.n, np.uint8))   # all-zero IS a codeword\n"
    "1\n"
    ">>> rs.codeword_ok(np.zeros(rs.n - 1, np.uint8))   # at the right size\n"
    "0\n" },
  { "generator", (PyCFunction)(void *)ReedSolomonObj_generator,
    METH_VARARGS | METH_KEYWORDS,
    "generator(out) -> int\n"
    "\n"
    "The `nroots + 1` coefficients of `g(x)`, `out[i]` for `x^i`.\n"
    "\n"
    "Exposed because standards PUBLISH them — CCSDS 131.0-B Annex G prints\n"
    "all 33 for `E = 16` — so a caller who has just configured a code from a\n"
    "document can check that they read the five numbers correctly, against\n"
    "the document rather than against this implementation.\n"
    "\n"
    "The caller supplies the buffer rather than being handed one, because\n"
    "the length is a property of the CODE and not of the call: `g(x)` has\n"
    "exactly `nroots + 1` coefficients and there is no other number a caller\n"
    "could ask for. A self-sizing method would carry a `count` parameter\n"
    "that means nothing, which is a worse trade than one line of allocation.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : NDArray[np.uint8]\n"
    "    Receives `nroots + 1` coefficients; `out[i]` is the coefficient of\n"
    "    `x^i`, so `out[nroots]` is 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    `nroots + 1`, or 0 if out is too small.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ReedSolomon\n"
    ">>> rs = ReedSolomon(nroots=32, field_poly=0x87, first_root=112,\n"
    "...                  root_stride=11)          # CCSDS 131.0-B 4.3\n"
    ">>> g = np.empty(rs.nroots + 1, np.uint8)\n"
    ">>> rs.generator(g)                  # Annex G prints all 33\n"
    "33\n"
    ">>> int(g[0]), int(g[-1])\n"
    "(1, 1)\n" },
  { "destroy", (PyCFunction)ReedSolomonObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)ReedSolomonObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a ReedSolomon be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "ReedSolomon\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ReedSolomonObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the ReedSolomon.\n"
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

static PyTypeObject ReedSolomonObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "coding.ReedSolomon",
  .tp_basicsize                           = sizeof (ReedSolomonObject),
  .tp_dealloc                             = (destructor)ReedSolomonObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a codec for the code named by the five arguments.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "nroots : int\n"
    "    Parity symbols per codeword, `2E` — even, at least 2, and small "
    "enough\n"
    "    to leave one information symbol. The code corrects `E = nroots / 2`\n"
    "    symbol errors.\n"
    "symbol_bits : int, default 8\n"
    "    `J`, the symbol width in bits, 2..8. A codeword is `n = 2**J - 1`\n"
    "    symbols, one per byte, so `J = 8` gives the familiar 255.\n"
    "field_poly : int, default 29\n"
    "    `F(x)`, low `J` bits, with `x**J` implicit. Must be PRIMITIVE — a\n"
    "    polynomial that generates a subgroup instead of the field produces\n"
    "    perfectly self-consistent arithmetic that interoperates with "
    "nothing,\n"
    "    so the constructor checks rather than trusts. The default 29 is "
    "`x**8 +\n"
    "    x**4 + x**3 + x**2 + 1`.\n"
    "first_root : int, default 1\n"
    "    `j0`: the generator's first root is `a**(root_stride * j0)`.\n"
    "root_stride : int, default 1\n"
    "    `s`: the roots are powers of `a**s`. Must be coprime with `n`, or "
    "the\n"
    "    `nroots` roots are not distinct and the code corrects fewer errors "
    "than\n"
    "    its parity count claims (CCSDS 4.3.4 states this as a note about\n"
    "    `a**11`; for a general code it is a condition, and the constructor\n"
    "    checks it).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``ReedSolomon: not "
    "a\n"
    "    usable code — need an even nroots in 2..64 leaving at least one\n"
    "    information symbol, 2 <= symbol_bits <= 8, a PRIMITIVE field_poly, "
    "and\n"
    "    a root_stride coprime with 2**symbol_bits - 1``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.coding import ReedSolomon\n"
    ">>> rs = ReedSolomon(nroots=32)      # RS(255,223) over the usual "
    "GF(256)\n"
    ">>> rs.n, rs.k, rs.e\n"
    "(255, 223, 16)\n"
    ">>> ReedSolomon(nroots=4, symbol_bits=4, field_poly=0b0011).n\n"
    "15\n",
  .tp_methods = ReedSolomonObj_methods,
  .tp_getset  = ReedSolomon_getset,
  .tp_new     = ReedSolomonObj_new,
  .tp_init    = (initproc)ReedSolomonObj_init,
};
