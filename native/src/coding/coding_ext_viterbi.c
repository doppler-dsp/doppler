/*
 * coding_ext_viterbi.c — Viterbi type for the coding module.
 *
 * Included by coding_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only coding_ext.c is compiled.
 */
/* ======================================================== */
/* ViterbiObject — wraps viterbi_state_t *       */
/* ======================================================== */

#include "viterbi/viterbi_core.h"

typedef struct
{
  PyObject_HEAD viterbi_state_t *handle;
} ViterbiObject;

static void
ViterbiObj_dealloc (ViterbiObject *self)
{
  if (self->handle)
    viterbi_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ViterbiObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ViterbiObject *self = (ViterbiObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ViterbiObj_init (ViterbiObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]   = { "poly", "k", "invert", "depth", NULL };
  PyObject          *poly_obj   = NULL;
  unsigned long      k_raw      = 7;
  unsigned long      invert_raw = 0;
  unsigned long long depth_raw  = 35;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|kkK", kwlist, &poly_obj,
                                    &k_raw, &invert_raw, &depth_raw))
    return -1;
  uint32_t       k        = (uint32_t)k_raw;
  uint32_t       invert   = (uint32_t)invert_raw;
  size_t         depth    = (size_t)depth_raw;
  PyArrayObject *poly_arr = (PyArrayObject *)PyArray_FROM_OTF (
      poly_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!poly_arr)
    {
      return -1;
    }
  size_t poly_len = (size_t)PyArray_SIZE (poly_arr);
  self->handle    = viterbi_create ((const uint32_t *)PyArray_DATA (poly_arr),
                                    poly_len, k, invert, depth);
  Py_DECREF (poly_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "Viterbi: not a usable code (need 1 to 6 non-zero "
                       "polynomials, each under 2**k, 2 <= k <= 9, and depth "
                       ">= 1)");
      return -1;
    }
  return 0;
}

static PyObject *
ViterbiObj_reset (ViterbiObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  viterbi_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
ViterbiObj_decode_max_out (ViterbiObject *self, PyObject *args)
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
      viterbi_decode_max_out (self->handle, (size_t)n_in));
}

static PyObject *
ViterbiObj_decode (ViterbiObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = viterbi_decode_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = viterbi_decode (
          self->handle, (const float *)PyArray_DATA (in_arr), (size_t)n,
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
  size_t _cap  = viterbi_decode_max_out (self->handle, (size_t)n);
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
      = viterbi_decode (self->handle, (const float *)PyArray_DATA (in_arr),
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
ViterbiObj_state_bytes (ViterbiObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (viterbi_state_bytes (self->handle));
}

static PyObject *
ViterbiObj_get_state (ViterbiObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = viterbi_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  viterbi_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
ViterbiObj_set_state (ViterbiObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != viterbi_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (viterbi_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ViterbiObj_destroy (ViterbiObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      viterbi_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ViterbiObj_enter (ViterbiObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ViterbiObj_exit (ViterbiObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      viterbi_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ViterbiObj_methods[] = {
  { "reset", (PyCFunction)ViterbiObj_reset, METH_NOARGS,
    "Return to the all-zero start state, discarding the traceback.\n"
    "\n"
    "The code and the depth are unchanged — this is the boundary between two\n"
    "independent captures, not a reconfiguration. The next decode refills\n"
    "the traceback before it emits, exactly as after create, and the\n"
    "all-zero state is given the winning metric, matching an encoder that\n"
    "starts from a reset register.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.coding import Viterbi\n"
    ">>> v = Viterbi([0o171, 0o133], k=7, depth=35)\n"
    ">>> v.reset()\n" },

  { "decode", (PyCFunction)(void *)ViterbiObj_decode,
    METH_VARARGS | METH_KEYWORDS,
    "decode(x) -> ndarray\n"
    "\n"
    "Decode soft channel symbols into information bits.\n"
    "\n"
    "The input carries one value per channel symbol, in the convention\n"
    "`mpsk_soft_demap` produces: `L = log(P(0)/P(1))`, so **positive means\n"
    "symbol 0**. The branch metric for an expected symbol e is `+L` when `e\n"
    "== 0` and `-L` otherwise, and the survivor maximises the sum — which\n"
    "makes the decoder agree with `mpsk_demap` on hard decisions by\n"
    "construction rather than by a second convention.\n"
    "\n"
    "A maximum-likelihood path cannot move when every metric is scaled by a\n"
    "positive constant, so **the LLRs need no accurate scaling** — a caller\n"
    "with no SNR estimate may pass unscaled values.\n"
    "\n"
    "Streaming: state carries across calls, so a long capture may be fed in\n"
    "blocks and the bits come out continuously. The first `depth - 1`\n"
    "branches of a stream produce no output — the traceback walks `depth -\n"
    "1` steps back, so a decision needs that many branches BEHIND it — and\n"
    "thereafter one bit is emitted per `n` symbols consumed.\n"
    "viterbi_decode_max_out is the same statement as arithmetic, and is what\n"
    "a caller should size a buffer with rather than repeating this sentence:\n"
    "they disagreed by one until a test asserted the count against a\n"
    "literal.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Bits written, which may be 0 while the traceback fills.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Viterbi\n"
    ">>> v = Viterbi([0o171, 0o133], k=7, depth=35)\n"
    ">>> llr = np.array([2.0, -2.0] * 128, dtype=np.float32)\n"
    ">>> bits = v.decode(llr)\n"
    ">>> set(np.unique(bits)) <= {0, 1}\n"
    "True\n" },
  { "decode_max_out", (PyCFunction)ViterbiObj_decode_max_out, METH_VARARGS,
    "decode_max_out(n_in) -> int\n"
    "\n"
    "Bits viterbi_decode will emit for n_in soft symbols.\n"
    "\n"
    "Accounts for the fill still owed at the start of a stream, so a caller\n"
    "can\n"
    "\n"
    "size a buffer exactly rather than conservatively.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Number of soft symbols the next call would be given.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Bits that call would write.\n" },
  { "state_bytes", (PyCFunction)ViterbiObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Viterbi has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)ViterbiObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the Viterbi has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)ViterbiObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Viterbi has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)ViterbiObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)ViterbiObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Viterbi be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Viterbi\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ViterbiObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Viterbi.\n"
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

static PyTypeObject ViterbiObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "coding.Viterbi",
  .tp_basicsize                           = sizeof (ViterbiObject),
  .tp_dealloc                             = (destructor)ViterbiObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Build a decoder for the code the polynomials describe.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "poly : NDArray[np.uint32]\n"
    "    Generator polynomials, one per output. The array IS the code;\n"
    "    `poly_len` gives `n`.\n"
    "k : int, default 7\n"
    "    k (default: 7).\n"
    "invert : int, default 0\n"
    "    invert (default: 0).\n"
    "depth : int, default 35\n"
    "    depth (default: 35).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``Viterbi: not a "
    "usable\n"
    "    code (need 1 to 6 non-zero polynomials, each under 2**k, 2 <= k <= "
    "9,\n"
    "    and depth >= 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import Viterbi\n"
    ">>> v = Viterbi([0o171, 0o133], k=7, depth=35)\n"
    ">>> v.decode(np.zeros(8, dtype=np.float32)).dtype\n"
    "dtype('uint8')\n",
  .tp_methods = ViterbiObj_methods,
  .tp_new     = ViterbiObj_new,
  .tp_init    = (initproc)ViterbiObj_init,
};
