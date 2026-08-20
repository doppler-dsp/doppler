/*
 * coding_ext_conv_enc.c — ConvEncoder type for the coding module.
 *
 * Included by coding_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only coding_ext.c is compiled.
 */
/* ======================================================== */
/* ConvEncoderObject — wraps conv_enc_state_t *       */
/* ======================================================== */

#include "conv_enc/conv_enc_core.h"

typedef struct
{
  PyObject_HEAD conv_enc_state_t *handle;
} ConvEncoderObject;

static void
ConvEncoderObj_dealloc (ConvEncoderObject *self)
{
  if (self->handle)
    conv_enc_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ConvEncoderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ConvEncoderObject *self = (ConvEncoderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ConvEncoderObj_init (ConvEncoderObject *self, PyObject *args, PyObject *kwds)
{
  static char  *kwlist[]   = { "poly", "k", "invert", NULL };
  PyObject     *poly_obj   = NULL;
  unsigned long k_raw      = 7;
  unsigned long invert_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|kk", kwlist, &poly_obj,
                                    &k_raw, &invert_raw))
    return -1;
  uint32_t       k        = (uint32_t)k_raw;
  uint32_t       invert   = (uint32_t)invert_raw;
  PyArrayObject *poly_arr = (PyArrayObject *)PyArray_FROM_OTF (
      poly_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!poly_arr)
    {
      return -1;
    }
  size_t poly_len = (size_t)PyArray_SIZE (poly_arr);
  self->handle    = conv_enc_create ((const uint32_t *)PyArray_DATA (poly_arr),
                                     poly_len, k, invert);
  Py_DECREF (poly_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "ConvEncoder: not a usable code (need 1 to 6 non-zero "
                       "polynomials, each under 2**k, and 2 <= k <= 9)");
      return -1;
    }
  return 0;
}

static PyObject *
ConvEncoderObj_reset (ConvEncoderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  conv_enc_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
ConvEncoderObj_encode_max_out (ConvEncoderObject *self, PyObject *args)
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
      conv_enc_encode_max_out (self->handle, (size_t)n_in));
}

static PyObject *
ConvEncoderObj_encode (ConvEncoderObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = conv_enc_encode_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = conv_enc_encode (
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
  size_t _cap  = conv_enc_encode_max_out (self->handle, (size_t)n);
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
      = conv_enc_encode (self->handle, (const uint8_t *)PyArray_DATA (in_arr),
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
ConvEncoderObj_state_bytes (ConvEncoderObject *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (conv_enc_state_bytes (self->handle));
}

static PyObject *
ConvEncoderObj_get_state (ConvEncoderObject *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = conv_enc_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  conv_enc_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
ConvEncoderObj_set_state (ConvEncoderObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != conv_enc_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (conv_enc_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ConvEncoderObj_destroy (ConvEncoderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      conv_enc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ConvEncoderObj_enter (ConvEncoderObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ConvEncoderObj_exit (ConvEncoderObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      conv_enc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ConvEncoderObj_methods[] = {
  { "reset", (PyCFunction)ConvEncoderObj_reset, METH_NOARGS,
    "Return the register to all-zero, keeping the code.\n"
    "\n"
    "The boundary between two independent records, not a reconfiguration.\n"
    "The next encode starts from the same state a freshly created encoder is\n"
    "in, which is what makes a reset stream byte-identical to a fresh one.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ConvEncoder\n"
    ">>> e = ConvEncoder([0o171, 0o133], k=7)\n"
    ">>> e.reset()\n" },

  { "encode", (PyCFunction)(void *)ConvEncoderObj_encode,
    METH_VARARGS | METH_KEYWORDS,
    "encode(x) -> ndarray\n"
    "\n"
    "Encode information bits into channel symbols.\n"
    "\n"
    "The register carries across calls, so a long record may be fed in\n"
    "blocks and the symbol sequence is identical to one call — which is the\n"
    "property a standard fixes and a chunked encoder silently breaks.\n"
    "\n"
    "Outputs are emitted in polynomial order per input bit: for `[G1, G2]`,\n"
    "`out[2i]` is `G1`'s symbol for input bit `i` and `out[2i+1]` is `G2`'s.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Symbols written, or 0 if max_out is too small — in which case out\n"
    "    is untouched.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ConvEncoder, Viterbi\n"
    ">>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 40, dtype=np.uint8)\n"
    ">>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)\n"
    ">>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)\n"
    ">>> out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)\n"
    ">>> bool(np.array_equal(out, bits[: out.size]))\n"
    "True\n" },
  { "encode_max_out", (PyCFunction)ConvEncoderObj_encode_max_out, METH_VARARGS,
    "encode_max_out(n_in) -> int\n"
    "\n"
    "Symbols conv_enc_encode writes for n_in input bits.\n"
    "\n"
    "Exactly `n_in * n` — a convolutional code has no fill and no latency on\n"
    "\n"
    "the encode side, which is the asymmetry with viterbi_decode_max_out,\n"
    "\n"
    "where the traceback still owes bits at the start of a stream.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Number of input bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Symbols that call will write.\n" },
  { "state_bytes", (PyCFunction)ConvEncoderObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the ConvEncoder has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)ConvEncoderObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the ConvEncoder has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)ConvEncoderObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the ConvEncoder has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)ConvEncoderObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)ConvEncoderObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a ConvEncoder be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "ConvEncoder\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ConvEncoderObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the ConvEncoder.\n"
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

static PyTypeObject ConvEncoderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "coding.ConvEncoder",
  .tp_basicsize                           = sizeof (ConvEncoderObject),
  .tp_dealloc                             = (destructor)ConvEncoderObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Build an encoder for the code the polynomials describe.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "poly : NDArray[np.uint32]\n"
    "    Generator polynomials, one per output. The array IS the code;\n"
    "    `poly_len` gives `n`.\n"
    "k : int, default 7\n"
    "    Constraint length, 2 to `CONV_K_MAX`.\n"
    "invert : int, default 0\n"
    "    Bit `j` complements output `j`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``ConvEncoder: not "
    "a\n"
    "    usable code (need 1 to 6 non-zero polynomials, each under 2**k, and "
    "2\n"
    "    <= k <= 9)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ConvEncoder\n"
    ">>> e = ConvEncoder([0o171, 0o133], k=7, invert=0x2)\n"
    ">>> e.encode(np.zeros(8, dtype=np.uint8)).size\n"
    "16\n",
  .tp_methods = ConvEncoderObj_methods,
  .tp_new     = ConvEncoderObj_new,
  .tp_init    = (initproc)ConvEncoderObj_init,
};
