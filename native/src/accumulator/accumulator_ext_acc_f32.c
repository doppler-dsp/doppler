/*
 * accumulator_ext_acc_f32.c — AccF32 type for the accumulator module.
 *
 * Included by accumulator_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only accumulator_ext.c is compiled.
 */
/* ======================================================== */
/* AccF32Object — wraps acc_f32_state_t *       */
/* ======================================================== */

#include "acc_f32/acc_f32_core.h"

typedef struct
{
  PyObject_HEAD acc_f32_state_t *handle;
} AccF32Object;

static void
AccF32_dealloc (AccF32Object *self)
{
  if (self->handle)
    acc_f32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AccF32_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AccF32Object *self = (AccF32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AccF32_init (AccF32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "acc", NULL };
  float        acc      = 0.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &acc))
    return -1;
  self->handle = acc_f32_create (acc);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acc_f32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AccF32_reset (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  acc_f32_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AccF32_step (AccF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  acc_f32_step (self->handle, x);
  Py_RETURN_NONE;
}

static PyObject *
AccF32_steps (AccF32Object *self, PyObject *args)
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
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  acc_f32_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                 (size_t)PyArray_SIZE (in_arr));
  Py_DECREF (in_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccF32_get_acc (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)acc_f32_get_acc (self->handle));
}

