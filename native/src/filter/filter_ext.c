/*
 * filter_ext.c — Python extension module filter
 *
 * Objects: Fir
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

/* ======================================================== */
/* FirObject — wraps fir_state_t *       */
/* ======================================================== */

#include "fir/fir_core.h"

typedef struct
{
  PyObject_HEAD fir_state_t *handle;
  float complex *_execute_buf; /* grow-on-demand output buffer */
  size_t _execute_cap;         /* capacity in complex samples  */
} FirObject;

static void
Fir_dealloc (FirObject *self)
{
  if (self->handle)
    fir_destroy (self->handle);
  free (self->_execute_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Fir_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FirObject *self = (FirObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->handle = NULL;
      self->_execute_buf = NULL;
      self->_execute_cap = 0;
    }
  return (PyObject *)self;
}

/*
 * Fir(taps)
 *
 * taps: float32 ndarray  → fir_create_real (1 FMA/tap)
 *       complex64 ndarray → fir_create     (full complex multiply)
 */
static int
Fir_init (FirObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "taps", NULL };
  PyObject *taps_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &taps_obj))
    return -1;

  PyArrayObject *arr = (PyArrayObject *)PyArray_CheckFromAny (
      taps_obj, NULL, 1, 1, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_NOTSWAPPED,
      NULL);
  if (!arr)
    return -1;

  size_t n = (size_t)PyArray_SIZE (arr);
  int dtype = PyArray_TYPE (arr);

  if (dtype == NPY_FLOAT32)
    self->handle = fir_create_real ((const float *)PyArray_DATA (arr), n);
  else if (dtype == NPY_COMPLEX64)
    self->handle = fir_create ((const float complex *)PyArray_DATA (arr), n);
  else
    {
      PyErr_SetString (PyExc_TypeError, "taps must be float32 or complex64");
      Py_DECREF (arr);
      return -1;
    }
  Py_DECREF (arr);

  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fir_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
Fir_reset (FirObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  fir_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
Fir_execute (FirObject *self, PyObject *args)
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
  size_t n = (size_t)PyArray_SIZE (in_arr);

  if (n > self->_execute_cap)
    {
      free (self->_execute_buf);
      self->_execute_buf
          = (float complex *)malloc (n * sizeof (float complex));
      if (!self->_execute_buf)
        {
          self->_execute_cap = 0;
          Py_DECREF (in_arr);
          return PyErr_NoMemory ();
        }
      self->_execute_cap = n;
    }

  size_t n_out = fir_execute (self->handle,
                              (const float complex *)PyArray_DATA (in_arr), n,
                              self->_execute_buf);
  Py_DECREF (in_arr);

  npy_intp dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  return arr;
}
static PyObject *
Fir_getprop_num_taps (FirObject *self, void *Py_UNUSED (closure))
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
Fir_getprop_is_real (FirObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(fir_get_is_real (self->handle)));
}

static PyGetSetDef Fir_getset[]
    = { { "num_taps", (getter)Fir_getprop_num_taps, NULL, NULL, NULL },
        { "is_real", (getter)Fir_getprop_is_real, NULL, NULL, NULL },
        { NULL } };

static PyObject *
Fir_destroy (FirObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fir_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Fir_enter (FirObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Fir_exit (FirObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fir_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef Fir_methods[]
    = { { "reset", (PyCFunction)Fir_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "execute", (PyCFunction)Fir_execute, METH_VARARGS,
          "execute(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FIR\n"
          "    >>> obj = FIR()\n"
          "    >>> y = obj.execute(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "destroy", (PyCFunction)Fir_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)Fir_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)Fir_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject FirType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "filter.FIR",
  .tp_basicsize = sizeof (FirObject),
  .tp_dealloc = (destructor)Fir_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "FIR type.",
  .tp_methods = Fir_methods,
  .tp_getset = Fir_getset,
  .tp_new = Fir_new,
  .tp_init = (initproc)Fir_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef filter_moduledef = {
  PyModuleDef_HEAD_INIT, .m_name = "filter", .m_doc = "Filter module.",
  .m_size = -1,          .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_filter (void)
{
  import_array ();
  if (PyType_Ready (&FirType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&filter_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&FirType);
  if (PyModule_AddObject (m, "FIR", (PyObject *)&FirType) < 0)
    {
      Py_DECREF (&FirType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
