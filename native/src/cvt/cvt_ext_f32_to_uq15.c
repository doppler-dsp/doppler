/*
 * cvt_ext_f32_to_uq15.c — F32ToUQ15 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* F32ToUQ15Object — wraps f32_to_uq15_state_t *       */
/* ======================================================== */

#include "f32_to_uq15/f32_to_uq15_core.h"

typedef struct
{
  PyObject_HEAD f32_to_uq15_state_t *handle;
} F32ToUQ15Object;

static void
F32ToUQ15Obj_dealloc (F32ToUQ15Object *self)
{
  if (self->handle)
    f32_to_uq15_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
F32ToUQ15Obj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  F32ToUQ15Object *self = (F32ToUQ15Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
F32ToUQ15Obj_init (F32ToUQ15Object *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "scale", NULL };
  float        scale    = 32768.0f;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|f", kwlist, &scale))
    return -1;
  self->handle = f32_to_uq15_create (scale);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "f32_to_uq15_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
F32ToUQ15Obj_reset (F32ToUQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  f32_to_uq15_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
F32ToUQ15_step (F32ToUQ15Object *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  uint16_t y = f32_to_uq15_step (self->handle, x);
  return PyLong_FromUnsignedLong ((unsigned long)y);
}

static PyObject *
F32ToUQ15_steps (F32ToUQ15Object *self, PyObject *args, PyObject *kwds)
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
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT16
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT16, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      f32_to_uq15_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                         (uint16_t *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_UINT16);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  f32_to_uq15_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                     (uint16_t *)PyArray_DATA ((PyArrayObject *)out_arr),
                     (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
F32ToUQ15Obj_state_bytes (F32ToUQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (f32_to_uq15_state_bytes (self->handle));
}

static PyObject *
F32ToUQ15Obj_get_state (F32ToUQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = f32_to_uq15_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  f32_to_uq15_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
F32ToUQ15Obj_set_state (F32ToUQ15Object *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != f32_to_uq15_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (f32_to_uq15_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
F32ToUQ15_getprop_clipped (F32ToUQ15Object *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}

static PyGetSetDef F32ToUQ15_getset[]
    = { { "clipped", (getter)F32ToUQ15_getprop_clipped, NULL,
          "True if any sample has been saturated since the last reset().\n",
          NULL },
        { NULL } };

static PyObject *
F32ToUQ15Obj_destroy (F32ToUQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      f32_to_uq15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
F32ToUQ15Obj_enter (F32ToUQ15Object *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
F32ToUQ15Obj_exit (F32ToUQ15Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      f32_to_uq15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef F32ToUQ15Obj_methods[] = {
  { "reset", (PyCFunction)F32ToUQ15Obj_reset, METH_NOARGS,
    "Clear the sticky clip flag, starting a fresh saturation history." },
  { "step", (PyCFunction)F32ToUQ15_step, METH_VARARGS,
    "step(x) -> uint16_t\n"
    "\n"
    "Scale one float sample to an offset-binary UQ15 uint16 code.\n"
    "\n"
    "Computes round(x * scale), clamps to `[-32768, 32767]`, then adds the\n"
    "32768 offset-binary bias so the signed float domain maps onto the full\n"
    "unsigned uint16 range. Latches the sticky clipped flag if the scaled\n"
    "value saturated before clamping. Suits DAC and file formats that store\n"
    "only unsigned integers.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input sample, normally a normalised float in `[-1, +1]`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Offset-binary uint16 in `[0, 65535]`: -1.0 -> 0, 0.0 -> 32768, +1.0\n"
    "    -> 65535.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToUQ15\n"
    ">>> c = F32ToUQ15(scale=32768.0)\n"
    ">>> c.step(0.0)          # midscale maps to the offset-binary bias\n"
    "32768\n"
    ">>> c.step(-1.0)         # full-negative maps to code 0\n"
    "0\n"
    "\n" },
  { "steps", (PyCFunction)(void *)F32ToUQ15_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of float samples to UQ15 uint16.\n"
    "\n"
    "Applies step() to every element. The clipped flag is updated\n"
    "cumulatively across the block. Accepts an optional pre-allocated output\n"
    "array; allocates a fresh one when output is NULL.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint16]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToUQ15\n"
    ">>> import numpy as np\n"
    ">>> F32ToUQ15().steps(\n"
    "...     np.array([-1.0, 0.0, 0.999], dtype=np.float32)).tolist()\n"
    "[0, 32768, 65503]\n"
    "\n" },

  { "state_bytes", (PyCFunction)F32ToUQ15Obj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the F32ToUQ15 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)F32ToUQ15Obj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the F32ToUQ15 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)F32ToUQ15Obj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the F32ToUQ15 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)F32ToUQ15Obj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)F32ToUQ15Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a F32ToUQ15 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "F32ToUQ15\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)F32ToUQ15Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the F32ToUQ15.\n"
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

static PyTypeObject F32ToUQ15ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.F32ToUQ15",
  .tp_basicsize                           = sizeof (F32ToUQ15Object),
  .tp_dealloc                             = (destructor)F32ToUQ15Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create a f32_to_uq15 instance.\n",
  .tp_methods                             = F32ToUQ15Obj_methods,
  .tp_getset                              = F32ToUQ15_getset,
  .tp_new                                 = F32ToUQ15Obj_new,
  .tp_init                                = (initproc)F32ToUQ15Obj_init,
};
