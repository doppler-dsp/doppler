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
  double complex              *_ptr_buf;     /* pre-allocated output for ptr */
  size_t                       _ptr_buf_cap; /* allocated capacity for ptr */
  void                       **_ptr_retired; /* gh-219 deferred free */
  size_t                       _ptr_retired_n;
  size_t                       _ptr_retired_cap;
  double complex *_push_ptr_buf;     /* pre-allocated output for push_ptr */
  size_t          _push_ptr_buf_cap; /* allocated capacity for push_ptr */
  void          **_push_ptr_retired; /* gh-219 deferred free */
  size_t          _push_ptr_retired_n;
  size_t          _push_ptr_retired_cap;
} DelayCf64Object;

static void
DelayCf64Obj_dealloc (DelayCf64Object *self)
{
  if (self->handle)
    delay_destroy (self->handle);
  free (self->_ptr_buf);
  for (size_t _i = 0; _i < self->_ptr_retired_n; _i++)
    free (self->_ptr_retired[_i]);
  free (self->_ptr_retired);
  free (self->_push_ptr_buf);
  for (size_t _i = 0; _i < self->_push_ptr_retired_n; _i++)
    free (self->_push_ptr_retired[_i]);
  free (self->_push_ptr_retired);
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
  unsigned long long num_taps_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|K", kwlist, &num_taps_raw))
    return -1;
  size_t num_taps = (size_t)num_taps_raw;
  self->handle    = delay_create (num_taps);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "delay_create returned NULL");
      return -1;
    }
  {
    size_t _max = delay_ptr_max_out (self->handle);
    if (_max)
      {
        self->_ptr_buf = malloc (_max * sizeof (double complex));
        if (!self->_ptr_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_ptr_buf_cap = _max;
      }
  }
  {
    size_t _max = delay_push_ptr_max_out (self->handle);
    if (_max)
      {
        self->_push_ptr_buf = malloc (_max * sizeof (double complex));
        if (!self->_push_ptr_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_push_ptr_buf_cap = _max;
      }
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
DelayCf64Obj_push (DelayCf64Object *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", NULL };
  Py_complex   x_raw     = { 0.0, 0.0 };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "D", _kwlist, &x_raw))
    return NULL;
  double complex x = x_raw.real + x_raw.imag * I;
  delay_push (self->handle, x);
  Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_ptr_max_out (DelayCf64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (delay_ptr_max_out (self->handle));
}

static PyObject *
DelayCf64Obj_ptr (DelayCf64Object *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* Hand-patched: jm's generic implicit-count default is a static 1;
   * doppler's n defaults to the full window (num_taps), computed from
   * instance state at call time -- jm's manifest has no way to express
   * that, so the default is restored here after each regeneration. Also
   * renamed jm's default "count" kwarg to "n" to match this method's
   * long-standing docstring/test naming. */
  static char *_kwlist[] = { "n", "out", NULL };
  Py_ssize_t   n         = (Py_ssize_t)self->handle->num_taps;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = delay_ptr_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t    n_out  = delay_ptr (self->handle, (size_t)n,
                                    (double complex *)PyArray_DATA (out_arr));
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
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
  if (!self->_ptr_buf || self->_ptr_buf_cap < _need)
    {
      size_t _max = delay_ptr_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_ptr_buf && self->_ptr_retired_n == self->_ptr_retired_cap)
        {
          size_t _rcap
              = self->_ptr_retired_cap ? self->_ptr_retired_cap * 2 : 4;
          void **_rt = realloc (self->_ptr_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              PyErr_NoMemory ();
              return NULL;
            }
          self->_ptr_retired     = _rt;
          self->_ptr_retired_cap = _rcap;
        }
      double complex *_tmp = malloc (_max * sizeof (double complex));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_ptr_buf)
        self->_ptr_retired[self->_ptr_retired_n++] = self->_ptr_buf;
      self->_ptr_buf     = _tmp;
      self->_ptr_buf_cap = _max;
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
DelayCf64Obj_push_ptr_max_out (DelayCf64Object *self,
                               PyObject        *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (delay_push_ptr_max_out (self->handle));
}

static PyObject *
DelayCf64Obj_push_ptr (DelayCf64Object *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* Hand-patched: push_ptr's sole param (x) is a scalar, not the array
   * input jm's out= feature (gh-219, gh-thing-single-array-param) covers —
   * that predicate only recognizes a single *array*-typed param. Mirrors
   * the same shape/validation jm generates elsewhere by hand. */
  static char *_kwlist[] = { "x", "out", NULL };
  Py_complex   x_raw     = { 0.0, 0.0 };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "D|O", _kwlist, &x_raw,
                                    &out_obj))
    return NULL;
  double complex x = x_raw.real + x_raw.imag * I;
  /* delay_push_ptr(state, x, out) always writes exactly num_taps elements —
   * no max_out clamp inside the kernel — so out= validation must be exact,
   * not >=, and must run before the kernel call (no second chance). */
  size_t _need = delay_push_ptr_max_out (self->handle);
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        return NULL;
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (_cap != _need)
        {
          PyErr_Format (PyExc_ValueError,
                        "out length %zu != required length %zu (num_taps)",
                        _cap, _need);
          Py_DECREF (out_arr);
          return NULL;
        }
      delay_push_ptr (self->handle, x,
                      (double complex *)PyArray_DATA (out_arr));
      return (PyObject *)out_arr;
    }
  if (!self->_push_ptr_buf || self->_push_ptr_buf_cap < _need)
    {
      size_t _max = _need;
      if (self->_push_ptr_buf
          && self->_push_ptr_retired_n == self->_push_ptr_retired_cap)
        {
          size_t _rcap = self->_push_ptr_retired_cap
                             ? self->_push_ptr_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_push_ptr_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              PyErr_NoMemory ();
              return NULL;
            }
          self->_push_ptr_retired     = _rt;
          self->_push_ptr_retired_cap = _rcap;
        }
      double complex *_tmp = malloc (_max * sizeof (double complex));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_push_ptr_buf)
        self->_push_ptr_retired[self->_push_ptr_retired_n++]
            = self->_push_ptr_buf;
      self->_push_ptr_buf     = _tmp;
      self->_push_ptr_buf_cap = _max;
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
    = { { "num_taps", (getter)DelayCf64_getprop_num_taps, NULL, "Num taps.\n",
          NULL },
        { "capacity", (getter)DelayCf64_getprop_capacity, NULL, "Capacity.\n",
          NULL },
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

static PyMethodDef DelayCf64Obj_methods[] = {
  { "reset", (PyCFunction)DelayCf64Obj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "push", (PyCFunction)(void *)DelayCf64Obj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x) -> None\n"
    "\n"
    "Advance the write pointer and insert a new sample. The head pointer "
    "decrements (mod capacity) before the write so that `buf[head]` always "
    "holds the most recent sample.  The same value is simultaneously written "
    "at `buf[head + capacity]` to keep the mirror half in sync; this ensures "
    "any num_taps-length window starting at head is contiguous without an "
    "extra copy.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> obj.push(0j)\n" },
  { "ptr", (PyCFunction)(void *)DelayCf64Obj_ptr, METH_VARARGS | METH_KEYWORDS,
    "ptr(n=num_taps, out=None) -> ndarray\n"
    "\n"
    "Return a zero-copy view of the n most recent samples. Copies at most "
    "min(n, num_taps) samples starting from `buf[head]` into out.  Because "
    "the dual-buffer layout guarantees contiguity, this is a single memcpy of "
    "up to num_taps elements; no wrap-around logic is needed.  Without out=, "
    "the Python binding returns a NumPy array backed directly by the "
    "pre-allocated output buffer (base object is the DelayCf64 itself); "
    "with out= (must have at least max(ptr_max_out(), n) elements), writes "
    "directly into the caller's array and returns a view of it.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> y = obj.ptr(4)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "ptr_max_out", (PyCFunction)DelayCf64Obj_ptr_max_out, METH_NOARGS,
    "ptr_max_out() -> int\n\nMax output length ptr() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "push_ptr", (PyCFunction)(void *)DelayCf64Obj_push_ptr,
    METH_VARARGS | METH_KEYWORDS,
    "push_ptr(x, out=None) -> ndarray\n"
    "\n"
    "Atomically push a sample and snapshot the current window. Equivalent to "
    "calling push(x) then ptr(num_taps), but avoids the overhead of a "
    "second function call.  Always writes exactly num_taps samples.  "
    "Without out=, the Python binding returns a NumPy array backed by the "
    "pre-allocated push_ptr output buffer; with out= (must have exactly "
    "num_taps elements), writes directly into the caller's array and "
    "returns it.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DelayCf64\n"
    "    >>> obj = DelayCf64(1)\n"
    "    >>> y = obj.push_ptr(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "push_ptr_max_out", (PyCFunction)DelayCf64Obj_push_ptr_max_out,
    METH_NOARGS,
    "push_ptr_max_out() -> int\n\nMax output length push_ptr() can produce "
    "for the current state (always exactly num_taps).\nUse to size the "
    "``out=`` buffer." },
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
  { "state_bytes", (PyCFunction)DelayCf64Obj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)DelayCf64Obj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)DelayCf64Obj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)DelayCf64Obj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DelayCf64Obj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DelayCf64Obj_exit, METH_VARARGS, NULL },
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
