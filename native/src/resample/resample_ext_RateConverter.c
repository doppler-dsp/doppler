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
  static char *kwlist[]   = { "rate", "compensate", "pulse",      "beta",
                              "span", "pulse_sps",  "num_phases", NULL };
  double       rate       = 1.0;
  int          compensate = 0;
  const char  *pulse      = "none";
  double       beta       = 0.35;
  Py_ssize_t   span       = 8;
  double       pulse_sps  = 2.0;
  Py_ssize_t   num_phases = 1024;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|disdndn", kwlist, &rate,
                                    &compensate, &pulse, &beta, &span,
                                    &pulse_sps, &num_phases))
    return -1;

  int pulse_id;
  if (strcmp (pulse, "none") == 0)
    pulse_id = RC_PULSE_NONE;
  else if (strcmp (pulse, "rrc") == 0)
    pulse_id = RC_PULSE_RRC;
  else if (strcmp (pulse, "iandd") == 0)
    pulse_id = RC_PULSE_IANDD;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "pulse must be 'none', 'rrc' or 'iandd', not '%s'", pulse);
      return -1;
    }
  if (pulse_id != RC_PULSE_NONE && (span < 1 || num_phases < 2))
    {
      PyErr_SetString (PyExc_ValueError,
                       "span must be >= 1 and num_phases >= 2");
      return -1;
    }

  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }

  if (pulse_id == RC_PULSE_NONE)
    {
      self->handle = RateConverter_create (rate, compensate);
      if (!self->handle)
        {
          PyErr_SetString (PyExc_ValueError,
                           "RateConverter_create returned NULL"
                           " (rate must be > 0)");
          return -1;
        }
      return 0;
    }

  self->handle = RateConverter_create_matched (rate, compensate, pulse_id,
                                               beta, (size_t)span, pulse_sps,
                                               (size_t)num_phases);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "RateConverter_create_matched returned NULL (need"
                       " rate > 0, 0 <= beta <= 1, span >= 1,"
                       " pulse_sps > 0, num_phases a power of two >= 2)");
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

/* bank_shape -> (num_phases, num_taps) of the terminal polyphase stage, or
 * None when the cascade ends in an integer decimator.  This is the cost the
 * matched-filter design is built around — taps per arm sized by the
 * POST-decimation rate rather than the input rate — so it has to be
 * observable, not just asserted in a header comment. */
static PyObject *
RateConverterObj_get_bank_shape (RateConverterObject *self,
                                 void                *Py_UNUSED (c))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int last = self->handle->n_stages - 1;
  if (last < 0 || self->handle->stage_types[last] != RC_STAGE_RESAMP)
    Py_RETURN_NONE;
  const resamp_state_t *r
      = (const resamp_state_t *)self->handle->stage_ptrs[last];
  return Py_BuildValue ("(nn)", (Py_ssize_t)resamp_get_num_phases (r),
                        (Py_ssize_t)resamp_get_num_taps (r));
}

/* clipped -> bool: has any planned CIC stage clipped since the last reset?
 * The cascade inherits cic_core's input bound whenever a CIC is planned (any
 * decimation by 8 or more) and the clip is invisible in the samples, so this
 * is the only reliable check -- and free, since the boundary comparisons run
 * regardless. */
static PyObject *
RateConverterObj_get_clipped (RateConverterObject *self, void *Py_UNUSED (c))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)RateConverter_get_clipped (self->handle));
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

/* -------------------------------------------------------------- */
/* execute_ctrl(x, ctrl) -> ndarray[complex64]                    */
/*                                                                */
/* Control-port form: a scalar rate deviation steers the terminal */
/* Resampler stage (no-op for a pure integer HB/CIC cascade).     */
/* -------------------------------------------------------------- */
static PyObject *
RateConverterObj_execute_ctrl (RateConverterObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "x", "ctrl", NULL };
  PyObject    *x_obj    = NULL;
  double       ctrl     = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", kwlist, &x_obj, &ctrl))
    return NULL;

  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;

  size_t n_in  = (size_t)PyArray_SIZE (x_arr);
  double rate  = RateConverter_get_rate (self->handle);
  double ratio = (rate > 1.0) ? rate : 1.0;
  size_t need  = (size_t)(n_in * ratio) + 4;

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

  size_t n_out = RateConverter_execute_ctrl (
      self->handle, (const float complex *)PyArray_DATA (x_arr), n_in, ctrl,
      self->_execute_buf, self->_execute_buf_cap);

  npy_intp  dim = (npy_intp)n_out;
  PyObject *out
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!out)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  PyArray_SetBaseObject ((PyArrayObject *)out, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return out;
}

