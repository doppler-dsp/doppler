/*
 * accumulator_ext_acc_cf64.c — AccCf64 type for the accumulator module.
 *
 * Included by accumulator_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only accumulator_ext.c is compiled.
 */
/* ======================================================== */
/* AccCf64Object — wraps acc_cf64_state_t *       */
/* ======================================================== */

#include "doppler/acc_cf64/acc_cf64_core.h"

typedef struct
{
  PyObject_HEAD acc_cf64_state_t *handle;
} AccCf64Object;

static void
AccCf64_dealloc (AccCf64Object *self)
{
  if (self->handle)
    acc_cf64_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AccCf64_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AccCf64Object *self = (AccCf64Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AccCf64_init (AccCf64Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "acc", NULL };
  Py_complex   acc_raw  = { 0.0, 0.0 };

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|D", kwlist, &acc_raw))
    return -1;
  double _Complex acc = acc_raw.real + acc_raw.imag * I;
  self->handle        = acc_cf64_create (acc);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acc_cf64_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AccCf64_reset (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  acc_cf64_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_step (AccCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  double _Complex x = x_raw.real + x_raw.imag * I;
  acc_cf64_step (self->handle, x);
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_steps (AccCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;

  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  acc_cf64_steps (self->handle, (const double _Complex *)PyArray_DATA (in_arr),
                  (size_t)PyArray_SIZE (in_arr));
  Py_DECREF (in_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_get_acc (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyComplex_FromDoubles (creal (acc_cf64_get_acc (self->handle)),
                                cimag (acc_cf64_get_acc (self->handle)));
}

static PyObject *
AccCf64_set_acc (AccCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex v_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &v_raw))
    return NULL;
  double _Complex v = v_raw.real + v_raw.imag * I;
  acc_cf64_set_acc (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
AccCf64_get (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _Complex y = acc_cf64_get (self->handle);
  return PyComplex_FromDoubles (creal (y), cimag (y));
}

static PyObject *
AccCf64_dump (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _Complex y = acc_cf64_dump (self->handle);
  return PyComplex_FromDoubles (creal (y), cimag (y));
}

static PyObject *
AccCf64_madd (AccCf64Object *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "h", NULL };
  PyObject    *x_obj     = NULL;
  PyObject    *h_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &x_obj, &h_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const double _Complex *x     = (const double _Complex *)PyArray_DATA (x_arr);
  size_t                 x_len = (size_t)PyArray_SIZE (x_arr);
  PyArrayObject         *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  const float *h     = (const float *)PyArray_DATA (h_arr);
  size_t       h_len = (size_t)PyArray_SIZE (h_arr);
  acc_cf64_madd (self->handle, x, x_len, h, h_len);
  Py_DECREF (x_arr);
  Py_DECREF (h_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_add2d (AccCf64Object *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", NULL };
  PyObject    *x_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &x_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const double _Complex *x     = (const double _Complex *)PyArray_DATA (x_arr);
  size_t                 x_len = (size_t)PyArray_SIZE (x_arr);
  acc_cf64_add2d (self->handle, x, x_len);
  Py_DECREF (x_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_madd2d (AccCf64Object *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "h", NULL };
  PyObject    *x_obj     = NULL;
  PyObject    *h_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &x_obj, &h_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const double _Complex *x     = (const double _Complex *)PyArray_DATA (x_arr);
  size_t                 x_len = (size_t)PyArray_SIZE (x_arr);
  PyArrayObject         *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  const float *h     = (const float *)PyArray_DATA (h_arr);
  size_t       h_len = (size_t)PyArray_SIZE (h_arr);
  acc_cf64_madd2d (self->handle, x, x_len, h, h_len);
  Py_DECREF (x_arr);
  Py_DECREF (h_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_state_bytes (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acc_cf64_state_bytes (self->handle));
}

static PyObject *
AccCf64_get_state (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = acc_cf64_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  acc_cf64_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AccCf64_set_state (AccCf64Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != acc_cf64_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (acc_cf64_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_destroy (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acc_cf64_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccCf64_enter (AccCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AccCf64_exit (AccCf64Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acc_cf64_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AccCf64_methods[] = {
  { "reset", (PyCFunction)AccCf64_reset, METH_NOARGS,
    "Zero the accumulator, restoring the same state as a fresh\n"
    "``AccCf64(0j)`` — regardless of the value supplied to\n"
    "``acc_cf64_create``. Both the real and imaginary parts are set to 0.0.\n"
    "Subsequent ``get`` / ``dump`` calls return ``0j`` until new samples are\n"
    "processed.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> obj.step(3+2j)\n"
    ">>> obj.reset()\n"
    ">>> obj.get_acc()\n"
    "0j\n" },
  { "step", (PyCFunction)AccCf64_step, METH_VARARGS,
    "step(x) -> None\n"
    "\n"
    "Add one complex sample to the running sum (``acc += x``). This is\n"
    "the hot-path entry for sample-by-sample processing. For block inputs\n"
    "prefer ``acc_cf64_steps`` to amortise call overhead.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input sample (complex).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> obj.step(3+2j)\n"
    ">>> obj.get()\n"
    "(3+2j)\n"
    "\n" },
  { "steps", (PyCFunction)AccCf64_steps, METH_VARARGS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Add all samples in ``input`` to the running sum. Equivalent to\n"
    "calling ``acc_cf64_step`` for each element; iterates element-by-element\n"
    "over double-precision complex samples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex128]\n"
    "    Input sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> obj.steps(np.array([1+0j, 2+1j, 3+2j], dtype=np.complex128))\n"
    ">>> obj.get()\n"
    "(6+3j)\n"
    "\n" },

  { "get_acc", (PyCFunction)AccCf64_get_acc, METH_NOARGS,
    "Return the current accumulator value without modifying state. Use this "
    "when you need to read the running sum mid-accumulation without "
    "disturbing it. For a read-and-reset in one call use "
    "``acc_cf64_dump``.\n" },
  { "set_acc", (PyCFunction)AccCf64_set_acc, METH_VARARGS,
    "Overwrite the accumulator with a new complex value. Useful for seeding "
    "the accumulator to a known baseline before processing a new segment "
    "without a full ``reset``; subsequent ``step`` / ``steps`` samples "
    "accumulate on top of the seeded value.\n" },
  { "get", (PyCFunction)AccCf64_get, METH_NOARGS,
    "get() -> complex\n"
    "\n"
    "Return the current accumulated sum without resetting state.\n"
    "Identical to reading the ``acc`` property directly; retained as an\n"
    "explicit method so call sites that need the value can be uniform with\n"
    "``dump`` without a conditional.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    Current value of ``acc`` (complex).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> obj.step(2+0j)\n"
    ">>> obj.step(0+3j)\n"
    ">>> obj.get()\n"
    "(2+3j)\n" },
  { "dump", (PyCFunction)AccCf64_dump, METH_NOARGS,
    "dump() -> complex\n"
    "\n"
    "Return the accumulated sum and atomically reset it to zero. This is\n"
    "the canonical \"drain\" primitive: read the period total, then start a\n"
    "fresh accumulation interval without a separate ``reset`` call. Both\n"
    "real and imaginary parts are zeroed unconditionally.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    Value of ``acc`` just before the reset (complex).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> obj.step(3+2j)\n"
    ">>> obj.step(1+1j)\n"
    ">>> obj.dump()\n"
    "(4+3j)\n"
    ">>> obj.get()\n"
    "0j\n" },
  { "madd", (PyCFunction)(void *)AccCf64_madd, METH_VARARGS | METH_KEYWORDS,
    "madd(x, h) -> None\n"
    "\n"
    "Dot-product accumulate with complex signal and float weights: ``acc\n"
    "+= sum(x[i] * h[i])`` for ``i`` in ``0 .. min(x_len, h_len) - 1``. The\n"
    "signal array ``x`` is double-precision complex; the coefficient array\n"
    "``h`` is single-precision float (widened to double before\n"
    "multiplication). The shorter of the two arrays limits iteration.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex128]\n"
    "    Complex signal samples (complex128 array).\n"
    "h : NDArray[np.float32]\n"
    "    Real coefficient / weight array (float32 array).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)\n"
    ">>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)\n"
    ">>> obj.madd(x, h)\n"
    ">>> obj.get()\n"
    "(5+0j)\n" },
  { "add2d", (PyCFunction)(void *)AccCf64_add2d, METH_VARARGS | METH_KEYWORDS,
    "add2d(x) -> None\n"
    "\n"
    "Sum all elements of a (logically) 2-D complex array into the\n"
    "accumulator. The array is treated as a flat C-order buffer of ``x_len``\n"
    "complex128 samples regardless of the original shape; the caller is\n"
    "responsible for passing the total element count.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex128]\n"
    "    Input array (complex128, any shape — passed as flat buffer).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> grid = np.array([[1+0j, 2+0j], [3+0j, 4+0j]], dtype=np.complex128)\n"
    ">>> obj.add2d(grid)\n"
    ">>> obj.get()\n"
    "(10+0j)\n" },
  { "madd2d", (PyCFunction)(void *)AccCf64_madd2d,
    METH_VARARGS | METH_KEYWORDS,
    "madd2d(x, h) -> None\n"
    "\n"
    "Dot-product accumulate over a flat 2-D complex buffer: ``acc +=\n"
    "sum(x[i] * h[i])`` for ``i`` in ``0 .. min(x_len, h_len) - 1``.\n"
    "Combines ``add2d`` and ``madd`` semantics for 2-D data — a complex\n"
    "signal grid is weighted element-wise by a real coefficient buffer and\n"
    "folded into the running sum.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex128]\n"
    "    Complex signal samples (complex128, flat buffer).\n"
    "h : NDArray[np.float32]\n"
    "    Real coefficient / weight array (float32).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)\n"
    ">>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)\n"
    ">>> obj.madd2d(x, h)\n"
    ">>> obj.get()\n"
    "(5+0j)\n" },
  { "state_bytes", (PyCFunction)AccCf64_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AccCf64 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AccCf64_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AccCf64 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AccCf64_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AccCf64 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AccCf64_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)AccCf64_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AccCf64 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AccCf64\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AccCf64_exit, METH_VARARGS,
    "Exit a context manager, releasing the AccCf64.\n"
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

static PyTypeObject AccCf64Type = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "accumulator.AccCf64",
  .tp_basicsize                           = sizeof (AccCf64Object),
  .tp_dealloc                             = (destructor)AccCf64_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Double-precision complex scalar accumulator. Maintains one running\n"
    "complex sum (``acc``) across calls to ``step``, ``steps``, ``madd``,\n"
    "``add2d``, and ``madd2d``. The signal path is double-precision complex\n"
    "(128-bit per sample); coefficient arrays for ``madd``/``madd2d`` are\n"
    "single-precision float to match typical FIR weight storage. Use ``get`` "
    "to\n"
    "read without side-effects or ``dump`` to read and zero atomically.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "acc : complex, default 0j\n"
    "    acc state variable.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccCf64\n"
    ">>> obj = AccCf64(0j)\n"
    ">>> obj.get_acc()\n"
    "0j\n"
    ">>> obj.set_acc(3+4j)\n"
    ">>> obj.get_acc()\n"
    "(3+4j)\n"
    ">>> obj.reset()\n"
    ">>> obj.get_acc()\n"
    "0j\n",
  .tp_methods = AccCf64_methods,
  .tp_new     = AccCf64_new,
  .tp_init    = (initproc)AccCf64_init,
};
