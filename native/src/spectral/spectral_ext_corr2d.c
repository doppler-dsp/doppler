/*
 * spectral_ext_corr2d.c — Corr2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* Corr2DObject — wraps corr2d_state_t *       */
/* ======================================================== */

#include "corr2d/corr2d_core.h"

typedef struct
{
  PyObject_HEAD corr2d_state_t *handle;
} Corr2DObject;

static void
Corr2DObj_dealloc (Corr2DObject *self)
{
  if (self->handle)
    corr2d_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Corr2DObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Corr2DObject *self = (Corr2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
Corr2DObj_init (Corr2DObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "ref", "dwell", "nthreads", "ny_out", "nx_out", NULL };
  PyObject          *ref_obj    = NULL;
  unsigned long long dwell_raw  = 1;
  int                nthreads   = 1;
  unsigned long long ny_out_raw = 0;
  unsigned long long nx_out_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KiKK", kwlist, &ref_obj,
                                    &dwell_raw, &nthreads, &ny_out_raw,
                                    &nx_out_raw))
    return -1;
  size_t         dwell   = (size_t)dwell_raw;
  size_t         ny_out  = (size_t)ny_out_raw;
  size_t         nx_out  = (size_t)nx_out_raw;
  PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF (
      ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!ref_arr)
    {
      return -1;
    }
  /* Hand-patch (sacred fragment): corr2d_create takes the reference's two
     dimensions split out, which a flat array init-param cannot express, so
     this marshaling stays hand-written. Regenerating this file drops it —
     see the note in docs/dev/adding-a-module.md. */
  if (PyArray_NDIM (ref_arr) != 2)
    {
      Py_DECREF (ref_arr);
      PyErr_SetString (PyExc_ValueError, "ref must be a 2-D (ny, nx) array");
      return -1;
    }
  size_t ref_dim0 = (size_t)PyArray_DIM (ref_arr, 0);
  size_t ref_dim1 = (size_t)PyArray_DIM (ref_arr, 1);
  self->handle
      = corr2d_create ((const float complex *)PyArray_DATA (ref_arr), ref_dim0,
                       ref_dim1, dwell, nthreads, ny_out, nx_out);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "corr2d_create returned NULL");
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
  corr2d_reset (self->handle);
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
  return PyLong_FromSize_t (corr2d_execute_max_out (self->handle));
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
      size_t _omax    = corr2d_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = corr2d_execute (
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
  size_t _cap  = corr2d_execute_max_out (self->handle);
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
  size_t n_out = corr2d_execute (self->handle,
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
Corr2DObj_state_bytes (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (corr2d_state_bytes (self->handle));
}

static PyObject *
Corr2DObj_get_state (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = corr2d_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  corr2d_get_state (self->handle, PyBytes_AS_STRING (_b));
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
  if ((size_t)PyBytes_GET_SIZE (arg) != corr2d_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (corr2d_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
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
        { "dwell", (getter)Corr2D_getprop_dwell, NULL, "Integration depth.\n",
          NULL },
        { "count", (getter)Corr2D_getprop_count, NULL,
          "Frames accumulated (0 … dwell-1).\n", NULL },
        { NULL } };

static PyObject *
Corr2DObj_destroy (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      corr2d_destroy (self->handle);
      self->handle = NULL;
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
      corr2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef Corr2DObj_methods[] = {
  { "reset", (PyCFunction)Corr2DObj_reset, METH_NOARGS,
    "Zero the accumulator and reset the integration counter to 0. Equivalent "
    "to starting a fresh dwell cycle without rebuilding FFT plans or "
    "recomputing ref_spec." },

  { "execute", (PyCFunction)(void *)Corr2DObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Correlate one 2-D frame and optionally dump the coherent accumulator. "
    "Runs the 2-D pipeline: FFT2 → pointwise multiply with ref_spec → "
    "accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny*nx).  "
    "Accumulating in the frequency domain and inverting once is exactly the "
    "per-frame inverse summed, by linearity of the IFFT — valid because the "
    "dwell is **coherent** (a complex sum); a non-coherent (magnitude) "
    "integration could not defer the inverse.  The Python wrapper accepts a "
    "(ny, nx) CF32 ndarray; a dump returns a flat length-ny*nx ndarray, a "
    "no-dump returns None.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    ny*nx on a dump (or max_out if smaller), 0 otherwise (None in\n"
    "    Python).\n"
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
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)Corr2DObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Corr2DObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)Corr2DObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the Corr2DObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)Corr2DObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Corr2DObj has already been destroyed.\n"
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
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)Corr2DObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Corr2d be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Corr2d\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)Corr2DObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Corr2d.\n"
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

static PyTypeObject Corr2DObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.Corr2D",
  .tp_basicsize                           = sizeof (Corr2DObject),
  .tp_dealloc                             = (destructor)Corr2DObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Allocate a 2-D FFT correlator with coherent integrate-and-dump. "
            "Two-dimensional extension of corr_create().  The reference is a "
            "flat row-major ny×nx CF32 array; its conjugate spectrum is "
            "pre-computed once so each execute() call costs two 2-D FFTs plus "
            "ny*nx complex multiplies. The Python wrapper requires ref to be "
            "a 2-D ndarray with shape (ny, nx); it passes a flat view to C.\n",
  .tp_methods = Corr2DObj_methods,
  .tp_getset  = Corr2D_getset,
  .tp_new     = Corr2DObj_new,
  .tp_init    = (initproc)Corr2DObj_init,
};
