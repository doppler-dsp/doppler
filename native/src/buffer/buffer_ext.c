/*
 * buffer_ext.c — Python C extension for dp/buffer.h
 *
 * Exposes three types:
 *   doppler.buffer.F32Buffer  — float _Complex  (complex64)
 *   doppler.buffer.F64Buffer  — double _Complex (complex128)
 *   doppler.buffer.I16Buffer  — int16 IQ pairs (structured array)
 *
 * Zero-copy contract
 * ------------------
 * wait(n) returns a NumPy array that is a *view* directly into the
 * double-mapped circular buffer region.  The caller MUST call consume(n)
 * before requesting the next batch.  Using the array after consume() is
 * undefined behaviour (the memory is still mapped, but the producer may
 * have overwritten it).
 *
 * Thread safety
 * -------------
 * Each buffer is single-producer / single-consumer.  Do not share a
 * buffer object between multiple Python threads without external locking.
 * wait() releases the GIL so a producer running in another thread (or
 * process) can write concurrently.
 */

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <numpy/arrayobject.h>

#include "buffer/buffer.h"
#include "dp_interrupt_pyadopt.h"

/* =====================================================================
 * The faces all three widths share, written ONCE.
 *
 * This file is three hand-written blocks that are ~99% identical, and the
 * 1% is where doppler#1346 came from (i16 grew a different rank). So what
 * is added for the ring's non-blocking surface is stamped from one body
 * per width rather than pasted three times -- and wait()'s give-up logic,
 * which WAS pasted three times, moves onto dp_*_wait_status(), the one
 * owner of that precedence in C.
 *
 * It all goes away when the binding is generated (doppler#1358); until
 * then this is the smallest thing that cannot drift.
 *
 *   CLS    Python class / C prefix          F32Buffer
 *   NAME   ring instantiation               f32
 *   CTYPE  the ring's scalar type           float
 *   NPYT   numpy type of a returned view    NPY_COMPLEX64
 *   NDIM   rank of a returned view          1   (2 for i16: shape (n, 2))
 * ===================================================================== */
