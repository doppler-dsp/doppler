/*
 * detection_ext_lockdet.c — LockDet type for the detection module.
 *
 * Included by detection_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only detection_ext.c is compiled.
 */
/* ======================================================== */
/* LockDetObject — wraps lockdet_state_t *       */
/* ======================================================== */

#include "lockdet/lockdet_core.h"

typedef struct
{
  PyObject_HEAD lockdet_state_t *handle;
} LockDetObject;

static void
LockDetObj_dealloc (LockDetObject *self)
{
  if (self->handle)
    lockdet_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
LockDetObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  LockDetObject *self = (LockDetObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
LockDetObj_init (LockDetObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "up_thresh", "down_thresh", "n_up", "n_down", NULL };
  double        up_thresh   = 1.0;
  double        down_thresh = 1.0;
  unsigned long n_up_raw    = 1;
  unsigned long n_down_raw  = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddkk", kwlist, &up_thresh,
                                    &down_thresh, &n_up_raw, &n_down_raw))
    return -1;
  uint32_t n_up   = (uint32_t)n_up_raw;
  uint32_t n_down = (uint32_t)n_down_raw;
  self->handle    = lockdet_create (up_thresh, down_thresh, n_up, n_down);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "lockdet_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
LockDet_step (LockDetObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double x;
  if (!PyArg_ParseTuple (args, "d", &x))
    return NULL;
  int y = lockdet_step (self->handle, x);
  return PyLong_FromLong ((long)y);
}

static PyObject *
LockDet_steps (LockDetObject *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_INT32
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_INT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      lockdet_steps (self->handle, (const double *)PyArray_DATA (in_arr),
                     (int *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_INT32);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  lockdet_steps (self->handle, (const double *)PyArray_DATA (in_arr),
                 (int *)PyArray_DATA ((PyArrayObject *)out_arr), (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
LockDetObj_configure (LockDetObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "up_thresh", "down_thresh", "n_up", "n_down", NULL };
  double        up_thresh   = 0.0;
  double        down_thresh = 0.0;
  unsigned long n_up_raw    = 0UL;
  unsigned long n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_up_raw, &n_down_raw))
    return NULL;
  uint32_t n_up   = (uint32_t)n_up_raw;
  uint32_t n_down = (uint32_t)n_down_raw;
  lockdet_configure (self->handle, up_thresh, down_thresh, n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
LockDetObj_reset (LockDetObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  lockdet_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
LockDetObj_state_bytes (LockDetObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (lockdet_state_bytes (self->handle));
}

static PyObject *
LockDetObj_get_state (LockDetObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = lockdet_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  lockdet_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
LockDetObj_set_state (LockDetObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != lockdet_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (lockdet_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
LockDet_getprop_up_thresh (LockDetObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->up_thresh);
}
static int
LockDet_setprop_up_thresh (LockDetObject *self, PyObject *value,
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
  self->handle->up_thresh = v;
  return 0;
}
static PyObject *
LockDet_getprop_down_thresh (LockDetObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->down_thresh);
}
static int
LockDet_setprop_down_thresh (LockDetObject *self, PyObject *value,
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
  self->handle->down_thresh = v;
  return 0;
}
static PyObject *
LockDet_getprop_n_up (LockDetObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->n_up);
}
static PyObject *
LockDet_getprop_n_down (LockDetObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->n_down);
}
static PyObject *
LockDet_getprop_cnt (LockDetObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->cnt);
}
static PyObject *
LockDet_getprop_locked (LockDetObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->locked));
}

static PyGetSetDef LockDet_getset[]
    = { { "up_thresh", (getter)LockDet_getprop_up_thresh,
          (setter)LockDet_setprop_up_thresh,
          "declare side: hit when metric > up_thresh.\n", NULL },
        { "down_thresh", (getter)LockDet_getprop_down_thresh,
          (setter)LockDet_setprop_down_thresh,
          "drop side: miss when metric < down_thresh.\n", NULL },
        { "n_up", (getter)LockDet_getprop_n_up, NULL,
          "consecutive hits required to declare (>= 1).\n", NULL },
        { "n_down", (getter)LockDet_getprop_n_down, NULL,
          "consecutive misses required to drop (>= 1).\n", NULL },
        { "cnt", (getter)LockDet_getprop_cnt, NULL,
          "Running consecutive-look verify counter: hits toward a declare "
          "while unlocked, misses toward a drop while locked.\n",
          NULL },
        { "locked", (getter)LockDet_getprop_locked, NULL,
          "Current decision (True = locked).\n", NULL },
        { NULL } };

