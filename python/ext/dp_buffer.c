/* dp_buffer
 * python/src/dp_buffer.c
 * Python C extension — doppler circular buffer bindings.
 *
 * Exposes three types:
 *   doppler.dp_buffer.F32Buffer  — float complex  (complex64)
 *   doppler.dp_buffer.F64Buffer  — double complex (complex128)
 *   doppler.dp_buffer.I16Buffer  — int16 IQ pairs (structured array)
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

#define NPY_NO_DEPRECATED_API NPY_2_0_API_VERSION
#define _GNU_SOURCE

#include <Python.h>
#include <numpy/arrayobject.h>

/* Pull in the header-only buffer implementation */
#include "../../c/include/dp/buffer.h"

/* -----------------------------------------------------------------------
 * Helper: build a 1-D NumPy array that is a zero-copy view of ptr[0..n-1]
 * with the given typenum.  The base object (owner) keeps the PyObject alive
 * so NumPy will not attempt to free the underlying memory.
 * --------------------------------------------------------------------- */
static PyObject *
make_view (void *ptr, npy_intp n, int typenum, PyObject *owner)
{
  npy_intp dims[1] = { n };
  PyObject *arr = PyArray_SimpleNewFromData (1, dims, typenum, ptr);
  if (!arr)
    return NULL;
  /* Transfer ownership to the Python buffer object so the memory
   * stays valid as long as at least one of them is alive.        */
  PyArray_SetBaseObject ((PyArrayObject *)arr, owner);
  Py_INCREF (owner);
  return arr;
}

/* =====================================================================
 * F32Buffer  (float complex / complex64)
 * ===================================================================== */

typedef struct
{
  PyObject_HEAD dp_f32 *buf; /* NULL after destroy() */
  npy_intp wait_n;           /* samples currently outstanding (for consume) */
} F32BufferObject;

static PyObject *
F32Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32BufferObject *self = (F32BufferObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->buf = NULL;
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
                       "dp_f32_create failed — n_samples must be a power of 2 "
                       "and aligned to the system page size");
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

static PyObject *
F32Buffer_write (F32BufferObject *self, PyObject *args)
{
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return NULL;

  if (PyArray_TYPE (arr) != NPY_COMPLEX64)
    {
      PyErr_SetString (
          PyExc_TypeError,
          "F32Buffer.write() requires a complex64 (numpy.complex64) array");
      return NULL;
    }
  if (PyArray_NDIM (arr) != 1 || !PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_SetString (PyExc_ValueError,
                       "F32Buffer.write() requires a contiguous 1-D array");
      return NULL;
    }

  npy_intp n = PyArray_SIZE (arr);
  /* Each complex64 element = 2 floats = one IQ sample in the buffer */
  bool ok
      = dp_f32_write (self->buf, (const float *)PyArray_DATA (arr), (size_t)n);
  return PyBool_FromLong (ok ? 1 : 0);
}

/* wait() spins in C with GIL released so the Python producer thread
 * (or a C thread) can make progress while the consumer waits.        */
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
  Py_BEGIN_ALLOW_THREADS ptr = dp_f32_wait (self->buf, (size_t)n);
  Py_END_ALLOW_THREADS

      /* Reinterpret the float* (IQ pairs) as complex64 */
      npy_intp dims[1] = { n };
  PyObject *arr
      = PyArray_SimpleNewFromData (1, dims, NPY_COMPLEX64, (void *)ptr);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);

  self->wait_n = n;
  return arr;
}

static PyObject *
F32Buffer_consume (F32BufferObject *self, PyObject *args)
{
  Py_ssize_t n = -1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    n = self->wait_n; /* default: consume last wait batch */
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

static PyGetSetDef F32Buffer_getset[]
    = { { "capacity", (getter)F32Buffer_capacity, NULL,
          "Buffer capacity in complex samples.", NULL },
        { "dropped", (getter)F32Buffer_dropped, NULL,
          "Number of samples dropped due to buffer overrun.", NULL },
        { NULL } };

static PyMethodDef F32Buffer_methods[] = {
  { "write", (PyCFunction)F32Buffer_write, METH_VARARGS,
    "write(arr) -> bool\n\n"
    "Non-blocking write. arr must be a contiguous complex64 1-D array.\n"
    "Returns True on success, False if the buffer was full (samples "
    "dropped)." },
  { "wait", (PyCFunction)F32Buffer_wait, METH_VARARGS,
    "wait(n) -> np.ndarray[complex64]\n\n"
    "Block until n samples are available, then return a zero-copy view.\n"
    "MUST call consume(n) when done." },
  { "consume", (PyCFunction)F32Buffer_consume, METH_VARARGS,
    "consume([n]) -> None\n\n"
    "Release n samples (defaults to the last wait() count)." },
  { "destroy", (PyCFunction)F32Buffer_destroy, METH_NOARGS,
    "destroy() -> None\n\nUnmap the buffer. Called automatically on GC." },
  { NULL }
};

static PyTypeObject F32BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "na_dp_buffer.F32Buffer",
  .tp_basicsize = sizeof (F32BufferObject),
  .tp_dealloc = (destructor)F32Buffer_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "F32Buffer(n_samples)\n\n"
    "High-performance double-mapped circular buffer for float-complex\n"
    "(complex64 / IQ-float) samples.  n_samples must be a power of 2\n"
    "and must satisfy n_samples * 8 % page_size == 0.",
  .tp_methods = F32Buffer_methods,
  .tp_getset = F32Buffer_getset,
  .tp_init = (initproc)F32Buffer_init,
  .tp_new = F32Buffer_new,
};

