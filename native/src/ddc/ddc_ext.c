/*
 * ddc_ext.c — Python C extension for doppler.ddc
 *
 * Exposes two types:
 *   Ddc(norm_freq, rate)
 *     norm_freq : float  — LO frequency in cycles/sample at input rate.
 *     rate      : float  — fs_out / fs_in.
 *
 *   DdcR(norm_freq, rate)
 *     norm_freq : float  — Fine NCO frequency at intermediate rate (fs_in/2).
 *     rate      : float  — Total fs_out / fs_in.  Must be < 0.5.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include "ddc/ddc_core.h"

/* ================================================================== */
/* DdcObject — wraps ddc_state_t *                                    */
/* ================================================================== */

#define DDC_MAX_OUT 131072

typedef struct
{
  PyObject_HEAD ddc_state_t *state;
} DdcObject;

static void
Ddc_dealloc (DdcObject *self)
{
  ddc_destroy (self->state);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Ddc_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DdcObject *self = (DdcObject *)type->tp_alloc (type, 0);
  if (self)
    self->state = NULL;
  return (PyObject *)self;
}

static int
Ddc_init (DdcObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "norm_freq", "rate", NULL };
  double norm_freq = 0.0, rate = 1.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", kwlist, &norm_freq,
                                    &rate))
    return -1;
  if (rate <= 0.0)
    {
      PyErr_SetString (PyExc_ValueError, "rate must be > 0");
      return -1;
    }
  if (self->state)
    ddc_destroy (self->state);
  self->state = ddc_create (norm_freq, rate);
  if (!self->state)
    {
      PyErr_SetString (PyExc_MemoryError, "ddc_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
Ddc_execute (DdcObject *self, PyObject *args)
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDC destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t n_in = (size_t)PyArray_SIZE (in_arr);
  size_t max_out = DDC_MAX_OUT;
  npy_intp dim = (npy_intp)max_out;
  PyObject *out_arr = PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  size_t nout = ddc_execute (
      self->state, (const float _Complex *)PyArray_DATA (in_arr), n_in,
      (float _Complex *)PyArray_DATA ((PyArrayObject *)out_arr), max_out);
  Py_DECREF (in_arr);
  PyObject *slice
      = PySlice_New (NULL, PyLong_FromSsize_t ((npy_intp)nout), NULL);
  PyObject *result = PyObject_GetItem (out_arr, slice);
  Py_DECREF (slice);
  Py_DECREF (out_arr);
  return result;
}

static PyObject *
Ddc_reset (DdcObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDC destroyed");
      return NULL;
    }
  ddc_reset (self->state);
  Py_RETURN_NONE;
}

static PyObject *
Ddc_get_norm_freq (DdcObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDC destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (ddc_get_norm_freq (self->state));
}

static PyObject *
Ddc_set_norm_freq (DdcObject *self, PyObject *args)
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDC destroyed");
      return NULL;
    }
  double nf;
  if (!PyArg_ParseTuple (args, "d", &nf))
    return NULL;
  ddc_set_norm_freq (self->state, nf);
  Py_RETURN_NONE;
}

static PyObject *
Ddc_get_rate_py (DdcObject *self, void *Py_UNUSED (closure))
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDC destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (ddc_get_rate (self->state));
}

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
  ddc_destroy (self->state);
  self->state = NULL;
  Py_RETURN_NONE;
}

static PyMethodDef Ddc_methods[] = {
  { "execute", (PyCFunction)Ddc_execute, METH_VARARGS,
    "execute(x) -> ndarray[complex64]\n"
    "Mix input block with LO, then resample." },
  { "reset", (PyCFunction)Ddc_reset, METH_NOARGS,
    "reset() — zero LO phase and resampler history." },
  { "get_norm_freq", (PyCFunction)Ddc_get_norm_freq, METH_NOARGS,
    "get_norm_freq() -> float" },
  { "set_norm_freq", (PyCFunction)Ddc_set_norm_freq, METH_VARARGS,
    "set_norm_freq(norm_freq) — retune LO without resetting state." },
  { "__enter__", (PyCFunction)Ddc_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Ddc_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyGetSetDef Ddc_getset[] = {
  { "rate", (getter)Ddc_get_rate_py, NULL, "Output / input rate.", NULL },
  { NULL },
};

static PyTypeObject DdcType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ddc.DDC",
  .tp_basicsize = sizeof (DdcObject),
  .tp_dealloc = (destructor)Ddc_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "DDC(norm_freq, rate)\n\n"
            "Digital Down-Converter: LO mix + polyphase resample.\n\n"
            "Parameters\n"
            "----------\n"
            "norm_freq : float  LO frequency in cycles/sample (input rate).\n"
            "rate      : float  fs_out / fs_in.",
  .tp_methods = Ddc_methods,
  .tp_getset = Ddc_getset,
  .tp_new = Ddc_new,
  .tp_init = (initproc)Ddc_init,
};

/* ================================================================== */
/* DdcRObject — wraps ddcr_state_t *                                  */
/* ================================================================== */

typedef struct
{
  PyObject_HEAD ddcr_state_t *state;
} DdcRObject;

static void
DdcR_dealloc (DdcRObject *self)
{
  ddcr_destroy (self->state);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DdcR_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DdcRObject *self = (DdcRObject *)type->tp_alloc (type, 0);
  if (self)
    self->state = NULL;
  return (PyObject *)self;
}

static int
DdcR_init (DdcRObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "norm_freq", "rate", NULL };
  double norm_freq = 0.0, rate = 0.25;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", kwlist, &norm_freq,
                                    &rate))
    return -1;
  if (rate <= 0.0 || rate >= 0.5)
    {
      PyErr_SetString (PyExc_ValueError, "rate must be in (0, 0.5)");
      return -1;
    }
  if (self->state)
    ddcr_destroy (self->state);
  self->state = ddcr_create (norm_freq, rate);
  if (!self->state)
    {
      PyErr_SetString (PyExc_MemoryError, "ddcr_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
DdcR_execute (DdcRObject *self, PyObject *args)
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDCR destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t n_in = (size_t)PyArray_SIZE (in_arr);
  size_t max_out = DDC_MAX_OUT;
  npy_intp dim = (npy_intp)max_out;
  PyObject *out_arr = PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  size_t nout = ddcr_execute (
      self->state, (const float *)PyArray_DATA (in_arr), n_in,
      (float _Complex *)PyArray_DATA ((PyArrayObject *)out_arr), max_out);
  Py_DECREF (in_arr);
  PyObject *slice
      = PySlice_New (NULL, PyLong_FromSsize_t ((npy_intp)nout), NULL);
  PyObject *result = PyObject_GetItem (out_arr, slice);
  Py_DECREF (slice);
  Py_DECREF (out_arr);
  return result;
}

static PyObject *
DdcR_reset (DdcRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDCR destroyed");
      return NULL;
    }
  ddcr_reset (self->state);
  Py_RETURN_NONE;
}

