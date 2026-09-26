/*
 * cvt_ext_u8_to_f32.c — U8ToF32 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* U8ToF32Object — wraps dp_u8_to_f32_state_t *       */
/* ======================================================== */

#include "doppler/u8_to_f32/u8_to_f32_core.h"

typedef struct
{
  PyObject_HEAD dp_u8_to_f32_state_t *handle;
} U8ToF32Object;

static void
U8ToF32Obj_dealloc (U8ToF32Object *self)
{
  if (self->handle)
    dp_u8_to_f32_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
U8ToF32Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  U8ToF32Object *self = (U8ToF32Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
U8ToF32Obj_init (U8ToF32Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "mode", NULL };
  const char  *mode_str = "shift";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|s", kwlist, &mode_str))
    return -1;
  int mode = 0;
  if (strcmp (mode_str, "shift") == 0)
    mode = 0;
  else if (strcmp (mode_str, "midpoint") == 0)
    mode = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "mode must be one of \"shift\", \"midpoint\", got '%s'",
                    mode_str);
      return -1;
    }
  self->handle = dp_u8_to_f32_create (mode);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_u8_to_f32_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
U8ToF32Obj_reset (U8ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_u8_to_f32_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
U8ToF32_step (U8ToF32Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  unsigned int x_raw = 0U;
  if (!PyArg_ParseTuple (args, "I", &x_raw))
    return NULL;
  uint8_t x = (uint8_t)x_raw;
  float   y = dp_u8_to_f32_step (self->handle, x);
  return PyFloat_FromDouble ((double)y);
}

static PyObject *
U8ToF32_steps (U8ToF32Object *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
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
      dp_u8_to_f32_steps (self->handle, (const uint8_t *)PyArray_DATA (in_arr),
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

  dp_u8_to_f32_steps (self->handle, (const uint8_t *)PyArray_DATA (in_arr),
                      (float *)PyArray_DATA ((PyArrayObject *)out_arr),
                      (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
U8ToF32Obj_destroy (U8ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_u8_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
U8ToF32Obj_enter (U8ToF32Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
U8ToF32Obj_exit (U8ToF32Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_u8_to_f32_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef U8ToF32Obj_methods[] = {
  { "reset", (PyCFunction)U8ToF32Obj_reset, METH_NOARGS,
    "No-op reset, provided only for lifecycle symmetry.\n"
    "\n"
    "The mode and its reciprocal are fixed at construction and nothing else\n"
    "is held, so there is nothing to clear; the method exists so every\n"
    "converter in the module presents the same create / step / reset /\n"
    "destroy lifecycle.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import U8ToF32\n"
    ">>> c = U8ToF32()\n"
    ">>> c.reset()          # stateless converter -> reset is a no-op\n"
    ">>> c.step(0)\n"
    "-1.0\n" },
  { "step", (PyCFunction)U8ToF32_step, METH_VARARGS,
    "step(x) -> float\n"
    "\n"
    "Convert one offset-binary code to a normalised float.\n"
    "\n"
    "Dispatches on the mode chosen at construction. For a block, steps()\n"
    "resolves the mode once and runs a branch-free loop instead.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : int\n"
    "    Offset-binary code in `[0, 255]`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    The mapped sample (see the table at the top of this file).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import U8ToF32\n"
    ">>> c = U8ToF32()             # mode=\"shift\": (x - 128) / 128, exact\n"
    ">>> c.step(0), c.step(128), c.step(192)\n"
    "(-1.0, 0.0, 0.5)\n"
    ">>> m = U8ToF32(mode=\"midpoint\")\n"
    ">>> m.step(0), m.step(255)    # symmetric: both rails reach full scale\n"
    "(-1.0, 1.0)\n"
    "\n" },
  { "steps", (PyCFunction)(void *)U8ToF32_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Convert a block of offset-binary codes to float32.\n"
    "\n"
    "The mode is resolved once for the block, then one branch-free loop\n"
    "runs, so the per-sample work is only the mapping itself. Feed it the\n"
    "flat interleaved I/Q buffer; the output is then complex samples in\n"
    "`float _Complex` layout.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.uint8]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import U8ToF32\n"
    ">>> import numpy as np\n"
    ">>> cu8 = np.array([128, 0, 192, 64], dtype=np.uint8)  # 2 I/Q pairs\n"
    ">>> U8ToF32().steps(cu8).view(np.complex64).tolist()\n"
    "[-1j, (0.5-0.5j)]\n"
    "\n" },

  { "destroy", (PyCFunction)U8ToF32Obj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)U8ToF32Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a U8ToF32 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "U8ToF32\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)U8ToF32Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the U8ToF32.\n"
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

static PyTypeObject U8ToF32ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.cvt.U8ToF32",
  .tp_basicsize                           = sizeof (U8ToF32Object),
  .tp_dealloc                             = (destructor)U8ToF32Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "U8ToF32 type.\n",
  .tp_methods                             = U8ToF32Obj_methods,
  .tp_new                                 = U8ToF32Obj_new,
  .tp_init                                = (initproc)U8ToF32Obj_init,
};
