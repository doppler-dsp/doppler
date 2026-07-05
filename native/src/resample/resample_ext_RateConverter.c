/*
 * resample_ext_RateConverter.c — RateConverter type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* RateConverterObject — wraps RateConverter_state_t *      */
/* ======================================================== */

#include "RateConverter/RateConverter_core.h"
#include "dp_state_pyhelp.h"

typedef struct
{
  PyObject_HEAD RateConverter_state_t *handle;
  float complex                       *_execute_buf;
  size_t                               _execute_buf_cap;
  void **_execute_retired; /* gh-219 deferred free */
  size_t _execute_retired_n;
  size_t _execute_retired_cap;
} RateConverterObject;

static void
RateConverterObj_dealloc (RateConverterObject *self)
{
  if (self->handle)
    RateConverter_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t i = 0; i < self->_execute_retired_n; i++)
    free (self->_execute_retired[i]);
  free (self->_execute_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
RateConverterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  RateConverterObject *self = (RateConverterObject *)type->tp_alloc (type, 0);
  if (self)
    {
      self->handle           = NULL;
      self->_execute_buf     = NULL;
      self->_execute_buf_cap = 0;
    }
  return (PyObject *)self;
}

static int
RateConverterObj_init (RateConverterObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char *kwlist[]   = { "rate", "compensate", NULL };
  double       rate       = 1.0;
  int          compensate = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|di", kwlist, &rate,
                                    &compensate))
    return -1;
  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }
  self->handle = RateConverter_create (rate, compensate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError, "RateConverter_create returned NULL"
                                         " (rate must be > 0)");
      return -1;
    }
  return 0;
}

/* gh-219: retire (don't free) the old execute buffer so a previously
 * returned view stays valid until dealloc. Used on growth and on any
 * rate change (both invalidate the buffer's sizing for future calls,
 * but must not free memory a live numpy array still points to). */
static int
RateConverterObj_retire_execute_buf (RateConverterObject *self)
{
  if (!self->_execute_buf)
    return 0;
  if (self->_execute_retired_n == self->_execute_retired_cap)
    {
      size_t _rcap
          = self->_execute_retired_cap ? self->_execute_retired_cap * 2 : 4;
      void **_rt = realloc (self->_execute_retired, _rcap * sizeof (void *));
      if (!_rt)
        return -1;
      self->_execute_retired     = _rt;
      self->_execute_retired_cap = _rcap;
    }
  self->_execute_retired[self->_execute_retired_n++] = self->_execute_buf;
  self->_execute_buf                                 = NULL;
  return 0;
}

/* -------------------------------------------------------------- */
/* execute(x, out=None) -> ndarray[complex64]                     */
/*                                                                */
/* Output length varies with rate; buffer is grown on demand.     */
/* -------------------------------------------------------------- */
static PyObject *
RateConverterObj_execute (RateConverterObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "x", "out", NULL };
  PyObject    *x_obj    = NULL;
  PyObject    *out_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", kwlist, &x_obj,
                                    &out_obj))
    return NULL;

  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;

  size_t n_in = (size_t)PyArray_SIZE (x_arr);

  /* Required output capacity for this block size — mirrors
   * RateConverter_execute_max_out()'s own (65536-input-sized) bound,
   * but computed for the actual n_in since a block can exceed that. */
  double rate  = RateConverter_get_rate (self->handle);
  double ratio = (rate > 1.0) ? rate : 1.0;
  size_t need  = (size_t)(n_in * ratio) + 4;

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
      size_t _omax    = RateConverter_execute_max_out (self->handle);
      size_t _min_cap = _omax > need ? _omax : need;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t n_out = RateConverter_execute (
          self->handle, (const float complex *)PyArray_DATA (x_arr), n_in,
          (float complex *)PyArray_DATA (out_arr), _cap);
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

  if (need > self->_execute_buf_cap)
    {
      if (RateConverterObj_retire_execute_buf (self) != 0)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      self->_execute_buf = malloc (need * sizeof (float complex));
      if (!self->_execute_buf)
        {
          self->_execute_buf_cap = 0;
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      self->_execute_buf_cap = need;
    }

  size_t n_out = RateConverter_execute (
      self->handle, (const float complex *)PyArray_DATA (x_arr), n_in,
      self->_execute_buf, self->_execute_buf_cap);

  npy_intp  dim = (npy_intp)n_out;
  PyObject *out
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!out)
    {
      Py_DECREF (x_arr);
      return NULL;
    }

  /* Keep self alive as long as the returned array holds a view into
   * _execute_buf — prevents use-after-free if the caller drops self. */
  PyArray_SetBaseObject ((PyArrayObject *)out, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return out;
}

