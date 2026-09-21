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
  size_t                            _jm_borrowed; /* last borrow's count */
} F32BufferObject;

static void
F32BufferObj_dealloc (F32BufferObject *self)
{
  if (self->handle)
    dp_f32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
F32BufferObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32BufferObject *self = (F32BufferObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
F32BufferObj_init (F32BufferObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]     = { "capacity", NULL };
  unsigned long long capacity_raw = 0ULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", kwlist, &capacity_raw))
    return -1;
  size_t capacity = (size_t)capacity_raw;
  self->handle    = dp_f32_create (capacity);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "capacity must be a power of two (and the mapping "
                       "must succeed)");
      return -1;
    }
  return 0;
}

static PyObject *
F32BufferObj_write (F32BufferObject *self, PyObject *args, PyObject *kwds)
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
  if (!PyArray_Check (x_obj)
      || PyArray_TYPE ((PyArrayObject *)x_obj) != NPY_COMPLEX64)
    {
      PyErr_Format (PyExc_TypeError,
                    "x must be an ndarray of dtype np.complex64"
                    " (got %R)",
                    PyArray_Check (x_obj)
                        ? (PyObject *)PyArray_DESCR ((PyArrayObject *)x_obj)
                        : (PyObject *)Py_TYPE (x_obj));
      return NULL;
    }
  if (PyArray_NDIM ((PyArrayObject *)x_obj) != 1
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)x_obj))
    {
      PyErr_Format (PyExc_ValueError,
                    "x must be a 1-D C-contiguous array"
                    " (got %d-D%s)",
                    PyArray_NDIM ((PyArrayObject *)x_obj),
                    PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)x_obj)
                        ? ""
                        : ", not contiguous");
      return NULL;
    }
  Py_INCREF (x_obj);
  PyArrayObject        *x_arr = (PyArrayObject *)x_obj;
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  bool                  y     = dp_f32_write_view (self->handle, x, x_len);
  Py_DECREF (x_arr);
  return PyBool_FromLong ((long)(y));
}

static PyObject *
F32BufferObj_write_some (F32BufferObject *self, PyObject *args, PyObject *kwds)
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
  if (!PyArray_Check (x_obj)
      || PyArray_TYPE ((PyArrayObject *)x_obj) != NPY_COMPLEX64)
    {
      PyErr_Format (PyExc_TypeError,
                    "x must be an ndarray of dtype np.complex64"
                    " (got %R)",
                    PyArray_Check (x_obj)
                        ? (PyObject *)PyArray_DESCR ((PyArrayObject *)x_obj)
                        : (PyObject *)Py_TYPE (x_obj));
      return NULL;
    }
  if (PyArray_NDIM ((PyArrayObject *)x_obj) != 1
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)x_obj))
    {
      PyErr_Format (PyExc_ValueError,
                    "x must be a 1-D C-contiguous array"
                    " (got %d-D%s)",
                    PyArray_NDIM ((PyArrayObject *)x_obj),
                    PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)x_obj)
                        ? ""
                        : ", not contiguous");
      return NULL;
    }
  Py_INCREF (x_obj);
  PyArrayObject        *x_arr = (PyArrayObject *)x_obj;
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  size_t                y = dp_f32_write_some_view (self->handle, x, x_len);
  Py_DECREF (x_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
F32BufferObj_wait (F32BufferObject *self, PyObject *args, PyObject *kwds)
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
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  float _Complex *_p;
  Py_BEGIN_ALLOW_THREADS
    _p = dp_f32_wait_view (self->handle, n);
  Py_END_ALLOW_THREADS
  if (!_p)
    {
      if (PyErr_CheckSignals ())
        return NULL;
      switch (dp_f32_wait_status (self->handle, n))
        {
        case DP_WAIT_TOO_LARGE:
          PyErr_Format (
              PyExc_ValueError,
              "wait(%lld) can never be satisfied: the ring holds %lld",
              (long long)n, (long long)f32_buffer_get_capacity (self->handle));
          return NULL;
        case DP_WAIT_CLOSED:
          PyErr_SetString (PyExc_EOFError,
                           "end of stream: the producer closed the ring");
          return NULL;
        case DP_WAIT_INTERRUPTED:
          PyErr_SetString (PyExc_KeyboardInterrupt, "interrupted");
          return NULL;
        default:
          break;
        }
      PyErr_SetString (PyExc_ValueError, "wait failed");
      return NULL;
    }
  self->_jm_borrowed = (size_t)(n);
  npy_intp  _dim     = (npy_intp)(n);
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
F32BufferObj_peek (F32BufferObject *self, PyObject *args, PyObject *kwds)
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
  float _Complex *_p = dp_f32_peek_view (self->handle, n);
  if (!_p)
    {
      if (PyErr_CheckSignals ())
        return NULL;
      switch (dp_f32_wait_status (self->handle, n))
        {
        case DP_WAIT_TOO_LARGE:
          PyErr_Format (
              PyExc_ValueError,
              "peek(%lld) can never be satisfied: the ring holds %lld",
              (long long)n, (long long)f32_buffer_get_capacity (self->handle));
          return NULL;
        case DP_WAIT_CLOSED:
          PyErr_SetString (PyExc_EOFError,
                           "end of stream: the producer closed the ring");
          return NULL;
        case DP_WAIT_INTERRUPTED:
          PyErr_SetString (PyExc_KeyboardInterrupt, "interrupted");
          return NULL;
        default:
          break;
        }
      Py_RETURN_NONE;
    }
  self->_jm_borrowed = (size_t)(n);
  npy_intp  _dim     = (npy_intp)(n);
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
F32BufferObj_consume (F32BufferObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|K", _kwlist, &n_raw))
    return NULL;
  size_t n = (size_t)n_raw;
  if (!n)
    {
      n = self->_jm_borrowed;
      if (!n)
        {
          PyErr_SetString (PyExc_RuntimeError,
                           "consume() has no outstanding borrow to release; "
                           "pass a count, or call it after a borrow");
          return NULL;
        }
    }
  self->_jm_borrowed = 0;
  dp_f32_consume (self->handle, n);
  Py_RETURN_NONE;
}

static PyObject *
F32BufferObj_close (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
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
F32BufferObj_reset (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  self->_jm_borrowed = 0;
  dp_f32_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
F32Buffer_getprop_capacity (F32BufferObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)f32_buffer_get_capacity (self->handle));
}
static PyObject *
F32Buffer_getprop_available (F32BufferObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)f32_buffer_get_available (self->handle));
}
static PyObject *
F32Buffer_getprop_space (F32BufferObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)f32_buffer_get_space (self->handle));
}
static PyObject *
F32Buffer_getprop_dropped (F32BufferObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)f32_buffer_get_dropped (self->handle));
}
static PyObject *
F32Buffer_getprop_closed (F32BufferObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(f32_buffer_get_closed (self->handle)));
}

