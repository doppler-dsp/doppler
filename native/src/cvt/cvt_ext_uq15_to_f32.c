/*
 * cvt_ext_uq15_to_f32.c — UQ15ToF32 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* UQ15ToF32Object — wraps uq15_to_f32_state_t *       */
/* ======================================================== */

#include "uq15_to_f32/uq15_to_f32_core.h"

typedef struct
{
  PyObject_HEAD uq15_to_f32_state_t *handle;
} UQ15ToF32Object;

static void
UQ15ToF32Obj_dealloc (UQ15ToF32Object *self)
{
  if (self->handle)
    uq15_to_f32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
UQ15ToF32Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  UQ15ToF32Object *self = (UQ15ToF32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
UQ15ToF32Obj_init (UQ15ToF32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 32768.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = uq15_to_f32_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "uq15_to_f32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
UQ15ToF32Obj_reset (UQ15ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  uq15_to_f32_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
UQ15ToF32_step (UQ15ToF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  unsigned int x_raw = 0U;
  if (!PyArg_ParseTuple (args, "I", &x_raw))
    return NULL;
  uint16_t x = (uint16_t)x_raw;
  float    y = uq15_to_f32_step (self->handle, x);
  return PyFloat_FromDouble ((double)y);
}

static PyObject *
UQ15ToF32_steps (UQ15ToF32Object *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_UINT16, NPY_ARRAY_C_CONTIGUOUS);
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
      uq15_to_f32_steps (self->handle, (const uint16_t *)PyArray_DATA (in_arr),
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

  uq15_to_f32_steps (self->handle, (const uint16_t *)PyArray_DATA (in_arr),
                     (float *)PyArray_DATA ((PyArrayObject *)out_arr),
                     (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
UQ15ToF32Obj_destroy (UQ15ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      uq15_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
UQ15ToF32Obj_enter (UQ15ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
UQ15ToF32Obj_exit (UQ15ToF32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      uq15_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef UQ15ToF32Obj_methods[] = {
  { "reset", (PyCFunction)UQ15ToF32Obj_reset, METH_NOARGS,
    "No-op reset, provided only for lifecycle symmetry." },
  { "step", (PyCFunction)UQ15ToF32_step, METH_VARARGS,
    "step(x) -> float\n"
    "\n"
    "Decode one offset-binary UQ15 uint16 code to a normalised float.\n"
    "\n"
    "Computes ((int32_t)x - 32768) * iscale — removes the 32768 "
    "offset-binary\n"
    "bias and applies 1/scale. The int32_t cast prevents signed overflow "
    "when\n"
    "x is 0 (which yields -32768 after bias removal). Exact inverse of\n"
    "F32ToUQ15 at the same scale.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    UQ15 offset-binary uint16 code: 0 -> -1.0, 32768 -> 0.0, 65535 ->\n"
    "    +32767/32768.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Normalised float in `[-1.0, ~+1.0)`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import UQ15ToF32\n"
    ">>> c = UQ15ToF32(scale=32768.0)\n"
    ">>> round(c.step(32768), 4)   # midscale code -> 0.0\n"
    "0.0\n"
    ">>> round(c.step(0), 4)       # zero code -> -1.0\n"
    "-1.0\n"
    "\n" },
  { "steps", (PyCFunction)(void *)UQ15ToF32_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of UQ15 samples to float32.\n"
    "\n"
    "Applies step() to every element. State is not mutated (no clipped "
    "flag).\n"
    "Accepts an optional pre-allocated output array; allocates a fresh one\n"
    "when output is NULL.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.uint16]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import UQ15ToF32\n"
    ">>> import numpy as np\n"
    ">>> UQ15ToF32().steps(np.array([0, 32768], dtype=np.uint16)).tolist()\n"
    "[-1.0, 0.0]\n"
    "\n" },

  { "destroy", (PyCFunction)UQ15ToF32Obj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)UQ15ToF32Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a UQ15ToF32 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "UQ15ToF32\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)UQ15ToF32Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the UQ15ToF32.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never\n"
    "suppresses one.\n"
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

static PyTypeObject UQ15ToF32ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.UQ15ToF32",
  .tp_basicsize                           = sizeof (UQ15ToF32Object),
  .tp_dealloc                             = (destructor)UQ15ToF32Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create a uq15_to_f32 instance.\n",
  .tp_methods                             = UQ15ToF32Obj_methods,
  .tp_new                                 = UQ15ToF32Obj_new,
  .tp_init                                = (initproc)UQ15ToF32Obj_init,
};
