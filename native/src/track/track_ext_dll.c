/*
 * track_ext_dll.c — Dll type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* DllObject — wraps dll_state_t *       */
/* ======================================================== */

#include "dll/dll_core.h"

typedef struct
{
  PyObject_HEAD dll_state_t *handle;
  float complex             *_steps_buf;     /* internal scratch for steps */
  size_t                     _steps_buf_cap; /* allocated capacity for steps */
} DllObject;

static void
DllObj_dealloc (DllObject *self)
{
  if (self->handle)
    dll_destroy (self->handle);
  free (self->_steps_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DllObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DllObject *self = (DllObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DllObj_init (DllObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]     = { "code", "sps",     "init_chip", "bn",
                                      "zeta", "spacing", "segments",  NULL };
  PyObject          *code_obj     = NULL;
  unsigned long long sps_raw      = 2;
  double             init_chip    = 0.0;
  double             bn           = 0.01;
  double             zeta         = 0.707;
  double             spacing      = 0.5;
  unsigned long long segments_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KddddK", kwlist, &code_obj,
                                    &sps_raw, &init_chip, &bn, &zeta, &spacing,
                                    &segments_raw))
    return -1;
  size_t         sps      = (size_t)sps_raw;
  size_t         segments = (size_t)segments_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle
      = dll_create ((const uint8_t *)PyArray_DATA (code_arr), code_len, sps,
                    init_chip, bn, zeta, spacing, segments);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dll_create returned NULL");
      return -1;
    }
  {
    size_t _max = dll_steps_max_out (self->handle);
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
DllObj_steps (DllObject *self, PyObject *args)
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
      /* Grow-on-demand internal scratch. The returned array is an independent
       * copy (below), so no view ever aliases this buffer — a plain realloc is
       * safe; the old contents are not needed after the kernel runs. */
      size_t _max = dll_steps_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      float complex *_tmp
          = realloc (self->_steps_buf, _max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
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
    n_out = dll_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                       self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  /* NumPy owns the output: an independent array per call, copied from the
   * internal scratch. A reused/grown scratch buffer must never be exposed as a
   * view — successive steps() calls would alias the same memory (a streaming
   * despreader keeps every block), and a realloc would dangle prior views. */
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  memcpy (PyArray_DATA ((PyArrayObject *)arr), self->_steps_buf,
          (size_t)n_out * sizeof (float complex));
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
DllObj_configure (DllObject *self, PyObject *args, PyObject *kwds)
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
  dll_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
DllObj_reset (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dll_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
Dll_getprop_bn (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_bn (self->handle));
}
static int
Dll_setprop_bn (DllObject *self, PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  dll_set_bn (self->handle, v);
  return 0;
}
static PyObject *
Dll_getprop_code_phase (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_code_phase (self->handle));
}
static PyObject *
Dll_getprop_code_rate (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_code_rate (self->handle));
}
static PyObject *
Dll_getprop_last_error (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_last_error (self->handle));
}
static PyObject *
Dll_getprop_segments (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dll_get_segments (self->handle));
}

static PyGetSetDef Dll_getset[] = {
  { "bn", (getter)Dll_getprop_bn, (setter)Dll_setprop_bn, "Bn.\n", NULL },
  { "code_phase", (getter)Dll_getprop_code_phase, NULL, "Code phase.\n",
    NULL },
  { "code_rate", (getter)Dll_getprop_code_rate, NULL, "Code rate.\n", NULL },
  { "last_error", (getter)Dll_getprop_last_error, NULL, "Last error.\n",
    NULL },
  { "segments", (getter)Dll_getprop_segments, NULL, "Segments.\n", NULL },
  { NULL }
};

static PyObject *
DllObj_destroy (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dll_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DllObj_enter (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DllObj_exit (DllObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dll_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DllObj_methods[] = {

  { "steps", (PyCFunction)DllObj_steps, METH_VARARGS,
    "steps(x) -> ndarray\n"
    "\n"
    "Correlate a carrier-wiped cf32 block against the local code with "
    "early/prompt/late taps and steer the code NCO each code period on the "
    "non-coherent (sum|E|-sum|L|)/(sum|E|+sum|L|) discriminator. With "
    "segments=1 (default) this is a coherent full-epoch integrate-and-dump: "
    "one prompt symbol per period. With segments>1 each epoch is split into "
    "that many sub-epoch partial correlations: it emits that many partial "
    "prompts per period (a stream at ~segments samples/symbol when the symbol "
    "rate is near the code rate, for a downstream symbol matched filter + "
    "SymbolSync) and tracks the code non-coherently across the partials, "
    "which a data flip cannot collapse (robust to an asynchronous data-symbol "
    "clock).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "configure", (PyCFunction)(void *)DllObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the code "
    "phase/rate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "reset", (PyCFunction)DllObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time code phase; preserve config.\n"
    "\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> obj.reset()\n" },
  { "destroy", (PyCFunction)DllObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DllObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DllObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject DllObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.Dll",
  .tp_basicsize                           = sizeof (DllObject),
  .tp_dealloc                             = (destructor)DllObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a DLL instance (COPIES code).\n",
  .tp_methods = DllObj_methods,
  .tp_getset  = Dll_getset,
  .tp_new     = DllObj_new,
  .tp_init    = (initproc)DllObj_init,
};