/* -------------------------------------------------------------- */
/* reset() -> None                                                */
/* -------------------------------------------------------------- */
static PyObject *
RateConverterObj_reset (RateConverterObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  RateConverter_reset (self->handle);
  Py_RETURN_NONE;
}

/* -------------------------------------------------------------- */
/* destroy() -> None                                              */
/* -------------------------------------------------------------- */
static PyObject *
RateConverterObj_destroy (RateConverterObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }
  free (self->_execute_buf);
  self->_execute_buf     = NULL;
  self->_execute_buf_cap = 0;
  Py_RETURN_NONE;
}

static PyObject *
RateConverterObj_enter (RateConverterObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
RateConverterObj_exit (RateConverterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }
  free (self->_execute_buf);
  self->_execute_buf     = NULL;
  self->_execute_buf_cap = 0;
  Py_RETURN_NONE;
}

/* -------------------------------------------------------------- */
/* Properties: rate (read/write), stages (read-only)              */
/* -------------------------------------------------------------- */

static PyObject *
RateConverterObj_get_rate (RateConverterObject *self, void *Py_UNUSED (c))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (RateConverter_get_rate (self->handle));
}

static int
RateConverterObj_set_rate (RateConverterObject *self, PyObject *value,
                           void *Py_UNUSED (c))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = PyFloat_AsDouble (value);
  if (PyErr_Occurred ())
    return -1;
  RateConverter_set_rate (self->handle, v);

  /* Invalidate execute buffer — rate change means different output size.
   * gh-219: retire rather than free — a previously returned view may
   * still be live. */
  if (RateConverterObj_retire_execute_buf (self) != 0)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_buf_cap = 0;
  return 0;
}

static PyObject *
RateConverterObj_get_stages (RateConverterObject *self, void *Py_UNUSED (c))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int       n    = self->handle->n_stages;
  PyObject *list = PyList_New (n);
  if (!list)
    return NULL;
  for (int i = 0; i < n; i++)
    {
      char buf[64];
      RateConverter_stage_label (self->handle, i, buf, sizeof (buf));
      PyObject *s = PyUnicode_FromString (buf);
      if (!s)
        {
          Py_DECREF (list);
          return NULL;
        }
      PyList_SET_ITEM (list, i, s);
    }
  return list;
}

/* serializable (gh-400): the standard state triplet, generated by the
 * shared macro (see dp_state_pyhelp.h) — byte-identical to jm's output.
 * The matching PyMethodDef rows are below. */
DP_PY_STATE_METHODS (RateConverterObj, RateConverterObject, self->handle,
                     RateConverter)

static PyObject *
RateConverterObj_execute_max_out (RateConverterObject *self,
                                  PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (RateConverter_execute_max_out (self->handle));
}

