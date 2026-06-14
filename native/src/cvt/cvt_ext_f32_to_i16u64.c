/*
 * cvt_ext_f32_to_i16u64.c — F32ToI16U64 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* F32ToI16U64Object — wraps f32_to_i16u64_state_t *       */
/* ======================================================== */

#include "f32_to_i16u64/f32_to_i16u64_core.h"

typedef struct
{
  PyObject_HEAD f32_to_i16u64_state_t *handle;
} F32ToI16U64Object;

static void
F32ToI16U64Obj_dealloc (F32ToI16U64Object *self)
{
  if (self->handle)
    f32_to_i16u64_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
F32ToI16U64Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32ToI16U64Object *self = (F32ToI16U64Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
F32ToI16U64Obj_init (F32ToI16U64Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 32768.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = f32_to_i16u64_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "f32_to_i16u64_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
F32ToI16U64Obj_reset (F32ToI16U64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  f32_to_i16u64_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
F32ToI16U64_step (F32ToI16U64Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  uint64_t y = f32_to_i16u64_step (self->handle, x);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
F32ToI16U64_steps (F32ToI16U64Object *self, PyObject *args, PyObject *kwds)
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
          out_obj, NPY_UINT64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      f32_to_i16u64_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                           (uint64_t *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_UINT64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  f32_to_i16u64_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                       (uint64_t *)PyArray_DATA ((PyArrayObject *)out_arr),
                       (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
F32ToI16U64_getprop_clipped (F32ToI16U64Object *self,
                             void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}

static PyGetSetDef F32ToI16U64_getset[]
    = { { "clipped", (getter)F32ToI16U64_getprop_clipped, NULL,
          "True if any sample has been saturated since the last reset().\n",
          NULL },
        { NULL } };

static PyObject *
F32ToI16U64Obj_destroy (F32ToI16U64Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      f32_to_i16u64_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32ToI16U64Obj_enter (F32ToI16U64Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
F32ToI16U64Obj_exit (F32ToI16U64Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      f32_to_i16u64_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef F32ToI16U64Obj_methods[]
    = { { "reset", (PyCFunction)F32ToI16U64Obj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },
        { "step", (PyCFunction)F32ToI16U64_step, METH_VARARGS,
          "step(x) -> uint64_t\n"
          "\n"
          "Process one input sample.\n"
          "\n"
          "    >>> from doppler import F32ToI16U64\n"
          "    >>> obj = F32ToI16U64(32768.0)\n"
          "    >>> obj.step(1.0)\n"
          "    0\n" },
        { "steps", (PyCFunction)(void *)F32ToI16U64_steps,
          METH_VARARGS | METH_KEYWORDS,
          "steps(x[, out]) -> ndarray\n"
          "\n"
          "Process a block of float samples to Q15-in-uint64.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import F32ToI16U64\n"
          "    >>> obj = F32ToI16U64(32768.0)\n"
          "    >>> y = obj.steps(np.zeros(4, dtype=np.float32))\n"
          "    >>> y.shape\n"
          "    (4,)\n"
          "    >>> y.dtype\n"
          "    dtype('uint64')\n" },

        { "destroy", (PyCFunction)F32ToI16U64Obj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)F32ToI16U64Obj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)F32ToI16U64Obj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject F32ToI16U64ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.F32ToI16U64",
  .tp_basicsize                           = sizeof (F32ToI16U64Object),
  .tp_dealloc                             = (destructor)F32ToI16U64Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "F32ToI16U64 type.\n",
  .tp_methods                             = F32ToI16U64Obj_methods,
  .tp_getset                              = F32ToI16U64_getset,
  .tp_new                                 = F32ToI16U64Obj_new,
  .tp_init                                = (initproc)F32ToI16U64Obj_init,
};
