/*
 * cvt_ext_f32_to_i16u32.c — F32ToI16U32 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* F32ToI16U32Object — wraps f32_to_i16u32_state_t *       */
/* ======================================================== */

#include "f32_to_i16u32/f32_to_i16u32_core.h"

typedef struct
{
  PyObject_HEAD f32_to_i16u32_state_t *handle;
} F32ToI16U32Object;

static void
F32ToI16U32Obj_dealloc (F32ToI16U32Object *self)
{
  if (self->handle)
    f32_to_i16u32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
F32ToI16U32Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32ToI16U32Object *self = (F32ToI16U32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
F32ToI16U32Obj_init (F32ToI16U32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 32768.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = f32_to_i16u32_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "f32_to_i16u32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
F32ToI16U32Obj_reset (F32ToI16U32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  f32_to_i16u32_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
F32ToI16U32_step (F32ToI16U32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  uint32_t y = f32_to_i16u32_step (self->handle, x);
  return PyLong_FromUnsignedLong ((unsigned long)y);
}

static PyObject *
F32ToI16U32_steps (F32ToI16U32Object *self, PyObject *args, PyObject *kwds)
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
          out_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      f32_to_i16u32_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                           (uint32_t *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_UINT32);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  f32_to_i16u32_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                       (uint32_t *)PyArray_DATA ((PyArrayObject *)out_arr),
                       (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
F32ToI16U32_getprop_clipped (F32ToI16U32Object *self,
                             void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}

static PyGetSetDef F32ToI16U32_getset[]
    = { { "clipped", (getter)F32ToI16U32_getprop_clipped, NULL,
          "True if any sample has been saturated since the last reset().\n",
          NULL },
        { NULL } };

static PyObject *
F32ToI16U32Obj_destroy (F32ToI16U32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      f32_to_i16u32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32ToI16U32Obj_enter (F32ToI16U32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
F32ToI16U32Obj_exit (F32ToI16U32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      f32_to_i16u32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32ToI16U32Obj_state_bytes (F32ToI16U32Object *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (f32_to_i16u32_state_bytes (self->handle));
}

static PyObject *
F32ToI16U32Obj_get_state (F32ToI16U32Object *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = f32_to_i16u32_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  f32_to_i16u32_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
F32ToI16U32Obj_set_state (F32ToI16U32Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg)
      != f32_to_i16u32_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (f32_to_i16u32_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef F32ToI16U32Obj_methods[]
    = { { "reset", (PyCFunction)F32ToI16U32Obj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },
        { "step", (PyCFunction)F32ToI16U32_step, METH_VARARGS,
          "step(x) -> uint32_t\n"
          "\n"
          "Process one input sample.\n"
          "\n"
          "    >>> from doppler import F32ToI16U32\n"
          "    >>> obj = F32ToI16U32(32768.0)\n"
          "    >>> obj.step(1.0)\n"
          "    0\n" },
        { "steps", (PyCFunction)(void *)F32ToI16U32_steps,
          METH_VARARGS | METH_KEYWORDS,
          "steps(x[, out]) -> ndarray\n"
          "\n"
          "Process a block of float samples to Q15-in-uint32.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import F32ToI16U32\n"
          "    >>> obj = F32ToI16U32(32768.0)\n"
          "    >>> y = obj.steps(np.zeros(4, dtype=np.float32))\n"
          "    >>> y.shape\n"
          "    (4,)\n"
          "    >>> y.dtype\n"
          "    dtype('uint32')\n" },

        { "destroy", (PyCFunction)F32ToI16U32Obj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)F32ToI16U32Obj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)F32ToI16U32Obj_exit, METH_VARARGS, NULL },
        { "state_bytes", (PyCFunction)F32ToI16U32Obj_state_bytes, METH_NOARGS,
          "Serialized state size in bytes." },
        { "get_state", (PyCFunction)F32ToI16U32Obj_get_state, METH_NOARGS,
          "Serialize the engine's mutable state to bytes." },
        { "set_state", (PyCFunction)F32ToI16U32Obj_set_state, METH_O,
          "Restore mutable state from a get_state() blob." },
        { NULL } };

static PyTypeObject F32ToI16U32ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.F32ToI16U32",
  .tp_basicsize                           = sizeof (F32ToI16U32Object),
  .tp_dealloc                             = (destructor)F32ToI16U32Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "F32ToI16U32 type.\n",
  .tp_methods                             = F32ToI16U32Obj_methods,
  .tp_getset                              = F32ToI16U32_getset,
  .tp_new                                 = F32ToI16U32Obj_new,
  .tp_init                                = (initproc)F32ToI16U32Obj_init,
};
