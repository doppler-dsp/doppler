/*
 * track_ext_symsync.c — SymbolSync type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* SymbolSyncObject — wraps symsync_state_t *       */
/* ======================================================== */

#include "symsync/symsync_core.h"

typedef struct
{
  PyObject_HEAD symsync_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
} SymbolSyncObject;

static void
SymbolSyncObj_dealloc (SymbolSyncObject *self)
{
  if (self->handle)
    symsync_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SymbolSyncObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  SymbolSyncObject *self = (SymbolSyncObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SymbolSyncObj_init (SymbolSyncObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "order", "sps", "bn", "zeta", NULL };
  const char        *order_str = "cubic";
  unsigned long long sps_raw   = 4;
  double             bn        = 0.01;
  double             zeta      = 0.707;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|sKdd", kwlist, &order_str,
                                    &sps_raw, &bn, &zeta))
    return -1;
  int order = 0;
  if (strcmp (order_str, "linear") == 0)
    order = 0;
  else if (strcmp (order_str, "parabolic") == 0)
    order = 1;
  else if (strcmp (order_str, "cubic") == 0)
    order = 2;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "order must be one of \"linear\", \"parabolic\", "
                    "\"cubic\", got '%s'",
                    order_str);
      return -1;
    }
  size_t sps   = (size_t)sps_raw;
  self->handle = symsync_create (sps, bn, zeta, order);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "symsync_create returned NULL");
      return -1;
    }
  {
    size_t _max = symsync_steps_max_out (self->handle);
    if (_max)
      {
        self->_steps_buf = malloc (_max * sizeof (float complex));
        if (!self->_steps_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_steps_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
SymbolSyncObj_steps (SymbolSyncObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_steps_buf || self->_steps_buf_cap < _need)
    {
      size_t _max = symsync_steps_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_steps_buf
          && self->_steps_retired_n == self->_steps_retired_cap)
        {
          size_t _rcap
              = self->_steps_retired_cap ? self->_steps_retired_cap * 2 : 4;
          void **_rt = realloc (self->_steps_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_steps_retired     = _rt;
          self->_steps_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_steps_buf)
        self->_steps_retired[self->_steps_retired_n++] = self->_steps_buf;
      self->_steps_buf     = _tmp;
      self->_steps_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = symsync_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                           self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_steps_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
SymbolSyncObj_configure (SymbolSyncObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bn", "zeta", NULL };
  double       bn        = 0.0;
  double       zeta      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", _kwlist, &bn, &zeta))
    return NULL;
  symsync_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_reset (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  symsync_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
SymbolSync_getprop_bn (SymbolSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_bn (self->handle));
}
static int
SymbolSync_setprop_bn (SymbolSyncObject *self, PyObject *value,
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
  symsync_set_bn (self->handle, v);
  return 0;
}
static PyObject *
SymbolSync_getprop_timing_error (SymbolSyncObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_timing_error (self->handle));
}
static PyObject *
SymbolSync_getprop_rate (SymbolSyncObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (symsync_get_rate (self->handle));
}

static PyGetSetDef SymbolSync_getset[]
    = { { "bn", (getter)SymbolSync_getprop_bn, (setter)SymbolSync_setprop_bn,
          "Bn.\n", NULL },
        { "timing_error", (getter)SymbolSync_getprop_timing_error, NULL,
          "Timing error.\n", NULL },
        { "rate", (getter)SymbolSync_getprop_rate, NULL, "Rate.\n", NULL },
        { NULL } };

static PyObject *
SymbolSyncObj_destroy (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      symsync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SymbolSyncObj_enter (SymbolSyncObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
SymbolSyncObj_exit (SymbolSyncObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      symsync_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef SymbolSyncObj_methods[] = {

  { "steps", (PyCFunction)SymbolSyncObj_steps, METH_VARARGS,
    "steps(x) -> ndarray\n"
    "\n"
    "Recover symbol timing from an oversampled cf32 baseband block: a Gardner "
    "timing-error detector drives an integer timing NCO whose post-wrap value "
    "gives the interpolation fraction for free, and a Farrow interpolator "
    "emits one symbol-rate sample per recovered symbol instant.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(\"cubic\", 4, 0.01, 0.707)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "configure", (PyCFunction)(void *)SymbolSyncObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserve the timing "
    "estimate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(\"cubic\", 4, 0.01, 0.707)\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "reset", (PyCFunction)SymbolSyncObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the timing loop to its nominal rate and zero phase.\n"
    "\n"
    "    >>> from doppler import SymbolSync\n"
    "    >>> obj = SymbolSync(\"cubic\", 4, 0.01, 0.707)\n"
    "    >>> obj.reset()\n" },
  { "destroy", (PyCFunction)SymbolSyncObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)SymbolSyncObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)SymbolSyncObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject SymbolSyncObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.SymbolSync",
  .tp_basicsize                           = sizeof (SymbolSyncObject),
  .tp_dealloc                             = (destructor)SymbolSyncObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "SymbolSync type.\n",
  .tp_methods                             = SymbolSyncObj_methods,
  .tp_getset                              = SymbolSync_getset,
  .tp_new                                 = SymbolSyncObj_new,
  .tp_init                                = (initproc)SymbolSyncObj_init,
};
