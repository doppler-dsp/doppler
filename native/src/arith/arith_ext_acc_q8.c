/*
 * arith_ext_acc_q8.c — AccQ8 type for the arith module.
 *
 * Included by arith_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only arith_ext.c is compiled.
 */
/* ======================================================== */
/* AccQ8Object — wraps acc_q8_state_t *       */
/* ======================================================== */

#include "acc_q8/acc_q8_core.h"

typedef struct
{
  PyObject_HEAD acc_q8_state_t *handle;
} AccQ8Object;

static void
AccQ8_dealloc (AccQ8Object *self)
{
  if (self->handle)
    acc_q8_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AccQ8_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AccQ8Object *self = (AccQ8Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AccQ8_init (AccQ8Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "acc", NULL };
  long         acc_raw  = 0L;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|l", kwlist, &acc_raw))
    return -1;
  int32_t acc  = (int32_t)acc_raw;
  self->handle = acc_q8_create (acc);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acc_q8_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AccQ8_reset (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  acc_q8_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AccQ8_step (AccQ8Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int x_raw = 0;
  if (!PyArg_ParseTuple (args, "i", &x_raw))
    return NULL;
  int8_t x = (int8_t)x_raw;
  acc_q8_step (self->handle, x);
  Py_RETURN_NONE;
}

static PyObject *
AccQ8_steps (AccQ8Object *self, PyObject *args)
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
      in_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  acc_q8_steps (self->handle, (const int8_t *)PyArray_DATA (in_arr),
                (size_t)PyArray_SIZE (in_arr));
  Py_DECREF (in_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccQ8_get_acc (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)acc_q8_get_acc (self->handle));
}

static PyObject *
AccQ8_set_acc (AccQ8Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long v_raw = 0L;
  if (!PyArg_ParseTuple (args, "l", &v_raw))
    return NULL;
  int32_t v = (int32_t)v_raw;
  acc_q8_set_acc (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
AccQ8_get (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int32_t y = acc_q8_get (self->handle);
  return PyLong_FromLong ((long)y);
}

static PyObject *
AccQ8_dump (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int32_t y = acc_q8_dump (self->handle);
  return PyLong_FromLong ((long)y);
}

static PyObject *
AccQ8_madd (AccQ8Object *self, PyObject *args)
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
      a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!a_arr)
    {
      return NULL;
    }
  const int8_t  *a     = (const int8_t *)PyArray_DATA (a_arr);
  size_t         a_len = (size_t)PyArray_SIZE (a_arr);
  PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF (
      b_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!b_arr)
    {
      Py_DECREF (a_arr);
      return NULL;
    }
  const int8_t *b     = (const int8_t *)PyArray_DATA (b_arr);
  size_t        b_len = (size_t)PyArray_SIZE (b_arr);
  acc_q8_madd (self->handle, a, a_len, b, b_len);
  Py_DECREF (a_arr);
  Py_DECREF (b_arr);
  Py_RETURN_NONE;
}

static PyObject *
AccQ8_destroy (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acc_q8_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccQ8_enter (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AccQ8_exit (AccQ8Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acc_q8_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AccQ8_state_bytes (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acc_q8_state_bytes (self->handle));
}

static PyObject *
AccQ8_get_state (AccQ8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = acc_q8_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  acc_q8_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AccQ8_set_state (AccQ8Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != acc_q8_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (acc_q8_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AccQ8_methods[] = {
  { "reset", (PyCFunction)AccQ8_reset, METH_NOARGS,
    "Reset the accumulator to zero, mirroring the post-create state. Always "
    "resets to zero regardless of the original constructor value, so it is "
    "safe to call at the start of any new accumulation window." },
  { "step", (PyCFunction)AccQ8_step, METH_VARARGS,
    "step(x) -> None\n"
    "\n"
    "Accumulate one Q8 sample into the running total. The sample is "
    "sign-extended to 32 bits before addition so negative samples correctly "
    "subtract from the accumulator.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Q8 input sample (int8_t, range `[-128, 127]`).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ8\n"
    ">>> obj = AccQ8(0)\n"
    ">>> obj.step(10)\n"
    ">>> obj.step(20)\n"
    ">>> obj.get()\n"
    "30\n"
    "\n" },
  { "steps", (PyCFunction)AccQ8_steps, METH_VARARGS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Accumulate a contiguous block of Q8 samples. Equivalent to calling "
    "step() n times; the single loop is more amenable to auto-vectorisation "
    "than repeated method calls.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.int8]\n"
    "    Input sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ8\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ8(0)\n"
    ">>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))\n"
    ">>> obj.get()\n"
    "15\n"
    "\n" },

  { "get_acc", (PyCFunction)AccQ8_get_acc, METH_NOARGS,
    "Read the current accumulator value without modifying it. Permits "
    "repeated snapshots of the running sum mid-stream.\n" },
  { "set_acc", (PyCFunction)AccQ8_set_acc, METH_VARARGS,
    "Overwrite the accumulator with a new value. Useful for applying a bias "
    "before a new accumulation window, or for restoring a checkpointed "
    "accumulator state.\n" },
  { "get", (PyCFunction)AccQ8_get, METH_NOARGS,
    "get() -> int\n"
    "\n"
    "Return the current accumulated value without resetting.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Current accumulator value (int32_t).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ8\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ8(0)\n"
    ">>> obj.steps(np.array([10, 20, 30], dtype=np.int8))\n"
    ">>> obj.get()\n"
    "60\n" },
  { "dump", (PyCFunction)AccQ8_dump, METH_NOARGS,
    "dump() -> int\n"
    "\n"
    "Return the accumulated value and reset to zero.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Accumulator value before the reset (int32_t).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ8\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ8(0)\n"
    ">>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))\n"
    ">>> obj.dump()\n"
    "15\n"
    ">>> obj.get()\n"
    "0\n" },
  { "madd", (PyCFunction)AccQ8_madd, METH_VARARGS,
    "madd(a, b) -> None\n"
    "\n"
    "Multiply-accumulate: acc += sum(a[i] * b[i]) for i in [0, len(a)).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "a : NDArray[np.int8]\n"
    "    First input array (int8_t).\n"
    "b : NDArray[np.int8]\n"
    "    Second input array (int8_t), same length as a.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.arith import AccQ8\n"
    ">>> import numpy as np\n"
    ">>> obj = AccQ8(0)\n"
    ">>> a = np.array([10, 20, 30], dtype=np.int8)\n"
    ">>> b = np.array([1, 2, 3], dtype=np.int8)\n"
    ">>> obj.madd(a, b)\n"
    ">>> obj.get()\n"
    "140\n" },
  { "destroy", (PyCFunction)AccQ8_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AccQ8_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a AccQ8 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "AccQ8\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AccQ8_exit, METH_VARARGS,
    "Exit a context manager, releasing the AccQ8.\n"
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
  { "state_bytes", (PyCFunction)AccQ8_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AccQ8 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AccQ8_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AccQ8 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AccQ8_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AccQ8 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { NULL }
};

static PyTypeObject AccQ8Type = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "arith.AccQ8",
  .tp_basicsize                           = sizeof (AccQ8Object),
  .tp_dealloc                             = (destructor)AccQ8_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Allocate and initialise an AccQ8 accumulator. The accumulator "
            "starts at the supplied initial value and accepts Q8 (int8_t) "
            "samples via step(), steps(), or madd(). The 32-bit internal "
            "register handles up to roughly 16 million max-magnitude samples "
            "before wrap — sufficient for all standard DSP block sizes.\n",
  .tp_methods = AccQ8_methods,
  .tp_new     = AccQ8_new,
  .tp_init    = (initproc)AccQ8_init,
};
