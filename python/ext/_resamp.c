/*
 * dp_resamp.c — Python C extension for dp/resamp.h
 *
 * Hand-written (gen_pyext.py produces a useful skeleton but cannot
 * infer the polyphase bank layout or output-buffer sizing).
 *
 * Regenerate skeleton with:
 *   python tools/gen_pyext.py c/include/dp/resamp.h
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <dp/resamp.h>
#include <dp/stream.h>
#include <math.h>

/* ======================================================== */
/* ResampCf32Object — wraps dp_resamp_cf32_t *              */
/* ======================================================== */

typedef struct
{
  PyObject_HEAD dp_resamp_cf32_t *handle;
} ResampCf32Object;

static void
ResampCf32_dealloc (ResampCf32Object *self)
{
  if (self->handle)
    dp_resamp_cf32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ResampCf32_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ResampCf32Object *self = (ResampCf32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

/* __init__(self, bank, rate)
 *   bank : float32 ndarray, shape (L, N), C-contiguous, row-major.
 *          bank[p, k] is the k-th tap of polyphase branch p.
 *          L = number of phases (must be a power of two).
 *          N = taps per phase.
 *          Pass the second return value of kaiser_prototype() directly.
 *   rate : float  — fs_out / fs_in  (> 1 = interpolate, < 1 = decimate)
 *
 * The bank is copied internally; the caller may free it after __init__.
 */
static int
ResampCf32_init (ResampCf32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "bank", "rate", NULL };
  PyObject *bank_obj = NULL;
  double rate = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", kwlist, &bank_obj,
                                    &rate))
    return -1;

  PyArrayObject *bank_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bank_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!bank_arr)
    return -1;

  if (PyArray_NDIM (bank_arr) != 2)
    {
      Py_DECREF (bank_arr);
      PyErr_SetString (PyExc_ValueError,
                       "bank must be a 2-D array of shape (L, N)");
      return -1;
    }

  size_t L = (size_t)PyArray_DIM (bank_arr, 0); /* num phases */
  size_t N = (size_t)PyArray_DIM (bank_arr, 1); /* taps/phase */

  self->handle = dp_resamp_cf32_create (
      L, N, (const float *)PyArray_DATA (bank_arr), rate);
  Py_DECREF (bank_arr);

  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_resamp_cf32_create returned NULL");
      return -1;
    }
  return 0;
}

/* reset() */
static PyObject *
ResampCf32_reset (ResampCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_resamp_cf32_reset (self->handle);
  Py_RETURN_NONE;
}

/* rate -> float */
static PyObject *
ResampCf32_rate (ResampCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (dp_resamp_cf32_rate (self->handle));
}

/* num_phases -> int */
static PyObject *
ResampCf32_num_phases (ResampCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_resamp_cf32_num_phases (self->handle));
}

/* num_taps -> int */
static PyObject *
ResampCf32_num_taps (ResampCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_resamp_cf32_num_taps (self->handle));
}

/* execute(x) -> np.ndarray[complex64]
 *   x : complex64 ndarray, 1-D input samples
 * Returns a 1-D complex64 array of output samples.
 */
static PyObject *
ResampCf32_execute (ResampCf32Object *self, PyObject *args)
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
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  size_t num_in = (size_t)PyArray_SIZE (in_arr);
  double rate = dp_resamp_cf32_rate (self->handle);

  /* Over-allocate output: ceil(num_in * rate) + num_taps */
  size_t num_taps = dp_resamp_cf32_num_taps (self->handle);
  size_t max_out = (size_t)ceil (num_in * rate) + num_taps + 4;

  npy_intp out_dim = (npy_intp)max_out;
  PyObject *out_arr = PyArray_SimpleNew (1, &out_dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  size_t n_out = dp_resamp_cf32_execute (
      self->handle, (const dp_cf32_t *)PyArray_DATA (in_arr), num_in,
      (dp_cf32_t *)PyArray_DATA ((PyArrayObject *)out_arr), max_out);
  Py_DECREF (in_arr);

  /* Return a view trimmed to the actual output count */
  npy_intp trim = (npy_intp)n_out;
  PyObject *result
      = PyArray_NewLikeArray ((PyArrayObject *)out_arr, NPY_CORDER, NULL, 0);
  Py_DECREF (result); /* we'll use slicing instead */

  PyObject *slice
      = PySlice_New (PyLong_FromLong (0), PyLong_FromSsize_t (trim), NULL);
  result = PyObject_GetItem (out_arr, slice);
  Py_DECREF (slice);
  Py_DECREF (out_arr);
  return result;
}

/* context manager */
static PyObject *
ResampCf32_enter (ResampCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ResampCf32_exit (ResampCf32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_resamp_cf32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ResampCf32_methods[]
    = { { "reset", (PyCFunction)ResampCf32_reset, METH_NOARGS,
          "Zero history and reset phase accumulator." },
        { "rate", (PyCFunction)ResampCf32_rate, METH_NOARGS,
          "Return fs_out / fs_in." },
        { "num_phases", (PyCFunction)ResampCf32_num_phases, METH_NOARGS,
          "Return number of polyphase branches (L)." },
        { "num_taps", (PyCFunction)ResampCf32_num_taps, METH_NOARGS,
          "Return taps per phase (N)." },
        { "execute", (PyCFunction)ResampCf32_execute, METH_VARARGS,
          "execute(x) -> np.ndarray[complex64]" },
        { "__enter__", (PyCFunction)ResampCf32_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)ResampCf32_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject ResampCf32Type = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "_resamp.ResampCf32",
  .tp_basicsize = sizeof (ResampCf32Object),
  .tp_dealloc = (destructor)ResampCf32_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Wraps dp_resamp_cf32_t — Kaiser polyphase resampler.",
  .tp_methods = ResampCf32_methods,
  .tp_new = ResampCf32_new,
  .tp_init = (initproc)ResampCf32_init,
};

/* ======================================================== */
/* Module                                                   */
/* ======================================================== */

static PyModuleDef dp_resamp_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "_resamp",
  .m_doc = "Python binding for dp/resamp.h.",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit__resamp (void)
{
  import_array ();
  if (PyType_Ready (&ResampCf32Type) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&dp_resamp_module);
  if (!m)
    return NULL;

  Py_INCREF (&ResampCf32Type);
  if (PyModule_AddObject (m, "ResampCf32", (PyObject *)&ResampCf32Type) < 0)
    {
      Py_DECREF (&ResampCf32Type);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
