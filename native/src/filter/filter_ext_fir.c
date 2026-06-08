/*
 * filter_ext_fir.c — FIR type for the filter module.
 *
 * Included by filter_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only filter_ext.c is compiled.
 */
/* ======================================================== */
/* FIRObject — wraps fir_state_t *       */
/* ======================================================== */

#include "fir/fir_core.h"

typedef struct
{
  PyObject_HEAD fir_state_t *handle;
  float complex *_execute_buf; /* pre-allocated output for execute */
} FIRObject;

static void
FIRObj_dealloc (FIRObject *self)
{
  if (self->handle)
    fir_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FIRObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FIRObject *self = (FIRObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FIRObj_init (FIRObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "taps", NULL };
  PyObject    *taps_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &taps_obj))
    return -1;
  int            want_dtype = (PyArray_Check (taps_obj)
                    && PyArray_TYPE ((PyArrayObject *)taps_obj) == NPY_FLOAT32)
                                  ? NPY_FLOAT32
                                  : NPY_COMPLEX64;
  PyArrayObject *taps_arr   = (PyArrayObject *)PyArray_FROM_OTF (
      taps_obj, want_dtype, NPY_ARRAY_C_CONTIGUOUS);
  if (!taps_arr)
    {
      return -1;
    }
  size_t taps_len = (size_t)PyArray_SIZE (taps_arr);
  if (want_dtype == NPY_FLOAT32)
    {
      self->handle
          = fir_create_real ((const float *)PyArray_DATA (taps_arr), taps_len);
    }
  else
    {
      self->handle = fir_create (
          (const float complex *)PyArray_DATA (taps_arr), taps_len);
    }
  Py_DECREF (taps_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fir_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
FIRObj_reset (FIRObject *self, PyObject *Py_UNUSED (ignored))
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
FIRObj_execute (FIRObject *self, PyObject *args)
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
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->_execute_buf)
    {
      size_t _max = fir_execute_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_buf = malloc (_max * sizeof (float complex));
      if (!self->_execute_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t    n_out = fir_execute (self->handle,
                                 (const float complex *)PyArray_DATA (in_arr),
                                 (size_t)n, self->_execute_buf);
  npy_intp  dim   = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}
static PyObject *
FIR_getprop_num_taps (FIRObject *self, void *Py_UNUSED (closure))
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
FIR_getprop_is_real (FIRObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(fir_get_is_real (self->handle)));
}

static PyGetSetDef FIR_getset[]
    = { { "num_taps", (getter)FIR_getprop_num_taps, NULL,
          "Number of tap coefficients.\n", NULL },
        { "is_real", (getter)FIR_getprop_is_real, NULL,
          "1 if filter was created with real taps, 0 if complex.\n", NULL },
        { NULL } };

static PyObject *
FIRObj_destroy (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fir_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FIRObj_enter (FIRObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FIRObj_exit (FIRObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fir_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FIRObj_methods[]
    = { { "reset", (PyCFunction)FIRObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "execute", (PyCFunction)FIRObj_execute, METH_VARARGS,
          "execute(x) -> ndarray\n"
          "\n"
          "Filter n_in CF32 samples; write results to out.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FIR\n"
          "    >>> obj = FIR(np.zeros(1, dtype=np.complex64))\n"
          "    >>> y = obj.execute(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "destroy", (PyCFunction)FIRObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)FIRObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)FIRObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject FIRObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "filter.FIR",
  .tp_basicsize                           = sizeof (FIRObject),
  .tp_dealloc                             = (destructor)FIRObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a FIR filter from complex CF32 tap coefficients.\n",
  .tp_methods = FIRObj_methods,
  .tp_getset  = FIR_getset,
  .tp_new     = FIRObj_new,
  .tp_init    = (initproc)FIRObj_init,
};
