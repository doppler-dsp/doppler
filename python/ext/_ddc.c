/*
 * _ddc.c — Python C extension for dp/ddc.h
 *
 * Exposes dp_ddc_t as the Python class Ddc:
 *   Ddc(norm_freq, num_in, rate)
 *     norm_freq : float  — NCO normalised frequency (cycles/sample)
 *     num_in    : int    — fixed input block size
 *     rate      : float  — fs_out / fs_in  (1.0 = bypass resampler)
 *
 * Uses built-in M=3 N=19 Kaiser-DPMFS filter coefficients
 * (passband ≤ 0.4·fs_out, stopband ≥ 0.6·fs_out, 60 dB rejection).
 * For custom coefficients create dp_ddc_create_custom in C and
 * pass a pre-built ResampDpmfs — not exposed here yet.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <dp/ddc.h>
#include <dp/stream.h>

/* ======================================================== */
/* DdcObject — wraps dp_ddc_t *                            */
/* ======================================================== */

typedef struct
{
  PyObject_HEAD dp_ddc_t *handle;
} DdcObject;

static void
Ddc_dealloc (DdcObject *self)
{
  if (self->handle)
    dp_ddc_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Ddc_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DdcObject *self = (DdcObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

/* __init__(self, norm_freq, num_in, rate) */
static int
Ddc_init (DdcObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "norm_freq", "num_in", "rate", NULL };
  float norm_freq = 0.0f;
  int num_in = 0;
  double rate = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "fid", kwlist, &norm_freq,
                                    &num_in, &rate))
    return -1;

  if (num_in <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "num_in must be > 0");
      return -1;
    }
  if (rate <= 0.0)
    {
      PyErr_SetString (PyExc_ValueError, "rate must be > 0");
      return -1;
    }

  if (self->handle)
    {
      dp_ddc_destroy (self->handle);
      self->handle = NULL;
    }

  self->handle = dp_ddc_create (norm_freq, (size_t)num_in, rate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_ddc_create returned NULL");
      return -1;
    }
  return 0;
}

/* execute(x) -> np.ndarray[complex64]
 *
 * x    : array_like, dtype=complex64 (or castable), length num_in
 * Returns output samples as a contiguous complex64 array of length nout.
 */
static PyObject *
Ddc_execute (DdcObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "Ddc has been destroyed");
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
  size_t max_out = dp_ddc_max_out (self->handle);
  npy_intp out_dim = (npy_intp)max_out;

  PyObject *out_arr = PyArray_SimpleNew (1, &out_dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  dp_ddc_execute (self->handle, (const dp_cf32_t *)PyArray_DATA (in_arr),
                  num_in, (dp_cf32_t *)PyArray_DATA ((PyArrayObject *)out_arr),
                  max_out);
  Py_DECREF (in_arr);

  /* Trim to actual output count */
  size_t nout = dp_ddc_nout (self->handle);
  PyObject *zero = PyLong_FromLong (0);
  PyObject *n_obj = PyLong_FromSsize_t ((npy_intp)nout);
  PyObject *slice = PySlice_New (zero, n_obj, NULL);
  Py_DECREF (zero);
  Py_DECREF (n_obj);
  PyObject *result = PyObject_GetItem (out_arr, slice);
  Py_DECREF (slice);
  Py_DECREF (out_arr);
  return result;
}

/* set_freq(norm_freq) */
static PyObject *
Ddc_set_freq (DdcObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "Ddc has been destroyed");
      return NULL;
    }
  float norm_freq;
  if (!PyArg_ParseTuple (args, "f", &norm_freq))
    return NULL;
  dp_ddc_set_freq (self->handle, norm_freq);
  Py_RETURN_NONE;
}

/* get_freq() -> float */
static PyObject *
Ddc_get_freq (DdcObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "Ddc has been destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)dp_ddc_get_freq (self->handle));
}

/* reset() */
static PyObject *
Ddc_reset (DdcObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "Ddc has been destroyed");
      return NULL;
    }
  dp_ddc_reset (self->handle);
  Py_RETURN_NONE;
}

/* max_out property */
static PyObject *
Ddc_get_max_out (DdcObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "Ddc has been destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_ddc_max_out (self->handle));
}

/* nout property */
static PyObject *
Ddc_get_nout (DdcObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "Ddc has been destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_ddc_nout (self->handle));
}

/* context manager */
static PyObject *
Ddc_enter (DdcObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Ddc_exit (DdcObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_ddc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* ---- method / property tables ---- */

static PyMethodDef Ddc_methods[] = {
  { "execute", (PyCFunction)Ddc_execute, METH_VARARGS,
    "execute(x) -> np.ndarray[complex64]\n"
    "Mix and resample a block of CF32 samples.\n"
    "Returns a trimmed array of length nout." },
  { "set_freq", (PyCFunction)Ddc_set_freq, METH_VARARGS,
    "set_freq(norm_freq) — retune NCO without resetting phase." },
  { "get_freq", (PyCFunction)Ddc_get_freq, METH_NOARGS,
    "get_freq() -> float — current NCO normalised frequency." },
  { "reset", (PyCFunction)Ddc_reset, METH_NOARGS,
    "reset() — zero NCO phase and resampler history." },
  { "__enter__", (PyCFunction)Ddc_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Ddc_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyGetSetDef Ddc_getset[] = {
  { "max_out", (getter)Ddc_get_max_out, NULL,
    "Maximum output samples per execute() call (fixed at construction).",
    NULL },
  { "nout", (getter)Ddc_get_nout, NULL,
    "Actual output sample count from the last execute() call.", NULL },
  { NULL },
};

static PyTypeObject DdcType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "_ddc.Ddc",
  .tp_basicsize = sizeof (DdcObject),
  .tp_dealloc = (destructor)Ddc_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Ddc(norm_freq, num_in, rate)\n\n"
    "Digital Down-Converter: NCO mix + DPMFS decimation.\n\n"
    "Uses built-in M=3 N=19 Kaiser-DPMFS filter\n"
    "(passband <= 0.4*fs_out, stopband >= 0.6*fs_out, 60 dB rejection).\n\n"
    "Parameters\n"
    "----------\n"
    "norm_freq : float  NCO frequency in cycles/sample. Negative shifts\n"
    "                   a positive-offset signal to DC.\n"
    "num_in    : int    Fixed input block size. execute() clamps to this.\n"
    "rate      : float  fs_out / fs_in. 1.0 bypasses the resampler.\n",
  .tp_methods = Ddc_methods,
  .tp_getset = Ddc_getset,
  .tp_new = Ddc_new,
  .tp_init = (initproc)Ddc_init,
};

/* ======================================================== */
/* Module                                                   */
/* ======================================================== */

static PyModuleDef ddc_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "_ddc",
  .m_doc = "Python binding for dp/ddc.h.",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit__ddc (void)
{
  import_array ();
  if (PyType_Ready (&DdcType) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&ddc_module);
  if (!m)
    return NULL;

  Py_INCREF (&DdcType);
  if (PyModule_AddObject (m, "Ddc", (PyObject *)&DdcType) < 0)
    {
      Py_DECREF (&DdcType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
