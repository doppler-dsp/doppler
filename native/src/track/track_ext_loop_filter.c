/*
 * track_ext_loop_filter.c — LoopFilter type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* LoopFilterObject — wraps loop_filter_state_t *       */
/* ======================================================== */

#include "loop_filter/loop_filter_core.h"

typedef struct
{
  PyObject_HEAD loop_filter_state_t *handle;
} LoopFilterObject;

static void
LoopFilterObj_dealloc (LoopFilterObject *self)
{
  if (self->handle)
    loop_filter_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
LoopFilterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  LoopFilterObject *self = (LoopFilterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
LoopFilterObj_init (LoopFilterObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "bn", "zeta", "t", NULL };
  double       bn       = 0.01;
  double       zeta     = 0.707;
  double       t        = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddd", kwlist, &bn, &zeta,
                                    &t))
    return -1;
  self->handle = loop_filter_create (bn, zeta, t);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "loop_filter_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
LoopFilter_step (LoopFilterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double x;
  if (!PyArg_ParseTuple (args, "d", &x))
    return NULL;
  double y = loop_filter_step (self->handle, x);
  return PyFloat_FromDouble (y);
}

static PyObject *
LoopFilter_steps (LoopFilterObject *self, PyObject *args, PyObject *kwds)
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
          out_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      loop_filter_steps (self->handle, (const double *)PyArray_DATA (in_arr),
                         (double *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_DOUBLE);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  loop_filter_steps (self->handle, (const double *)PyArray_DATA (in_arr),
                     (double *)PyArray_DATA ((PyArrayObject *)out_arr),
                     (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
LoopFilterObj_configure (LoopFilterObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bn", "zeta", "t", NULL };
  double       bn        = 0.0;
  double       zeta      = 0.0;
  double       t         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddd", _kwlist, &bn, &zeta,
                                    &t))
    return NULL;
  loop_filter_configure (self->handle, bn, zeta, t);
  Py_RETURN_NONE;
}

static PyObject *
LoopFilterObj_reset (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  loop_filter_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
LoopFilter_getprop_kp (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->kp);
}
static PyObject *
LoopFilter_getprop_ki (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->ki);
}
static PyObject *
LoopFilter_getprop_integ (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->integ);
}
static int
LoopFilter_setprop_integ (LoopFilterObject *self, PyObject *value,
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
  self->handle->integ = v;
  return 0;
}
static PyObject *
LoopFilter_getprop_bn (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->bn);
}
static PyObject *
LoopFilter_getprop_zeta (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->zeta);
}
static PyObject *
LoopFilter_getprop_t (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->t);
}

static PyGetSetDef LoopFilter_getset[]
    = { { "kp", (getter)LoopFilter_getprop_kp, NULL, "Kp.\n", NULL },
        { "ki", (getter)LoopFilter_getprop_ki, NULL, "Ki.\n", NULL },
        { "integ", (getter)LoopFilter_getprop_integ,
          (setter)LoopFilter_setprop_integ, "Integ.\n", NULL },
        { "bn", (getter)LoopFilter_getprop_bn, NULL, "Bn.\n", NULL },
        { "zeta", (getter)LoopFilter_getprop_zeta, NULL, "Zeta.\n", NULL },
        { "t", (getter)LoopFilter_getprop_t, NULL, "T.\n", NULL },
        { NULL } };

static PyObject *
LoopFilterObj_destroy (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      loop_filter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
LoopFilterObj_enter (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
LoopFilterObj_exit (LoopFilterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      loop_filter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef LoopFilterObj_methods[]
    = { { "step", (PyCFunction)LoopFilter_step, METH_VARARGS,
          "step(x) -> double\n"
          "\n"
          "Advance the loop one update with error @p x; return the control.\n"
          "\n"
          "    >>> from doppler import LoopFilter\n"
          "    >>> obj = LoopFilter(0.01, 0.707, 1.0)\n"
          "    >>> obj.step(1.0)\n"
          "    0.0\n" },
        { "steps", (PyCFunction)(void *)LoopFilter_steps,
          METH_VARARGS | METH_KEYWORDS,
          "steps(x[, out]) -> ndarray\n"
          "\n"
          "Run a block of errors through the loop.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import LoopFilter\n"
          "    >>> obj = LoopFilter(0.01, 0.707, 1.0)\n"
          "    >>> y = obj.steps(np.zeros(4, dtype=np.float64))\n"
          "    >>> y.shape\n"
          "    (4,)\n"
          "    >>> y.dtype\n"
          "    dtype('float64')\n" },

        { "configure", (PyCFunction)(void *)LoopFilterObj_configure,
          METH_VARARGS | METH_KEYWORDS,
          "configure(bn, zeta, t) -> None\n"
          "\n"
          "Recompute the loop gains for a new (bn, zeta, t); preserves the "
          "integrator.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import LoopFilter\n"
          "    >>> obj = LoopFilter(0.01, 0.707, 1.0)\n"
          "    >>> obj.configure(0.0, 0.0, 0.0)\n" },
        { "reset", (PyCFunction)LoopFilterObj_reset, METH_NOARGS,
          "reset() -> None\n"
          "\n"
          "Zero the integrator; keep the configured gains.\n"
          "\n"
          "    >>> from doppler import LoopFilter\n"
          "    >>> obj = LoopFilter(0.01, 0.707, 1.0)\n"
          "    >>> obj.reset()\n" },
        { "destroy", (PyCFunction)LoopFilterObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)LoopFilterObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)LoopFilterObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject LoopFilterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.LoopFilter",
  .tp_basicsize                           = sizeof (LoopFilterObject),
  .tp_dealloc                             = (destructor)LoopFilterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "LoopFilter type.\n",
  .tp_methods                             = LoopFilterObj_methods,
  .tp_getset                              = LoopFilter_getset,
  .tp_new                                 = LoopFilterObj_new,
  .tp_init                                = (initproc)LoopFilterObj_init,
};