static PyObject *
AccF32_set_acc (AccF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float v = 0.0f;
  if (!PyArg_ParseTuple (args, "f", &v))
    return NULL;
  acc_f32_set_acc (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
AccF32_get (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float y = acc_f32_get (self->handle);
  return PyFloat_FromDouble ((double)y);
}

static PyObject *
AccF32_dump (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float y = acc_f32_dump (self->handle);
  return PyFloat_FromDouble ((double)y);
}

static PyObject *
AccF32_madd (AccF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *x_obj = NULL;
  PyObject *h_obj = NULL;
  if (!PyArg_ParseTuple (args, "OO", &x_obj, &h_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float   *x     = (const float *)PyArray_DATA (x_arr);
  size_t         x_len = (size_t)PyArray_SIZE (x_arr);
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  const float *h     = (const float *)PyArray_DATA (h_arr);
  size_t       h_len = (size_t)PyArray_SIZE (h_arr);
  acc_f32_madd (self->handle, x, x_len, h, h_len);
  Py_DECREF (x_arr);
  Py_DECREF (h_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccF32_add2d (AccF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *x_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float *x     = (const float *)PyArray_DATA (x_arr);
  size_t       x_len = (size_t)PyArray_SIZE (x_arr);
  acc_f32_add2d (self->handle, x, x_len);
  Py_DECREF (x_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccF32_madd2d (AccF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *x_obj = NULL;
  PyObject *h_obj = NULL;
  if (!PyArg_ParseTuple (args, "OO", &x_obj, &h_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float   *x     = (const float *)PyArray_DATA (x_arr);
  size_t         x_len = (size_t)PyArray_SIZE (x_arr);
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  const float *h     = (const float *)PyArray_DATA (h_arr);
  size_t       h_len = (size_t)PyArray_SIZE (h_arr);
  acc_f32_madd2d (self->handle, x, x_len, h, h_len);
  Py_DECREF (x_arr);
  Py_DECREF (h_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccF32_destroy (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acc_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccF32_enter (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AccF32_exit (AccF32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acc_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccF32_state_bytes (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acc_f32_state_bytes (self->handle));
}

static PyObject *
AccF32_get_state (AccF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = acc_f32_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  acc_f32_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AccF32_set_state (AccF32Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != acc_f32_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (acc_f32_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AccF32_methods[] = {
  { "reset", (PyCFunction)AccF32_reset, METH_NOARGS,
    "Zero the accumulator, restoring the same state as a fresh "
    "``AccF32(0.0)`` — regardless of the value supplied to "
    "``acc_f32_create``. Subsequent ``get`` / ``dump`` calls return ``0.0`` "
    "until new samples are processed." },
  { "step", (PyCFunction)AccF32_step, METH_VARARGS,
    "step(x) -> None\n"
    "\n"
    "Add one sample to the running sum (``acc += x``). This is the hot-path "
    "entry point for sample-by-sample processing. For block inputs prefer "
    "``acc_f32_steps`` to amortise call overhead and allow "
    "auto-vectorisation.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input sample (float).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccF32\n"
    ">>> obj = AccF32(0.0)\n"
    ">>> obj.step(3.0)\n"
    ">>> obj.get()\n"
    "3.0\n"
    "\n" },
  { "steps", (PyCFunction)AccF32_steps, METH_VARARGS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Add all samples in ``input`` to the running sum. Equivalent to calling "
    "``acc_f32_step`` for each element, but SIMD-vectorised on platforms that "
    "provide it (AVX-512 / AVX2 / SSE2). The loop uses JM_RESTRICT so the "
    "compiler can assume no aliasing between ``state`` and ``input``.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Input sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.accumulator import AccF32\n"
    ">>> obj = AccF32(0.0)\n"
    ">>> obj.steps(np.array([1.0, 2.0, 3.0], dtype=np.float32))\n"
    ">>> obj.get()\n"
    "6.0\n"
    "\n" },

  { "get_acc", (PyCFunction)AccF32_get_acc, METH_NOARGS,
    "Return the current accumulator value without modifying state. Use this "
    "when you need to read the running sum mid-accumulation without "
    "disturbing it. For a read-and-reset in one call use "
    "``acc_f32_dump``.\n" },
  { "set_acc", (PyCFunction)AccF32_set_acc, METH_VARARGS,
    "Overwrite the accumulator with a new value. Useful for seeding the "
    "accumulator to a known baseline before processing a new segment without "
    "a full ``reset``; subsequent ``step`` / ``steps`` samples accumulate on "
    "top of the seeded value.\n" },
  { "get", (PyCFunction)AccF32_get, METH_NOARGS,
    "get() -> float\n"
    "\n"
    "Return the current accumulated sum without resetting state. Identical to "
    "reading the ``acc`` property directly; retained as an explicit method so "
    "call sites that need the value can be uniform with ``dump`` without a "
    "conditional.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Current value of ``acc`` (float).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccF32\n"
    ">>> obj = AccF32(0.0)\n"
    ">>> obj.step(2.0)\n"
    ">>> obj.step(3.0)\n"
    ">>> obj.get()\n"
    "5.0\n" },
  { "dump", (PyCFunction)AccF32_dump, METH_NOARGS,
    "dump() -> float\n"
    "\n"
    "Return the accumulated sum and atomically reset it to zero. This is the "
    "canonical \"drain\" primitive: read the period total, then start a fresh "
    "accumulation interval without a separate ``reset`` call. The zero-reset "
    "is unconditional and always writes 0.0f.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Value of ``acc`` just before the reset (float).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.accumulator import AccF32\n"
    ">>> obj = AccF32(0.0)\n"
    ">>> obj.step(3.0)\n"
    ">>> obj.step(4.0)\n"
    ">>> obj.dump()\n"
    "7.0\n"
    ">>> obj.get()\n"
    "0.0\n" },
  { "madd", (PyCFunction)AccF32_madd, METH_VARARGS,
    "madd(x, h) -> None\n"
    "\n"
    "Multiply-accumulate: acc += sum(x * h) over x_len samples.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccF32\n"
    "    >>> obj = AccF32(0.0)\n"
    "    >>> obj.madd(np.zeros(4, dtype=np.float32), np.zeros(4, "
    "dtype=np.float32))\n" },
  { "add2d", (PyCFunction)AccF32_add2d, METH_VARARGS,
    "add2d(x) -> None\n"
    "\n"
    "Accumulate a 2-D array: acc += sum of all elements in x.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccF32\n"
    "    >>> obj = AccF32(0.0)\n"
    "    >>> obj.add2d(np.zeros(4, dtype=np.float32))\n" },
  { "madd2d", (PyCFunction)AccF32_madd2d, METH_VARARGS,
    "madd2d(x, h) -> None\n"
    "\n"
    "2-D multiply-accumulate: acc += sum(x * h) over x_len elements.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccF32\n"
    "    >>> obj = AccF32(0.0)\n"
    "    >>> obj.madd2d(np.zeros(4, dtype=np.float32), np.zeros(4, "
    "dtype=np.float32))\n" },
  { "destroy", (PyCFunction)AccF32_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AccF32_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AccF32 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AccF32\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AccF32_exit, METH_VARARGS,
    "Exit a context manager, releasing the AccF32.\n"
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
  { "state_bytes", (PyCFunction)AccF32_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AccF32 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AccF32_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AccF32 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AccF32_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AccF32 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { NULL }
};

static PyTypeObject AccF32Type = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "accumulator.AccF32",
  .tp_basicsize                           = sizeof (AccF32Object),
  .tp_dealloc                             = (destructor)AccF32_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Single-precision floating-point scalar accumulator. Maintains one "
    "running sum (``acc``) that persists across calls to ``step``, ``steps``, "
    "``madd``, ``add2d``, and ``madd2d``. Use ``get`` to read without "
    "side-effects or ``dump`` to read and atomically zero in a single call.\n",
  .tp_methods = AccF32_methods,
  .tp_new     = AccF32_new,
  .tp_init    = (initproc)AccF32_init,
};
