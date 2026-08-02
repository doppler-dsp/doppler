/*
 * source_ext_lo.c — LO type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* LOObject — wraps lo_state_t *       */
/* ======================================================== */

#include "lo/lo_core.h"

typedef struct
{
  PyObject_HEAD lo_state_t *handle;
} LOObject;

static void
LOObj_dealloc (LOObject *self)
{
  if (self->handle)
    lo_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
LOObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  LOObject *self = (LOObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
LOObj_init (LOObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "norm_freq", NULL };
  double       norm_freq = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|d", kwlist, &norm_freq))
    return -1;
  self->handle = lo_create (norm_freq);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "lo_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
LOObj_reset (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  lo_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
LOObj_steps_max_out (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (lo_steps_max_out (self->handle));
}

static PyObject *
LOObj_steps (LOObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = lo_steps_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = lo_steps (self->handle, (size_t)n,
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
  size_t _cap  = lo_steps_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = lo_steps (self->handle, (size_t)n, _d0, _cap);
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
LOObj_steps_ctrl_max_out (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (lo_steps_ctrl_max_out (self->handle));
}

static PyObject *
LOObj_steps_ctrl (LOObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
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
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = lo_steps_ctrl_max_out (self->handle);
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
      size_t n_out = lo_steps_ctrl (
          self->handle, (const float *)PyArray_DATA (ctrl_arr),
          (size_t)PyArray_SIZE (ctrl_arr),
          (float complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (ctrl_arr);
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
  size_t _need = (size_t)PyArray_SIZE (ctrl_arr);
  size_t _cap  = lo_steps_ctrl_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (ctrl_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out
      = lo_steps_ctrl (self->handle, (const float *)PyArray_DATA (ctrl_arr),
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
LOObj_state_bytes (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (lo_state_bytes (self->handle));
}

static PyObject *
LOObj_get_state (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = lo_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  lo_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
LOObj_set_state (LOObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != lo_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (lo_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
LO_getprop_norm_freq (LOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (lo_get_norm_freq (self->handle));
}
static int
LO_setprop_norm_freq (LOObject *self, PyObject *value,
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
  lo_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
LO_getprop_phase (LOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong ((unsigned long)lo_get_phase (self->handle));
}
static int
LO_setprop_phase (LOObject *self, PyObject *value, void *Py_UNUSED (closure))
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
  lo_set_phase (self->handle, v);
  return 0;
}
static PyObject *
LO_getprop_phase_inc (LOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong (
      (unsigned long)lo_get_phase_inc (self->handle));
}

static PyGetSetDef LO_getset[]
    = { { "norm_freq", (getter)LO_getprop_norm_freq,
          (setter)LO_setprop_norm_freq,
          "Normalised frequency (read/write). Setting norm_freq recomputes "
          "phase_inc = floor(frac(v) × 2^32) and takes effect on the next "
          "lo_steps call; phase is NOT reset.\n",
          NULL },
        { "phase", (getter)LO_getprop_phase, (setter)LO_setprop_phase,
          "Current phase accumulator value (read/write). Returns the current "
          "integer phase in `[0, 2^32)`.  Writing overrides the accumulator "
          "directly for phase-coherent frequency switching.\n",
          NULL },
        { "phase_inc", (getter)LO_getprop_phase_inc, NULL,
          "Per-sample phase increment (read-only). Derived from norm_freq as "
          "floor(frac(norm_freq) × 2^32).  A freq of 0.25 gives phase_inc = "
          "1073741824 (0x40000000).\n",
          NULL },
        { NULL } };

static PyObject *
LOObj_destroy (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      lo_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
LOObj_enter (LOObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
LOObj_exit (LOObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      lo_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef LOObj_methods[] = {
  { "reset", (PyCFunction)LOObj_reset, METH_NOARGS,
    "Zero the phase accumulator. Sets phase to 0 so the next lo_steps call "
    "starts at angle 0 (1+0j). norm_freq and phase_inc are unchanged." },

  { "steps", (PyCFunction)(void *)LOObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(n=1) -> ndarray\n"
    "\n"
    "Generate n CF32 phasors at the current norm_freq. Each sample is cos(θ) "
    "+ j·sin(θ) where θ is the phase BEFORE the accumulator is advanced, "
    "giving a unit-magnitude complex sinusoid via the 65536-entry LUT.  SFDR "
    "≈ 96 dBc.  Returns n.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import LO\n"
    "    >>> obj = LO(0.0)\n"
    "    >>> y = obj.steps(4)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)LOObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "steps_ctrl", (PyCFunction)(void *)LOObj_steps_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "steps_ctrl(ctrl) -> ndarray\n"
    "\n"
    "Generate CF32 phasors with per-sample FM deviation. For each sample i, "
    "`ctrl[i]`'s fractional part is converted to a delta phase-increment "
    "(delta = floor(frac(`ctrl[i]`) × 2^32)) that is added on top of the base "
    "phase_inc for that one step only.  The base norm_freq and phase_inc are "
    "NOT modified; the deviation is transient per sample, making this the "
    "natural API for FM synthesis and frequency-hopping.  Output length "
    "equals ctrl_len.  Returns ctrl_len.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ctrl : NDArray[np.float32]\n"
    "    Float32 array of per-sample normalised-frequency deviations. Only\n"
    "    the fractional part of each element contributes.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    min(ctrl_len, max_out) samples.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.source import LO\n"
    ">>> lo = LO(0.25)\n"
    ">>> ctrl = np.zeros(4, dtype=np.float32)\n"
    ">>> out = lo.steps_ctrl(ctrl)\n"
    ">>> out.dtype\n"
    "dtype('complex64')\n"
    ">>> out.shape\n"
    "(4,)\n"
    ">>> [round(float(abs(c)), 4) for c in out]\n"
    "[1.0, 1.0, 1.0, 1.0]\n" },
  { "steps_ctrl_max_out", (PyCFunction)LOObj_steps_ctrl_max_out, METH_NOARGS,
    "steps_ctrl_max_out() -> int\n\nMax output length steps_ctrl() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)LOObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the LOObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)LOObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the LOObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)LOObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the LOObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)LOObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)LOObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Lo be used in a `with` statement so its C resources are released\n"
    "deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Lo\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)LOObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Lo.\n"
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

static PyTypeObject LOObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "source.LO",
  .tp_basicsize                           = sizeof (LOObject),
  .tp_dealloc                             = (destructor)LOObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create an LO instance. Allocates state, sets phase to 0, and derives "
    "phase_inc from norm_freq.  Initialises the shared 65536-entry float LUT "
    "on the first call (single-threaded concern: call lo_create() before "
    "spawning threads that share LO instances).\n",
  .tp_methods = LOObj_methods,
  .tp_getset  = LO_getset,
  .tp_new     = LOObj_new,
  .tp_init    = (initproc)LOObj_init,
};
