/*
 * cvt_ext_i32_to_f32.c — I32ToF32 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* I32ToF32Object — wraps i32_to_f32_state_t *       */
/* ======================================================== */

#include "i32_to_f32/i32_to_f32_core.h"

typedef struct
{
  PyObject_HEAD i32_to_f32_state_t *handle;
} I32ToF32Object;

static void
I32ToF32Obj_dealloc (I32ToF32Object *self)
{
  if (self->handle)
    i32_to_f32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
I32ToF32Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  I32ToF32Object *self = (I32ToF32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
I32ToF32Obj_init (I32ToF32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 2147483648.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = i32_to_f32_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "i32_to_f32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
I32ToF32Obj_reset (I32ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  i32_to_f32_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
I32ToF32_step (I32ToF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long x_raw = 0L;
  if (!PyArg_ParseTuple (args, "l", &x_raw))
    return NULL;
  int32_t x = (int32_t)x_raw;
  float   y = i32_to_f32_step (self->handle, x);
  return PyFloat_FromDouble ((double)y);
}

static PyObject *
I32ToF32_steps (I32ToF32Object *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_INT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
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
      i32_to_f32_steps (self->handle, (const int32_t *)PyArray_DATA (in_arr),
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

  i32_to_f32_steps (self->handle, (const int32_t *)PyArray_DATA (in_arr),
                    (float *)PyArray_DATA ((PyArrayObject *)out_arr),
                    (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
I32ToF32Obj_destroy (I32ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      i32_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
I32ToF32Obj_enter (I32ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
I32ToF32Obj_exit (I32ToF32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      i32_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef I32ToF32Obj_methods[] = {
  { "reset", (PyCFunction)I32ToF32Obj_reset, METH_NOARGS,
    "No-op reset, provided only for lifecycle symmetry." },
  { "step", (PyCFunction)I32ToF32_step, METH_VARARGS,
    "step(x) -> float\n"
    "\n"
    "Convert one signed int32 sample to a normalised float via 1/scale.\n"
    "\n"
    "Returns (float)x * iscale, a single multiply on the hot path. At the\n"
    "default scale of 2^31 the full int32 range recovers `[-1.0, ~+1.0)`.\n"
    "Note that float32 carries only 23 mantissa bits, so int32 magnitudes\n"
    "beyond 2^24 are rounded to the nearest representable float.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Signed int32 code, normally a full-range fixed-point sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Normalised float, `x / scale`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import I32ToF32\n"
    ">>> c = I32ToF32(scale=2147483648.0)  # 2**31: int32 -> [-1, 1)\n"
    ">>> round(c.step(2**30), 4)            # quarter-scale code -> 0.5\n"
    "0.5\n"
    ">>> round(c.step(-2**31), 4)           # full-negative code -> -1.0\n"
    "-1.0\n"
    "\n" },
  { "steps", (PyCFunction)(void *)I32ToF32_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of int32 samples to float32.\n"
    "\n"
    "Applies step() to every element. Accepts an optional pre-allocated\n"
    "output array; allocates a fresh one when output is NULL.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.int32]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import I32ToF32\n"
    ">>> import numpy as np\n"
    ">>> I32ToF32().steps(\n"
    "...     np.array([0, 2**30, -2**31], dtype=np.int32)).tolist()\n"
    "[0.0, 0.5, -1.0]\n"
    "\n" },

  { "destroy", (PyCFunction)I32ToF32Obj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)I32ToF32Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a I32ToF32 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "I32ToF32\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)I32ToF32Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the I32ToF32.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
  { NULL }
};

static PyTypeObject I32ToF32ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.I32ToF32",
  .tp_basicsize                           = sizeof (I32ToF32Object),
  .tp_dealloc                             = (destructor)I32ToF32Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create a i32_to_f32 instance.\n",
  .tp_methods                             = I32ToF32Obj_methods,
  .tp_new                                 = I32ToF32Obj_new,
  .tp_init                                = (initproc)I32ToF32Obj_init,
};
