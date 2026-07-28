/*
 * resample_ext_RateConverter.c — RateConverter type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* RateConverterObject — wraps RateConverter_state_t *       */
/* ======================================================== */

#include "RateConverter/RateConverter_core.h"

typedef struct
{
  PyObject_HEAD RateConverter_state_t *handle;
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* allocated capacity for execute */
  void         **_execute_retired; /* gh-219 deferred free */
  size_t         _execute_retired_n;
  size_t         _execute_retired_cap;
  PyObject      *_execute_view_ref; /* gh-437 last returned view */
  float complex *_execute_ctrl_buf; /* pre-allocated output for execute_ctrl */
  size_t    _execute_ctrl_buf_cap;  /* allocated capacity for execute_ctrl */
  void    **_execute_ctrl_retired;  /* gh-219 deferred free */
  size_t    _execute_ctrl_retired_n;
  size_t    _execute_ctrl_retired_cap;
  PyObject *_execute_ctrl_view_ref; /* gh-437 last returned view */
  float complex
      *_execute_ctrl_push_buf; /* pre-allocated output for execute_ctrl_push */
  size_t _execute_ctrl_push_buf_cap;    /* allocated capacity for
                                           execute_ctrl_push */
  void    **_execute_ctrl_push_retired; /* gh-219 deferred free */
  size_t    _execute_ctrl_push_retired_n;
  size_t    _execute_ctrl_push_retired_cap;
  PyObject *_execute_ctrl_push_view_ref; /* gh-437 last returned view */
} RateConverterObject;

static void
RateConverterObj_dealloc (RateConverterObject *self)
{
  if (self->handle)
    RateConverter_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
  Py_XDECREF (self->_execute_view_ref);
  free (self->_execute_ctrl_buf);
  for (size_t _i = 0; _i < self->_execute_ctrl_retired_n; _i++)
    free (self->_execute_ctrl_retired[_i]);
  free (self->_execute_ctrl_retired);
  Py_XDECREF (self->_execute_ctrl_view_ref);
  free (self->_execute_ctrl_push_buf);
  for (size_t _i = 0; _i < self->_execute_ctrl_push_retired_n; _i++)
    free (self->_execute_ctrl_push_retired[_i]);
  free (self->_execute_ctrl_push_retired);
  Py_XDECREF (self->_execute_ctrl_push_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
RateConverterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  RateConverterObject *self = (RateConverterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
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
  self->handle = RateConverter_create (rate, compensate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "RateConverter: invalid parameter (need rate > 0, 0 "
                       "<= beta <= 1, span >= 1, pulse_sps > 0, num_phases a "
                       "power of two >= 2)");
      return -1;
    }
  {
    size_t _max = RateConverter_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_buf_cap = _max;
      }
  }
  {
    size_t _max = RateConverter_execute_ctrl_max_out (self->handle);
    if (_max)
      {
        self->_execute_ctrl_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_ctrl_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_ctrl_buf_cap = _max;
      }
  }
  {
    size_t _max = RateConverter_execute_ctrl_push_max_out (self->handle);
    if (_max)
      {
        self->_execute_ctrl_push_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_ctrl_push_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_ctrl_push_buf_cap = _max;
      }
  }
  return 0;
}

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

static PyObject *
RateConverterObj_execute (RateConverterObject *self, PyObject *args,
                          PyObject *kwds)
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
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (x_arr);
          return NULL;
        }
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
      size_t n_out = RateConverter_execute (
          self->handle, (const float complex *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr),
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
  size_t _need      = (size_t)PyArray_SIZE (x_arr);
  int    _view_live = 0;
  if (self->_execute_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_execute_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_execute_view_ref) != Py_None;
#endif
    }
  if (!self->_execute_buf || self->_execute_buf_cap < _need || _view_live)
    {
      size_t _max = RateConverter_execute_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_buf
          && self->_execute_retired_n == self->_execute_retired_cap)
        {
          size_t _rcap = self->_execute_retired_cap
                             ? self->_execute_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_retired     = _rt;
          self->_execute_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_buf)
        self->_execute_retired[self->_execute_retired_n++]
            = self->_execute_buf;
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  size_t n_out = RateConverter_execute (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), self->_execute_buf,
      self->_execute_buf_cap);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_execute_view_ref);
  self->_execute_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_execute_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
