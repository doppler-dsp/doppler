/*
 * resample_ext.c — Python extension module resample
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <math.h>
#include <numpy/arrayobject.h>

/* ======================================================== */
/* ResamplerObject — wraps resamp_state_t *       */
/* ======================================================== */

#include "hbdecim/hbdecim_core.h"
#include "resamp/resamp_core.h"

typedef struct
{
  PyObject_HEAD resamp_state_t *handle;
} ResamplerObject;

static void
Resampler_dealloc (ResamplerObject *self)
{
  if (self->handle)
    resamp_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Resampler_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ResamplerObject *self = (ResamplerObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
Resampler_init (ResamplerObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "rate", "bank", NULL };
  double rate = 1.0;
  PyObject *bank_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dO", kwlist, &rate,
                                    &bank_obj))
    return -1;

  if (bank_obj && bank_obj != Py_None)
    {
      PyArrayObject *arr = (PyArrayObject *)PyArray_FROM_OTF (
          bank_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
      if (!arr)
        return -1;
      if (PyArray_NDIM (arr) != 2)
        {
          PyErr_SetString (PyExc_ValueError,
                           "bank must be 2-D (num_phases, num_taps)");
          Py_DECREF (arr);
          return -1;
        }
      size_t np_ = (size_t)PyArray_DIM (arr, 0);
      size_t nt = (size_t)PyArray_DIM (arr, 1);
      self->handle = resamp_create_custom (
          np_, nt, (const float *)PyArray_DATA (arr), rate);
      Py_DECREF (arr);
      if (!self->handle)
        {
          PyErr_SetString (PyExc_MemoryError,
                           "resamp_create_custom returned NULL");
          return -1;
        }
    }
  else
    {
      self->handle = resamp_create (rate);
      if (!self->handle)
        {
          PyErr_SetString (PyExc_MemoryError, "resamp_create returned NULL");
          return -1;
        }
    }
  return 0;
}

static PyObject *
Resampler_reset (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  resamp_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
Resampler_get_rate (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (resamp_get_rate (self->handle));
}

static PyObject *
Resampler_set_rate (ResamplerObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double v = 0.0;
  if (!PyArg_ParseTuple (args, "d", &v))
    return NULL;
  resamp_set_rate (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
Resampler_execute (ResamplerObject *self, PyObject *args)
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
  double rate = resamp_get_rate (self->handle);
  size_t max_out = (size_t)ceil (num_in * (rate < 1.0 ? 1.0 : rate)) + 2;

  npy_intp dims[] = { (npy_intp)max_out };
  PyArrayObject *out_arr
      = (PyArrayObject *)PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  size_t n = resamp_execute (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), num_in,
      (float _Complex *)PyArray_DATA (out_arr), max_out);

  Py_DECREF (in_arr);

  PyObject *sliced
      = PySequence_GetSlice ((PyObject *)out_arr, 0, (Py_ssize_t)n);
  Py_DECREF (out_arr);
  return sliced;
}

static PyObject *
Resampler_execute_ctrl (ResamplerObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL, *ctrl_obj = NULL;
  if (!PyArg_ParseTuple (args, "OO", &in_obj, &ctrl_obj))
    return NULL;

  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
  if (!in_arr)
    return NULL;

  PyArrayObject *ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF (
      ctrl_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
  if (!ctrl_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  size_t num_in = (size_t)PyArray_SIZE (in_arr);
  if ((size_t)PyArray_SIZE (ctrl_arr) < num_in)
    {
      PyErr_SetString (PyExc_ValueError,
                       "ctrl must be at least as long as input");
      Py_DECREF (in_arr);
      Py_DECREF (ctrl_arr);
      return NULL;
    }

  double rate = resamp_get_rate (self->handle);
  size_t max_out = (size_t)ceil (num_in * (rate < 1.0 ? 1.0 : rate))
                   + (size_t)(num_in * 0.1) + 8;

  npy_intp dims[] = { (npy_intp)max_out };
  PyArrayObject *out_arr
      = (PyArrayObject *)PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      Py_DECREF (ctrl_arr);
      return NULL;
    }

  size_t n = resamp_execute_ctrl (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr),
      (const float _Complex *)PyArray_DATA (ctrl_arr), num_in,
      (float _Complex *)PyArray_DATA (out_arr), max_out);

  Py_DECREF (in_arr);
  Py_DECREF (ctrl_arr);

  PyObject *sliced
      = PySequence_GetSlice ((PyObject *)out_arr, 0, (Py_ssize_t)n);
  Py_DECREF (out_arr);
  return sliced;
}
static PyObject *
Resamp_getprop_rate (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (resamp_get_rate (self->handle));
}
static int
Resamp_setprop_rate (ResamplerObject *self, PyObject *value,
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
  resamp_set_rate (self->handle, v);
  return 0;
}
static PyObject *
Resamp_getprop_num_phases (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)resamp_get_num_phases (self->handle));
}
static PyObject *
Resamp_getprop_num_taps (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)resamp_get_num_taps (self->handle));
}

