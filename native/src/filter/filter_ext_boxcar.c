/*
 * filter_ext_boxcar.c — MovingAverage type for the filter module.
 *
 * Included by filter_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only filter_ext.c is compiled.
 */
/* ======================================================== */
/* MovingAverageObject — wraps boxcar_state_t *       */
/* ======================================================== */

#include "boxcar/boxcar_core.h"

typedef struct
{
  PyObject_HEAD boxcar_state_t *handle;
} MovingAverageObject;

static void
MovingAverageObj_dealloc (MovingAverageObject *self)
{
  if (self->handle)
    boxcar_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MovingAverageObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  MovingAverageObject *self = (MovingAverageObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MovingAverageObj_init (MovingAverageObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char       *kwlist[] = { "len", "gain", NULL };
  unsigned long long len_raw  = 4;
  double             gain     = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kd", kwlist, &len_raw,
                                    &gain))
    return -1;
  size_t len   = (size_t)len_raw;
  self->handle = boxcar_create (len, gain);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "boxcar_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
MovingAverage_step (MovingAverageObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  float complex x = (float)x_raw.real + (float)x_raw.imag * I;
  float complex y = boxcar_step (self->handle, x);
  return PyComplex_FromDoubles ((double)crealf (y), (double)cimagf (y));
}

static PyObject *
MovingAverage_steps (MovingAverageObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj   = NULL;
  PyObject    *out_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", kwlist, &in_obj,
                                    &out_obj))
    return NULL;

  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      if (PyArray_SIZE (out_arr) != n)
        {
          PyErr_Format (PyExc_ValueError, "out length %zd != input length %zd",
                        (Py_ssize_t)PyArray_SIZE (out_arr), (Py_ssize_t)n);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      boxcar_steps (self->handle, (const float complex *)PyArray_DATA (in_arr),
                    (float complex *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  boxcar_steps (self->handle, (const float complex *)PyArray_DATA (in_arr),
                (float complex *)PyArray_DATA ((PyArrayObject *)out_arr),
                (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
MovingAverageObj_reset (MovingAverageObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  boxcar_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
MovingAverageObj_state_bytes (MovingAverageObject *self,
                              PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (boxcar_state_bytes (self->handle));
}

static PyObject *
MovingAverageObj_get_state (MovingAverageObject *self,
                            PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = boxcar_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  boxcar_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
MovingAverageObj_set_state (MovingAverageObject *self, PyObject *arg)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!PyBytes_Check (arg))
    {
      PyErr_SetString (PyExc_TypeError, "set_state expects bytes");
      return NULL;
    }
  if ((size_t)PyBytes_GET_SIZE (arg) != boxcar_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (boxcar_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
MovingAverage_getprop_len (MovingAverageObject *self,
                           void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->len);
}
static PyObject *
MovingAverage_getprop_gain (MovingAverageObject *self,
                            void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (boxcar_get_gain (self->handle));
}
static int
MovingAverage_setprop_gain (MovingAverageObject *self, PyObject *value,
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
  boxcar_set_gain (self->handle, v);
  return 0;
}

static PyGetSetDef MovingAverage_getset[]
    = { { "len", (getter)MovingAverage_getprop_len, NULL, "Len.\n", NULL },
        { "gain", (getter)MovingAverage_getprop_gain,
          (setter)MovingAverage_setprop_gain, "Current output gain.\n", NULL },
        { NULL } };

static PyObject *
MovingAverageObj_destroy (MovingAverageObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      boxcar_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MovingAverageObj_enter (MovingAverageObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MovingAverageObj_exit (MovingAverageObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      boxcar_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef MovingAverageObj_methods[]
    = { { "step", (PyCFunction)MovingAverage_step, METH_VARARGS,
          "step(x) -> float complex\n"
          "\n"
          "Slide the window by one sample; return the gained moving average.\n"
          "\n"
          "    >>> from doppler import MovingAverage\n"
          "    >>> obj = MovingAverage(4, 1.0)\n"
          "    >>> obj.step(1.0 + 0.0j)\n"
          "    0j\n" },
        { "steps", (PyCFunction)(void *)MovingAverage_steps,
          METH_VARARGS | METH_KEYWORDS,
          "steps(x[, out]) -> ndarray\n"
          "\n"
          "Filter a block: write the gained moving average of each sample.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import MovingAverage\n"
          "    >>> obj = MovingAverage(4, 1.0)\n"
          "    >>> y = obj.steps(np.zeros(4, dtype=np.complex64))\n"
          "    >>> y.shape\n"
          "    (4,)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },

        { "reset", (PyCFunction)MovingAverageObj_reset, METH_NOARGS,
          "reset() -> None\n"
          "\n"
          "Clear the window (zero the ring and the running sum); keep the "
          "configured length and gain.\n"
          "\n"
          "    >>> from doppler import MovingAverage\n"
          "    >>> obj = MovingAverage(4, 1.0)\n"
          "    >>> obj.reset()\n" },
        { "state_bytes", (PyCFunction)MovingAverageObj_state_bytes,
          METH_NOARGS, "Serialized state size in bytes." },
        { "get_state", (PyCFunction)MovingAverageObj_get_state, METH_NOARGS,
          "Serialize the engine's mutable state to bytes." },
        { "set_state", (PyCFunction)MovingAverageObj_set_state, METH_O,
          "Restore mutable state from a get_state() blob." },
        { "destroy", (PyCFunction)MovingAverageObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)MovingAverageObj_enter, METH_NOARGS,
          NULL },
        { "__exit__", (PyCFunction)MovingAverageObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject MovingAverageObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "filter.MovingAverage",
  .tp_basicsize                           = sizeof (MovingAverageObject),
  .tp_dealloc = (destructor)MovingAverageObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "MovingAverage type.\n",
  .tp_methods = MovingAverageObj_methods,
  .tp_getset  = MovingAverage_getset,
  .tp_new     = MovingAverageObj_new,
  .tp_init    = (initproc)MovingAverageObj_init,
};
