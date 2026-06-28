/*
 * cvt_ext_adc.c — ADC type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* ADCObject — wraps adc_state_t *       */
/* ======================================================== */

#include "adc/adc_core.h"

typedef struct
{
  PyObject_HEAD adc_state_t *handle;
} ADCObject;

static void
ADCObj_dealloc (ADCObject *self)
{
  if (self->handle)
    adc_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ADCObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ADCObject *self = (ADCObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ADCObj_init (ADCObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "bits", "dbfs", "dithering", NULL };
  int          bits      = 16;
  float        dbfs      = -10.0f;
  int          dithering = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ifi", kwlist, &bits, &dbfs,
                                    &dithering))
    return -1;
  self->handle = adc_create (bits, dbfs, dithering);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "adc_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
ADCObj_reset (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  adc_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
ADC_step (ADCObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  int64_t y = adc_step (self->handle, x);
  return PyLong_FromLongLong ((long long)y);
}

static PyObject *
ADC_steps (ADCObject *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_INT64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      adc_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                 (int64_t *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_INT64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  adc_steps (self->handle, (const float *)PyArray_DATA (in_arr),
             (int64_t *)PyArray_DATA ((PyArrayObject *)out_arr), (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
ADC_getprop_clipped (ADCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}
static PyObject *
ADC_getprop_scale (ADCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->scale);
}
static PyObject *
ADC_getprop_bits (ADCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->bits);
}

static PyGetSetDef ADC_getset[]
    = { { "clipped", (getter)ADC_getprop_clipped, NULL, "Clipped.\n", NULL },
        { "scale", (getter)ADC_getprop_scale, NULL, "Scale.\n", NULL },
        { "bits", (getter)ADC_getprop_bits, NULL, "Bits.\n", NULL },
        { NULL } };

static PyObject *
ADCObj_destroy (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      adc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ADCObj_enter (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ADCObj_exit (ADCObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      adc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ADCObj_state_bytes (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (adc_state_bytes (self->handle));
}

static PyObject *
ADCObj_get_state (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = adc_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  adc_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
ADCObj_set_state (ADCObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != adc_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (adc_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ADCObj_methods[]
    = { { "reset", (PyCFunction)ADCObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },
        { "step", (PyCFunction)ADC_step, METH_VARARGS,
          "step(x) -> int64_t\n"
          "\n"
          "Process one input sample.\n"
          "\n"
          "    >>> from doppler import ADC\n"
          "    >>> obj = ADC(16, -10.0, 0)\n"
          "    >>> obj.step(1.0)\n"
          "    0\n" },
        { "steps", (PyCFunction)(void *)ADC_steps,
          METH_VARARGS | METH_KEYWORDS,
          "steps(x[, out]) -> ndarray\n"
          "\n"
          "Process a block of float samples to int64.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import ADC\n"
          "    >>> obj = ADC(16, -10.0, 0)\n"
          "    >>> y = obj.steps(np.zeros(4, dtype=np.float32))\n"
          "    >>> y.shape\n"
          "    (4,)\n"
          "    >>> y.dtype\n"
          "    dtype('int64')\n" },

        { "destroy", (PyCFunction)ADCObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)ADCObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)ADCObj_exit, METH_VARARGS, NULL },
        { "state_bytes", (PyCFunction)ADCObj_state_bytes, METH_NOARGS,
          "Serialized state size in bytes." },
        { "get_state", (PyCFunction)ADCObj_get_state, METH_NOARGS,
          "Serialize the engine's mutable state to bytes." },
        { "set_state", (PyCFunction)ADCObj_set_state, METH_O,
          "Restore mutable state from a get_state() blob." },
        { NULL } };

static PyTypeObject ADCObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.ADC",
  .tp_basicsize                           = sizeof (ADCObject),
  .tp_dealloc                             = (destructor)ADCObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an ADC instance.\n",
  .tp_methods                             = ADCObj_methods,
  .tp_getset                              = ADC_getset,
  .tp_new                                 = ADCObj_new,
  .tp_init                                = (initproc)ADCObj_init,
};
