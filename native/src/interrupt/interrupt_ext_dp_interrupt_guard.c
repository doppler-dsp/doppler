/*
 * interrupt_ext_dp_interrupt_guard.c — Interrupt type for the interrupt
 * module.
 *
 * Included by interrupt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only interrupt_ext.c is compiled.
 */
/* ======================================================== */
/* InterruptObject — wraps dp_interrupt_guard_state_t *       */
/* ======================================================== */

#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

typedef struct
{
  PyObject_HEAD dp_interrupt_guard_state_t *handle;
} InterruptObject;

static void
InterruptObj_dealloc (InterruptObject *self)
{
  if (self->handle)
    dp_interrupt_guard_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
InterruptObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  InterruptObject *self = (InterruptObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
InterruptObj_init (InterruptObject *self, PyObject *args, PyObject *kwds)
{
  static char  *kwlist[]       = { "signals", "latency_ms", NULL };
  PyObject     *signals_obj    = NULL;
  unsigned long latency_ms_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|k", kwlist, &signals_obj,
                                    &latency_ms_raw))
    return -1;
  uint32_t       latency_ms  = (uint32_t)latency_ms_raw;
  PyArrayObject *signals_arr = (PyArrayObject *)PyArray_FROM_OTF (
      signals_obj, NPY_INT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!signals_arr)
    {
      return -1;
    }
  size_t signals_len = (size_t)PyArray_SIZE (signals_arr);
  self->handle       = dp_interrupt_guard_create (
      (const int32_t *)PyArray_DATA (signals_arr), signals_len, latency_ms);
  Py_DECREF (signals_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_OSError,
                       "cannot install a handler for one of the signals "
                       "requested");
      return -1;
    }
  return 0;
}

static PyObject *
InterruptObj_interrupt (InterruptObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_interrupt_guard_interrupt (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
InterruptObj_interrupted (InterruptObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int y = dp_interrupt_guard_interrupted (self->handle);
  return PyLong_FromLong ((long)y);
}

static PyObject *
InterruptObj_resume (InterruptObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_interrupt_guard_resume (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
InterruptObj_latency_ms (InterruptObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  uint32_t y = dp_interrupt_guard_latency_ms (self->handle);
  return PyLong_FromUnsignedLong ((unsigned long)y);
}

static PyObject *
InterruptObj_destroy (InterruptObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_interrupt_guard_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
InterruptObj_enter (InterruptObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
InterruptObj_exit (InterruptObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_interrupt_guard_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef InterruptObj_methods[] = {

  { "interrupt", (PyCFunction)InterruptObj_interrupt, METH_NOARGS,
    "interrupt() -> None\n"
    "\n"
    "Ask every blocking wait in this process to stop.\n"
    "\n"
    "The object's face onto dp_interrupt(). It takes a guard because that is\n"
    "how a method is called, not because the request is scoped to one -- the\n"
    "flag is process-wide, and a request through any guard is seen by every\n"
    "waiter.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.interrupt import Interrupt\n"
    ">>> it = Interrupt([])\n"
    ">>> it.interrupt()\n"
    ">>> it.interrupted()\n"
    "1\n" },
  { "interrupted", (PyCFunction)InterruptObj_interrupted, METH_NOARGS,
    "interrupted() -> int\n"
    "\n"
    "Non-zero once a stop has been requested.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Non-zero if interrupted.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.interrupt import Interrupt\n"
    ">>> import numpy as np\n"
    ">>> it = Interrupt(np.array([], dtype=np.int32))\n"
    ">>> it.interrupted()\n"
    "0\n"
    ">>> it.interrupt()\n"
    ">>> it.interrupted()\n"
    "1\n" },
  { "resume", (PyCFunction)InterruptObj_resume, METH_NOARGS,
    "resume() -> None\n"
    "\n"
    "Clear the flag so waits proceed again.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.interrupt import Interrupt\n"
    ">>> it = Interrupt([])\n"
    ">>> it.interrupt()\n"
    ">>> it.resume()\n"
    ">>> it.interrupted()\n"
    "0\n" },
  { "latency_ms", (PyCFunction)InterruptObj_latency_ms, METH_NOARGS,
    "latency_ms() -> int\n"
    "\n"
    "The wait slice every blocking wait in this process uses.\n"
    "\n"
    "The readback for the constructor's `latency_ms`, and it reads the\n"
    "PROCESS setting rather than what this guard asked for -- those differ\n"
    "when the guard passed 0, which means \"leave it alone\". A value a "
    "caller\n"
    "can set and not read back is a value they cannot reason about.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Milliseconds.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.interrupt import Interrupt\n"
    ">>> it = Interrupt(np.array([], dtype=np.int32), latency_ms=25)\n"
    ">>> it.latency_ms()\n"
    "25\n" },
  { "destroy", (PyCFunction)InterruptObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)InterruptObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Interrupt be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Interrupt\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)InterruptObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Interrupt.\n"
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

static PyTypeObject InterruptObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "interrupt.Interrupt",
  .tp_basicsize                           = sizeof (InterruptObject),
  .tp_dealloc                             = (destructor)InterruptObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Clear the flag, optionally install handlers, and remember what to undo.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "signals : NDArray[np.int32]\n"
    "    Signals to install on; empty arms nothing and the guard is only a\n"
    "    handle to the flag.\n"
    "latency_ms : int, default 0\n"
    "    Wait-slice override; 0 leaves the process setting alone, and only a\n"
    "    non-zero value is restored.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If construction fails. The exception message is ``cannot install a\n"
    "    handler for one of the signals requested``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.interrupt import Interrupt\n"
    ">>> it = Interrupt([])\n"
    ">>> it.interrupted()\n"
    "0\n",
  .tp_methods = InterruptObj_methods,
  .tp_new     = InterruptObj_new,
  .tp_init    = (initproc)InterruptObj_init,
};
