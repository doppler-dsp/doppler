/* HalfbandDecimatorDpObject — wraps dp_hbdecim_cf32_t *             */
/* CF32 → CF32, uses the C library's AVX-512 halfband path  */
/* ======================================================== */

#include "hbdecim/hbdecim_r2c_core.h"

typedef struct
{
  PyObject_HEAD hbdecim_state_t *handle;
} HalfbandDecimatorDpObject;

static void
HalfbandDecimatorDp_dealloc (HalfbandDecimatorDpObject *self)
{
  if (self->handle)
    hbdecim_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorDp_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  HalfbandDecimatorDpObject *self
      = (HalfbandDecimatorDpObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

/* __init__(h) — h is a float32 1-D array; num_taps inferred from length */
static int
HalfbandDecimatorDp_init (HalfbandDecimatorDpObject *self, PyObject *args,
                          PyObject *kwds)
{
  static char *kwlist[] = { "h", NULL };
  PyObject *h_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &h_obj))
    return -1;
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT32,
      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED | NPY_ARRAY_FORCECAST);
  if (!h_arr)
    return -1;
  if (PyArray_NDIM (h_arr) != 1)
    {
      Py_DECREF (h_arr);
      PyErr_SetString (PyExc_ValueError, "h must be a 1-D float32 array");
      return -1;
    }
  size_t num_taps = (size_t)PyArray_DIM (h_arr, 0);
  if (self->handle)
    {
      hbdecim_destroy (self->handle);
      self->handle = NULL;
    }
  self->handle
      = hbdecim_create (num_taps, (const float *)PyArray_DATA (h_arr));
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "hbdecim_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorDp_reset (HalfbandDecimatorDpObject *self,
                           PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  hbdecim_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
HbDecimDp_rate (HalfbandDecimatorDpObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (hbdecim_get_rate (self->handle));
}

