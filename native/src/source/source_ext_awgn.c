/*
 * source_ext_awgn.c — AWGN type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* AWGNObject — wraps awgn_state_t *       */
/* ======================================================== */

#include "awgn/awgn_core.h"

typedef struct
{
  PyObject_HEAD awgn_state_t *handle;
} AWGNObject;

static void
AWGNObj_dealloc (AWGNObject *self)
{
  if (self->handle)
    awgn_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AWGNObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AWGNObject *self = (AWGNObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AWGNObj_init (AWGNObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "seed", "amplitude", NULL };
  unsigned long long seed_raw  = 0ULL;
  float              amplitude = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kf", kwlist, &seed_raw,
                                    &amplitude))
    return -1;
  uint64_t seed = (uint64_t)seed_raw;
  self->handle  = awgn_create (seed, amplitude);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "awgn_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AWGNObj_reset (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  awgn_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AWGNObj_generate (AWGNObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be >= 0");
      return NULL;
    }
  /* NumPy owns the output: allocate exactly n and write into it. Each call is
   * independent — a shared reuse buffer aliased/dangled earlier results across
   * calls and (sized to a fixed cap) overflowed for large n (#116). */
  npy_intp  dim = (npy_intp)n;
  PyObject *arr = PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!arr)
    return NULL;
  awgn_generate (self->handle, (size_t)n,
                 (float complex *)PyArray_DATA ((PyArrayObject *)arr));
  return arr;
}

static PyObject *
AWGNObj_reseed (AWGNObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  unsigned long long seed_raw = 0ULL;
  if (!PyArg_ParseTuple (args, "K", &seed_raw))
    return NULL;
  uint64_t seed = (uint64_t)seed_raw;
  awgn_reseed (self->handle, seed);
  Py_RETURN_NONE;
}
static PyObject *
AWGN_getprop_amplitude (AWGNObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble ((double)awgn_get_amplitude (self->handle));
}
static int
AWGN_setprop_amplitude (AWGNObject *self, PyObject *value,
                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  float v = 0.0f;
  if (!PyArg_Parse (value, "f", &v))
    return -1;
  awgn_set_amplitude (self->handle, v);
  return 0;
}

static PyGetSetDef AWGN_getset[]
    = { { "amplitude", (getter)AWGN_getprop_amplitude,
          (setter)AWGN_setprop_amplitude,
          "Return the current amplitude (per-component std dev).\n", NULL },
        { NULL } };

static PyObject *
AWGNObj_destroy (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      awgn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AWGNObj_enter (AWGNObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AWGNObj_exit (AWGNObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      awgn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AWGNObj_methods[]
    = { { "reset", (PyCFunction)AWGNObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "generate", (PyCFunction)AWGNObj_generate, METH_VARARGS,
          "generate(n=1) -> ndarray\n"
          "\n"
          "Generate n complex CF32 AWGN samples.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import AWGN\n"
          "    >>> obj = AWGN(0, 1.0)\n"
          "    >>> y = obj.generate(4)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "reseed", (PyCFunction)AWGNObj_reseed, METH_VARARGS,
          "reseed(seed) -> complex\n"
          "\n"
          "Reseed the RNG and reset state.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import AWGN\n"
          "    >>> obj = AWGN(0, 1.0)\n"
          "    >>> obj.reseed(0)\n"
          "    0j\n" },
        { "destroy", (PyCFunction)AWGNObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)AWGNObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)AWGNObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject AWGNObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "source.AWGN",
  .tp_basicsize                           = sizeof (AWGNObject),
  .tp_dealloc                             = (destructor)AWGNObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an AWGN generator.\n",
  .tp_methods                             = AWGNObj_methods,
  .tp_getset                              = AWGN_getset,
  .tp_new                                 = AWGNObj_new,
  .tp_init                                = (initproc)AWGNObj_init,
};
