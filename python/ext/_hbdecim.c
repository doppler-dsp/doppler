/*
 * dp_hbdecim.c — Python C extension for dp/hbdecim.h
 *
 * Wraps dp_hbdecim_cf32_t.  The Python caller passes the FIR branch
 * (bank[0] or bank[1] from kaiser_prototype(phases=2)); the pure-delay
 * branch is implicit in the C library.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <dp/hbdecim.h>

/* ======================================================== */
/* HbDecimCf32Object — wraps dp_hbdecim_cf32_t *           */
/* ======================================================== */

typedef struct
{
  PyObject_HEAD dp_hbdecim_cf32_t *handle;
} HbDecimCf32Object;

static void
HbDecimCf32_dealloc (HbDecimCf32Object *self)
{
  if (self->handle)
    dp_hbdecim_cf32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HbDecimCf32_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  HbDecimCf32Object *self = (HbDecimCf32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

/* __init__(self, num_taps, h)
 *   num_taps : int — FIR branch length (odd).
 *   h        : float32 ndarray, shape (num_taps,), the FIR branch.
 *              Pass bank[0] or bank[1] from
 *              kaiser_prototype(phases=2); the Python wrapper
 *              (HalfbandDecimator) detects which row is FIR.
 */
static int
HbDecimCf32_init (HbDecimCf32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "num_taps", "h", NULL };
  Py_ssize_t num_taps = 0;
  PyObject *h_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "nO", kwlist, &num_taps,
                                    &h_obj))
    return -1;

  if (num_taps <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "num_taps must be positive");
      return -1;
    }

  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    return -1;

  if (PyArray_NDIM (h_arr) != 1
      || PyArray_DIM (h_arr, 0) != (npy_intp)num_taps)
    {
      Py_DECREF (h_arr);
      PyErr_SetString (PyExc_ValueError,
                       "h must be a 1-D array of length num_taps");
      return -1;
    }

  self->handle = dp_hbdecim_cf32_create ((size_t)num_taps,
                                         (const float *)PyArray_DATA (h_arr));
  Py_DECREF (h_arr);

  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_hbdecim_cf32_create returned NULL");
      return -1;
    }
  return 0;
}

/* reset() */
static PyObject *
HbDecimCf32_reset (HbDecimCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_hbdecim_cf32_reset (self->handle);
  Py_RETURN_NONE;
}

/* rate -> float */
static PyObject *
HbDecimCf32_rate (HbDecimCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (dp_hbdecim_cf32_rate (self->handle));
}

/* num_taps -> int */
static PyObject *
HbDecimCf32_num_taps (HbDecimCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_hbdecim_cf32_num_taps (self->handle));
}

/* execute(x) -> np.ndarray[complex64] */
static PyObject *
HbDecimCf32_execute (HbDecimCf32Object *self, PyObject *args)
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
  /* Over-allocate: ceil(num_in / 2) + num_taps */
  size_t num_taps = dp_hbdecim_cf32_num_taps (self->handle);
  size_t max_out = (num_in + 1) / 2 + num_taps + 2;

  npy_intp out_dim = (npy_intp)max_out;
  PyObject *out_arr = PyArray_SimpleNew (1, &out_dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  size_t n_out = dp_hbdecim_cf32_execute (
      self->handle, (const dp_cf32_t *)PyArray_DATA (in_arr), num_in,
      (dp_cf32_t *)PyArray_DATA ((PyArrayObject *)out_arr), max_out);
  Py_DECREF (in_arr);

  PyObject *slice = PySlice_New (PyLong_FromLong (0),
                                 PyLong_FromSsize_t ((npy_intp)n_out), NULL);
  PyObject *result = PyObject_GetItem (out_arr, slice);
  Py_DECREF (slice);
  Py_DECREF (out_arr);
  return result;
}

/* context manager */
static PyObject *
HbDecimCf32_enter (HbDecimCf32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HbDecimCf32_exit (HbDecimCf32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_hbdecim_cf32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HbDecimCf32_methods[]
    = { { "reset", (PyCFunction)HbDecimCf32_reset, METH_NOARGS,
          "Zero history and clear any pending sample." },
        { "rate", (PyCFunction)HbDecimCf32_rate, METH_NOARGS,
          "Return decimation rate (always 0.5)." },
        { "num_taps", (PyCFunction)HbDecimCf32_num_taps, METH_NOARGS,
          "Return FIR branch length." },
        { "execute", (PyCFunction)HbDecimCf32_execute, METH_VARARGS,
          "execute(x) -> np.ndarray[complex64]" },
        { "__enter__", (PyCFunction)HbDecimCf32_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)HbDecimCf32_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject HbDecimCf32Type = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "_hbdecim.HbDecimCf32",
  .tp_basicsize = sizeof (HbDecimCf32Object),
  .tp_dealloc = (destructor)HbDecimCf32_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Wraps dp_hbdecim_cf32_t — halfband 2:1 decimator.",
  .tp_methods = HbDecimCf32_methods,
  .tp_new = HbDecimCf32_new,
  .tp_init = (initproc)HbDecimCf32_init,
};

/* ======================================================== */
/* Module                                                   */
/* ======================================================== */

static PyModuleDef dp_hbdecim_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "_hbdecim",
  .m_doc = "Python binding for dp/hbdecim.h.",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit__hbdecim (void)
{
  import_array ();
  if (PyType_Ready (&HbDecimCf32Type) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&dp_hbdecim_module);
  if (!m)
    return NULL;

  Py_INCREF (&HbDecimCf32Type);
  if (PyModule_AddObject (m, "HbDecimCf32", (PyObject *)&HbDecimCf32Type) < 0)
    {
      Py_DECREF (&HbDecimCf32Type);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