static PyGetSetDef F32Buffer_getset[] = {
  { "capacity", (getter)F32Buffer_getprop_capacity, NULL,
    "Buffer capacity in complex samples.\n", NULL },
  { "available", (getter)F32Buffer_getprop_available, NULL,
    "Samples written but not yet consumed.\n", NULL },
  { "space", (getter)F32Buffer_getprop_space, NULL,
    "Free room in samples: the largest :meth:`write` sure to fit.\n", NULL },
  { "dropped", (getter)F32Buffer_getprop_dropped, NULL,
    "Cumulative samples in REFUSED writes -- not samples lost.\n", NULL },
  { "closed", (getter)F32Buffer_getprop_closed, NULL,
    "``True`` once the producer has called :meth:`close`.\n", NULL },
  { NULL }
};

static PyObject *
F32BufferObj_destroy (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32BufferObj_enter (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
F32BufferObj_exit (F32BufferObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef F32BufferObj_methods[] = {

  { "write", (PyCFunction)(void *)F32BufferObj_write,
    METH_VARARGS | METH_KEYWORDS,
    "write(x) -> bool\n"
    "\n"
    "Write samples into the buffer without blocking.\n"
    "\n"
    "Copies the complex64 array into the ring buffer in a single ``memcpy``.\n"
    "If there is not enough free space for all ``len(x)`` samples the call\n"
    "is **refused entirely** — nothing is copied and ``x`` is untouched, so\n"
    "you still hold every sample and may retry once the consumer has made\n"
    "room. Nothing is dropped unless you discard it; the refusal is counted\n"
    "in :attr:`dropped`, which is not a loss count. The array must be 1-D\n"
    "and C-contiguous.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Samples to write. Must be 1-D and C-contiguous.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bool\n"
    "    ``True`` if all samples were written; ``False`` if the ring had no\n"
    "    room and the call was refused (``x`` untouched).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex64))\n"
    "True\n"
    ">>> buf2 = F32Buffer(1024)\n"
    ">>> buf2.write(np.zeros(1024, dtype=np.complex64))\n"
    "True\n"
    ">>> buf2.write(np.zeros(1, dtype=np.complex64))\n"
    "False\n" },
  { "write_some", (PyCFunction)(void *)F32BufferObj_write_some,
    METH_VARARGS | METH_KEYWORDS,
    "write_some(x) -> int\n"
    "\n"
    "Write as much of ``x`` as fits and say how much that was.\n"
    "\n"
    "The partial-write twin of :meth:`write`. Where :meth:`write` refuses a\n"
    "block that does not fit whole, this takes the leading samples that do\n"
    "and returns their count -- ``0`` when the ring is full. It never\n"
    "refuses, so it never touches :attr:`dropped`. It is the only way to\n"
    "feed a chunk larger than the ring: loop, advancing by the return value,\n"
    "draining in between.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Samples to write. Must be 1-D and C-contiguous.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Samples accepted, ``0 <= k <= len(x)``. The caller still owns\n"
    "    ``x[k:]``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "A chunk three times the size of the ring, fed by looping:\n"
    "\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> cap = buf.capacity\n"
    ">>> chunk = np.ones(3 * cap, dtype=np.complex64)\n"
    ">>> fed = 0\n"
    ">>> while fed < len(chunk):\n"
    "...     fed += buf.write_some(chunk[fed:])\n"
    "...     _ = buf.peek(buf.available); buf.consume()\n"
    ">>> fed == 3 * cap, buf.dropped\n"
    "(True, 0)\n" },
  { "wait", (PyCFunction)(void *)F32BufferObj_wait,
    METH_VARARGS | METH_KEYWORDS,
    "wait(n) -> ndarray\n"
    "\n"
    "Block until ``n`` samples are available, then lend a zero-copy view.\n"
    "\n"
    "Spins (releasing the GIL so a producer thread can run concurrently)\n"
    "until at least ``n`` samples have been written by the producer. Returns\n"
    "a 1-D complex64 NumPy array that is a *direct view* into the\n"
    "double-mapped ring buffer — no data is copied. Because of the\n"
    "double-mapping, the view is always contiguous even when the requested\n"
    "range wraps around the physical end of the ring.\n"
    "\n"
    "The caller **must** call :meth:`consume` before the next call to\n"
    "``wait``. Using the returned array after ``consume`` is undefined\n"
    "behaviour; the producer may overwrite it at any time.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Number of complex samples to wait for. Must be positive and not\n"
    "    larger than :attr:`capacity`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Zero-copy view of the next ``n`` samples in the ring.\n"
    "\n"
    "Raises\n"
    "------\n"
    "EOFError\n"
    "    The producer called :meth:`close` and fewer than ``n`` samples\n"
    "    remain. The tail is drained and no more is coming, so the wait ends\n"
    "    rather than blocking forever.\n"
    "KeyboardInterrupt\n"
    "    Somebody asked this process to stop, through a\n"
    "    :class:`doppler.interrupt.Interrupt` guard -- from any module: the\n"
    "    flag is process-wide. Without a guard the spin checks for no\n"
    "    signals at all.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.write(np.array([1+2j, 3+4j, 5+6j], dtype=np.complex64))\n"
    "True\n"
    ">>> view = buf.wait(3)\n"
    ">>> view.dtype\n"
    "dtype('complex64')\n"
    ">>> view.shape\n"
    "(3,)\n"
    ">>> view.tolist()\n"
    "[(1+2j), (3+4j), (5+6j)]\n"
    ">>> buf.consume(3)\n" },
  { "peek", (PyCFunction)(void *)F32BufferObj_peek,
    METH_VARARGS | METH_KEYWORDS,
    "peek(n) -> ndarray\n"
    "\n"
    ":meth:`wait` that never blocks: a view, or None for not yet.\n"
    "\n"
    "The single-threaded consumer's read. :meth:`wait` spins until a\n"
    "producer on *another* thread delivers, so a caller that is its own\n"
    "producer would deadlock in it; ``peek`` answers at once instead. When\n"
    "``n`` samples are buffered it returns the same zero-copy,\n"
    "always-contiguous view :meth:`wait` would (1-D complex64); otherwise it\n"
    "returns ``None``.\n"
    "\n"
    "``None`` means **not yet** and nothing else. The two conditions no\n"
    "amount of waiting can cure are raised, exactly as :meth:`wait` raises\n"
    "them, so a poll loop cannot mistake either for a slow producer.\n"
    "\n"
    "Peeking does not consume. Follow it with :meth:`consume`; a\n"
    "``consume(k)`` with ``k < n`` advances by a hop smaller than the frame,\n"
    "which is how overlapped frames are read.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Number of samples wanted. Must be positive and not larger than\n"
    "    :attr:`capacity`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64] | None\n"
    "    Zero-copy view of the next ``n`` samples, or ``None`` when fewer\n"
    "    than ``n`` have been written so far.\n"
    "\n"
    "Raises\n"
    "------\n"
    "EOFError\n"
    "    The ring is closed and fewer than ``n`` samples remain: the rest is\n"
    "    never coming.\n"
    "ValueError\n"
    "    ``n`` exceeds :attr:`capacity` (or is not positive), so no producer\n"
    "    could ever satisfy it.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.peek(4) is None\n"
    "True\n"
    ">>> buf.write_some(np.ones(8, dtype=np.complex64))\n"
    "8\n"
    ">>> buf.peek(4).shape\n"
    "(4,)\n"
    ">>> buf.consume(2)\n"
    ">>> buf.available\n"
    "6\n"
    ">>> buf.close()\n"
    ">>> buf.peek(8)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "EOFError: end of stream: the producer closed the ring\n" },
  { "consume", (PyCFunction)(void *)F32BufferObj_consume,
    METH_VARARGS | METH_KEYWORDS,
    "consume(n) -> None\n"
    "\n"
    "Release ``n`` samples back to the producer.\n"
    "\n"
    "Advances the consumer tail pointer by ``n``, making that space\n"
    "available for the producer to overwrite, and ends the loan: the view a\n"
    ":meth:`wait` or :meth:`peek` lent must not be used afterwards. If ``n``\n"
    "is omitted it is the count of that outstanding view, so the number is\n"
    "written once. ``n`` smaller than the view is how overlapped frames are\n"
    "read: release a hop, keep the rest.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Number of samples to release. Defaults to the count of the\n"
    "    outstanding :meth:`wait` / :meth:`peek` view.\n"
    "\n"
    "Raises\n"
    "------\n"
    "RuntimeError\n"
    "    ``n`` was omitted and nothing is outstanding -- no view was lent\n"
    "    since the last release, so there is no count to default to.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.write(np.ones(4, dtype=np.complex64))\n"
    "True\n"
    ">>> _ = buf.wait(4)\n"
    ">>> buf.consume()\n" },
  { "close", (PyCFunction)F32BufferObj_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Say that no more data is coming.\n"
    "\n"
    "The producer's half of end of stream. Until this exists a consumer\n"
    "cannot tell a slow producer from a finished one -- both look like an\n"
    "empty ring -- so :meth:`wait` had nothing to do but spin. Call it once,\n"
    "after the last write.\n"
    "\n"
    "Release ordering: every sample written before this is visible to a\n"
    "consumer that observes the flag. Closing does not discard what was\n"
    "already written; :meth:`wait` keeps returning batches until the ring is\n"
    "drained, and only then raises ``EOFError``.\n"
    "\n"
    "See ``docs/design/io-termination.md`` for the one termination contract\n"
    "shared with the network and disk transports.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.close()\n"
    ">>> buf.closed\n"
    "True\n"
    ">>> buf.wait(4)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "EOFError: end of stream: the producer closed the ring\n" },
  { "reset", (PyCFunction)F32BufferObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Empty the ring and reopen it.\n"
    "\n"
    "Discards everything buffered, and clears :attr:`closed` so the same\n"
    "ring can carry a second stream -- without it, reuse after :meth:`close`\n"
    "means destroying and re-mapping. :attr:`dropped` is a lifetime count\n"
    "and is kept.\n"
    "\n"
    "Not safe against a concurrent producer or consumer: it moves both ends\n"
    "of the ring. Call it only when both sides are idle.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.write_some(np.ones(8, dtype=np.complex64))\n"
    "8\n"
    ">>> buf.close()\n"
    ">>> buf.reset()\n"
    ">>> buf.available, buf.closed\n"
    "(0, False)\n" },
  { "destroy", (PyCFunction)F32BufferObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)F32BufferObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a F32Buffer be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "F32Buffer\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)F32BufferObj_exit, METH_VARARGS,
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

static PyTypeObject F32BufferObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "buffer.F32Buffer",
  .tp_basicsize                           = sizeof (F32BufferObject),
  .tp_dealloc                             = (destructor)F32BufferObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Lock-free SPSC ring buffer for complex64 (CF32) samples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "capacity : int\n"
    "    Requested buffer size in complex samples. Must be a power of two. "
    "The\n"
    "    VM mirror is built at page granularity, so ``capacity * 8`` must "
    "span a\n"
    "    whole page; a sub-page request is rounded **up** to the smallest\n"
    "    power-of-two that does (minimum 512 on 4 KiB pages, 2048 on 16 KiB\n"
    "    pages such as macOS arm64). Read :attr:`capacity` back for the size\n"
    "    actually allocated.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``capacity must be "
    "a\n"
    "    power of two (and the mapping must succeed)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> import numpy as np\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.capacity >= 1024\n"
    "True\n"
    ">>> buf.write(np.ones(512, dtype=np.complex64))\n"
    "True\n",
  .tp_methods = F32BufferObj_methods,
  .tp_getset  = F32Buffer_getset,
  .tp_new     = F32BufferObj_new,
  .tp_init    = (initproc)F32BufferObj_init,
};
