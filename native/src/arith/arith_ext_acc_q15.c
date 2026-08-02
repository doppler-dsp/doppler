/*
 * arith_ext_acc_q15.c — AccQ15 type for the arith module.
 *
 * Included by arith_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only arith_ext.c is compiled.
 */
/* ======================================================== */
/* AccQ15Object — wraps acc_q15_state_t *       */
/* ======================================================== */

#include "acc_q15/acc_q15_core.h"

typedef struct
{
  PyObject_HEAD acc_q15_state_t *handle;
} AccQ15Object;

static void
AccQ15_dealloc (AccQ15Object *self)
{
  if (self->handle)
    acc_q15_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AccQ15_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AccQ15Object *self = (AccQ15Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AccQ15_init (AccQ15Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "acc", NULL };
  long long    acc_raw  = 0LL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|L", kwlist, &acc_raw))
    return -1;
  int64_t acc  = (int64_t)acc_raw;
  self->handle = acc_q15_create (acc);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acc_q15_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AccQ15_reset (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  acc_q15_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AccQ15_step (AccQ15Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int x_raw = 0;
  if (!PyArg_ParseTuple (args, "i", &x_raw))
    return NULL;
  int16_t x = (int16_t)x_raw;
  acc_q15_step (self->handle, x);
  Py_RETURN_NONE;
}

static PyObject *
AccQ15_steps (AccQ15Object *self, PyObject *args)
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
      in_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  acc_q15_steps (self->handle, (const int16_t *)PyArray_DATA (in_arr),
                 (size_t)PyArray_SIZE (in_arr));
  Py_DECREF (in_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccQ15_get_acc (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLongLong ((long long)acc_q15_get_acc (self->handle));
}

static PyObject *
AccQ15_set_acc (AccQ15Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long long v_raw = 0LL;
  if (!PyArg_ParseTuple (args, "L", &v_raw))
    return NULL;
  int64_t v = (int64_t)v_raw;
  acc_q15_set_acc (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
AccQ15_get (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int64_t y = acc_q15_get (self->handle);
  return PyLong_FromLongLong ((long long)y);
}

static PyObject *
AccQ15_dump (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int64_t y = acc_q15_dump (self->handle);
  return PyLong_FromLongLong ((long long)y);
}

static PyObject *
AccQ15_madd (AccQ15Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *a_obj = NULL;
  PyObject *b_obj = NULL;
  if (!PyArg_ParseTuple (args, "OO", &a_obj, &b_obj))
    return NULL;
  PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF (
      a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
  if (!a_arr)
    {
      return NULL;
    }
  const int16_t *a     = (const int16_t *)PyArray_DATA (a_arr);
  size_t         a_len = (size_t)PyArray_SIZE (a_arr);
  PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF (
      b_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
  if (!b_arr)
    {
      Py_DECREF (a_arr);
      return NULL;
    }
  const int16_t *b     = (const int16_t *)PyArray_DATA (b_arr);
  size_t         b_len = (size_t)PyArray_SIZE (b_arr);
  acc_q15_madd (self->handle, a, a_len, b, b_len);
  Py_DECREF (a_arr);
  Py_DECREF (b_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccQ15_destroy (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acc_q15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccQ15_enter (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AccQ15_exit (AccQ15Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acc_q15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccQ15_state_bytes (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acc_q15_state_bytes (self->handle));
}

static PyObject *
AccQ15_get_state (AccQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = acc_q15_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  acc_q15_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AccQ15_set_state (AccQ15Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != acc_q15_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (acc_q15_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AccQ15_methods[] = {
  { "reset", (PyCFunction)AccQ15_reset, METH_NOARGS,
    "Reset the accumulator to zero, mirroring the post-create state. Does not "
    "re-initialise to the constructor's acc value — always resets to zero, "
    "matching the default initial state for a clean sweep." },
  { "step", (PyCFunction)AccQ15_step, METH_VARARGS,
    "step(x) -> None\n"
    "\n"
    "Accumulate one Q15 sample into the running total. The sample is "
    "sign-extended to 64 bits before addition, ensuring that negative samples "
    "subtract correctly from the accumulator without wrap.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Q15 input sample (int16_t, range `[-32768, 32767]`).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ15\n"
    ">>> obj = AccQ15(0)\n"
    ">>> obj.step(100)\n"
    ">>> obj.step(200)\n"
    ">>> obj.get()\n"
    "300\n"
    "\n" },
  { "steps", (PyCFunction)AccQ15_steps, METH_VARARGS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Accumulate a contiguous block of Q15 samples. Equivalent to calling "
    "step() n times but faster for large arrays because the loop can be "
    "auto-vectorised by the compiler.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.int16]\n"
    "    Input sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ15\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ15(0)\n"
    ">>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))\n"
    ">>> obj.get()\n"
    "15\n"
    "\n" },

  { "get_acc", (PyCFunction)AccQ15_get_acc, METH_NOARGS,
    "Read the current accumulator value without modifying it. Use this when "
    "you need to snapshot the running total mid-stream and continue "
    "accumulating afterward.\n" },
  { "set_acc", (PyCFunction)AccQ15_set_acc, METH_VARARGS,
    "Overwrite the accumulator with a new value. Useful for setting a bias "
    "before a new accumulation window, or for restoring a previously "
    "checkpointed value.\n" },
  { "get", (PyCFunction)AccQ15_get, METH_NOARGS,
    "get() -> int\n"
    "\n"
    "Return the current accumulated value without resetting.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Current accumulator value (int64_t).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ15\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ15(0)\n"
    ">>> obj.steps(np.array([10, 20, 30], dtype=np.int16))\n"
    ">>> obj.get()\n"
    "60\n" },
  { "dump", (PyCFunction)AccQ15_dump, METH_NOARGS,
    "dump() -> int\n"
    "\n"
    "Return the accumulated value and reset to zero.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Accumulator value before the reset (int64_t).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ15\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ15(0)\n"
    ">>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))\n"
    ">>> obj.dump()\n"
    "15\n"
    ">>> obj.get()\n"
    "0\n" },
  { "madd", (PyCFunction)AccQ15_madd, METH_VARARGS,
    "madd(a, b) -> None\n"
    "\n"
    "Multiply-accumulate: acc += sum(a[i] * b[i]) for i in [0, len(a)). Uses "
    "AVX2 when available.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "a : NDArray[np.int16]\n"
    "    First input array (int16_t).\n"
    "b : NDArray[np.int16]\n"
    "    Second input array (int16_t), same length as a.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ15\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ15(0)\n"
    ">>> a = np.array([100, 200, 300], dtype=np.int16)\n"
    ">>> b = np.array([10, 20, 30], dtype=np.int16)\n"
    ">>> obj.madd(a, b)\n"
    ">>> obj.get()\n"
    "14000\n" },
  { "destroy", (PyCFunction)AccQ15_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AccQ15_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AccQ15 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AccQ15\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AccQ15_exit, METH_VARARGS,
    "Exit a context manager, releasing the AccQ15.\n"
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
  { "state_bytes", (PyCFunction)AccQ15_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AccQ15 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AccQ15_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AccQ15 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AccQ15_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AccQ15 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { NULL }
};

static PyTypeObject AccQ15Type = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "arith.AccQ15",
  .tp_basicsize                           = sizeof (AccQ15Object),
  .tp_dealloc                             = (destructor)AccQ15_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate and initialise an AccQ15 accumulator. The accumulator starts at "
    "the supplied initial value and may be driven sample-by-sample (step), in "
    "bulk (steps), or via multiply-accumulate (madd). The internal register "
    "is a 64-bit signed integer so it will not overflow in any realistic DSP "
    "workload.\n",
  .tp_methods = AccQ15_methods,
  .tp_new     = AccQ15_new,
  .tp_init    = (initproc)AccQ15_init,
};