#define DP_RING_PY_FACES(CLS, NAME, CTYPE, NPYT, NDIM)                        \
                                                                              \
  /* Why wait()/peek() came back NULL, as the exception it means. Called     \
     with the GIL held. PENDING is peek()'s ordinary "not yet" and is the    \
     caller's to turn into None; it cannot reach wait(), which would still   \
     be spinning. */                                                         \
  static PyObject *CLS##_raise_status_ (CLS##Object *self, Py_ssize_t n)     \
  {                                                                           \
    switch (dp_##NAME##_wait_status (self->buf, (size_t)n))                   \
      {                                                                       \
      case DP_WAIT_TOO_LARGE:                                                 \
        /* A caller bug, not a state: no producer can ever make it true.     \
           Said plainly, because reporting it as end of stream would send    \
           the caller to look at the producer (doppler#1335). */             \
        PyErr_Format (PyExc_ValueError,                                       \
                      "wait(%zd) can never be satisfied: the ring holds %zu", \
                      n, (size_t)self->buf->capacity);                        \
        return NULL;                                                          \
      case DP_WAIT_CLOSED:                                                    \
        PyErr_SetString (PyExc_EOFError,                                      \
                         "end of stream: the producer closed the ring");     \
        return NULL;                                                          \
      default:                                                                \
        /* Interrupted. CPython may already have raised; do not raise a      \
           second. */                                                        \
        if (PyErr_CheckSignals () != 0)                                       \
          return NULL;                                                        \
        PyErr_SetString (PyExc_KeyboardInterrupt, "interrupted");             \
        return NULL;                                                          \
      }                                                                       \
  }                                                                           \
                                                                              \
  /* A zero-copy view of n samples at the read head, kept alive by `self`. */\
  static PyObject *CLS##_view_ (CLS##Object *self, CTYPE *ptr, Py_ssize_t n) \
  {                                                                           \
    npy_intp  dims[2] = { n, 2 };                                             \
    PyObject *arr                                                             \
        = PyArray_SimpleNewFromData ((NDIM), dims, (NPYT), (void *)ptr);      \
    if (!arr)                                                                 \
      return NULL;                                                            \
    PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);           \
    Py_INCREF (self);                                                         \
    self->wait_n = n;                                                         \
    return arr;                                                               \
  }                                                                           \
                                                                              \
  /* peek(n) -> view | None. wait() that never blocks, so it needs no GIL    \
     release: it returns at once. */                                         \
  static PyObject *CLS##_peek (CLS##Object *self, PyObject *args)            \
  {                                                                           \
    Py_ssize_t n;                                                             \
    if (!PyArg_ParseTuple (args, "n", &n))                                    \
      return NULL;                                                            \
    if (n <= 0)                                                               \
      {                                                                       \
        PyErr_SetString (PyExc_ValueError, "n must be positive");             \
        return NULL;                                                          \
      }                                                                       \
    CTYPE *ptr = dp_##NAME##_peek (self->buf, (size_t)n);                     \
    if (ptr)                                                                  \
      return CLS##_view_ (self, ptr, n);                                      \
    if (dp_##NAME##_wait_status (self->buf, (size_t)n) == DP_WAIT_PENDING)    \
      Py_RETURN_NONE; /* not yet -- the ordinary answer */                    \
    return CLS##_raise_status_ (self, n);                                     \
  }                                                                           \
                                                                              \
  /* write_some(arr) -> int. Takes what fits and says how much. */           \
  static PyObject *CLS##_write_some (CLS##Object *self, PyObject *args)      \
  {                                                                           \
    const CTYPE *src;                                                         \
    size_t       n;                                                           \
    if (!CLS##_src_ (args, "write_some", &src, &n))                           \
      return NULL;                                                            \
    return PyLong_FromSize_t (dp_##NAME##_write_some (self->buf, src, n));    \
  }                                                                           \
                                                                              \
  static PyObject *CLS##_reset (CLS##Object *self,                           \
                                PyObject    *Py_UNUSED (ignored))            \
  {                                                                           \
    dp_##NAME##_reset (self->buf);                                            \
    self->wait_n = 0;                                                         \
    Py_RETURN_NONE;                                                           \
  }                                                                           \
                                                                              \
  static PyObject *CLS##_space (CLS##Object *self, void *Py_UNUSED (closure))\
  {                                                                           \
    return PyLong_FromSize_t (dp_##NAME##_space (self->buf));                 \
  }

/* The runtime face of the docstrings buffer.pyi carries, to the same bar
 * (check_docstring_coverage scores both). ONES is the width's 8-sample
 * array constructor, the one thing an example cannot share. */
#define DP_RING_PY_IMPORT_(CLS)                                               \
  ">>> import numpy as np\n"                                                  \
  ">>> from doppler.buffer import " #CLS "\n"                                 \
  ">>> buf = " #CLS "(1024)\n"

#define DP_RING_PY_METHODS(CLS, ONES)                                         \
  { "peek", (PyCFunction)CLS##_peek, METH_VARARGS,                            \
    "peek(n) -> ndarray | None\n\n"                                           \
    "wait() that never blocks: a zero-copy view of n samples if they are\n"  \
    "there, else None. For a single-threaded user, where wait() would\n"     \
    "deadlock. None means NOT YET and nothing else. Does not consume --\n"   \
    "call consume(k); k < n reads overlapped frames.\n\n"                     \
    "Parameters\n----------\n"                                                \
    "n : int\n"                                                               \
    "    Samples wanted. Positive, and no larger than capacity.\n\n"          \
    "Returns\n-------\n"                                                      \
    "ndarray or None\n"                                                       \
    "    View of the next n samples, or None when fewer are buffered.\n\n"   \
    "Raises\n------\n"                                                        \
    "EOFError\n"                                                              \
    "    The ring is closed and fewer than n samples remain.\n"              \
    "ValueError\n"                                                            \
    "    n exceeds the capacity, or is not positive.\n\n"                     \
    "Examples\n--------\n"                                                    \
    DP_RING_PY_IMPORT_ (CLS)                                                  \
    ">>> buf.peek(4) is None\n"                                               \
    "True\n"                                                                  \
    ">>> buf.write_some(" ONES ")\n"                                          \
    "8\n"                                                                     \
    ">>> len(buf.peek(4))\n"                                                  \
    "4\n" },                                                                  \
  { "write_some", (PyCFunction)CLS##_write_some, METH_VARARGS,                \
    "write_some(arr) -> int\n\n"                                              \
    "Write as much of arr as fits and return how many samples that was\n"    \
    "(0 when full). Unlike write() it never refuses and never counts a\n"    \
    "drop, so a chunk larger than the ring is fed by looping.\n\n"            \
    "Parameters\n----------\n"                                                \
    "arr : ndarray\n"                                                         \
    "    Samples to write, in the dtype and shape write() takes.\n\n"        \
    "Returns\n-------\n"                                                      \
    "int\n"                                                                   \
    "    Samples accepted; the caller still owns arr[k:].\n\n"               \
    "Examples\n--------\n"                                                    \
    DP_RING_PY_IMPORT_ (CLS)                                                  \
    ">>> buf.write_some(" ONES ")\n"                                          \
    "8\n"                                                                     \
    ">>> buf.dropped\n"                                                       \
    "0\n" },                                                                  \
  { "reset", (PyCFunction)CLS##_reset, METH_NOARGS,                           \
    "reset()\n\n"                                                             \
    "Empty the ring and reopen it (closed becomes False). dropped is a\n"    \
    "lifetime count and is kept. Not safe against a concurrent thread.\n\n"  \
    "Examples\n--------\n"                                                    \
    DP_RING_PY_IMPORT_ (CLS)                                                  \
    ">>> buf.write_some(" ONES ")\n"                                          \
    "8\n"                                                                     \
    ">>> buf.close()\n"                                                       \
    ">>> buf.reset()\n"                                                       \
    ">>> buf.available, buf.closed\n"                                         \
    "(0, False)\n" }

#define DP_RING_PY_GETSET(CLS)                                                \
  { "space", (getter)CLS##_space, NULL,                                       \
    "Free room in samples: the largest write() guaranteed to be accepted.",  \
    NULL }

/* =====================================================================
 * F32Buffer  (float _Complex / complex64)
 * ===================================================================== */

typedef struct
{
  PyObject_HEAD dp_f32_t *buf;    /* NULL after destroy() */
  npy_intp                wait_n; /* samples currently outstanding */
} F32BufferObject;

static PyObject *
F32Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32BufferObject *self = (F32BufferObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->buf    = NULL;
      self->wait_n = 0;
    }
  return (PyObject *)self;
}

static int
F32Buffer_init (F32BufferObject *self, PyObject *args, PyObject *kwds)
{
  Py_ssize_t n_samples;
  if (!PyArg_ParseTuple (args, "n", &n_samples))
    return -1;
  if (n_samples <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "n_samples must be positive");
      return -1;
    }
  self->buf = dp_f32_create ((size_t)n_samples);
  if (!self->buf)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_f32_create failed — n_samples must be a power of "
                       "2 (sub-page sizes are rounded up to one page)");
      return -1;
    }
  return 0;
}

