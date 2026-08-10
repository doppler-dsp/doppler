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
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT32
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
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

static PyMethodDef F32ToI16U32Obj_methods[] = {
  { "reset", (PyCFunction)F32ToI16U32Obj_reset, METH_NOARGS,
    "Clear the sticky clip flag, starting a fresh saturation history." },
  { "step", (PyCFunction)F32ToI16U32_step, METH_VARARGS,
    "step(x) -> uint32_t\n"
    "\n"
    "Scale one float sample to a saturated Q15 code packed in a uint32.\n"
    "\n"
    "Computes round(x * scale), saturates to `[-32768, 32767]`, then\n"
    "zero-extends the 16-bit two's-complement pattern into the lower 16 bits\n"
    "of a uint32 (upper 16 bits are always zero — headroom for the CIC\n"
    "integrator cascade). Latches the sticky clipped flag on saturation.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input sample, normally a normalised float in `[-1, +1]`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Q15 code in the low 16 bits of a uint32; e.g. -32768 -> 0x8000.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToI16U32\n"
    ">>> c = F32ToI16U32(scale=32768.0)\n"
    ">>> c.step(0.5)              # 0.5 -> Q15 16384, upper 16 bits zero\n"
    "16384\n"
    ">>> hex(c.step(-1.0))        # -32768 as an unsigned low-16 pattern\n"
    "'0x8000'\n"
    "\n" },
  { "steps", (PyCFunction)(void *)F32ToI16U32_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of float samples to Q15-in-uint32.\n"
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
    "NDArray[np.uint32]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import F32ToI16U32\n"
    ">>> import numpy as np\n"
    ">>> F32ToI16U32().steps(\n"
    "...     np.array([0.0, 0.5], dtype=np.float32)).tolist()\n"
    "[0, 16384]\n"
    "\n" },

  { "state_bytes", (PyCFunction)F32ToI16U32Obj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the F32ToI16U32 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)F32ToI16U32Obj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the F32ToI16U32 has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)F32ToI16U32Obj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the F32ToI16U32 has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)F32ToI16U32Obj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)F32ToI16U32Obj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a F32ToI16U32 be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "F32ToI16U32\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)F32ToI16U32Obj_exit, METH_VARARGS,
    "Exit a context manager, releasing the F32ToI16U32.\n"
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

static PyTypeObject F32ToI16U32ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.F32ToI16U32",
  .tp_basicsize                           = sizeof (F32ToI16U32Object),
  .tp_dealloc                             = (destructor)F32ToI16U32Obj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a f32_to_i16u32 instance.\n",
  .tp_methods = F32ToI16U32Obj_methods,
  .tp_getset  = F32ToI16U32_getset,
  .tp_new     = F32ToI16U32Obj_new,
  .tp_init    = (initproc)F32ToI16U32Obj_init,
};
