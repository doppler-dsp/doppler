/*
 * track_ext_costas.c — Costas type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* CostasObject — wraps costas_state_t *       */
/* ======================================================== */

#include "costas/costas_core.h"

typedef struct
{
  PyObject_HEAD costas_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
} CostasObject;

static void
CostasObj_dealloc (CostasObject *self)
{
  if (self->handle)
    costas_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CostasObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CostasObject *self = (CostasObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CostasObj_init (CostasObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "bn", "zeta", "init_norm_freq", "tsamps", "bn_fll", NULL };
  double             bn             = 0.05;
  double             zeta           = 0.707;
  double             init_norm_freq = 0.0;
  unsigned long long tsamps_raw     = 64;
  double             bn_fll         = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddKd", kwlist, &bn, &zeta,
                                    &init_norm_freq, &tsamps_raw, &bn_fll))
    return -1;
  size_t tsamps = (size_t)tsamps_raw;
  self->handle  = costas_create (bn, zeta, init_norm_freq, tsamps, bn_fll);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "costas_create returned NULL");
      return -1;
    }
  {
    size_t _max = costas_steps_max_out (self->handle);
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
CostasObj_steps_max_out (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (costas_steps_max_out (self->handle));
}

static PyObject *
CostasObj_steps (CostasObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = costas_steps_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex       *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = costas_steps (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_steps_buf || self->_steps_buf_cap < _need)
    {
      size_t _max = costas_steps_max_out (self->handle);
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
    n_out = costas_steps (self->handle, _ng0, _ng1, self->_steps_buf,
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
CostasObj_configure (CostasObject *self, PyObject *args, PyObject *kwds)
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
  costas_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_reset (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  costas_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_state_bytes (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (costas_state_bytes (self->handle));
}

static PyObject *
CostasObj_get_state (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = costas_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  costas_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CostasObj_set_state (CostasObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != costas_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (costas_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Costas_getprop_bn (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_bn (self->handle));
}
static int
Costas_setprop_bn (CostasObject *self, PyObject *value,
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
  costas_set_bn (self->handle, v);
  return 0;
}
static PyObject *
Costas_getprop_norm_freq (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_norm_freq (self->handle));
}
static int
Costas_setprop_norm_freq (CostasObject *self, PyObject *value,
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
  costas_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
Costas_getprop_lock_metric (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_lock_metric (self->handle));
}
static PyObject *
Costas_getprop_last_error (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_last_error (self->handle));
}
static PyObject *
Costas_getprop_bn_fll (CostasObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (costas_get_bn_fll (self->handle));
}
static int
Costas_setprop_bn_fll (CostasObject *self, PyObject *value,
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
  costas_set_bn_fll (self->handle, v);
  return 0;
}

static PyGetSetDef Costas_getset[]
    = { { "bn", (getter)Costas_getprop_bn, (setter)Costas_setprop_bn, "Bn.\n",
          NULL },
        { "norm_freq", (getter)Costas_getprop_norm_freq,
          (setter)Costas_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock_metric", (getter)Costas_getprop_lock_metric, NULL,
          "Lock metric.\n", NULL },
        { "last_error", (getter)Costas_getprop_last_error, NULL,
          "Last error.\n", NULL },
        { "bn_fll", (getter)Costas_getprop_bn_fll,
          (setter)Costas_setprop_bn_fll, "Bn fll.\n", NULL },
        { NULL } };

static PyObject *
CostasObj_destroy (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      costas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CostasObj_enter (CostasObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CostasObj_exit (CostasObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      costas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CostasObj_methods[] = {

  { "steps", (PyCFunction)CostasObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "De-rotate a cf32 block with the integer-NCO carrier, coherently "
    "integrate over each tsamps-sample symbol, run the decision-directed "
    "Costas discriminator, and emit one complex prompt symbol per symbol.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Costas\n"
    "    >>> obj = Costas(0.05, 0.707, 0.0, 64, 0.0)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)CostasObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "configure", (PyCFunction)(void *)CostasObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the "
    "frequency/phase estimate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Costas\n"
    "    >>> obj = Costas(0.05, 0.707, 0.0, 64, 0.0)\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "reset", (PyCFunction)CostasObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time frequency/phase; preserve config.\n"
    "\n"
    "    >>> from doppler import Costas\n"
    "    >>> obj = Costas(0.05, 0.707, 0.0, 64, 0.0)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)CostasObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)CostasObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)CostasObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)CostasObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)CostasObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)CostasObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject CostasObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.Costas",
  .tp_basicsize                           = sizeof (CostasObject),
  .tp_dealloc                             = (destructor)CostasObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Costas type.\n",
  .tp_methods                             = CostasObj_methods,
  .tp_getset                              = Costas_getset,
  .tp_new                                 = CostasObj_new,
  .tp_init                                = (initproc)CostasObj_init,
};