RateConverterObj_execute_ctrl (RateConverterObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "ctrl", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  double         ctrl      = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", _kwlist, &x_obj, &ctrl))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need      = (size_t)PyArray_SIZE (x_arr);
  int    _view_live = 0;
  if (self->_execute_ctrl_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_execute_ctrl_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live
          = PyWeakref_GetObject (self->_execute_ctrl_view_ref) != Py_None;
#endif
    }
  if (!self->_execute_ctrl_buf || self->_execute_ctrl_buf_cap < _need
      || _view_live)
    {
      size_t _max = RateConverter_execute_ctrl_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_ctrl_buf
          && self->_execute_ctrl_retired_n == self->_execute_ctrl_retired_cap)
        {
          size_t _rcap = self->_execute_ctrl_retired_cap
                             ? self->_execute_ctrl_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_ctrl_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_ctrl_retired     = _rt;
          self->_execute_ctrl_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_ctrl_buf)
        self->_execute_ctrl_retired[self->_execute_ctrl_retired_n++]
            = self->_execute_ctrl_buf;
      self->_execute_ctrl_buf     = _tmp;
      self->_execute_ctrl_buf_cap = _max;
    }
  size_t n_out = RateConverter_execute_ctrl (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), ctrl, self->_execute_ctrl_buf,
      self->_execute_ctrl_buf_cap);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_ctrl_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_execute_ctrl_view_ref);
  self->_execute_ctrl_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_execute_ctrl_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
RateConverterObj_execute_ctrl_push (RateConverterObject *self, PyObject *args,
                                    PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "ctrl", NULL };
  Py_complex   x_raw     = { 0.0, 0.0 };
  double       ctrl      = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Dd", _kwlist, &x_raw, &ctrl))
    return NULL;
  float complex x     = (float)x_raw.real + (float)x_raw.imag * I;
  size_t        _need = RateConverter_execute_ctrl_push_max_out (self->handle);
  int           _view_live = 0;
  if (self->_execute_ctrl_push_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_execute_ctrl_push_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live
          = PyWeakref_GetObject (self->_execute_ctrl_push_view_ref) != Py_None;
#endif
    }
  if (!self->_execute_ctrl_push_buf || self->_execute_ctrl_push_buf_cap < _need
      || _view_live)
    {
      size_t _max = RateConverter_execute_ctrl_push_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_ctrl_push_buf
          && self->_execute_ctrl_push_retired_n
                 == self->_execute_ctrl_push_retired_cap)
        {
          size_t _rcap = self->_execute_ctrl_push_retired_cap
                             ? self->_execute_ctrl_push_retired_cap * 2
                             : 4;
          void **_rt   = realloc (self->_execute_ctrl_push_retired,
                                  _rcap * sizeof (void *));
          if (!_rt)
            {
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_ctrl_push_retired     = _rt;
          self->_execute_ctrl_push_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_ctrl_push_buf)
        self->_execute_ctrl_push_retired[self->_execute_ctrl_push_retired_n++]
            = self->_execute_ctrl_push_buf;
      self->_execute_ctrl_push_buf     = _tmp;
      self->_execute_ctrl_push_buf_cap = _max;
    }
  size_t n_out = RateConverter_execute_ctrl_push (
      self->handle, x, ctrl, self->_execute_ctrl_push_buf,
      self->_execute_ctrl_push_buf_cap);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_ctrl_push_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_execute_ctrl_push_view_ref);
  self->_execute_ctrl_push_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_execute_ctrl_push_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  return arr;
}

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

static PyObject *
RateConverterObj_state_bytes (RateConverterObject *self,
                              PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (RateConverter_state_bytes (self->handle));
}

static PyObject *
RateConverterObj_get_state (RateConverterObject *self,
                            PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = RateConverter_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  RateConverter_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
RateConverterObj_set_state (RateConverterObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg)
      != RateConverter_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (RateConverter_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
RateConverter_getprop_rate (RateConverterObject *self,
                            void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (RateConverter_get_rate (self->handle));
}
static int
RateConverter_setprop_rate (RateConverterObject *self, PyObject *value,
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
  RateConverter_set_rate (self->handle, v);
  return 0;
}
static PyObject *
RateConverter_getprop_clipped (RateConverterObject *self,
                               void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(RateConverter_get_clipped (self->handle)));
}
static PyObject *
RateConverter_getprop_narrow_pulse (RateConverterObject *self,
                                    void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong (
      (long)(RateConverter_get_narrow_pulse (self->handle)));
}
static PyObject *
RateConverter_getprop_stages (RateConverterObject *self,
                              void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = RateConverter_num_stages (self->handle);
  PyObject *_c = PyList_New ((Py_ssize_t)_n);
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      const char *_r = RateConverter_stages_value (self->handle, _i);
      if (!_r)
        {
          PyErr_Format (
              PyExc_RuntimeError,
              "stages: RateConverter_stages_value returned NULL at index %zu",
              _i);
          Py_DECREF (_c);
          return NULL;
        }
      PyObject *_v = PyUnicode_FromString (_r);
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      PyList_SET_ITEM (_c, (Py_ssize_t)_i, _v);
    }
  return _c;
}
static PyObject *
RateConverter_getprop_bank_shape (RateConverterObject *self,
                                  void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = RateConverter_num_bank_shape (self->handle);
  PyObject *_c = PyList_New ((Py_ssize_t)_n);
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      PyObject *_v = PyLong_FromUnsignedLongLong (
          (unsigned long long)RateConverter_bank_shape_value (self->handle,
                                                              _i));
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      PyList_SET_ITEM (_c, (Py_ssize_t)_i, _v);
    }
  return _c;
}

