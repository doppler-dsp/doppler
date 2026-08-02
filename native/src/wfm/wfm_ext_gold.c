/*
 * wfm_ext_gold.c — Gold type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* GoldObject — wraps gold_state_t *       */
/* ======================================================== */

#include "gold/gold_core.h"

typedef struct
{
  PyObject_HEAD gold_state_t *handle;
} GoldObject;

static void
GoldObj_dealloc (GoldObject *self)
{
  if (self->handle)
    gold_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
GoldObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  GoldObject *self = (GoldObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
GoldObj_init (GoldObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "taps_a", "seed_a", "taps_b", "seed_b", "length", NULL };
  unsigned long long taps_a_raw = 934;
  unsigned long long seed_a_raw = 350;
  unsigned long long taps_b_raw = 567;
  unsigned long long seed_b_raw = 73;
  unsigned long      length_raw = 10;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKKKk", kwlist, &taps_a_raw,
                                    &seed_a_raw, &taps_b_raw, &seed_b_raw,
                                    &length_raw))
    return -1;
  uint64_t taps_a = (uint64_t)taps_a_raw;
  uint64_t seed_a = (uint64_t)seed_a_raw;
  uint64_t taps_b = (uint64_t)taps_b_raw;
  uint64_t seed_b = (uint64_t)seed_b_raw;
  uint32_t length = (uint32_t)length_raw;
  self->handle    = gold_create (taps_a, seed_a, taps_b, seed_b, length);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "gold_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
GoldObj_reset (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  gold_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
GoldObj_generate_max_out (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (gold_generate_max_out (self->handle));
}

static PyObject *
GoldObj_generate (GoldObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = gold_generate_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t   n_out = gold_generate (self->handle, (size_t)n,
                                      (uint8_t *)PyArray_DATA (out_arr), _cap);
      npy_intp _odim = (npy_intp)n_out;
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
  size_t _cap  = gold_generate_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      return NULL;
    }
  uint8_t *_d0   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = gold_generate (self->handle, (size_t)n, _d0, _cap);
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
GoldObj_state_bytes (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (gold_state_bytes (self->handle));
}

static PyObject *
GoldObj_get_state (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = gold_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  gold_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
GoldObj_set_state (GoldObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != gold_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (gold_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
GoldObj_destroy (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      gold_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
GoldObj_enter (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
GoldObj_exit (GoldObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      gold_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef GoldObj_methods[] = {
  { "reset", (PyCFunction)GoldObj_reset, METH_NOARGS,
    "Reset Gold to its post-create state. Reloads both LFSR registers from "
    "their original seeds so the sequence restarts from chip 0. Useful for "
    "reproducible captures without re-allocating." },

  { "generate", (PyCFunction)(void *)GoldObj_generate,
    METH_VARARGS | METH_KEYWORDS,
    "generate(n=1) -> ndarray\n"
    "\n"
    "Generate ``n`` chips into ``out`` and advance both LFSRs by ``n`` "
    "positions. Each element of ``out`` is 0 or 1. Requesting more than one "
    "period is valid — the sequence simply wraps around. The Python binding "
    "returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy "
    "the result before calling generate again if you need a snapshot.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Gold\n"
    "    >>> obj = Gold(934, 350, 567, 73, 10)\n"
    "    >>> y = obj.generate(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "generate_max_out", (PyCFunction)GoldObj_generate_max_out, METH_NOARGS,
    "generate_max_out() -> int\n\nMax output length generate() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)GoldObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the GoldObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)GoldObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the GoldObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)GoldObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the GoldObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)GoldObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)GoldObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Gold be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Gold\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)GoldObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Gold.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
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

static PyTypeObject GoldObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm.Gold",
  .tp_basicsize                           = sizeof (GoldObject),
  .tp_dealloc                             = (destructor)GoldObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Allocate and initialise a CCSDS-style Gold code generator. Two "
            "independent Fibonacci LFSRs of the same ``length`` free-run in "
            "lock-step; each output chip is the XOR of both registers' "
            "current top-bit (stage ``length``, i.e. bit ``length-1``). Both "
            "registers shift left one bit per chip: the new bit (parity of "
            "the tapped stages, read *before* the shift) enters at stage 1 "
            "(bit 0), and the old stage-``length`` bit is discarded after "
            "being XORed into the output. The sequence period is ``2^length - "
            "1`` for primitive ``taps_a``/``taps_b``.\n",
  .tp_methods = GoldObj_methods,
  .tp_new     = GoldObj_new,
  .tp_init    = (initproc)GoldObj_init,
};