static PyMethodDef RateConverter_methods[]
    = { { "execute", (PyCFunction)(void *)RateConverterObj_execute,
          METH_VARARGS | METH_KEYWORDS,
          "execute(x, out=None) -> ndarray\n"
          "\n"
          "Convert a block of complex64 samples.\n"
          "\n"
          "Without out=, the returned array is a view into a buffer\n"
          "reused on the next call (see execute_max_out() to size an\n"
          "out= buffer for an independent, alias-free result).\n"
          "\n"
          "Parameters\n"
          "----------\n"
          "x : array_like, complex64\n"
          "    Input samples.\n"
          "out : ndarray, complex64, optional\n"
          "    Caller-provided output buffer, at least\n"
          "    max(execute_max_out(), len(x) * max(rate, 1.0) + 4) "
          "elements.\n"
          "\n"
          "Returns\n"
          "-------\n"
          "ndarray, complex64\n"
          "    Output samples.  Length is approximately ``len(x) * rate``.\n"
          "\n"
          "Examples\n"
          "--------\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler.resample import RateConverter\n"
          "    >>> rc = RateConverter(0.5)\n"
          "    >>> y = rc.execute(np.ones(256, dtype=np.complex64))\n"
          "    >>> len(y) == 128\n"
          "    True\n" },
        { "execute_max_out", (PyCFunction)RateConverterObj_execute_max_out,
          METH_NOARGS,
          "execute_max_out() -> int\n\nMax output length execute() can "
          "produce for the current state.\nUse to size the ``out=`` "
          "buffer." },
        { "reset", (PyCFunction)RateConverterObj_reset, METH_NOARGS,
          "reset() -> None\n"
          "\n"
          "Zero all sub-stage filter memories without changing the rate." },
        { "state_bytes", (PyCFunction)RateConverterObj_state_bytes,
          METH_NOARGS, "Serialized state size in bytes." },
        { "get_state", (PyCFunction)RateConverterObj_get_state, METH_NOARGS,
          "Serialize the cascade's mutable state to bytes." },
        { "set_state", (PyCFunction)RateConverterObj_set_state, METH_O,
          "Restore mutable state from a get_state() blob." },
        { "destroy", (PyCFunction)RateConverterObj_destroy, METH_NOARGS,
          "Release resources early." },
        { "__enter__", (PyCFunction)RateConverterObj_enter, METH_NOARGS,
          NULL },
        { "__exit__", (PyCFunction)RateConverterObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyGetSetDef RateConverter_getset[]
    = { { "rate", (getter)RateConverterObj_get_rate,
          (setter)RateConverterObj_set_rate,
          "Output-to-input sample rate ratio.", NULL },
        { "stages", (getter)RateConverterObj_get_stages, NULL,
          "List of stage labels (e.g. ['CIC(8)', 'Resampler(0.8)']).", NULL },
        { NULL } };

static PyTypeObject RateConverterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.RateConverter",
  .tp_basicsize                           = sizeof (RateConverterObject),
  .tp_dealloc = (destructor)RateConverterObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "RateConverter(rate=1.0, compensate=0)\n"
                "\n"
                "Optimal-speed rate conversion cascade.\n"
                "\n"
                "Selects the cheapest cascade of CIC, HalfbandDecimator, and/or\n"
                "polyphase Resampler stages at creation time.\n"
                "\n"
                "Parameters\n"
                "----------\n"
                "rate : float\n"
                "    Output-to-input sample rate ratio.  Any positive float.\n"
                "compensate : int\n"
                "    Non-zero to append a CIC passband-droop compensating FIR.\n"
                "\n"
                "Attributes\n"
                "----------\n"
                "rate : float\n"
                "    Current rate ratio (writable; rebuilds cascade on set).\n"
                "stages : list of str\n"
                "    Human-readable stage labels.\n"
                "\n"
                "Examples\n"
                "--------\n"
                "    >>> from doppler.resample import RateConverter\n"
                "    >>> rc = RateConverter(0.125)\n"
                "    >>> rc.stages\n"
                "    ['CIC(8)']\n",
  .tp_methods = RateConverter_methods,
  .tp_getset  = RateConverter_getset,
  .tp_new     = RateConverterObj_new,
  .tp_init    = (initproc)RateConverterObj_init,
};