static PyObject *
LockDetObj_destroy (LockDetObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      lockdet_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
LockDetObj_enter (LockDetObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
LockDetObj_exit (LockDetObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      lockdet_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef LockDetObj_methods[] = {
  { "step", (PyCFunction)LockDet_step, METH_VARARGS,
    "step(x) -> int\n"
    "\n"
    "Feed one look of the lock metric; return the current decision.\n"
    "\n"
    "Unlocked: a hit (`x > up_thresh`) advances the verify run and the\n"
    "n_up-th consecutive hit declares lock; any miss resets the run. Locked:\n"
    "a miss (`x < down_thresh`) advances the run and the n_down-th\n"
    "consecutive miss drops the lock; any hit (`x >= down_thresh`) resets "
    "it.\n"
    "A metric inside the `[down_thresh, up_thresh]` band is sticky — it\n"
    "neither advances a declare nor a drop.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Lock metric for this look.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Decision after this look (1 = locked, 0 = not).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.detection import LockDet\n"
    ">>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)\n"
    ">>> [d.step(2.0), d.step(2.0)]     # declared on the 2nd straight hit\n"
    "[0, 1]\n"
    ">>> d.step(1.3)                    # in the hysteresis band: stays up\n"
    "1\n"
    ">>> [d.step(1.0), d.step(1.0), d.step(1.0)]  # 3rd straight miss drops\n"
    "[1, 1, 0]\n"
    "\n" },
  { "steps", (PyCFunction)(void *)LockDet_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Run a block of lock-metric looks through the detector.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import LockDet\n"
    "    >>> obj = LockDet(1.0, 1.0, 1, 1)\n"
    "    >>> y = obj.steps(np.zeros(4, dtype=np.float64))\n"
    "    >>> y.shape\n"
    "    (4,)\n"
    "    >>> y.dtype\n"
    "    dtype('int32')\n" },

  { "configure", (PyCFunction)(void *)LockDetObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune thresholds and verify counts; a live lock survives, the "
    "in-flight verify run restarts under the new config.\n"
    "\n"
    "The current locked flag survives (a live lock is not dropped by a\n"
    "re-tune); the in-flight verify counter is cleared so the next run is\n"
    "counted entirely under the new config.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    Declare threshold (hit when metric > up_thresh).\n"
    "down_thresh : float\n"
    "    Drop threshold (miss when metric < down_thresh).\n"
    "n_up : int\n"
    "    Consecutive hits to declare; clamped to >= 1.\n"
    "n_down : int\n"
    "    Consecutive misses to drop; clamped to >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.detection import LockDet\n"
    ">>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)\n"
    ">>> d.configure(up_thresh=3.0, down_thresh=2.5, n_up=1, n_down=1)\n"
    ">>> d.up_thresh          # thresholds re-tuned in place\n"
    "3.0\n"
    ">>> d.step(4.0)          # a single hit now declares (n_up=1)\n"
    "1\n" },
  { "reset", (PyCFunction)LockDetObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Drop the lock and clear the verify counter; keep the config.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.detection import LockDet\n"
    ">>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)\n"
    ">>> d.step(2.0)          # one hit declares lock (n_up=1)\n"
    "1\n"
    ">>> d.reset()            # drop it and clear the verify run\n"
    ">>> d.locked\n"
    "False\n" },
  { "state_bytes", (PyCFunction)LockDetObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the LockDetObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)LockDetObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the LockDetObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)LockDetObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the LockDetObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)LockDetObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)LockDetObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Lockdet be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Lockdet\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)LockDetObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Lockdet.\n"
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

static PyTypeObject LockDetObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "detection.LockDet",
  .tp_basicsize                           = sizeof (LockDetObject),
  .tp_dealloc                             = (destructor)LockDetObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "LockDet type.\n",
  .tp_methods                             = LockDetObj_methods,
  .tp_getset                              = LockDet_getset,
  .tp_new                                 = LockDetObj_new,
  .tp_init                                = (initproc)LockDetObj_init,
};
