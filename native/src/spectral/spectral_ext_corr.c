/*
 * spectral_ext_corr.c — Corr type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* CorrObject — wraps corr_state_t *       */
/* ======================================================== */

#include "corr/corr_core.h"

typedef struct
{
  PyObject_HEAD corr_state_t *handle;
} CorrObject;

static void
CorrObj_dealloc (CorrObject *self)
{
  if (self->handle)
    corr_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CorrObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CorrObject *self = (CorrObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CorrObj_init (CorrObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "ref", "dwell", "nthreads", "n_out", NULL };
  PyObject          *ref_obj   = NULL;
  unsigned long long dwell_raw = 1;
  int                nthreads  = 1;
  unsigned long long n_out_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KiK", kwlist, &ref_obj,
                                    &dwell_raw, &nthreads, &n_out_raw))
    return -1;
  size_t         dwell   = (size_t)dwell_raw;
  size_t         n_out   = (size_t)n_out_raw;
  PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF (
      ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!ref_arr)
    {
      return -1;
    }
  size_t ref_len = (size_t)PyArray_SIZE (ref_arr);
  self->handle   = corr_create ((const float complex *)PyArray_DATA (ref_arr),
                                ref_len, dwell, nthreads, n_out);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "corr_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CorrObj_reset (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  corr_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CorrObj_execute_max_out (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (corr_execute_max_out (self->handle));
}

static PyObject *
CorrObj_execute (CorrObject *self, PyObject *args, PyObject *kwds)
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = corr_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = corr_execute (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = corr_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = corr_execute (self->handle,
                               (const float complex *)PyArray_DATA (in_arr),
                               (size_t)n, _d0, _cap);
  Py_DECREF (in_arr);
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
CorrObj_state_bytes (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (corr_state_bytes (self->handle));
}

static PyObject *
CorrObj_get_state (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = corr_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  corr_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CorrObj_set_state (CorrObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != corr_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (corr_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Corr_getprop_n (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
Corr_getprop_n_out (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n_out);
}
static PyObject *
Corr_getprop_dwell (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
Corr_getprop_count (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->count);
}

static PyGetSetDef Corr_getset[]
    = { { "n", (getter)Corr_getprop_n, NULL,
          "FFT / reference length (samples).\n", NULL },
        { "n_out", (getter)Corr_getprop_n_out, NULL,
          "Output length (== n unless decoupled).\n", NULL },
        { "dwell", (getter)Corr_getprop_dwell, NULL,
          "Integration depth; dump every dwell calls.\n", NULL },
        { "count", (getter)Corr_getprop_count, NULL,
          "Frames accumulated so far (0 … dwell-1).\n", NULL },
        { NULL } };

static PyObject *
CorrObj_destroy (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      corr_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrObj_enter (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CorrObj_exit (CorrObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      corr_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CorrObj_methods[] = {
  { "reset", (PyCFunction)CorrObj_reset, METH_NOARGS,
    "Zero the accumulator and reset the integration counter to 0. Equivalent "
    "to starting a fresh dwell cycle without tearing down the FFT plans.  "
    "Does NOT recompute ref_spec; use corr_set_ref() to replace the "
    "reference." },

  { "execute", (PyCFunction)(void *)CorrObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Correlate one frame and optionally dump the coherent accumulator. Runs: "
    "forward FFT → pointwise multiply with ref_spec → accumulate the "
    "cross-spectrum; on dump, inverse FFT → normalise (÷ n).  Accumulating in "
    "the frequency domain and inverting once is exactly the per-frame inverse "
    "summed, by linearity of the IFFT — valid because the dwell is "
    "**coherent** (a complex sum); a non-coherent (magnitude) integration "
    "could not defer the inverse. On the dwell-th call out is written, the "
    "accumulator is zeroed, and the counter resets; the function returns "
    "n_out.  All other calls return 0 and leave out unmodified.  In Python, a "
    "dump returns an ndarray and a no-dump returns None.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    n_out on a dump call (or max_out if smaller), 0 otherwise (None in\n"
    "    Python).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0\n"
    ">>> corr = Corr(ref=ref, dwell=2)\n"
    ">>> x = np.ones(4, dtype=np.complex64)\n"
    ">>> corr.execute(x) is None   # frame 1 — no dump yet\n"
    "True\n"
    ">>> corr.execute(x).tolist()  # frame 2 — dump\n"
    "[(2+0j), (2+0j), (2+0j), (2+0j)]\n" },
  { "execute_max_out", (PyCFunction)CorrObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)CorrObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the CorrObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CorrObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CorrObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CorrObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the CorrObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)CorrObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)CorrObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Corr be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Corr\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CorrObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Corr.\n"
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

static PyTypeObject CorrObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.Corr",
  .tp_basicsize                           = sizeof (CorrObject),
  .tp_dealloc                             = (destructor)CorrObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a 1-D FFT correlator with coherent integrate-and-dump. "
    "Pre-computes conj(FFT(ref)) once at construction so each execute() call "
    "costs only two FFTs and n complex multiplies.  ref may be freed after "
    "this returns.  With dwell == 1 every call produces output; with larger "
    "values the accumulator absorbs dwell frames before dumping.\n",
  .tp_methods = CorrObj_methods,
  .tp_getset  = Corr_getset,
  .tp_new     = CorrObj_new,
  .tp_init    = (initproc)CorrObj_init,
};