/* -------------------------------------------------------------- */
/* execute_ctrl_push(x, ctrl) -> ndarray[complex64]               */
/*                                                                */
/* Per-input streaming form: one sample in, 0..n outputs out. The */
/* result is an independent array (never a view into a reused     */
/* buffer) — a closed loop reads each output before deciding the  */
/* next ctrl, so aliasing the previous call's result would be a   */
/* trap, and the array is a handful of samples anyway.            */
/* -------------------------------------------------------------- */
static PyObject *
RateConverterObj_execute_ctrl_push (RateConverterObject *self, PyObject *args,
                                    PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "x", "ctrl", NULL };
  Py_complex   xc;
  double       ctrl = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Dd", kwlist, &xc, &ctrl))
    return NULL;

  /* One input can complete at most ceil(rate) + 1 output periods. */
  double         rate = RateConverter_get_rate (self->handle);
  size_t         cap  = (size_t)(rate > 1.0 ? rate : 1.0) + 2;
  float complex *tmp  = malloc (cap * sizeof (float complex));
  if (!tmp)
    return PyErr_NoMemory ();

  size_t n_out = RateConverter_execute_ctrl_push (
      self->handle, (float)xc.real + (float)xc.imag * I, ctrl, tmp, cap);

  npy_intp  dim = (npy_intp)n_out;
  PyObject *out = PyArray_SimpleNew (1, &dim, NPY_COMPLEX64);
  if (!out)
    {
      free (tmp);
      return NULL;
    }
  memcpy (PyArray_DATA ((PyArrayObject *)out), tmp,
          n_out * sizeof (float complex));
  free (tmp);
  return out;
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
        { "execute_ctrl", (PyCFunction)(void *)RateConverterObj_execute_ctrl,
          METH_VARARGS | METH_KEYWORDS,
          "execute_ctrl(x, ctrl) -> ndarray\n"
          "\n"
          "Convert a block, steering the cascade's fractional stage.\n"
          "\n"
          "The fixed integer stages (HalfbandDecimator / CIC) run\n"
          "unchanged; the scalar rate deviation ``ctrl`` is forwarded to\n"
          "the terminal Resampler stage's accumulator, so its effective\n"
          "rate becomes ``stage_rate + ctrl`` for this call. A timing or\n"
          "rate-tracking loop updates ``ctrl`` per block to align strobes\n"
          "after cheap integer decimation. No-op (falls through to\n"
          "``execute``) when the cascade has no terminal Resampler stage.\n"
          "\n"
          "The returned array is a view into a buffer reused on the next\n"
          "call.\n"
          "\n"
          "Parameters\n"
          "----------\n"
          "x : array_like, complex64\n"
          "    Input samples.\n"
          "ctrl : float\n"
          "    Rate deviation added to the terminal Resampler stage's rate.\n"
          "\n"
          "Returns\n"
          "-------\n"
          "ndarray, complex64\n"
          "    Output samples.\n"
          "\n"
          "Examples\n"
          "--------\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler.resample import RateConverter\n"
          "    >>> rc = RateConverter(0.5)\n"
          "    >>> y = rc.execute_ctrl(np.ones(256, dtype=np.complex64), "
          "0.0)\n"
          "    >>> y.dtype == np.complex64\n"
          "    True\n" },
        { "execute_ctrl_push",
          (PyCFunction)(void *)RateConverterObj_execute_ctrl_push,
          METH_VARARGS | METH_KEYWORDS,
          "execute_ctrl_push(x, ctrl) -> ndarray\n"
          "\n"
          "Push ONE input sample; return whatever outputs it completes.\n"
          "\n"
          "The per-input form of ``execute_ctrl`` — and the only form a\n"
          "closed loop can use, because a block call has to know its whole\n"
          "``ctrl`` history up front while a timing loop computes each\n"
          "correction *from* the outputs already emitted. Feeding a stream\n"
          "one sample at a time reproduces ``execute_ctrl`` on the same\n"
          "block bit-for-bit when ``ctrl`` is held constant.\n"
          "\n"
          "Returns 0 samples (a decimator between strobes — the common\n"
          "case), 1, or several. The array is independent, not a view.\n"
          "\n"
          "Parameters\n"
          "----------\n"
          "x : complex\n"
          "    One input sample.\n"
          "ctrl : float\n"
          "    Rate deviation added to the terminal Resampler stage's rate.\n"
          "\n"
          "Returns\n"
          "-------\n"
          "ndarray, complex64\n"
          "    The outputs this input completed; possibly empty.\n"
          "\n"
          "Examples\n"
          "--------\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler.resample import RateConverter\n"
          "    >>> rc = RateConverter(0.5)\n"
          "    >>> n = sum(len(rc.execute_ctrl_push(1 + 0j, 0.0))\n"
          "    ...         for _ in range(256))\n"
          "    >>> n\n"
          "    128\n" },
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
        { "clipped", (getter)RateConverterObj_get_clipped, NULL,
          "True if any planned CIC stage has clipped its input since the\n"
          "last reset().\n"
          "\n"
          "The cascade inherits the CIC's input bound (|Re|, |Im| <= 1.0)\n"
          "whenever `stages` names a CIC -- any decimation by 8 or more.\n"
          "The clip is invisible in the samples (finite, no NaN, merely\n"
          "distorted), so this is the only reliable check. Always False\n"
          "for a cascade with no CIC stage: those plans are scale-free.",
          NULL },
        { "bank_shape", (getter)RateConverterObj_get_bank_shape, NULL,
          "(num_phases, num_taps) of the terminal polyphase stage, or None\n"
          "when the cascade ends in an integer decimator.\n"
          "\n"
          "num_taps is the per-output MAC count and, times num_phases, the\n"
          "bank's size in floats. With pulse=, it is set by the terminal\n"
          "stage's rate, not the input rate -- which is what keeps a matched\n"
          "filter affordable at a high input samples-per-symbol.",
          NULL },
        { NULL } };