/* =====================================================================
 * F64Buffer  (double complex / complex128)
 * ===================================================================== */

typedef struct
{
  PyObject_HEAD dp_f64 *buf;
  npy_intp wait_n;
} F64BufferObject;

static PyObject *
F64Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F64BufferObject *self = (F64BufferObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->buf = NULL;
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
                       "dp_f64_create failed — n_samples must be a power of 2 "
                       "and aligned to the system page size");
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

static PyObject *
F64Buffer_write (F64BufferObject *self, PyObject *args)
{
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return NULL;

  if (PyArray_TYPE (arr) != NPY_COMPLEX128)
    {
      PyErr_SetString (
          PyExc_TypeError,
          "F64Buffer.write() requires a complex128 (numpy.complex128) array");
      return NULL;
    }
  if (PyArray_NDIM (arr) != 1 || !PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_SetString (PyExc_ValueError,
                       "F64Buffer.write() requires a contiguous 1-D array");
      return NULL;
    }

  npy_intp n = PyArray_SIZE (arr);
  bool ok = dp_f64_write (self->buf, (const double *)PyArray_DATA (arr),
                          (size_t)n);
  return PyBool_FromLong (ok ? 1 : 0);
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
  Py_BEGIN_ALLOW_THREADS ptr = dp_f64_wait (self->buf, (size_t)n);
  Py_END_ALLOW_THREADS

      npy_intp dims[1] = { n };
  PyObject *arr
      = PyArray_SimpleNewFromData (1, dims, NPY_COMPLEX128, (void *)ptr);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);

  self->wait_n = n;
  return arr;
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

static PyGetSetDef F64Buffer_getset[]
    = { { "capacity", (getter)F64Buffer_capacity, NULL,
          "Buffer capacity in complex samples.", NULL },
        { "dropped", (getter)F64Buffer_dropped, NULL,
          "Number of samples dropped due to buffer overrun.", NULL },
        { NULL } };

static PyMethodDef F64Buffer_methods[] = {
  { "write", (PyCFunction)F64Buffer_write, METH_VARARGS,
    "write(arr) -> bool\n\n"
    "Non-blocking write. arr must be a contiguous complex128 1-D array.\n"
    "Returns True on success, False if the buffer was full." },
  { "wait", (PyCFunction)F64Buffer_wait, METH_VARARGS,
    "wait(n) -> np.ndarray[complex128]\n\n"
    "Block until n samples are available, then return a zero-copy view.\n"
    "MUST call consume(n) when done." },
  { "consume", (PyCFunction)F64Buffer_consume, METH_VARARGS,
    "consume([n]) -> None\n\nRelease n samples." },
  { "destroy", (PyCFunction)F64Buffer_destroy, METH_NOARGS,
    "destroy() -> None\n\nUnmap the buffer." },
  { NULL }
};

static PyTypeObject F64BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "na_dp_buffer.F64Buffer",
  .tp_basicsize = sizeof (F64BufferObject),
  .tp_dealloc = (destructor)F64Buffer_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "F64Buffer(n_samples)\n\n"
    "High-performance double-mapped circular buffer for double-complex\n"
    "(complex128 / IQ-double) samples.  n_samples must be a power of 2.",
  .tp_methods = F64Buffer_methods,
  .tp_getset = F64Buffer_getset,
  .tp_init = (initproc)F64Buffer_init,
  .tp_new = F64Buffer_new,
};

/* =====================================================================
 * I16Buffer  (int16_t IQ pairs)
 *
 * NumPy has no native complex-int16 dtype.  wait() returns a view of
 * shape (n, 2) with dtype int16, where column 0 = I, column 1 = Q.
 * ===================================================================== */

typedef struct
{
  PyObject_HEAD dp_i16 *buf;
  npy_intp wait_n;
} I16BufferObject;

