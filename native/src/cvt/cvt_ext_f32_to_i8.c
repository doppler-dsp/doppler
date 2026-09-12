/*
 * cvt_ext_f32_to_i8.c — F32ToI8 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* F32ToI8Object — wraps f32_to_i8_state_t *       */
/* ======================================================== */

#include "f32_to_i8/f32_to_i8_core.h"

typedef struct
{
  PyObject_HEAD f32_to_i8_state_t *handle;
} F32ToI8Object;

static void
F32ToI8Obj_dealloc (F32ToI8Object *self)
{
  if (self->handle)
    f32_to_i8_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
F32ToI8Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32ToI8Object *self = (F32ToI8Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
F32ToI8Obj_init (F32ToI8Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 128.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = f32_to_i8_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "f32_to_i8_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
F32ToI8Obj_reset (F32ToI8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  f32_to_i8_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
F32ToI8_step (F32ToI8Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  int8_t y = f32_to_i8_step (self->handle, x);
  return PyLong_FromLong ((long)y);
}

static PyObject *
F32ToI8_steps (F32ToI8Object *self, PyObject *args, PyObject *kwds)
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
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_INT8
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
          out_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      f32_to_i8_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                       (int8_t *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_INT8);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  f32_to_i8_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                   (int8_t *)PyArray_DATA ((PyArrayObject *)out_arr),
                   (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
F32ToI8Obj_state_bytes (F32ToI8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (f32_to_i8_state_bytes (self->handle));
}

static PyObject *
F32ToI8Obj_get_state (F32ToI8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = f32_to_i8_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  f32_to_i8_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
F32ToI8Obj_set_state (F32ToI8Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != f32_to_i8_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (f32_to_i8_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
F32ToI8_getprop_clipped (F32ToI8Object *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}

static PyGetSetDef F32ToI8_getset[]
    = { { "clipped", (getter)F32ToI8_getprop_clipped, NULL,
          "True if any sample has been saturated since the last reset().\n",
          NULL },
        { NULL } };

static PyObject *
F32ToI8Obj_destroy (F32ToI8Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      f32_to_i8_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32ToI8Obj_enter (F32ToI8Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
F32ToI8Obj_exit (F32ToI8Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      f32_to_i8_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef F32ToI8Obj_methods[] = {
  { "reset", (PyCFunction)F32ToI8Obj_reset, METH_NOARGS,
    "Clear the sticky clip flag, starting a fresh saturation history.\n"
    "\n"
    "Zeroes clipped so a subsequent clipped query reflects only samples seen\n"
    "after this call; the immutable scale is preserved. Call it at a buffer\n"
    "or segment boundary so a saturation on one block does not leak into the\n"
    "next.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToI8\n"
    ">>> c = F32ToI8()\n"
    ">>> c.step(9.0)          # out of range -> saturates, latches clipped\n"
    "127\n"
    ">>> c.reset()            # forget the clip history\n"
    ">>> c.clipped\n"
    "False\n" },
  { "step", (PyCFunction)F32ToI8_step, METH_VARARGS,
    "step(x) -> int8_t\n"
    "\n"
    "Scale one float sample by scale, round, and saturate to int8.\n"
    "\n"
    "Computes round(x * scale), clamps to the int8 range `[-128, 127]`, and\n"
    "latches the sticky clipped flag if the scaled value fell outside that\n"
    "range before clamping. At the default scale of 128 a normalised `[-1,\n"
    "+1]` input maps to the full 8-bit code range.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input sample, normally a normalised float in `[-1, +1]`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Saturated int8 code in `[-128, 127]`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToI8\n"
    ">>> c = F32ToI8(scale=128.0)    # normalised float -> full-scale int8\n"
    ">>> c.step(0.5)                 # 0.5 * 128\n"
    "64\n"
    ">>> c.step(2.0)                 # beyond +1.0 -> saturates to max\n"
    "127\n"
    ">>> c.clipped                   # sticky flag latched by the clip\n"
    "True\n"
    "\n" },
  { "steps", (PyCFunction)(void *)F32ToI8_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of float samples to int8.\n"
    "\n"
    "Applies step() to every element. The clipped flag is updated\n"
    "cumulatively across the block — a single saturating sample raises it\n"
    "for the entire call. Accepts an optional pre-allocated output array;\n"
    "allocates a fresh one when output is NULL.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.int8]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToI8\n"
    ">>> import numpy as np\n"
    ">>> x = np.array([0.0, 0.5, -1.0, 0.99], dtype=np.float32)\n"
    ">>> F32ToI8().steps(x).tolist()   # scale=128 -> full-scale int8\n"
    "[0, 64, -128, 127]\n"
    "\n" },

  { "state_bytes", (PyCFunction)F32ToI8Obj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the F32ToI8 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)F32ToI8Obj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the F32ToI8 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)F32ToI8Obj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the F32ToI8 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)F32ToI8Obj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)F32ToI8Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a F32ToI8 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "F32ToI8\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)F32ToI8Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the F32ToI8.\n"
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

static PyTypeObject F32ToI8ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.F32ToI8",
  .tp_basicsize                           = sizeof (F32ToI8Object),
  .tp_dealloc                             = (destructor)F32ToI8Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a f32_to_i8 instance.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "scale : float, default 128.0\n"
    "    Multiply factor applied before rounding and saturation (default:\n"
    "    128.0f). Use 128.0 to convert a normalised `[-1, +1]` signal to the\n"
    "    full 8-bit range.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler.cvt import F32ToI8\n"
    ">>> obj = F32ToI8(scale=128.0)\n",
  .tp_methods = F32ToI8Obj_methods,
  .tp_getset  = F32ToI8_getset,
  .tp_new     = F32ToI8Obj_new,
  .tp_init    = (initproc)F32ToI8Obj_init,
};