static PyTypeObject RateConverterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.RateConverter",
  .tp_basicsize                           = sizeof (RateConverterObject),
  .tp_dealloc = (destructor)RateConverterObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "RateConverter(rate=1.0, compensate=0, pulse='none',\n"
                "              beta=0.35, span=8, pulse_sps=2.0,\n"
                "              num_phases=1024)\n"
                "\n"
                "Optimal-speed rate conversion cascade.\n"
                "\n"
                "Selects the cheapest cascade of CIC, HalfbandDecimator, and/or\n"
                "polyphase Resampler stages at creation time.\n"
                "\n"
                "Input amplitude is bounded whenever the plan contains a\n"
                "CIC stage -- any decimation by 8 or more: |Re| and |Im| <=\n"
                "1.0, clipped beyond that, before any filtering, silently.\n"
                "`stages` is how you tell: a plan naming CIC(...) is not\n"
                "scale-free, every other plan is.\n"
                "\n"
                "Parameters\n"
                "----------\n"
                "rate : float\n"
                "    Output-to-input sample rate ratio.  Any positive float.\n"
                "compensate : int\n"
                "    Non-zero to correct CIC passband droop.  With\n"
                "    pulse='none' this appends a compensating FIR; with a\n"
                "    pulse it folds into the bank instead (no extra stage),\n"
                "    where it is worth ~28 dB of EVM.\n"
                "pulse : {'none', 'rrc', 'iandd'}\n"
                "    Shape of the terminal stage's polyphase bank.  'none'\n"
                "    is the plain Kaiser anti-alias bank (pure rate\n"
                "    conversion); anything else makes the cascade its own\n"
                "    matched filter -- one dot product converts the rate and\n"
                "    matched-filters, and the stage's arm is the fractional\n"
                "    timing delay execute_ctrl steers.  Selecting a pulse\n"
                "    also guarantees that terminal stage exists.\n"
                "beta : float\n"
                "    RRC roll-off in [0, 1].  Ignored for 'iandd'.\n"
                "span : int\n"
                "    One-sided RRC span in symbols.  Ignored for 'iandd',\n"
                "    whose support is always exactly one symbol.\n"
                "pulse_sps : float\n"
                "    The pulse's period in OUTPUT samples.  A shape\n"
                "    parameter, not a rate-planning one: the planner knows\n"
                "    nothing of symbols, so a caller wanting m samples per\n"
                "    symbol at sps asks for rate=m/sps, pulse_sps=m.\n"
                "num_phases : int\n"
                "    Terminal-stage arms; a power of two.  Sets the timing\n"
                "    resolution to 1/num_phases of an output period.\n"
                "\n"
                "Attributes\n"
                "----------\n"
                "rate : float\n"
                "    Current rate ratio (writable; rebuilds cascade on set).\n"
                "stages : list of str\n"
                "    Human-readable stage labels.\n"
                "bank_shape : tuple of int, or None\n"
                "    (num_phases, num_taps) of the terminal polyphase stage.\n"
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
