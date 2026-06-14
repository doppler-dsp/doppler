/*
 * cvt_ext_i16u64_to_f32.c — I16U64ToF32 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* I16U64ToF32Object — wraps i16u64_to_f32_state_t *       */
/* ======================================================== */

#include "i16u64_to_f32/i16u64_to_f32_core.h"

typedef struct
{
  PyObject_HEAD i16u64_to_f32_state_t *handle;
} I16U64ToF32Object;

static void
I16U64ToF32Obj_dealloc (I16U64ToF32Object *self)
{
  if (self->handle)
    i16u64_to_f32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
I16U64ToF32Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  I16U64ToF32Object *self = (I16U64ToF32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
I16U64ToF32Obj_init (I16U64ToF32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 32768.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = i16u64_to_f32_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "i16u64_to_f32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
I16U64ToF32Obj_reset (I16U64ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  i16u64_to_f32_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
I16U64ToF32_step (I16U64ToF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  unsigned long long x_raw = 0ULL;
  if (!PyArg_ParseTuple (args, "K", &x_raw))
    return NULL;
  uint64_t x = (uint64_t)x_raw;
  float    y = i16u64_to_f32_step (self->handle, x);
  return PyFloat_FromDouble ((double)y);
}

static PyObject *
I16U64ToF32_steps (I16U64ToF32Object *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_UINT64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      i16u64_to_f32_steps (self->handle,
                           (const uint64_t *)PyArray_DATA (in_arr),
                           (float *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_FLOAT);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  i16u64_to_f32_steps (self->handle, (const uint64_t *)PyArray_DATA (in_arr),
                       (float *)PyArray_DATA ((PyArrayObject *)out_arr),
                       (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
I16U64ToF32Obj_destroy (I16U64ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      i16u64_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
I16U64ToF32Obj_enter (I16U64ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
I16U64ToF32Obj_exit (I16U64ToF32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      i16u64_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef I16U64ToF32Obj_methods[]
    = { { "reset", (PyCFunction)I16U64ToF32Obj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },
        { "step", (PyCFunction)I16U64ToF32_step, METH_VARARGS,
          "step(x) -> float\n"
          "\n"
          "Process one input sample.\n"
          "\n"
          "    >>> from doppler import I16U64ToF32\n"
          "    >>> obj = I16U64ToF32(32768.0)\n"
          "    >>> obj.step(1)\n"
          "    0.0\n" },
        { "steps", (PyCFunction)(void *)I16U64ToF32_steps,
          METH_VARARGS | METH_KEYWORDS,
          "steps(x[, out]) -> ndarray\n"
          "\n"
          "Process a block of Q15-in-uint64 samples to float32.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import I16U64ToF32\n"
          "    >>> obj = I16U64ToF32(32768.0)\n"
          "    >>> y = obj.steps(np.zeros(4, dtype=np.uint64))\n"
          "    >>> y.shape\n"
          "    (4,)\n"
          "    >>> y.dtype\n"
          "    dtype('float32')\n" },

        { "destroy", (PyCFunction)I16U64ToF32Obj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)I16U64ToF32Obj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)I16U64ToF32Obj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject I16U64ToF32ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.I16U64ToF32",
  .tp_basicsize                           = sizeof (I16U64ToF32Object),
  .tp_dealloc                             = (destructor)I16U64ToF32Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "I16U64ToF32 type.\n",
  .tp_methods                             = I16U64ToF32Obj_methods,
  .tp_new                                 = I16U64ToF32Obj_new,
  .tp_init                                = (initproc)I16U64ToF32Obj_init,
};
