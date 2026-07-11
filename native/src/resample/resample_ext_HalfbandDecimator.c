/*
 * resample_ext_HalfbandDecimator.c — HalfbandDecimator type for the resample
 * module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* HalfbandDecimatorObject — wraps HalfbandDecimator_state_t *       */
/* ======================================================== */

#include "HalfbandDecimator/HalfbandDecimator_core.h"
#include "dp_state_pyhelp.h"

typedef struct
{
  PyObject_HEAD HalfbandDecimator_state_t *handle;
  float complex *_execute_buf; /* pre-allocated output for execute */
} HalfbandDecimatorObject;

static void
HalfbandDecimatorObj_dealloc (HalfbandDecimatorObject *self)
{
  if (self->handle)
    HalfbandDecimator_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  HalfbandDecimatorObject *self
      = (HalfbandDecimatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimatorObj_init (HalfbandDecimatorObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[] = { "h", NULL };
  PyObject    *h_obj    = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &h_obj))
    return -1;
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      return -1;
    }
  if (PyArray_NDIM (h_arr) != 1)
    {
      Py_DECREF (h_arr);
      PyErr_SetString (PyExc_ValueError, "h must be a 1-D float32 array");
      return -1;
    }
  size_t h_len = (size_t)PyArray_SIZE (h_arr);
  /* HalfbandDecimator_create(num_taps, h) — num_taps first */
  self->handle
      = HalfbandDecimator_create (h_len, (const float *)PyArray_DATA (h_arr));
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "HalfbandDecimator_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorObj_execute (HalfbandDecimatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  if (!self->_execute_buf)
    {
      size_t _max = HalfbandDecimator_execute_max_out (self->handle);
      if (!_max)
        _max = (size_t)PyArray_SIZE (x_arr);
      self->_execute_buf = malloc (_max * sizeof (float complex));
      if (!self->_execute_buf)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = HalfbandDecimator_execute (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), self->_execute_buf);
  Py_DECREF (x_arr);
  npy_intp       dim = (npy_intp)n_out;
  PyArrayObject *out_arr
      = (PyArrayObject *)PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!out_arr)
    return NULL;
  memcpy (PyArray_DATA (out_arr), self->_execute_buf,
          n_out * sizeof (float complex));
  return (PyObject *)out_arr;
}

static PyObject *
HalfbandDecimatorObj_reset (HalfbandDecimatorObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  HalfbandDecimator_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
HalfbandDecimator_getprop_rate (HalfbandDecimatorObject *self,
                                void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (HalfbandDecimator_get_rate (self->handle));
}
static PyObject *
HalfbandDecimator_getprop_num_taps (HalfbandDecimatorObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)HalfbandDecimator_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimator_getset[]
    = { { "rate", (getter)HalfbandDecimator_getprop_rate, NULL,
          "Always returns 0.5.\n", NULL },
        { "num_taps", (getter)HalfbandDecimator_getprop_num_taps, NULL,
          "Returns the FIR branch length passed to create.\n", NULL },
        { NULL } };

static PyObject *
HalfbandDecimatorObj_destroy (HalfbandDecimatorObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      HalfbandDecimator_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorObj_enter (HalfbandDecimatorObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HalfbandDecimatorObj_exit (HalfbandDecimatorObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      HalfbandDecimator_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* serializable (gh-400): the standard state triplet, generated by the
 * shared macro (see dp_state_pyhelp.h) — byte-identical to jm's output.
 * The matching PyMethodDef rows are below. */
DP_PY_STATE_METHODS (HalfbandDecimatorObj, HalfbandDecimatorObject,
                     self->handle, HalfbandDecimator)

static PyObject *
HalfbandDecimatorObj_execute_max_out (HalfbandDecimatorObject *self,
                                      PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (HalfbandDecimator_execute_max_out (self->handle));
}

static PyMethodDef HalfbandDecimatorObj_methods[] = {

  { "execute", (PyCFunction)HalfbandDecimatorObj_execute, METH_VARARGS,
    "execute(x) -> ndarray\n"
    "\n"
    "Decimate x(0..x_len-1) by 2 into out(0..n_out-1).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import HalfbandDecimator\n"
    "    >>> obj = HalfbandDecimator(np.zeros(1, dtype=np.float32))\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)HalfbandDecimatorObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero delay lines.  Coefficients preserved.\n"
    "\n"
    "    >>> from doppler import HalfbandDecimator\n"
    "    >>> obj = HalfbandDecimator(np.zeros(1, dtype=np.float32))\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)HalfbandDecimatorObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)HalfbandDecimatorObj_get_state, METH_NOARGS,
    "Serialize the decimator's mutable state to bytes." },
  { "set_state", (PyCFunction)HalfbandDecimatorObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)HalfbandDecimatorObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)HalfbandDecimatorObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)HalfbandDecimatorObj_exit, METH_VARARGS, NULL },
  { "execute_max_out", (PyCFunction)HalfbandDecimatorObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { NULL }
};

static PyTypeObject HalfbandDecimatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.HalfbandDecimator",
  .tp_basicsize                           = sizeof (HalfbandDecimatorObject),
  .tp_dealloc = (destructor)HalfbandDecimatorObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a HalfbandDecimator.\n",
  .tp_methods = HalfbandDecimatorObj_methods,
  .tp_getset  = HalfbandDecimator_getset,
  .tp_new     = HalfbandDecimatorObj_new,
  .tp_init    = (initproc)HalfbandDecimatorObj_init,
};
