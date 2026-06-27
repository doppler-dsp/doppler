/*
 * track_ext_pdespread.c — PartialDespreader type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* PartialDespreaderObject — wraps pdespread_state_t *       */
/* ======================================================== */

#include "pdespread/pdespread_core.h"

typedef struct
{
  PyObject_HEAD pdespread_state_t *handle;
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
} PartialDespreaderObject;

static void
PartialDespreaderObj_dealloc (PartialDespreaderObject *self)
{
  if (self->handle)
    pdespread_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PartialDespreaderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PartialDespreaderObject *self
      = (PartialDespreaderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PartialDespreaderObj_init (PartialDespreaderObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[]
      = { "code", "sps", "k", "init_chip", "bn", "zeta", "spacing", NULL };
  PyObject          *code_obj  = NULL;
  unsigned long long sps_raw   = 4;
  unsigned long long k_raw     = 4;
  double             init_chip = 0.0;
  double             bn        = 0.002;
  double             zeta      = 0.707;
  double             spacing   = 0.5;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKdddd", kwlist, &code_obj,
                                    &sps_raw, &k_raw, &init_chip, &bn, &zeta,
                                    &spacing))
    return -1;
  size_t         sps      = (size_t)sps_raw;
  size_t         k        = (size_t)k_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle
      = pdespread_create ((const uint8_t *)PyArray_DATA (code_arr), code_len,
                          sps, k, init_chip, bn, zeta, spacing);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "pdespread_create returned NULL");
      return -1;
    }
  {
    size_t _max = pdespread_steps_max_out (self->handle);
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
PartialDespreaderObj_steps (PartialDespreaderObject *self, PyObject *args)
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
      size_t _max = pdespread_steps_max_out (self->handle);
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
    n_out = pdespread_steps (self->handle, _ng0, _ng1, self->_steps_buf,
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
PartialDespreaderObj_reset (PartialDespreaderObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  pdespread_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
PartialDespreader_getprop_code_phase (PartialDespreaderObject *self,
                                      void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (pdespread_get_code_phase (self->handle));
}
static PyObject *
PartialDespreader_getprop_code_rate (PartialDespreaderObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (pdespread_get_code_rate (self->handle));
}
static PyObject *
PartialDespreader_getprop_last_error (PartialDespreaderObject *self,
                                      void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (pdespread_get_last_error (self->handle));
}
static PyObject *
PartialDespreader_getprop_k (PartialDespreaderObject *self,
                             void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)pdespread_get_k (self->handle));
}

static PyGetSetDef PartialDespreader_getset[]
    = { { "code_phase", (getter)PartialDespreader_getprop_code_phase, NULL,
          "Tracked code phase, chips.\n", NULL },
        { "code_rate", (getter)PartialDespreader_getprop_code_rate, NULL,
          "Tracked code rate (chips advanced per nominal chip, ~1.0).\n",
          NULL },
        { "last_error", (getter)PartialDespreader_getprop_last_error, NULL,
          "Last non-coherent discriminator output (loop stress).\n", NULL },
        { "k", (getter)PartialDespreader_getprop_k, NULL,
          "Partial correlations per code epoch.\n", NULL },
        { NULL } };

static PyObject *
PartialDespreaderObj_destroy (PartialDespreaderObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      pdespread_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PartialDespreaderObj_enter (PartialDespreaderObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PartialDespreaderObj_exit (PartialDespreaderObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      pdespread_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef PartialDespreaderObj_methods[] = {

  { "steps", (PyCFunction)PartialDespreaderObj_steps, METH_VARARGS,
    "steps(x) -> ndarray\n"
    "\n"
    "Despread a carrier-wiped cf32 block, emitting k sub-epoch partial "
    "prompts per code period and steering the code NCO once per period on the "
    "non-coherent early-late discriminator (robust to an asynchronous "
    "data-symbol clock). The partial-prompt stream feeds a downstream symbol "
    "matched filter + SymbolSync.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PartialDespreader\n"
    "    >>> obj = PartialDespreader(np.zeros(1, dtype=np.uint8), 4, 4, 0.0, "
    "0.002, 0.707, 0.5)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)PartialDespreaderObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the code loop to the create-time code phase; preserve config.\n"
    "\n"
    "    >>> from doppler import PartialDespreader\n"
    "    >>> obj = PartialDespreader(np.zeros(1, dtype=np.uint8), 4, 4, 0.0, "
    "0.002, 0.707, 0.5)\n"
    "    >>> obj.reset()\n" },
  { "destroy", (PyCFunction)PartialDespreaderObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)PartialDespreaderObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)PartialDespreaderObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject PartialDespreaderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.PartialDespreader",
  .tp_basicsize                           = sizeof (PartialDespreaderObject),
  .tp_dealloc = (destructor)PartialDespreaderObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a partial-correlation despreader (COPIES code).\n",
  .tp_methods = PartialDespreaderObj_methods,
  .tp_getset  = PartialDespreader_getset,
  .tp_new     = PartialDespreaderObj_new,
  .tp_init    = (initproc)PartialDespreaderObj_init,
};
