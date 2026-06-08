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

static PyMethodDef AccQ8_methods[] = {
  { "reset", (PyCFunction)AccQ8_reset, METH_NOARGS,
    "Reset state to post-create defaults." },
  { "step", (PyCFunction)AccQ8_step, METH_VARARGS,
    "step(x) -> None\n"
    "\n"
    "Consume one input sample (sink; no output).\n"
    "\n"
    "    >>> from doppler import AccQ8\n"
    "    >>> obj = AccQ8(0)\n"
    "    >>> obj.step(1)\n" },
  { "steps", (PyCFunction)AccQ8_steps, METH_VARARGS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Accumulate a contiguous block of Q8 samples. Equivalent to calling "
    "step() n times; the single loop is more amenable to auto-vectorisation "
    "than repeated method calls.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccQ8\n"
    "    >>> obj = AccQ8(0)\n"
    "    >>> y = obj.steps(np.zeros(4, dtype=np.int8))\n" },

  { "get_acc", (PyCFunction)AccQ8_get_acc, METH_NOARGS, "Get acc." },
  { "set_acc", (PyCFunction)AccQ8_set_acc, METH_VARARGS, "Set acc." },
  { "get", (PyCFunction)AccQ8_get, METH_NOARGS,
    "get() -> int\n"
    "\n"
    "Return the current accumulated value without resetting.\n"
    "\n"
    "    >>> from doppler import AccQ8\n"
    "    >>> obj = AccQ8(0)\n"
    "    >>> obj.get()\n"
    "    0\n" },
  { "dump", (PyCFunction)AccQ8_dump, METH_NOARGS,
    "dump() -> int\n"
    "\n"
    "Return the accumulated value and reset to zero.\n"
    "\n"
    "    >>> from doppler import AccQ8\n"
    "    >>> obj = AccQ8(0)\n"
    "    >>> obj.dump()\n"
    "    0\n" },
  { "madd", (PyCFunction)AccQ8_madd, METH_VARARGS,
    "madd(a, b) -> None\n"
    "\n"
    "Multiply-accumulate: acc += sum(a[i] * b[i]) for i in [0, len(a)).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import AccQ8\n"
    "    >>> obj = AccQ8(0)\n"
    "    >>> obj.madd(np.zeros(4, dtype=np.int8), np.zeros(4, "
    "dtype=np.int8))\n" },
  { "destroy", (PyCFunction)AccQ8_destroy, METH_NOARGS, "Release resources." },
  { "__enter__", (PyCFunction)AccQ8_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)AccQ8_exit, METH_VARARGS, NULL },
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