static PyObject *
DdcR_get_norm_freq (DdcRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDCR destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (ddcr_get_norm_freq (self->state));
}

static PyObject *
DdcR_set_norm_freq (DdcRObject *self, PyObject *args)
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDCR destroyed");
      return NULL;
    }
  double nf;
  if (!PyArg_ParseTuple (args, "d", &nf))
    return NULL;
  ddcr_set_norm_freq (self->state, nf);
  Py_RETURN_NONE;
}

static PyObject *
DdcR_get_rate_py (DdcRObject *self, void *Py_UNUSED (closure))
{
  if (!self->state)
    {
      PyErr_SetString (PyExc_RuntimeError, "DDCR destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (ddcr_get_rate (self->state));
}

static PyObject *
DdcR_enter (DdcRObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DdcR_exit (DdcRObject *self, PyObject *args)
{
  (void)args;
  ddcr_destroy (self->state);
  self->state = NULL;
  Py_RETURN_NONE;
}

static PyMethodDef DdcR_methods[] = {
  { "execute", (PyCFunction)DdcR_execute, METH_VARARGS,
    "execute(x) -> ndarray[complex64]\n"
    "Halfband R2C decimate, LO mix, then resample." },
  { "reset", (PyCFunction)DdcR_reset, METH_NOARGS,
    "reset() — zero halfband, LO phase, and resampler history." },
  { "get_norm_freq", (PyCFunction)DdcR_get_norm_freq, METH_NOARGS,
    "get_norm_freq() -> float" },
  { "set_norm_freq", (PyCFunction)DdcR_set_norm_freq, METH_VARARGS,
    "set_norm_freq(norm_freq) — retune fine NCO without resetting state." },
  { "__enter__", (PyCFunction)DdcR_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DdcR_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyGetSetDef DdcR_getset[] = {
  { "rate", (getter)DdcR_get_rate_py, NULL, "Total output / input rate.",
    NULL },
  { NULL },
};

static PyTypeObject DdcRType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ddc.DDCR",
  .tp_basicsize = sizeof (DdcRObject),
  .tp_dealloc = (destructor)DdcR_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "DDCR(norm_freq, rate)\n\n"
            "Architecture D2 DDC: halfband R2C (2:1) → LO mix → resample.\n\n"
            "Parameters\n"
            "----------\n"
            "norm_freq : float  Fine NCO frequency at intermediate rate "
            "(fs_in/2).\n"
            "rate      : float  Total fs_out / fs_in.  Must be < 0.5.",
  .tp_methods = DdcR_methods,
  .tp_getset = DdcR_getset,
  .tp_new = DdcR_new,
  .tp_init = (initproc)DdcR_init,
};

/* ================================================================== */
/* Module                                                              */
/* ================================================================== */

static PyModuleDef ddc_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "ddc",
  .m_doc = "Digital Down-Converter module.",
  .m_size = -1,
};

PyMODINIT_FUNC
PyInit_ddc (void)
{
  import_array ();
  if (PyType_Ready (&DdcType) < 0)
    return NULL;
  if (PyType_Ready (&DdcRType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&ddc_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&DdcType);
  if (PyModule_AddObject (m, "DDC", (PyObject *)&DdcType) < 0)
    {
      Py_DECREF (&DdcType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&DdcRType);
  if (PyModule_AddObject (m, "DDCR", (PyObject *)&DdcRType) < 0)
    {
      Py_DECREF (&DdcRType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