static PyObject *
I16Buffer_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  I16BufferObject *self = (I16BufferObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->buf = NULL;
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
                       "dp_i16_create failed — n_samples must be a power of 2 "
                       "and aligned to the system page size");
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

static PyObject *
I16Buffer_write (I16BufferObject *self, PyObject *args)
{
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return NULL;

  /* Accept int16 shape-(n,2) or shape-(2n,) — both are n IQ samples */
  if (PyArray_TYPE (arr) != NPY_INT16)
    {
      PyErr_SetString (PyExc_TypeError,
                       "I16Buffer.write() requires an int16 array");
      return NULL;
    }
  if (!PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_SetString (PyExc_ValueError,
                       "I16Buffer.write() requires a C-contiguous array");
      return NULL;
    }

  /* Total int16 elements must be even (I+Q pairs) */
  npy_intp total = PyArray_SIZE (arr);
  if (total % 2 != 0)
    {
      PyErr_SetString (
          PyExc_ValueError,
          "I16Buffer.write(): array size must be even (I/Q pairs)");
      return NULL;
    }
  npy_intp n_samples = total / 2;

  bool ok = dp_i16_write (self->buf, (const int16_t *)PyArray_DATA (arr),
                          (size_t)n_samples);
  return PyBool_FromLong (ok ? 1 : 0);
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
  Py_BEGIN_ALLOW_THREADS ptr = dp_i16_wait (self->buf, (size_t)n);
  Py_END_ALLOW_THREADS

      /* Return shape (n, 2) int16 view — I in col 0, Q in col 1 */
      npy_intp dims[2] = { n, 2 };
  PyObject *arr = PyArray_SimpleNewFromData (2, dims, NPY_INT16, (void *)ptr);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);

  self->wait_n = n;
  return arr;
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

static PyGetSetDef I16Buffer_getset[]
    = { { "capacity", (getter)I16Buffer_capacity, NULL,
          "Buffer capacity in IQ sample pairs.", NULL },
        { "dropped", (getter)I16Buffer_dropped, NULL,
          "Number of sample pairs dropped due to buffer overrun.", NULL },
        { NULL } };

static PyMethodDef I16Buffer_methods[] = {
  { "write", (PyCFunction)I16Buffer_write, METH_VARARGS,
    "write(arr) -> bool\n\n"
    "Non-blocking write. arr must be a C-contiguous int16 array of shape\n"
    "(n, 2) or (2n,) where n is the number of IQ samples.\n"
    "Returns True on success, False if the buffer was full." },
  { "wait", (PyCFunction)I16Buffer_wait, METH_VARARGS,
    "wait(n) -> np.ndarray[int16, shape=(n,2)]\n\n"
    "Block until n IQ samples are available. Returns a zero-copy view\n"
    "of shape (n, 2) where column 0 = I, column 1 = Q.\n"
    "MUST call consume(n) when done." },
  { "consume", (PyCFunction)I16Buffer_consume, METH_VARARGS,
    "consume([n]) -> None\n\nRelease n IQ sample pairs." },
  { "destroy", (PyCFunction)I16Buffer_destroy, METH_NOARGS,
    "destroy() -> None\n\nUnmap the buffer." },
  { NULL }
};

static PyTypeObject I16BufferType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "na_dp_buffer.I16Buffer",
  .tp_basicsize = sizeof (I16BufferObject),
  .tp_dealloc = (destructor)I16Buffer_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "I16Buffer(n_samples)\n\n"
    "High-performance double-mapped circular buffer for int16 IQ samples.\n"
    "Each sample is one IQ pair (2 × int16 = 4 bytes).\n"
    "wait(n) returns an (n, 2) int16 array — column 0 = I, column 1 = Q.\n"
    "n_samples must be a power of 2.",
  .tp_methods = I16Buffer_methods,
  .tp_getset = I16Buffer_getset,
  .tp_init = (initproc)I16Buffer_init,
  .tp_new = I16Buffer_new,
};

/* =====================================================================
 * Module
 * ===================================================================== */

static struct PyModuleDef buffermodule
    = { PyModuleDef_HEAD_INIT, "dp_buffer",
        "Doppler double-mapped circular buffer bindings.\n\n"
        "Types: F32Buffer (complex64), F64Buffer (complex128), I16Buffer "
        "(int16 IQ).",
        -1, NULL };

PyMODINIT_FUNC
PyInit_dp_buffer (void)
{
  import_array (); /* initialise NumPy C-API */

  if (PyType_Ready (&F32BufferType) < 0)
    return NULL;
  if (PyType_Ready (&F64BufferType) < 0)
    return NULL;
  if (PyType_Ready (&I16BufferType) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&buffermodule);
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

  return m;
}
