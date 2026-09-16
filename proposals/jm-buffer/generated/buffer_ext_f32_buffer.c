/*
 * buffer_ext_f32_buffer.c — F32Buffer type for the buffer module.
 *
 * Included by buffer_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only buffer_ext.c is compiled.
 */
/* ======================================================== */
/* F32BufferObject — wraps f32_buffer_state_t *       */
/* ======================================================== */

#include "f32_buffer/f32_buffer_core.h"

typedef struct
{
  PyObject_HEAD f32_buffer_state_t *handle;
} F32BufferObject;

static void
F32Buffer_dealloc (F32BufferObject *self)
{
  if (self->handle)
    f32_buffer_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
F32Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32BufferObject *self = (F32BufferObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
F32Buffer_init (F32BufferObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]      = { "n_samples", NULL };
  unsigned long long n_samples_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|K", kwlist, &n_samples_raw))
    return -1;
  size_t n_samples = (size_t)n_samples_raw;
  self->handle     = dp_f32_create (n_samples);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_f32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
F32Buffer_reset (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  f32_buffer_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_get_gain (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (f32_buffer_get_gain (self->handle));
}

static PyObject *
F32Buffer_set_gain (F32BufferObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double v = 0.0;
  if (!PyArg_ParseTuple (args, "d", &v))
    return NULL;
  f32_buffer_set_gain (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
F32Buffer_wait (F32BufferObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_raw))
    return NULL;
  size_t          n  = (size_t)n_raw;
  float _Complex *_p = dp_f32_wait (self->handle, n);
  if (!_p)
    {
      PyErr_SetString (PyExc_ValueError, "wait failed");
      return NULL;
    }
  npy_intp  _dim = (npy_intp)(n);
  PyObject *_view
      = PyArray_SimpleNewFromData (1, &_dim, NPY_COMPLEX64, (void *)(_p));
  if (!_view)
    return NULL;
  PyArray_CLEARFLAGS ((PyArrayObject *)_view, NPY_ARRAY_WRITEABLE);
  Py_INCREF (self);
  if (PyArray_SetBaseObject ((PyArrayObject *)_view, (PyObject *)self) < 0)
    {
      Py_DECREF (self);
      Py_DECREF (_view);
      return NULL;
    }
  return _view;
}

static PyObject *
F32Buffer_consume (F32BufferObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_raw))
    return NULL;
  size_t n = (size_t)n_raw;
  dp_f32_consume (self->handle, n);
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_available (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_f32_available (self->handle);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
F32Buffer_closed (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int y = dp_f32_closed (self->handle);
  return PyLong_FromLong ((long)y);
}

static PyObject *
F32Buffer_close (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_f32_close (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_destroy (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      f32_buffer_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_enter (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
F32Buffer_exit (F32BufferObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      f32_buffer_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef F32Buffer_methods[] = {
  { "reset", (PyCFunction)F32Buffer_reset, METH_NOARGS,
    "Reset state to post-create defaults.\n" },

  { "get_gain", (PyCFunction)F32Buffer_get_gain, METH_NOARGS, "Get gain.\n" },
  { "set_gain", (PyCFunction)F32Buffer_set_gain, METH_VARARGS, "Set gain.\n" },
  { "wait", (PyCFunction)(void *)F32Buffer_wait, METH_VARARGS | METH_KEYWORDS,
    "wait(n) -> ndarray\n"
    "\n"
    "wait.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from dpring.buffer import F32Buffer\n"
    "    >>> obj = F32Buffer(n_samples=0)\n"
    "    >>> y = obj.wait(0)\n"
    "    >>> y.ndim\n"
    "    1\n" },
  { "consume", (PyCFunction)(void *)F32Buffer_consume,
    METH_VARARGS | METH_KEYWORDS,
    "consume(n) -> None\n"
    "\n"
    "consume.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Input.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from dpring.buffer import F32Buffer\n"
    "    >>> obj = F32Buffer(n_samples=0)\n"
    "    >>> obj.consume(0)\n" },
  { "available", (PyCFunction)F32Buffer_available, METH_NOARGS,
    "available() -> int\n"
    "\n"
    "available.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from dpring.buffer import F32Buffer\n"
    "    >>> obj = F32Buffer(n_samples=0)\n"
    "    >>> obj.available()\n"
    "    0\n" },
  { "closed", (PyCFunction)F32Buffer_closed, METH_NOARGS,
    "closed() -> int\n"
    "\n"
    "closed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from dpring.buffer import F32Buffer\n"
    "    >>> obj = F32Buffer(n_samples=0)\n"
    "    >>> obj.closed()\n"
    "    0\n" },
  { "close", (PyCFunction)F32Buffer_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "close.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from dpring.buffer import F32Buffer\n"
    "    >>> obj = F32Buffer(n_samples=0)\n"
    "    >>> obj.close()\n" },
  { "destroy", (PyCFunction)F32Buffer_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)F32Buffer_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a F32Buffer be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "F32Buffer\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)F32Buffer_exit, METH_VARARGS,
    "Exit a context manager, releasing the F32Buffer.\n"
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

static PyTypeObject F32BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "buffer.F32Buffer",
  .tp_basicsize                           = sizeof (F32BufferObject),
  .tp_dealloc                             = (destructor)F32Buffer_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "F32Buffer type.\n",
  .tp_methods                             = F32Buffer_methods,
  .tp_new                                 = F32Buffer_new,
  .tp_init                                = (initproc)F32Buffer_init,
};
