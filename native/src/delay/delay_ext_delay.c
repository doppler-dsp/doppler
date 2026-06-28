/*
 * delay_ext_delay.c — DelayCf64 type for the delay module.
 *
 * Included by delay_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only delay_ext.c is compiled.
 */
/* ======================================================== */
/* DelayCf64Object — wraps delay_state_t *       */
/* ======================================================== */

#include "delay/delay_core.h"

typedef struct
{
  PyObject_HEAD delay_state_t *handle;
  double complex              *_ptr_buf; /* pre-allocated output for ptr */
  double complex *_push_ptr_buf; /* pre-allocated output for push_ptr */
} DelayCf64Object;

static void
DelayCf64Obj_dealloc (DelayCf64Object *self)
{
  if (self->handle)
    delay_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DelayCf64Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DelayCf64Object *self = (DelayCf64Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DelayCf64Obj_init (DelayCf64Object *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]     = { "num_taps", NULL };
  unsigned long long num_taps_raw = 0ULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|K", kwlist, &num_taps_raw))
    return -1;
  size_t num_taps = (size_t)num_taps_raw;
  self->handle    = delay_create (num_taps);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "delay_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
DelayCf64Obj_reset (DelayCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  delay_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_push (DelayCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  double complex x = x_raw.real + x_raw.imag * I;
  delay_push (self->handle, x);
  Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_ptr (DelayCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = (Py_ssize_t)self->handle->num_taps;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (!self->_ptr_buf)
    {
      size_t _max = delay_ptr_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_ptr_buf = malloc (_max * sizeof (double complex));
      if (!self->_ptr_buf)
        {
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t    n_out = delay_ptr (self->handle, (size_t)n, self->_ptr_buf);
  npy_intp  dim   = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128, self->_ptr_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  return arr;
}

static PyObject *
DelayCf64Obj_push_ptr (DelayCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  double complex x = x_raw.real + x_raw.imag * I;
  if (!self->_push_ptr_buf)
    {
      size_t _max = delay_push_ptr_max_out (self->handle);
      if (!_max)
        _max = 1;
      self->_push_ptr_buf = malloc (_max * sizeof (double complex));
      if (!self->_push_ptr_buf)
        {
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t    n_out = delay_push_ptr (self->handle, x, self->_push_ptr_buf);
  npy_intp  dim   = (npy_intp)n_out;
  PyObject *arr   = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128,
                                               self->_push_ptr_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  return arr;
}

static PyObject *
DelayCf64Obj_write (DelayCf64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  double complex x = x_raw.real + x_raw.imag * I;
  delay_write (self->handle, x);
  Py_RETURN_NONE;
}
static PyObject *
DelayCf64_getprop_num_taps (DelayCf64Object *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->num_taps);
}
static PyObject *
DelayCf64_getprop_capacity (DelayCf64Object *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->capacity);
}

static PyGetSetDef DelayCf64_getset[]
    = { { "num_taps", (getter)DelayCf64_getprop_num_taps, NULL, NULL, NULL },
        { "capacity", (getter)DelayCf64_getprop_capacity, NULL, NULL, NULL },
        { NULL } };

static PyObject *
DelayCf64Obj_destroy (DelayCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      delay_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_enter (DelayCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DelayCf64Obj_exit (DelayCf64Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      delay_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_state_bytes (DelayCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (delay_state_bytes (self->handle));
}

static PyObject *
DelayCf64Obj_get_state (DelayCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = delay_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  delay_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DelayCf64Obj_set_state (DelayCf64Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != delay_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (delay_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DelayCf64Obj_methods[] = {
  { "reset", (PyCFunction)DelayCf64Obj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "push", (PyCFunction)DelayCf64Obj_push, METH_VARARGS,
    "push(x) -> None\n"
    "\n"
    "Advance the write pointer and insert a new sample. The head pointer "
    "decrements (mod capacity) before the write so that buf[head] always "
    "holds the most recent sample.  The same value is simultaneously written "
    "at buf[head + capacity] to keep the mirror half in sync; this ensures "
    "any num_taps-length window starting at head is contiguous without an "
    "extra copy.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> obj.push(0j)\n" },
  { "ptr", (PyCFunction)DelayCf64Obj_ptr, METH_VARARGS,
    "ptr(n=1) -> ndarray\n"
    "\n"
    "Return a zero-copy view of the n most recent samples. Copies at most "
    "min(n, num_taps) samples starting from buf[head] into out.  Because the "
    "dual-buffer layout guarantees contiguity, this is a single memcpy of up "
    "to num_taps elements; no wrap-around logic is needed.  The Python "
    "binding returns a NumPy array backed directly by the pre-allocated "
    "output buffer (base object is the DelayCf64 itself).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> y = obj.ptr(4)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "push_ptr", (PyCFunction)DelayCf64Obj_push_ptr, METH_VARARGS,
    "push_ptr(n=1) -> ndarray\n"
    "\n"
    "Atomically push a sample and snapshot the current window. Equivalent to "
    "calling delay_push() then delay_ptr(num_taps), but avoids the overhead "
    "of a second function call.  Always writes exactly num_taps samples to "
    "out.  The Python binding returns a NumPy array backed by the "
    "pre-allocated push_ptr output buffer.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> y = obj.push_ptr(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "write", (PyCFunction)DelayCf64Obj_write, METH_VARARGS,
    "write(x) -> None\n"
    "\n"
    "Alias for delay_push(); insert a sample without reading back. Provided "
    "for API symmetry with write-then-read patterns where the caller wants to "
    "decouple sample ingestion from window inspection. Internally delegates "
    "to delay_push() with no additional overhead.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> obj.write(1.0 + 0.0j)\n" },
  { "destroy", (PyCFunction)DelayCf64Obj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DelayCf64Obj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DelayCf64Obj_exit, METH_VARARGS, NULL },
  { "state_bytes", (PyCFunction)DelayCf64Obj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)DelayCf64Obj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)DelayCf64Obj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { NULL }
};

static PyTypeObject DelayCf64ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "delay.DelayCf64",
  .tp_basicsize                           = sizeof (DelayCf64Object),
  .tp_dealloc                             = (destructor)DelayCf64Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Create a dual-buffer circular delay line of length num_taps. The "
            "internal capacity is rounded up to the next power of two so that "
            "modular indexing reduces to a single bitwise AND.  Any window of "
            "num_taps consecutive samples is always contiguous in the backing "
            "store; no wrap-around copy is ever needed.\n",
  .tp_methods = DelayCf64Obj_methods,
  .tp_getset  = DelayCf64_getset,
  .tp_new     = DelayCf64Obj_new,
  .tp_init    = (initproc)DelayCf64Obj_init,
};