static PyGetSetDef Resampler_getset[]
    = { { "rate", (getter)Resamp_getprop_rate, (setter)Resamp_setprop_rate,
          NULL, NULL },
        { "num_phases", (getter)Resamp_getprop_num_phases, NULL, NULL, NULL },
        { "num_taps", (getter)Resamp_getprop_num_taps, NULL, NULL, NULL },
        { NULL } };

static PyObject *
Resampler_destroy (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      resamp_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Resamp_enter (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Resamp_exit (ResamplerObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      resamp_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef Resampler_methods[] = {
  { "reset", (PyCFunction)Resampler_reset, METH_NOARGS,
    "Reset state to post-create defaults." },
  { "get_rate", (PyCFunction)Resampler_get_rate, METH_NOARGS, "Get rate." },
  { "set_rate", (PyCFunction)Resampler_set_rate, METH_VARARGS, "Set rate." },
  { "execute", (PyCFunction)Resampler_execute, METH_VARARGS, "execute." },
  { "execute_ctrl", (PyCFunction)Resampler_execute_ctrl, METH_VARARGS,
    "execute_ctrl." },
  { "destroy", (PyCFunction)Resampler_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)Resamp_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Resamp_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject ResamplerType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.Resampler",
  .tp_basicsize = sizeof (ResamplerObject),
  .tp_dealloc = (destructor)Resampler_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Resamp type.",
  .tp_methods = Resampler_methods,
  .tp_getset = Resampler_getset,
  .tp_new = Resampler_new,
  .tp_init = (initproc)Resampler_init,
};
/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

/* ======================================================== */
/* HalfbandDecimatorObject — wraps hbdecim_state_t *                 */
/* ======================================================== */

typedef struct
{
  PyObject_HEAD hbdecim_state_t *handle;
} HalfbandDecimatorObject;

static void
HalfbandDecimator_dealloc (HalfbandDecimatorObject *self)
{
  if (self->handle)
    hbdecim_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimator_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  HalfbandDecimatorObject *self
      = (HalfbandDecimatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimator_init (HalfbandDecimatorObject *self, PyObject *args,
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
HalfbandDecimator_reset (HalfbandDecimatorObject *self,
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
HbDecim_rate (HalfbandDecimatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (hbdecim_get_rate (self->handle));
}

static PyObject *
HbDecim_num_taps (HalfbandDecimatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (hbdecim_get_num_taps (self->handle));
}

static PyGetSetDef HalfbandDecimator_getset[]
    = { { "rate", (getter)HbDecim_rate, NULL, "Output-to-input rate ratio.",
          NULL },
        { "num_taps", (getter)HbDecim_num_taps, NULL, "FIR branch tap count.",
          NULL },
        { NULL } };

static PyObject *
HalfbandDecimator_execute (HalfbandDecimatorObject *self, PyObject *args)
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
HbDecim_enter (HalfbandDecimatorObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HbDecim_exit (HalfbandDecimatorObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      hbdecim_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimator_methods[]
    = { { "reset", (PyCFunction)HalfbandDecimator_reset, METH_NOARGS, NULL },
        { "execute", (PyCFunction)HalfbandDecimator_execute, METH_VARARGS,
          NULL },
        { "__enter__", (PyCFunction)HbDecim_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)HbDecim_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject HalfbandDecimatorType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.HalfbandDecimator",
  .tp_basicsize = sizeof (HalfbandDecimatorObject),
  .tp_dealloc = (destructor)HalfbandDecimator_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Halfband 2:1 decimator for CF32 IQ samples.",
  .tp_methods = HalfbandDecimator_methods,
  .tp_getset = HalfbandDecimator_getset,
  .tp_new = HalfbandDecimator_new,
  .tp_init = (initproc)HalfbandDecimator_init,
};

/* ======================================================== */
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

static PyModuleDef resample_moduledef = {
  PyModuleDef_HEAD_INIT, .m_name = "resample", .m_doc = "Resample module.",
  .m_size = -1,          .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_resample (void)
{
  import_array ();
  if (PyType_Ready (&ResamplerType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorDpType) < 0)
    return NULL;
  if (PyType_Ready (&HalfbandDecimatorR2CType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&resample_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&ResamplerType);
  if (PyModule_AddObject (m, "Resampler", (PyObject *)&ResamplerType) < 0)
    {
      Py_DECREF (&ResamplerType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorType);
  if (PyModule_AddObject (m, "HalfbandDecimator",
                          (PyObject *)&HalfbandDecimatorType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorDpType);
  if (PyModule_AddObject (m, "HalfbandDecimatorDp",
                          (PyObject *)&HalfbandDecimatorDpType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorDpType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&HalfbandDecimatorR2CType);
  if (PyModule_AddObject (m, "HalfbandDecimatorR2C",
                          (PyObject *)&HalfbandDecimatorR2CType)
      < 0)
    {
      Py_DECREF (&HalfbandDecimatorR2CType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