static void
F32Buffer_dealloc (F32BufferObject *self)
{
  if (self->buf)
    {
      dp_f32_destroy (self->buf);
      self->buf = NULL;
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

/* One validation for every method that takes samples IN, so write() and
   write_some() cannot disagree about what an acceptable array is. */
static int
F32Buffer_src_ (PyObject *args, const char *who, const float **src, size_t *n)
{
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return 0;
  if (PyArray_TYPE (arr) != NPY_COMPLEX64)
    {
      PyErr_Format (PyExc_TypeError, "F32Buffer.%s() requires a complex64 array",
                    who);
      return 0;
    }
  if (PyArray_NDIM (arr) != 1 || !PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_Format (PyExc_ValueError,
                    "F32Buffer.%s() requires a contiguous 1-D array", who);
      return 0;
    }
  *src = (const float *)PyArray_DATA (arr);
  *n   = (size_t)PyArray_SIZE (arr);
  return 1;
}

DP_RING_PY_FACES (F32Buffer, f32, float, NPY_COMPLEX64, 1)

static PyObject *
F32Buffer_write (F32BufferObject *self, PyObject *args)
{
  const float *src;
  size_t n;
  if (!F32Buffer_src_ (args, "write", &src, &n))
    return NULL;
  return PyBool_FromLong (dp_f32_write (self->buf, src, n) ? 1 : 0);
}

/* wait() releases the GIL so a producer thread can write concurrently. */
static PyObject *
F32Buffer_wait (F32BufferObject *self, PyObject *args)
{
  Py_ssize_t n;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  if (n <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be positive");
      return NULL;
    }
  float *ptr;
  Py_BEGIN_ALLOW_THREADS
    ptr = dp_f32_wait (self->buf, (size_t)n);
  Py_END_ALLOW_THREADS

  if (!ptr)
    return F32Buffer_raise_status_ (self, n); /* one owner: dp_*_wait_status */

  return F32Buffer_view_ (self, ptr, n);
}

static PyObject *
F32Buffer_consume (F32BufferObject *self, PyObject *args)
{
  Py_ssize_t n = -1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    n = self->wait_n;
  dp_f32_consume (self->buf, (size_t)n);
  self->wait_n = 0;
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_destroy (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->buf)
    {
      dp_f32_destroy (self->buf);
      self->buf = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_capacity (F32BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSsize_t ((Py_ssize_t)self->buf->capacity);
}

static PyObject *
F32Buffer_dropped (F32BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSsize_t ((Py_ssize_t)self->buf->dropped);
}

static PyObject *
F32Buffer_available (F32BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSize_t (dp_f32_available (self->buf));
}

static PyObject *
F32Buffer_close (F32BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  dp_f32_close (self->buf);
  Py_RETURN_NONE;
}

static PyObject *
F32Buffer_closed (F32BufferObject *self, void *Py_UNUSED (closure))
{
  return PyBool_FromLong (dp_f32_closed (self->buf));
}

static PyGetSetDef F32Buffer_getset[] = {
  DP_RING_PY_GETSET (F32Buffer),
  { "closed", (getter)F32Buffer_closed, NULL,
    "True once the producer has called close(): no more data is coming.",
    NULL },
  { "capacity", (getter)F32Buffer_capacity, NULL,
    "Buffer capacity in complex samples.", NULL },
  { "available", (getter)F32Buffer_available, NULL,
    "Samples written but not yet consumed -- the largest n that wait()\n"
    "will return for without spinning.",
    NULL },
  { "dropped", (getter)F32Buffer_dropped, NULL,
    "Samples in REFUSED writes -- not samples lost. write() adds len(arr)\n"
    "on each rejection and copies nothing, so a producer that retries keeps\n"
    "its data and still moves this counter.",
    NULL },
  { NULL },
};

static PyMethodDef F32Buffer_methods[] = {
  DP_RING_PY_METHODS (F32Buffer, "np.ones(8, np.complex64)"),
  { "write", (PyCFunction)F32Buffer_write, METH_VARARGS,
    "write(arr) -> bool\n\nNon-blocking write (complex64). Returns True\n"
    "if every sample was written, False if the ring had no room for all of\n"
    "them and the call was REFUSED -- nothing is dropped and arr is\n"
    "untouched, so you may retry once the consumer makes room." },
  { "close", (PyCFunction)F32Buffer_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Say that no more data is coming -- the producer's half of end of\n"
    "stream. Until this exists a consumer cannot tell \"the producer is\n"
    "slow\" from \"the producer has finished\": both look like an empty\n"
    "ring. Call it once, after the last write.\n"
    "\n"
    "wait() then raises EOFError once the ring is drained, instead of\n"
    "blocking for data that will never arrive.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F32Buffer\n"
    ">>> buf = F32Buffer(1024)\n"
    ">>> buf.close()\n"
    ">>> buf.closed\n"
    "True\n" },
  { "wait", (PyCFunction)F32Buffer_wait, METH_VARARGS,
    "wait(n) -> np.ndarray[complex64]\n\n"
    "Block until n samples are available; return zero-copy view.\n"
    "MUST call consume(n) when done.\n"
    "\n"
    "Two things end the wait instead of returning samples, and a\n"
    "consumer loop has to handle both:\n"
    "\n"
    "- ``EOFError`` -- the producer called close() AND fewer than n\n"
    "  samples remain. The tail is drained and no more is coming.\n"
    "- ``KeyboardInterrupt`` -- somebody asked this process to stop,\n"
    "  through a doppler.interrupt.Interrupt guard that armed the\n"
    "  signal. WARNING: a guard constructed in another extension\n"
    "  module does NOT reach this wait today -- each module links the\n"
    "  interrupt primitive statically and gets its own copy of the\n"
    "  flag (doppler#976).\n"
    "\n"
    "Without close() a consumer cannot tell a slow producer from a\n"
    "finished one -- both look like an empty ring -- so this used to\n"
    "spin forever at 100% CPU. See docs/design/io-termination.md." },
  { "consume", (PyCFunction)F32Buffer_consume, METH_VARARGS,
    "consume([n]) -> None\n\nRelease n samples (defaults to last wait)." },
  { "destroy", (PyCFunction)F32Buffer_destroy, METH_NOARGS,
    "destroy() -> None\n\nUnmap the buffer." },
  { NULL },
};

static PyTypeObject F32BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "buffer.F32Buffer",
  .tp_basicsize                           = sizeof (F32BufferObject),
  .tp_dealloc                             = (destructor)F32Buffer_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "F32Buffer(n_samples)\n\n"
                                            "Double-mapped SPSC circular buffer for complex64 samples.\n"
                                            "n_samples must be a power of 2; a sub-page request is rounded\n"
                                            "up to one page, so read the real size from `.capacity`.",
  .tp_methods                             = F32Buffer_methods,
  .tp_getset                              = F32Buffer_getset,
  .tp_init                                = (initproc)F32Buffer_init,
  .tp_new                                 = F32Buffer_new,
};

/* =====================================================================
 * F64Buffer  (double _Complex / complex128)
 * ===================================================================== */

typedef struct
{
  PyObject_HEAD dp_f64_t *buf;
  npy_intp                wait_n;
} F64BufferObject;

static PyObject *
F64Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F64BufferObject *self = (F64BufferObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->buf    = NULL;
      self->wait_n = 0;
    }
  return (PyObject *)self;
}

static int
F64Buffer_init (F64BufferObject *self, PyObject *args, PyObject *kwds)
{
  Py_ssize_t n_samples;
  if (!PyArg_ParseTuple (args, "n", &n_samples))
    return -1;
  if (n_samples <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "n_samples must be positive");
      return -1;
    }
  self->buf = dp_f64_create ((size_t)n_samples);
  if (!self->buf)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_f64_create failed — n_samples must be a power of "
                       "2 (sub-page sizes are rounded up to one page)");
      return -1;
    }
  return 0;
}