static PyGetSetDef RateConverter_getset[] = {
  { "rate", (getter)RateConverter_getprop_rate,
    (setter)RateConverter_setprop_rate,
    "Get / set the output-to-input sample rate ratio. The setter rebuilds the "
    "entire cascade (new stage selection, new sub-objects) and resets all "
    "filter memories — equivalent to destroying and recreating with the new "
    "rate. Setting rate <= 0 is silently ignored.\n",
    NULL },
  { "clipped", (getter)RateConverter_getprop_clipped, NULL,
    "True if any planned CIC stage has clipped its input since the last "
    "`reset()`. The cascade inherits the CIC's input bound (`|Re|`, `|Im| <= "
    "1.0`) whenever `stages` names a CIC -- any decimation by 8 or more. The "
    "clip is invisible in the samples (finite, no NaN, merely distorted), so "
    "this is the only reliable check, and it is free: the boundary "
    "comparisons run on every sample regardless. Always False for a cascade "
    "with no CIC stage -- those plans are scale-free.\n",
    NULL },
  { "narrow_pulse", (getter)RateConverter_getprop_narrow_pulse, NULL,
    "True when a rectangular pulse was selected with fewer than four output "
    "samples per symbol, where its matched filter degenerates to a 2-3 tap "
    "sum. Construction also raises a UserWarning; this is the same diagnostic "
    "to pull rather than catch. Always False for `pulse=\"rrc\"` and for a "
    "plain converter.\n",
    NULL },
  { "stages", (getter)RateConverter_getprop_stages, NULL,
    "Stage labels for the planned cascade, e.g. `['CIC(8)', "
    "'Resampler(0.8)']`. A terminal stage carrying a pulse-shaped bank names "
    "its pulse: `'Resampler(0.923077,rrc)'`.\n",
    NULL },
  { "bank_shape", (getter)RateConverter_getprop_bank_shape, NULL,
    "`[num_phases, num_taps]` of the terminal polyphase stage, or `[]` when "
    "the cascade ends in an integer decimator and so has no bank to describe. "
    "`num_taps` is the per-output MAC count and, times `num_phases`, the "
    "bank's size in floats. With a pulse selected it is set by the terminal "
    "stage's rate rather than the input rate -- which is what keeps a matched "
    "filter affordable at a high input samples-per-symbol: the same 34 taps "
    "per arm at 4 samples/symbol and at 256, where filtering at the input "
    "rate would need 4225.\n",
    NULL },
  { NULL }
};

static PyObject *
RateConverterObj_destroy (RateConverterObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }
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
  Py_RETURN_NONE;
}

static PyMethodDef RateConverterObj_methods[] = {

  { "execute", (PyCFunction)RateConverterObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Convert a block of CF32 samples through the cascade. Passes input "
    "through each stage in order, ping-ponging between two intermediate "
    "buffers. State persists between calls, so contiguous calls on sequential "
    "blocks give the same result as one large call. Output length is "
    "approximately n_in * rate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateConverter\n"
    "    >>> obj = RateConverter(1.0, 0)\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_max_out", (PyCFunction)RateConverterObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "execute_ctrl", (PyCFunction)RateConverterObj_execute_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl(x) -> ndarray\n"
    "\n"
    "Convert a block, steering the cascade's fractional stage by ctrl.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateConverter\n"
    "    >>> obj = RateConverter(1.0, 0)\n"
    "    >>> y = obj.execute_ctrl(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_ctrl_push", (PyCFunction)RateConverterObj_execute_ctrl_push,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl_push(n=1) -> ndarray\n"
    "\n"
    "Push ONE input sample; emit whatever outputs it completes.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import RateConverter\n"
    "    >>> obj = RateConverter(1.0, 0)\n"
    "    >>> y = obj.execute_ctrl_push(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)RateConverterObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero all sub-stage filter memories. Rate, stage count, and stage types "
    "are preserved. Processing from a reset state produces the same output as "
    "a freshly created converter fed the same input. Use between signal "
    "bursts to suppress transient artefacts from prior filter memory.\n"
    "\n"
    "    >>> from doppler import RateConverter\n"
    "    >>> obj = RateConverter(1.0, 0)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)RateConverterObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)RateConverterObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)RateConverterObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)RateConverterObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)RateConverterObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)RateConverterObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject RateConverterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.RateConverter",
  .tp_basicsize                           = sizeof (RateConverterObject),
  .tp_dealloc = (destructor)RateConverterObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a rate converter for the given output/input rate ratio. Selects "
    "the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase "
    "Resampler stages at construction time (see file header for the selection "
    "table). Setting compensate=1 appends a closed-form Molnar-Vucic CIC "
    "droop-compensating FIR after any CIC stage, which improves passband "
    "flatness at the cost of one extra FIR stage.\n",
  .tp_methods = RateConverterObj_methods,
  .tp_getset  = RateConverter_getset,
  .tp_new     = RateConverterObj_new,
  .tp_init    = (initproc)RateConverterObj_init,
};
