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
          (setter)LockDet_setprop_up_thresh, "Up thresh.\n", NULL },
        { "down_thresh", (getter)LockDet_getprop_down_thresh,
          (setter)LockDet_setprop_down_thresh, "Down thresh.\n", NULL },
        { "n_up", (getter)LockDet_getprop_n_up, NULL, "N up.\n", NULL },
        { "n_down", (getter)LockDet_getprop_n_down, NULL, "N down.\n", NULL },
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

static PyMethodDef LockDetObj_methods[]
    = { { "step", (PyCFunction)LockDet_step, METH_VARARGS,
          "step(x) -> int\n"
          "\n"
          "Feed one look of the lock metric; return the current decision.\n"
          "\n"
          "    >>> from doppler import LockDet\n"
          "    >>> obj = LockDet(1.0, 1.0, 1, 1)\n"
          "    >>> obj.step(1.0)\n"
          "    0\n" },
        { "steps", (PyCFunction)(void *)LockDet_steps,
          METH_VARARGS | METH_KEYWORDS,
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
          "    >>> import numpy as np\n"
          "    >>> from doppler import LockDet\n"
          "    >>> obj = LockDet(1.0, 1.0, 1, 1)\n"
          "    >>> obj.configure(0.0, 0.0, 0, 0)\n" },
        { "reset", (PyCFunction)LockDetObj_reset, METH_NOARGS,
          "reset() -> None\n"
          "\n"
          "Drop the lock and clear the verify counter; keep the config.\n"
          "\n"
          "    >>> from doppler import LockDet\n"
          "    >>> obj = LockDet(1.0, 1.0, 1, 1)\n"
          "    >>> obj.reset()\n" },
        { "state_bytes", (PyCFunction)LockDetObj_state_bytes, METH_NOARGS,
          "Serialized state size in bytes." },
        { "get_state", (PyCFunction)LockDetObj_get_state, METH_NOARGS,
          "Serialize the engine's mutable state to bytes." },
        { "set_state", (PyCFunction)LockDetObj_set_state, METH_O,
          "Restore mutable state from a get_state() blob." },
        { "destroy", (PyCFunction)LockDetObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)LockDetObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)LockDetObj_exit, METH_VARARGS, NULL },
        { NULL } };

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