static void
F64Buffer_dealloc (F64BufferObject *self)
{
  if (self->buf)
    {
      dp_f64_destroy (self->buf);
      self->buf = NULL;
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

/* One validation for every method that takes samples IN, so write() and
   write_some() cannot disagree about what an acceptable array is. */
static int
F64Buffer_src_ (PyObject *args, const char *who, const double **src, size_t *n)
{
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return 0;
  if (PyArray_TYPE (arr) != NPY_COMPLEX128)
    {
      PyErr_Format (PyExc_TypeError, "F64Buffer.%s() requires a complex128 array",
                    who);
      return 0;
    }
  if (PyArray_NDIM (arr) != 1 || !PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_Format (PyExc_ValueError,
                    "F64Buffer.%s() requires a contiguous 1-D array", who);
      return 0;
    }
  *src = (const double *)PyArray_DATA (arr);
  *n   = (size_t)PyArray_SIZE (arr);
  return 1;
}

DP_RING_PY_FACES (F64Buffer, f64, double, NPY_COMPLEX128, 1)

static PyObject *
F64Buffer_write (F64BufferObject *self, PyObject *args)
{
  const double *src;
  size_t n;
  if (!F64Buffer_src_ (args, "write", &src, &n))
    return NULL;
  return PyBool_FromLong (dp_f64_write (self->buf, src, n) ? 1 : 0);
}

static PyObject *
F64Buffer_wait (F64BufferObject *self, PyObject *args)
{
  Py_ssize_t n;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  if (n <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be positive");
      return NULL;
    }
  double *ptr;
  Py_BEGIN_ALLOW_THREADS
    ptr = dp_f64_wait (self->buf, (size_t)n);
  Py_END_ALLOW_THREADS

  if (!ptr)
    return F64Buffer_raise_status_ (self, n); /* one owner: dp_*_wait_status */

  return F64Buffer_view_ (self, ptr, n);
}

static PyObject *
F64Buffer_consume (F64BufferObject *self, PyObject *args)
{
  Py_ssize_t n = -1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    n = self->wait_n;
  dp_f64_consume (self->buf, (size_t)n);
  self->wait_n = 0;
  Py_RETURN_NONE;
}

static PyObject *
F64Buffer_destroy (F64BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->buf)
    {
      dp_f64_destroy (self->buf);
      self->buf = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F64Buffer_capacity (F64BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSsize_t ((Py_ssize_t)self->buf->capacity);
}

static PyObject *
F64Buffer_dropped (F64BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSsize_t ((Py_ssize_t)self->buf->dropped);
}

static PyObject *
F64Buffer_available (F64BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSize_t (dp_f64_available (self->buf));
}

static PyObject *
F64Buffer_close (F64BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  dp_f64_close (self->buf);
  Py_RETURN_NONE;
}

static PyObject *
F64Buffer_closed (F64BufferObject *self, void *Py_UNUSED (closure))
{
  return PyBool_FromLong (dp_f64_closed (self->buf));
}

static PyGetSetDef F64Buffer_getset[] = {
  DP_RING_PY_GETSET (F64Buffer),
  { "closed", (getter)F64Buffer_closed, NULL,
    "True once the producer has called close(): no more data is coming.",
    NULL },
  { "capacity", (getter)F64Buffer_capacity, NULL,
    "Buffer capacity in complex samples.", NULL },
  { "available", (getter)F64Buffer_available, NULL,
    "Samples written but not yet consumed -- the largest n that wait()\n"
    "will return for without spinning.",
    NULL },
  { "dropped", (getter)F64Buffer_dropped, NULL,
    "Samples in REFUSED writes -- not samples lost. write() adds len(arr)\n"
    "on each rejection and copies nothing, so a producer that retries keeps\n"
    "its data and still moves this counter.",
    NULL },
  { NULL },
};

static PyMethodDef F64Buffer_methods[] = {
  DP_RING_PY_METHODS (F64Buffer, "np.ones(8, np.complex128)"),
  { "write", (PyCFunction)F64Buffer_write, METH_VARARGS,
    "write(arr) -> bool\n\nNon-blocking write (complex128). Returns True\n"
    "if every sample was written, False if the call was REFUSED for want of\n"
    "room -- nothing is dropped and arr is untouched, so you may retry." },
  { "close", (PyCFunction)F64Buffer_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Say that no more data is coming -- the producer's half of end of\n"
    "stream. Until this exists a consumer cannot tell \"the producer is\n"
    "slow\" from \"the producer has finished\": both look like an empty\n"
    "ring. Call it once, after the last write.\n"
    "\n"
    "wait() then raises EOFError once the ring is drained, instead of\n"
    "blocking for data that will never arrive.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import F64Buffer\n"
    ">>> buf = F64Buffer(1024)\n"
    ">>> buf.close()\n"
    ">>> buf.closed\n"
    "True\n" },
  { "wait", (PyCFunction)F64Buffer_wait, METH_VARARGS,
    "wait(n) -> np.ndarray[complex128]\n\n"
    "Block until n samples are available; return zero-copy view.\n"
    "MUST call consume(n) when done.\n"
    "\n"
    "Two things end the wait instead of returning samples, and a\n"
    "consumer loop has to handle both:\n"
    "\n"
    "- ``EOFError`` -- the producer called close() AND fewer than n\n"
    "  samples remain. The tail is drained and no more is coming.\n"
    "- ``KeyboardInterrupt`` -- somebody asked this process to stop,\n"
    "  through a doppler.interrupt.Interrupt guard that armed the\n"
    "  signal. WARNING: a guard constructed in another extension\n"
    "  module does NOT reach this wait today -- each module links the\n"
    "  interrupt primitive statically and gets its own copy of the\n"
    "  flag (doppler#976).\n"
    "\n"
    "Without close() a consumer cannot tell a slow producer from a\n"
    "finished one -- both look like an empty ring -- so this used to\n"
    "spin forever at 100% CPU. See docs/design/io-termination.md." },
  { "consume", (PyCFunction)F64Buffer_consume, METH_VARARGS,
    "consume([n]) -> None\n\nRelease n samples." },
  { "destroy", (PyCFunction)F64Buffer_destroy, METH_NOARGS,
    "destroy() -> None\n\nUnmap the buffer." },
  { NULL },
};

static PyTypeObject F64BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "buffer.F64Buffer",
  .tp_basicsize                           = sizeof (F64BufferObject),
  .tp_dealloc                             = (destructor)F64Buffer_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "F64Buffer(n_samples)\n\n"
                                            "Double-mapped SPSC circular buffer for complex128 samples.\n"
                                            "n_samples must be a power of 2; a sub-page request is rounded\n"
                                            "up to one page, so read the real size from `.capacity`.",
  .tp_methods                             = F64Buffer_methods,
  .tp_getset                              = F64Buffer_getset,
  .tp_init                                = (initproc)F64Buffer_init,
  .tp_new                                 = F64Buffer_new,
};

/* =====================================================================
 * I16Buffer  (int16_t IQ pairs)
 *
 * wait() returns a view of shape (n, 2) with dtype int16,
 * where column 0 = I, column 1 = Q.
 * ===================================================================== */

typedef struct
{
  PyObject_HEAD dp_i16_t *buf;
  npy_intp                wait_n;
} I16BufferObject;

static PyObject *
I16Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  I16BufferObject *self = (I16BufferObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->buf    = NULL;
      self->wait_n = 0;
    }
  return (PyObject *)self;
}

static int
I16Buffer_init (I16BufferObject *self, PyObject *args, PyObject *kwds)
{
  Py_ssize_t n_samples;
  if (!PyArg_ParseTuple (args, "n", &n_samples))
    return -1;
  if (n_samples <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "n_samples must be positive");
      return -1;
    }
  self->buf = dp_i16_create ((size_t)n_samples);
  if (!self->buf)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_i16_create failed — n_samples must be a power of "
                       "2 (sub-page sizes are rounded up to one page)");
      return -1;
    }
  return 0;
}