static PyObject *
HbDecimDp_num_taps (HalfbandDecimatorDpObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (hbdecim_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimatorDp_getset[]
    = { { "rate", (getter)HbDecimDp_rate, NULL, "Output-to-input rate ratio.",
          NULL },
        { "num_taps", (getter)HbDecimDp_num_taps, NULL,
          "FIR branch tap count.", NULL },
        { NULL } };

static PyObject *
HalfbandDecimatorDp_execute (HalfbandDecimatorDpObject *self, PyObject *args)
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
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
  if (!in_arr)
    return NULL;
  size_t num_in = (size_t)PyArray_SIZE (in_arr);
  size_t num_taps = hbdecim_get_num_taps (self->handle);
  size_t max_out = (num_in + 1) / 2 + num_taps + 2;
  npy_intp out_dim = (npy_intp)max_out;
  PyArrayObject *out_arr
      = (PyArrayObject *)PyArray_SimpleNew (1, &out_dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  size_t n = hbdecim_execute (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), num_in,
      (float _Complex *)PyArray_DATA (out_arr), max_out);
  Py_DECREF (in_arr);
  PyObject *sliced
      = PySequence_GetSlice ((PyObject *)out_arr, 0, (Py_ssize_t)n);
  Py_DECREF (out_arr);
  return sliced;
}

static PyObject *
HbDecimDp_enter (HalfbandDecimatorDpObject *self,
                 PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HbDecimDp_exit (HalfbandDecimatorDpObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      hbdecim_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorDp_methods[]
    = { { "reset", (PyCFunction)HalfbandDecimatorDp_reset, METH_NOARGS, NULL },
        { "execute", (PyCFunction)HalfbandDecimatorDp_execute, METH_VARARGS,
          NULL },
        { "__enter__", (PyCFunction)HbDecimDp_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)HbDecimDp_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject HalfbandDecimatorDpType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.HalfbandDecimatorDp",
  .tp_basicsize = sizeof (HalfbandDecimatorDpObject),
  .tp_dealloc = (destructor)HalfbandDecimatorDp_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "HbDecimDp(h) — C-library halfband 2:1 decimator (CF32→CF32).",
  .tp_methods = HalfbandDecimatorDp_methods,
  .tp_getset = HalfbandDecimatorDp_getset,
  .tp_new = HalfbandDecimatorDp_new,
  .tp_init = (initproc)HalfbandDecimatorDp_init,
};

/* ======================================================== */
/* HalfbandDecimatorR2CObject — wraps dp_hbdecim_r2cf32_t *          */
/* float32 → CF32, embedded fs/4 mix (Architecture D2)     */
/* ======================================================== */

typedef struct
{
  PyObject_HEAD hbdecim_r2c_state_t *handle;
} HalfbandDecimatorR2CObject;

static void
HalfbandDecimatorR2C_dealloc (HalfbandDecimatorR2CObject *self)
{
  if (self->handle)
    hbdecim_r2c_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorR2C_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  HalfbandDecimatorR2CObject *self
      = (HalfbandDecimatorR2CObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimatorR2C_init (HalfbandDecimatorR2CObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[] = { "h", NULL };
  PyObject *h_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &h_obj))
    return -1;
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
  if (!h_arr)
    return -1;
  if (PyArray_NDIM (h_arr) != 1)
    {
      Py_DECREF (h_arr);
      PyErr_SetString (PyExc_ValueError, "h must be a 1-D float32 array");
      return -1;
    }
  size_t num_taps = (size_t)PyArray_DIM (h_arr, 0);
  if (self->handle)
    {
      hbdecim_r2c_destroy (self->handle);
      self->handle = NULL;
    }
  self->handle
      = hbdecim_r2c_create (num_taps, (const float *)PyArray_DATA (h_arr));
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "hbdecim_r2c_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
HalfbandDecimatorR2C_reset (HalfbandDecimatorR2CObject *self,
                            PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  hbdecim_r2c_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
HbDecimR2C_rate (HalfbandDecimatorR2CObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (hbdecim_r2c_get_rate (self->handle));
}

static PyObject *
HbDecimR2C_num_taps (HalfbandDecimatorR2CObject *self,
                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (hbdecim_r2c_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimatorR2C_getset[]
    = { { "rate", (getter)HbDecimR2C_rate, NULL, "Output-to-input rate ratio.",
          NULL },
        { "num_taps", (getter)HbDecimR2C_num_taps, NULL,
          "FIR branch tap count.", NULL },
        { NULL } };

static PyObject *
HalfbandDecimatorR2C_execute (HalfbandDecimatorR2CObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  /* Accept float32 input; also accept float64 and cast down. */
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_FLOAT32,
      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED | NPY_ARRAY_FORCECAST);
  if (!in_arr)
    return NULL;
  size_t num_in = (size_t)PyArray_SIZE (in_arr);
  size_t num_taps = hbdecim_r2c_get_num_taps (self->handle);
  size_t max_out = (num_in + 1) / 2 + num_taps + 2;
  npy_intp out_dim = (npy_intp)max_out;
  PyArrayObject *out_arr
      = (PyArrayObject *)PyArray_SimpleNew (1, &out_dim, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  size_t n = hbdecim_r2c_execute (
      self->handle, (const float *)PyArray_DATA (in_arr), num_in,
      (float _Complex *)PyArray_DATA (out_arr), max_out);
  Py_DECREF (in_arr);
  PyObject *sliced
      = PySequence_GetSlice ((PyObject *)out_arr, 0, (Py_ssize_t)n);
  Py_DECREF (out_arr);
  return sliced;
}

static PyObject *
HbDecimR2C_enter (HalfbandDecimatorR2CObject *self,
                  PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HbDecimR2C_exit (HalfbandDecimatorR2CObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      hbdecim_r2c_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorR2C_methods[] = {
  { "reset", (PyCFunction)HalfbandDecimatorR2C_reset, METH_NOARGS, NULL },
  { "execute", (PyCFunction)HalfbandDecimatorR2C_execute, METH_VARARGS, NULL },
  { "__enter__", (PyCFunction)HbDecimR2C_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)HbDecimR2C_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject HalfbandDecimatorR2CType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.HalfbandDecimatorR2C",
  .tp_basicsize = sizeof (HalfbandDecimatorR2CObject),
  .tp_dealloc = (destructor)HalfbandDecimatorR2C_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "HbDecimR2C(h) — Architecture D2 halfband decimator (float32→CF32).",
  .tp_methods = HalfbandDecimatorR2C_methods,
  .tp_getset = HalfbandDecimatorR2C_getset,
  .tp_new = HalfbandDecimatorR2C_new,
  .tp_init = (initproc)HalfbandDecimatorR2C_init,
};
