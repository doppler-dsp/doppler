/*
 * source_ext_awgn.c — AWGN type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* AWGNObject — wraps awgn_state_t *       */
/* ======================================================== */

#include "awgn/awgn_core.h"

typedef struct
{
  PyObject_HEAD awgn_state_t *handle;
} AWGNObject;

static void
AWGNObj_dealloc (AWGNObject *self)
{
  if (self->handle)
    awgn_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AWGNObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AWGNObject *self = (AWGNObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AWGNObj_init (AWGNObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "seed", "amplitude", NULL };
  unsigned long long seed_raw  = 0;
  float              amplitude = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kf", kwlist, &seed_raw,
                                    &amplitude))
    return -1;
  uint64_t seed = (uint64_t)seed_raw;
  self->handle  = awgn_create (seed, amplitude);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "awgn_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AWGNObj_reset (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  awgn_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AWGNObj_generate_max_out (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (awgn_generate_max_out (self->handle));
}

static PyObject *
AWGNObj_generate (AWGNObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = awgn_generate_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out
          = awgn_generate (self->handle, (size_t)n,
                           (float complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = awgn_generate_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = awgn_generate (self->handle, (size_t)n, _d0, _cap);
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
AWGNObj_reseed (AWGNObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "seed", NULL };
  unsigned long long seed_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &seed_raw))
    return NULL;
  uint64_t seed = (uint64_t)seed_raw;
  awgn_reseed (self->handle, seed);
  Py_RETURN_NONE;
}

static PyObject *
AWGNObj_state_bytes (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (awgn_state_bytes (self->handle));
}

static PyObject *
AWGNObj_get_state (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = awgn_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  awgn_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AWGNObj_set_state (AWGNObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != awgn_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (awgn_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
AWGN_getprop_amplitude (AWGNObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble ((double)awgn_get_amplitude (self->handle));
}
static int
AWGN_setprop_amplitude (AWGNObject *self, PyObject *value,
                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  float v = 0.0f;
  if (!PyArg_Parse (value, "f", &v))
    return -1;
  awgn_set_amplitude (self->handle, v);
  return 0;
}

static PyGetSetDef AWGN_getset[]
    = { { "amplitude", (getter)AWGN_getprop_amplitude,
          (setter)AWGN_setprop_amplitude,
          "Return the current amplitude (per-component std dev).\n", NULL },
        { NULL } };

static PyObject *
AWGNObj_destroy (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      awgn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AWGNObj_enter (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AWGNObj_exit (AWGNObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      awgn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AWGNObj_methods[] = {
  { "reset", (PyCFunction)AWGNObj_reset, METH_NOARGS,
    "Reset RNG to the seed supplied at create time. Re-runs the "
    "SplitMix64 seeding procedure with the original seed so the next "
    "awgn_generate() call produces exactly the same samples as the "
    "first call after awgn_create().  amplitude is not changed." },

  { "generate", (PyCFunction)(void *)AWGNObj_generate,
    METH_VARARGS | METH_KEYWORDS,
    "generate(n=1) -> ndarray\n"
    "\n"
    "Generate n complex CF32 AWGN samples. Uses Box-Muller with "
    "xoshiro256++ to fill `out` with independent complex Gaussians: Re "
    "and Im each have zero mean and standard deviation `amplitude`.  "
    "Total complex power = 2 × amplitude². The AVX2 path processes 8 "
    "samples in parallel when available.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AWGN\n"
    "    >>> obj = AWGN(0, 1.0)\n"
    "    >>> y = obj.generate(4)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "generate_max_out", (PyCFunction)AWGNObj_generate_max_out, METH_NOARGS,
    "generate_max_out() -> int\n\nMax output length generate() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "reseed", (PyCFunction)(void *)AWGNObj_reseed,
    METH_VARARGS | METH_KEYWORDS,
    "reseed(seed) -> None\n"
    "\n"
    "Reseed the RNG and reset all xoshiro256++ state. Equivalent to "
    "calling awgn_destroy() and awgn_create(seed, amplitude) but reuses "
    "the existing allocation.  amplitude is unchanged.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "seed : int\n"
    "    New 64-bit RNG seed.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.source import AWGN\n"
    ">>> gen = AWGN(seed=0, amplitude=1.0)\n"
    ">>> gen.reseed(42)\n"
    ">>> out1 = gen.generate(4)\n"
    ">>> gen2 = AWGN(seed=42, amplitude=1.0)\n"
    ">>> out2 = gen2.generate(4)\n"
    ">>> bool(np.all(out1 == out2))\n"
    "True\n" },
  { "state_bytes", (PyCFunction)AWGNObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AWGN has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AWGNObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AWGN has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AWGNObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AWGN has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AWGNObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AWGNObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AWGN be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AWGN\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AWGNObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the AWGN.\n"
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

static PyTypeObject AWGNObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "source.AWGN",
  .tp_basicsize                           = sizeof (AWGNObject),
  .tp_dealloc                             = (destructor)AWGNObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an AWGN generator. Allocates state, seeds the "
                "xoshiro256++ RNG via SplitMix64, and sets up both the scalar and "
                "the AVX2 parallel streams.  The initial seed is stored so "
                "awgn_reset() can reproduce the exact same stream.\n",
  .tp_methods = AWGNObj_methods,
  .tp_getset  = AWGN_getset,
  .tp_new     = AWGNObj_new,
  .tp_init    = (initproc)AWGNObj_init,
};