static void
I16Buffer_dealloc (I16BufferObject *self)
{
  if (self->buf)
    {
      dp_i16_destroy (self->buf);
      self->buf = NULL;
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

/* One validation for every method that takes samples IN, so write() and
   write_some() cannot disagree about what an acceptable array is. */
static int
I16Buffer_src_ (PyObject *args, const char *who, const int16_t **src, size_t *n)
{
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return 0;
  if (PyArray_TYPE (arr) != NPY_INT16)
    {
      PyErr_Format (PyExc_TypeError, "I16Buffer.%s() requires an int16 array",
                    who);
      return 0;
    }
  if (!PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_Format (PyExc_ValueError,
                    "I16Buffer.%s() requires a C-contiguous array", who);
      return 0;
    }
  npy_intp total = PyArray_SIZE (arr);
  if (total % 2 != 0)
    {
      PyErr_Format (PyExc_ValueError,
                    "I16Buffer.%s(): array size must be even (I/Q pairs)", who);
      return 0;
    }
  *src = (const int16_t *)PyArray_DATA (arr);
  *n   = (size_t)(total / 2);
  return 1;
}

DP_RING_PY_FACES (I16Buffer, i16, int16_t, NPY_INT16, 2)

static PyObject *
I16Buffer_write (I16BufferObject *self, PyObject *args)
{
  const int16_t *src;
  size_t n;
  if (!I16Buffer_src_ (args, "write", &src, &n))
    return NULL;
  return PyBool_FromLong (dp_i16_write (self->buf, src, n) ? 1 : 0);
}

static PyObject *
I16Buffer_wait (I16BufferObject *self, PyObject *args)
{
  Py_ssize_t n;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  if (n <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be positive");
      return NULL;
    }
  int16_t *ptr;
  Py_BEGIN_ALLOW_THREADS
    ptr = dp_i16_wait (self->buf, (size_t)n);
  Py_END_ALLOW_THREADS

  if (!ptr)
    return I16Buffer_raise_status_ (self, n); /* one owner: dp_*_wait_status */

  /* Shape (n, 2) int16: column 0 = I, column 1 = Q */
  return I16Buffer_view_ (self, ptr, n);
}

static PyObject *
I16Buffer_consume (I16BufferObject *self, PyObject *args)
{
  Py_ssize_t n = -1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    n = self->wait_n;
  dp_i16_consume (self->buf, (size_t)n);
  self->wait_n = 0;
  Py_RETURN_NONE;
}

static PyObject *
I16Buffer_destroy (I16BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->buf)
    {
      dp_i16_destroy (self->buf);
      self->buf = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
I16Buffer_capacity (I16BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSsize_t ((Py_ssize_t)self->buf->capacity);
}

static PyObject *
I16Buffer_dropped (I16BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSsize_t ((Py_ssize_t)self->buf->dropped);
}

static PyObject *
I16Buffer_available (I16BufferObject *self, void *Py_UNUSED (closure))
{
  return PyLong_FromSize_t (dp_i16_available (self->buf));
}

static PyObject *
I16Buffer_close (I16BufferObject *self, PyObject *Py_UNUSED (ignored))
{
  dp_i16_close (self->buf);
  Py_RETURN_NONE;
}

static PyObject *
I16Buffer_closed (I16BufferObject *self, void *Py_UNUSED (closure))
{
  return PyBool_FromLong (dp_i16_closed (self->buf));
}

static PyGetSetDef I16Buffer_getset[] = {
  DP_RING_PY_GETSET (I16Buffer),
  { "closed", (getter)I16Buffer_closed, NULL,
    "True once the producer has called close(): no more data is coming.",
    NULL },
  { "capacity", (getter)I16Buffer_capacity, NULL,
    "Buffer capacity in IQ sample pairs.", NULL },
  { "available", (getter)I16Buffer_available, NULL,
    "Samples written but not yet consumed -- the largest n that wait()\n"
    "will return for without spinning.",
    NULL },
  { "dropped", (getter)I16Buffer_dropped, NULL,
    "Sample pairs in REFUSED writes -- not pairs lost. write() adds the\n"
    "pair count on each rejection and copies nothing, so a producer that\n"
    "retries keeps its data and still moves this counter.",
    NULL },
  { NULL },
};

static PyMethodDef I16Buffer_methods[] = {
  DP_RING_PY_METHODS (I16Buffer, "np.ones((8, 2), np.int16)"),
  { "write", (PyCFunction)I16Buffer_write, METH_VARARGS,
    "write(arr) -> bool\n\nNon-blocking write (int16, shape (n,2) or\n"
    "(2n,)). Returns True if every pair was written, False if the call was\n"
    "REFUSED for want of room -- nothing is dropped and arr is untouched." },
  { "close", (PyCFunction)I16Buffer_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Say that no more data is coming -- the producer's half of end of\n"
    "stream. Until this exists a consumer cannot tell \"the producer is\n"
    "slow\" from \"the producer has finished\": both look like an empty\n"
    "ring. Call it once, after the last write.\n"
    "\n"
    "wait() then raises EOFError once the ring is drained, instead of\n"
    "blocking for data that will never arrive.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.buffer import I16Buffer\n"
    ">>> buf = I16Buffer(1024)\n"
    ">>> buf.close()\n"
    ">>> buf.closed\n"
    "True\n" },
  { "wait", (PyCFunction)I16Buffer_wait, METH_VARARGS,
    "wait(n) -> np.ndarray[int16, shape=(n,2)]\n\n"
    "Block until n IQ samples are available; return zero-copy view.\n"
    "Column 0 = I, column 1 = Q. MUST call consume(n) when done.\n"
    "\n"
    "Two things end the wait instead of returning samples, and a\n"
    "consumer loop has to handle both:\n"
    "\n"
    "- ``EOFError`` -- the producer called close() AND fewer than n\n"
    "  samples remain. The tail is drained and no more is coming.\n"
    "- ``KeyboardInterrupt`` -- somebody asked this process to stop,\n"
    "  through a doppler.interrupt.Interrupt guard that armed the\n"
    "  signal. WARNING: a guard constructed in another extension\n"
    "  module does NOT reach this wait today -- each module links the\n"
    "  interrupt primitive statically and gets its own copy of the\n"
    "  flag (doppler#976).\n"
    "\n"
    "Without close() a consumer cannot tell a slow producer from a\n"
    "finished one -- both look like an empty ring -- so this used to\n"
    "spin forever at 100% CPU. See docs/design/io-termination.md." },
  { "consume", (PyCFunction)I16Buffer_consume, METH_VARARGS,
    "consume([n]) -> None\n\nRelease n IQ sample pairs." },
  { "destroy", (PyCFunction)I16Buffer_destroy, METH_NOARGS,
    "destroy() -> None\n\nUnmap the buffer." },
  { NULL },
};

static PyTypeObject I16BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "buffer.I16Buffer",
  .tp_basicsize                           = sizeof (I16BufferObject),
  .tp_dealloc                             = (destructor)I16Buffer_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "I16Buffer(n_samples)\n\n"
                                            "Double-mapped SPSC circular buffer for int16 IQ samples.\n"
                                            "wait(n) returns an (n, 2) int16 array (col 0=I, col 1=Q).\n"
                                            "n_samples must be a power of 2; a sub-page request is rounded\n"
                                            "up to one page, so read the real size from `.capacity`.",
  .tp_methods                             = I16Buffer_methods,
  .tp_getset                              = I16Buffer_getset,
  .tp_init                                = (initproc)I16Buffer_init,
  .tp_new                                 = I16Buffer_new,
};

/* =====================================================================
 * Module
 * ===================================================================== */

static PyModuleDef buffer_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "buffer",
  .m_doc  = "Doppler double-mapped circular buffer bindings.\n\n"
            "Types: F32Buffer (complex64), F64Buffer (complex128), "
            "I16Buffer (int16 IQ).",
  .m_size = -1,
};

PyMODINIT_FUNC
PyInit_buffer (void)
{
  import_array ();

  if (PyType_Ready (&F32BufferType) < 0)
    return NULL;
  if (PyType_Ready (&F64BufferType) < 0)
    return NULL;
  if (PyType_Ready (&I16BufferType) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&buffer_module);
  if (!m)
    return NULL;

  Py_INCREF (&F32BufferType);
  Py_INCREF (&F64BufferType);
  Py_INCREF (&I16BufferType);

  if (PyModule_AddObject (m, "F32Buffer", (PyObject *)&F32BufferType) < 0
      || PyModule_AddObject (m, "F64Buffer", (PyObject *)&F64BufferType) < 0
      || PyModule_AddObject (m, "I16Buffer", (PyObject *)&I16BufferType) < 0)
    {
      Py_DECREF (&F32BufferType);
      Py_DECREF (&F64BufferType);
      Py_DECREF (&I16BufferType);
      Py_DECREF (m);
      return NULL;
    }

  /* ONE flag per process. buffer.h's wait() consults dp_interrupted(), and
     this module is `no_generate`, so jm writes no PyInit_ here to put its
     rendezvous in -- without this call a stop requested through
     doppler.interrupt leaves a ring wait spinning on a different variable
     (doppler#976, and the wait is an unbounded spin, so "different
     variable" means "forever at 100% CPU"). */
  if (!dp_interrupt_pyadopt (m))
    return NULL;

  return m;
}
