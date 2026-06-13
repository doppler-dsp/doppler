/*
 * ddc_ext_ddc.c — DDC type for the ddc module.
 *
 * Included by ddc_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only ddc_ext.c is compiled.
 */
/* ======================================================== */
/* DDCObject — wraps ddc_state_t *       */
/* ======================================================== */

#include "ddc/ddc_core.h"

typedef struct
{
  PyObject_HEAD ddc_state_t *handle;
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* allocated capacity for execute */
} DDCObject;

static void
DDCObj_dealloc (DDCObject *self)
{
  if (self->handle)
    ddc_destroy (self->handle);
  free (self->_execute_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DDCObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DDCObject *self = (DDCObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DDCObj_init (DDCObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "norm_freq", "rate", NULL };
  double       norm_freq = 0.0;
  double       rate      = 0.25;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dd", kwlist, &norm_freq,
                                    &rate))
    return -1;
  self->handle = ddc_create (norm_freq, rate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "ddc_create returned NULL");
      return -1;
    }
  {
    size_t _max = ddc_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
DDCObj_execute (DDCObject *self, PyObject *args)
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
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_execute_buf || self->_execute_buf_cap < _need)
    {
      size_t _max = ddc_execute_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      float complex *_tmp
          = realloc (self->_execute_buf, _max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = ddc_execute (self->handle, _ng0, _ng1, self->_execute_buf,
                         self->_execute_buf_cap);
  Py_END_ALLOW_THREADS
  /* NumPy owns the output: an independent array per call, copied from the
   * internal grow-on-demand buffer.  Returning a view of _execute_buf instead
   * dangled when a later, larger execute realloc'd it (the view pins self, not
   * the buffer) — the gh-219 use-after-free.  Matches the numpy-owned source
   * objects (lo/nco/awgn). */
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  memcpy (PyArray_DATA ((PyArrayObject *)arr), self->_execute_buf,
          (size_t)n_out * sizeof (float complex));
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
DDCObj_reset (DDCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ddc_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
DDC_getprop_norm_freq (DDCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ddc_get_norm_freq (self->handle));
}
static int
DDC_setprop_norm_freq (DDCObject *self, PyObject *value,
                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  ddc_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
DDC_getprop_rate (DDCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ddc_get_rate (self->handle));
}

static PyGetSetDef DDC_getset[]
    = { { "norm_freq", (getter)DDC_getprop_norm_freq,
          (setter)DDC_setprop_norm_freq,
          "Return the current LO normalised frequency.\n", NULL },
        { "rate", (getter)DDC_getprop_rate, NULL,
          "Return the configured output/input rate ratio.\n", NULL },
        { NULL } };

static PyObject *
DDCObj_destroy (DDCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ddc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DDCObj_enter (DDCObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DDCObj_exit (DDCObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      ddc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DDCObj_methods[] = {

  { "execute", (PyCFunction)DDCObj_execute, METH_VARARGS,
    "execute(x) -> ndarray\n"
    "\n"
    "Mix input block with LO, then rate-convert.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import DDC\n"
    "    >>> obj = DDC(0.0, 0.25)\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)DDCObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero LO phase and filter history.\n"
    "\n"
    "    >>> from doppler import DDC\n"
    "    >>> obj = DDC(0.0, 0.25)\n"
    "    >>> obj.reset()\n" },
  { "destroy", (PyCFunction)DDCObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DDCObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DDCObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject DDCObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ddc.DDC",
  .tp_basicsize                           = sizeof (DDCObject),
  .tp_dealloc                             = (destructor)DDCObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create a complex-input DDC.\n",
  .tp_methods                             = DDCObj_methods,
  .tp_getset                              = DDC_getset,
  .tp_new                                 = DDCObj_new,
  .tp_init                                = (initproc)DDCObj_init,
};
