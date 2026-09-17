/*
 * wfm_ext_pn.c — PN type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* PNObject — wraps pn_state_t *       */
/* ======================================================== */

#include "pn/pn_core.h"

typedef struct
{
  PyObject_HEAD pn_state_t *handle;
} PNObject;

static void
PNObj_dealloc (PNObject *self)
{
  if (self->handle)
    pn_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PNObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PNObject *self = (PNObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PNObj_init (PNObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]   = { "poly", "seed", "length", "lfsr", NULL };
  unsigned long long poly_raw   = 0;
  unsigned long long seed_raw   = 0;
  unsigned long      length_raw = 0;
  const char        *lfsr_str   = "galois";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKks", kwlist, &poly_raw,
                                    &seed_raw, &length_raw, &lfsr_str))
    return -1;
  uint64_t poly   = (uint64_t)poly_raw;
  uint64_t seed   = (uint64_t)seed_raw;
  uint32_t length = (uint32_t)length_raw;
  int      lfsr   = 0;
  if (strcmp (lfsr_str, "galois") == 0)
    lfsr = 0;
  else if (strcmp (lfsr_str, "fibonacci") == 0)
    lfsr = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "lfsr must be one of \"galois\", \"fibonacci\", got '%s'",
                    lfsr_str);
      return -1;
    }
  /* Hand-patch (sacred fragment): poly=0 means auto-select the MLS primitive
     polynomial for `length`, matching the Synth(pn_poly=0) convention (#191).
     The MLS table starts at n=2, so guard length >= 2 — below that there is
     no maximal-length sequence and poly stays 0. Not expressible in the
     manifest; re-apply after any regeneration of this fragment. */
  if (poly == 0 && length >= 2)
    poly = mls_poly (length);
  self->handle = pn_create (poly, seed, length, lfsr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "pn_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
PNObj_reset (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  pn_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
PNObj_generate_max_out (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (pn_generate_max_out (self->handle));
}

static PyObject *
PNObj_generate (PNObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = pn_generate_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t    n_out  = pn_generate (self->handle, (size_t)n,
                                      (uint8_t *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = pn_generate_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      return NULL;
    }
  uint8_t *_d0   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = pn_generate (self->handle, (size_t)n, _d0, _cap);
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
PNObj_state_bytes (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (pn_state_bytes (self->handle));
}

static PyObject *
PNObj_get_state (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = pn_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  pn_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
PNObj_set_state (PNObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != pn_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (pn_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PNObj_destroy (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      pn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PNObj_enter (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PNObj_exit (PNObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      pn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef PNObj_methods[] = {
  { "reset", (PyCFunction)PNObj_reset, METH_NOARGS,
    "Reset PN to its post-create state. Reloads the LFSR register from\n"
    "the original seed so the sequence restarts from chip 0. Useful for\n"
    "reproducible captures without re-allocating.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import PN\n"
    ">>> import numpy as np\n"
    ">>> p = PN(poly=96, seed=1, length=7)\n"
    ">>> a = p.generate(8).copy()\n"
    ">>> p.reset()\n"
    ">>> np.array_equal(a, p.generate(8))\n"
    "True\n" },

  { "generate", (PyCFunction)(void *)PNObj_generate,
    METH_VARARGS | METH_KEYWORDS,
    "generate(count=1) -> ndarray\n"
    "\n"
    "Generate ``n`` chips into ``out`` and advance the LFSR by ``n``\n"
    "positions. Each element of ``out`` is 0 or 1. Requesting more than one\n"
    "MLS period is valid — the sequence simply wraps around. The Python\n"
    "binding returns a zero-copy NumPy uint8 view over a pre-allocated\n"
    "buffer; copy the result before calling generate again if you need a\n"
    "snapshot.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[np.uint8] | None\n"
    "    Output buffer of at least ``n`` uint8 elements; each element\n"
    "    receives 0 or 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    min(n, max_out) chips.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import PN\n"
    ">>> import numpy as np\n"
    ">>> p = PN(poly=96, seed=1, length=7)\n"
    ">>> chips = p.generate(127)\n"
    ">>> chips[:8].tolist()\n"
    "[1, 0, 0, 0, 0, 0, 1, 1]\n"
    ">>> int(chips.sum())   # 64 ones per MLS period\n"
    "64\n" },
  { "generate_max_out", (PyCFunction)PNObj_generate_max_out, METH_NOARGS,
    "generate_max_out() -> int\n"
    "\n"
    "Largest number of samples generate() can return in the current\n"
    "state.\n"
    "\n"
    "Size an `out=` buffer with this before calling generate(), or use it to\n"
    "allocate one up front. The bound is this object's own: what it depends\n"
    "on is a property of the algorithm, so a header block on\n"
    "generate_max_out() replaces this text.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Upper bound on the output length; the actual call may return "
    "fewer.\n" },
  { "state_bytes", (PyCFunction)PNObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the PN has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)PNObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the PN has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)PNObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the PN has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)PNObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)PNObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a PN be used in a `with` statement so its C resources are released\n"
    "deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "PN\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)PNObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the PN.\n"
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

static PyTypeObject PNObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm.PN",
  .tp_basicsize                           = sizeof (PNObject),
  .tp_dealloc                             = (destructor)PNObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate and initialise a maximal-length-sequence LFSR. The register is\n"
    "seeded from ``seed`` and will produce a pseudo-random binary sequence "
    "with\n"
    "period 2^length - 1 for any primitive ``poly``. Both Galois and "
    "Fibonacci\n"
    "realizations share the same primitive polynomial and therefore the same\n"
    "period; they differ only in chip ordering/phase.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "poly : int, default 0\n"
    "    Galois feedback tap polynomial (right-shift convention). The LSB is "
    "the\n"
    "    tap at position 0 (always 1 for a primitive poly); bit k=1 means tap "
    "at\n"
    "    position k. Default 96 (0x60) is primitive for length=7, giving "
    "period\n"
    "    127. The Fibonacci taps are derived automatically so you only "
    "supply\n"
    "    one value.\n"
    "seed : int, default 0\n"
    "    Initial LFSR register state; must be non-zero (the all-zero state is "
    "a\n"
    "    fixed point). Default 1.\n"
    "length : int, default 0\n"
    "    Register width in bits, 1..64. The sequence period is 2^length - 1 "
    "for\n"
    "    a primitive polynomial. Default 7.\n"
    "lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    Realization: PN_GALOIS (0, default) or PN_FIBONACCI (1).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import PN\n"
    ">>> import numpy as np\n"
    ">>> p = PN(poly=96, seed=1, length=7)\n"
    ">>> chips = p.generate(127)\n"
    ">>> chips.dtype\n"
    "dtype('uint8')\n"
    ">>> int(chips.sum())   # 64 ones per MLS period (2^(n-1))\n"
    "64\n",
  .tp_methods = PNObj_methods,
  .tp_new     = PNObj_new,
  .tp_init    = (initproc)PNObj_init,
};
