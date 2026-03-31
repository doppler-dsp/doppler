/*
 * dp_resamp_dpmfs.c — Python C extension for dp/resamp_dpmfs.h
 *
 * Hand-written (gen_pyext.py cannot infer the dual-bank coefficient
 * layout or output-buffer sizing).
 *
 * Regenerate skeleton with:
 *   python tools/gen_pyext.py c/include/dp/resamp_dpmfs.h
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <dp/resamp_dpmfs.h>
#include <dp/stream.h>
#include <math.h>

/* ======================================================== */
/* ResampDpmfsObject — wraps dp_resamp_dpmfs_t *            */
/* ======================================================== */

typedef struct
{
  PyObject_HEAD dp_resamp_dpmfs_t *handle;
} ResampDpmfsObject;

static void
ResampDpmfs_dealloc (ResampDpmfsObject *self)
{
  if (self->handle)
    dp_resamp_dpmfs_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ResampDpmfs_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ResampDpmfsObject *self = (ResampDpmfsObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

/* __init__(self, coeffs, rate)
 *
 *   coeffs : DPMFSCoeffs (from doppler.polyphase.fit_dpmfs) or any
 *            object with .c attribute — a float32 ndarray of shape
 *            (2, M+1, N), where:
 *              coeffs.c[0, :, :] = c0  — (M+1) x N coefficients for
 *                                         j=0 (mu in [0, 0.5))
 *              coeffs.c[1, :, :] = c1  — (M+1) x N coefficients for
 *                                         j=1 (mu in [0.5, 1.0))
 *            Row-major [m, k]: c[j, m, k] is the k-th tap coefficient
 *            for polynomial order m and phase half j.
 *            M = polynomial order (1-3), N = taps per branch.
 *
 *   rate   : float — fs_out / fs_in (> 1 = interpolate, < 1 = decimate)
 *
 * The coefficient arrays are copied internally; the caller may free
 * coeffs immediately after __init__ returns.
 */
static int
ResampDpmfs_init (ResampDpmfsObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "coeffs", "rate", NULL };
  PyObject *coeffs_obj = NULL;
  double rate = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", kwlist, &coeffs_obj,
                                    &rate))
    return -1;

  /* Extract .c attribute (shape (2, M+1, N)) */
  PyObject *c_attr = PyObject_GetAttrString (coeffs_obj, "c");
  if (!c_attr)
    return -1;

  PyArrayObject *c_arr = (PyArrayObject *)PyArray_FROM_OTF (
      c_attr, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
  Py_DECREF (c_attr);
  if (!c_arr)
    return -1;

  if (PyArray_NDIM (c_arr) != 3 || PyArray_DIM (c_arr, 0) != 2)
    {
      Py_DECREF (c_arr);
      PyErr_SetString (PyExc_ValueError,
                       "coeffs.c must have shape (2, M+1, N) — "
                       "use doppler.polyphase.fit_dpmfs() to generate it");
      return -1;
    }

  /* c[j, m, k] — axes: j=0..1, m=0..M, k=0..N-1 */
  size_t M1 = (size_t)PyArray_DIM (c_arr, 1); /* M+1 */
  size_t N = (size_t)PyArray_DIM (c_arr, 2);
  size_t M = M1 - 1;

  const float *data = (const float *)PyArray_DATA (c_arr);
  const float *c0 = data;          /* c[0, :, :] — (M+1)*N floats */
  const float *c1 = data + M1 * N; /* c[1, :, :] — (M+1)*N floats */

  self->handle = dp_resamp_dpmfs_create (M, N, c0, c1, rate);
  Py_DECREF (c_arr);

  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_resamp_dpmfs_create returned NULL");
      return -1;
    }
  return 0;
}

/* reset() */
static PyObject *
ResampDpmfs_reset (ResampDpmfsObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_resamp_dpmfs_reset (self->handle);
  Py_RETURN_NONE;
}

/* rate -> float */
static PyObject *
ResampDpmfs_rate (ResampDpmfsObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (dp_resamp_dpmfs_rate (self->handle));
}

/* num_taps -> int */
static PyObject *
ResampDpmfs_num_taps (ResampDpmfsObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_resamp_dpmfs_num_taps (self->handle));
}

/* poly_order -> int */
static PyObject *
ResampDpmfs_poly_order (ResampDpmfsObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_resamp_dpmfs_poly_order (self->handle));
}

/* execute(x) -> np.ndarray[complex64] */
static PyObject *
ResampDpmfs_execute (ResampDpmfsObject *self, PyObject *args)
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
  double rate = dp_resamp_dpmfs_rate (self->handle);
  size_t N = dp_resamp_dpmfs_num_taps (self->handle);
  size_t max_out = (size_t)ceil (num_in * rate) + N + 4;

  npy_intp out_dim = (npy_intp)max_out;
  PyObject *out_arr = PyArray_SimpleNew (1, &out_dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  size_t n_out = dp_resamp_dpmfs_execute (
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
ResampDpmfs_enter (ResampDpmfsObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ResampDpmfs_exit (ResampDpmfsObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_resamp_dpmfs_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ResampDpmfs_methods[]
    = { { "reset", (PyCFunction)ResampDpmfs_reset, METH_NOARGS,
          "Zero history and reset phase accumulator." },
        { "rate", (PyCFunction)ResampDpmfs_rate, METH_NOARGS,
          "Return fs_out / fs_in." },
        { "num_taps", (PyCFunction)ResampDpmfs_num_taps, METH_NOARGS,
          "Return taps per phase (N)." },
        { "poly_order", (PyCFunction)ResampDpmfs_poly_order, METH_NOARGS,
          "Return polynomial order (M)." },
        { "execute", (PyCFunction)ResampDpmfs_execute, METH_VARARGS,
          "execute(x) -> np.ndarray[complex64]" },
        { "__enter__", (PyCFunction)ResampDpmfs_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)ResampDpmfs_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject ResampDpmfsType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "_resamp_dpmfs.ResampDpmfs",
  .tp_basicsize = sizeof (ResampDpmfsObject),
  .tp_dealloc = (destructor)ResampDpmfs_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Wraps dp_resamp_dpmfs_t — DPMFS polyphase resampler.",
  .tp_methods = ResampDpmfs_methods,
  .tp_new = ResampDpmfs_new,
  .tp_init = (initproc)ResampDpmfs_init,
};

/* ======================================================== */
/* Module                                                   */
/* ======================================================== */

static PyModuleDef dp_resamp_dpmfs_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "_resamp_dpmfs",
  .m_doc = "Python binding for dp/resamp_dpmfs.h.",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit__resamp_dpmfs (void)
{
  import_array ();
  if (PyType_Ready (&ResampDpmfsType) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&dp_resamp_dpmfs_module);
  if (!m)
    return NULL;

  Py_INCREF (&ResampDpmfsType);
  if (PyModule_AddObject (m, "ResampDpmfs", (PyObject *)&ResampDpmfsType) < 0)
    {
      Py_DECREF (&ResampDpmfsType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
