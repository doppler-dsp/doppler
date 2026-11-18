/* jm:generated ofdm_ext_ofdm.c */
/*
 * ofdm_ext_ofdm.c — Ofdm type for the ofdm module.
 *
 * Included by ofdm_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in ofdm_ext_ofdm_extra.c.
 * Do NOT compile this file directly — only ofdm_ext.c is compiled.
 */
/* ======================================================== */
/* OfdmObject — wraps dp_ofdm_state_t *       */
/* ======================================================== */

#include "doppler/ofdm/ofdm_core.h"

typedef struct
{
  PyObject_HEAD dp_ofdm_state_t *handle;
} OfdmObject;

static void
OfdmObj_dealloc (OfdmObject *self)
{
  if (self->handle)
    dp_ofdm_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
OfdmObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  OfdmObject *self = (OfdmObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
OfdmObj_init (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  (void)args;
  (void)kwds;
  self->handle = dp_ofdm_create ();
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "Ofdm: out of memory");
      return -1;
    }
  return 0;
}

static PyObject *
OfdmObj_reset (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_ofdm_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
OfdmObj_add_class (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "n_fft",  "subcarrier_map", "useful",  "cyclic_prefix",
          "window", "scale",          "n_phase", NULL };
  long long n_fft_raw          = 0LL;
  PyObject *subcarrier_map_obj = NULL;
  long long useful_raw         = 0;
  long long cyclic_prefix_raw  = 0;
  long long window_raw         = 0;
  double    scale              = 1.0;
  long long n_phase_raw        = 1;
  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "LO|LLLdL", _kwlist, &n_fft_raw, &subcarrier_map_obj,
          &useful_raw, &cyclic_prefix_raw, &window_raw, &scale, &n_phase_raw))
    return NULL;
  if (n_fft_raw < 0 || n_fft_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "n_fft: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)n_fft_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t n_fft = (uint32_t)n_fft_raw;
  if (useful_raw < 0 || useful_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "useful: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)useful_raw, (long long)0,
                    (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t useful = (uint32_t)useful_raw;
  if (cyclic_prefix_raw < 0 || cyclic_prefix_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "cyclic_prefix: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)cyclic_prefix_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t cyclic_prefix = (uint32_t)cyclic_prefix_raw;
  if (window_raw < 0 || window_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "window: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)window_raw, (long long)0,
                    (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t window = (uint32_t)window_raw;
  if (n_phase_raw < 0 || n_phase_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "n_phase: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)n_phase_raw, (long long)0,
                    (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t       n_phase            = (uint32_t)n_phase_raw;
  PyArrayObject *subcarrier_map_arr = jm_array_arg (
      subcarrier_map_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "subcarrier_map");
  if (!subcarrier_map_arr)
    {
      return NULL;
    }
  const uint8_t *subcarrier_map
      = (const uint8_t *)PyArray_DATA (subcarrier_map_arr);
  size_t subcarrier_map_len = (size_t)PyArray_SIZE (subcarrier_map_arr);
  if (!self->handle)
    {
      Py_DECREF (subcarrier_map_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_ofdm_add_class (self->handle, n_fft, subcarrier_map,
                               subcarrier_map_len, useful, cyclic_prefix,
                               window, scale, n_phase);
  Py_DECREF (subcarrier_map_arr);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a class: n_fft is under 2, subcarrier_map "
                    "is not n_fft long or holds a value other than 0 or 1, "
                    "the cyclic prefix is longer than the useful length, the "
                    "window is longer than the symbol, or n_phase is 0",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
OfdmObj_add_subcarrier_gain (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]       = { "class_index", "gain", NULL };
  long long    class_index_raw = 0LL;
  PyObject    *gain_obj        = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "LO", _kwlist,
                                    &class_index_raw, &gain_obj))
    return NULL;
  if (class_index_raw < 0 || class_index_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "class_index: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)class_index_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t       class_index = (uint32_t)class_index_raw;
  PyArrayObject *gain_arr
      = jm_array_arg (gain_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "gain");
  if (!gain_arr)
    {
      return NULL;
    }
  const float _Complex *gain = (const float _Complex *)PyArray_DATA (gain_arr);
  size_t                gain_len = (size_t)PyArray_SIZE (gain_arr);
  if (!self->handle)
    {
      Py_DECREF (gain_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_ofdm_add_subcarrier_gain (self->handle, class_index, gain,
                                         gain_len);
  Py_DECREF (gain_arr);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot add gains: class_index names no class, the class "
                    "already has gains, or gain is not n_fft long",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
OfdmObj_add_polarity (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]       = { "class_index", "polarity", NULL };
  long long    class_index_raw = 0LL;
  PyObject    *polarity_obj    = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "LO", _kwlist,
                                    &class_index_raw, &polarity_obj))
    return NULL;
  if (class_index_raw < 0 || class_index_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "class_index: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)class_index_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t       class_index  = (uint32_t)class_index_raw;
  PyArrayObject *polarity_arr = jm_array_arg (
      polarity_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "polarity");
  if (!polarity_arr)
    {
      return NULL;
    }
  const float _Complex *polarity
      = (const float _Complex *)PyArray_DATA (polarity_arr);
  size_t polarity_len = (size_t)PyArray_SIZE (polarity_arr);
  if (!self->handle)
    {
      Py_DECREF (polarity_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_ofdm_add_polarity (self->handle, class_index, polarity,
                                  polarity_len);
  Py_DECREF (polarity_arr);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot add a polarity sequence: class_index names no "
                    "class, the class already has one, or it is empty",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
OfdmObj_add_fixed (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "class_index", "phase", "k", "value", NULL };
  long long    class_index_raw = 0LL;
  long long    phase_raw       = 0LL;
  long long    k_raw           = 0LL;
  Py_complex   value_raw       = { 0.0, 0.0 };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "LLLD", _kwlist,
                                    &class_index_raw, &phase_raw, &k_raw,
                                    &value_raw))
    return NULL;
  if (class_index_raw < 0 || class_index_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "class_index: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)class_index_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t class_index = (uint32_t)class_index_raw;
  if (phase_raw < 0 || phase_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "phase: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)phase_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t phase = (uint32_t)phase_raw;
  if (k_raw < (long long)INT32_MIN || k_raw > (long long)INT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "k: %lld is out of range for int32_t [%lld, %lld]",
                    (long long)k_raw, (long long)INT32_MIN,
                    (long long)INT32_MAX);
      return NULL;
    }
  int32_t k             = (int32_t)k_raw;
  double _Complex value = value_raw.real + value_raw.imag * I;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_ofdm_add_fixed (self->handle, class_index, phase, k, value);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a fixed entry: class_index names no "
                    "class, phase is not below n_phase, or k is outside "
                    "-n_fft/2 .. n_fft/2 - 1",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
OfdmObj_add_run (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "class_index",     "count",       "cyclic_prefix",
                             "polarity_offset", "first_phase", NULL };
  long long    class_index_raw     = 0LL;
  long long    count_raw           = 0LL;
  long long    cyclic_prefix_raw   = 4294967295;
  long long    polarity_offset_raw = 0;
  long long    first_phase_raw     = 0;
  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "LL|LLL", _kwlist, &class_index_raw, &count_raw,
          &cyclic_prefix_raw, &polarity_offset_raw, &first_phase_raw))
    return NULL;
  if (class_index_raw < 0 || class_index_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "class_index: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)class_index_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t class_index = (uint32_t)class_index_raw;
  if (count_raw < 0 || count_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "count: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)count_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t count = (uint32_t)count_raw;
  if (cyclic_prefix_raw < 0 || cyclic_prefix_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "cyclic_prefix: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)cyclic_prefix_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t cyclic_prefix = (uint32_t)cyclic_prefix_raw;
  if (polarity_offset_raw < 0 || polarity_offset_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "polarity_offset: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)polarity_offset_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t polarity_offset = (uint32_t)polarity_offset_raw;
  if (first_phase_raw < 0 || first_phase_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "first_phase: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)first_phase_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t first_phase = (uint32_t)first_phase_raw;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_ofdm_add_run (self->handle, class_index, count, cyclic_prefix,
                             polarity_offset, first_phase);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a run: class_index names no class, count "
                    "is 0, first_phase is not below n_phase, or the prefix "
                    "does not fit the class's useful length",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
OfdmObj_check (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  int         _rc  = dp_ofdm_check_why (self->handle, &_why);
  if (_rc != 0)
    {
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                      "the layout is not usable", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
OfdmObj_points_remaining (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_ofdm_points_remaining (self->handle);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
OfdmObj_modulate_max_out (OfdmObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n_in = 0;
  if (!PyArg_ParseTuple (args, "n", &n_in))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_ofdm_modulate_why_max_out (self->handle, (size_t)n_in));
}

static PyObject *
OfdmObj_modulate (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_ofdm_modulate_why_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      const char *_why = NULL;
      int64_t     _rc  = dp_ofdm_modulate_why (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap, &_why);
      Py_DECREF (in_arr);
      if (_rc < 0)
        {
          Py_DECREF (out_arr);
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "modulate refused",
                          (long long)_rc);
          return NULL;
        }
      size_t n_out = (size_t)_rc;
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Ofdm.modulate: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_ofdm_modulate_why_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Ofdm.modulate: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  int64_t     _rc  = dp_ofdm_modulate_why (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap, &_why);
  Py_DECREF (in_arr);
  if (_rc < 0)
    {
      Py_DECREF (arr0);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "modulate refused",
                      (long long)_rc);
      return NULL;
    }
  size_t n_out = (size_t)_rc;
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Ofdm.modulate: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
OfdmObj_finish_max_out (OfdmObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_ofdm_finish_max_out (self->handle, (size_t)n));
}

static PyObject *
OfdmObj_finish (OfdmObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = (Py_ssize_t)(self->handle->tail_len);
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_ofdm_finish_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t n_out
          = dp_ofdm_finish (self->handle, (size_t)n,
                            (float _Complex *)PyArray_DATA (out_arr), _cap);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Ofdm.finish: wrote %zu elements into a buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_ofdm_finish_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "Ofdm.finish: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t n_out = dp_ofdm_finish (self->handle, (size_t)n, _d0, _cap);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Ofdm.finish: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
OfdmObj_state_bytes (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_ofdm_state_bytes (self->handle));
}

static PyObject *
OfdmObj_get_state (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_ofdm_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_ofdm_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
OfdmObj_set_state (OfdmObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != dp_ofdm_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_ofdm_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Ofdm_getprop_run_index (OfdmObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->run_index);
}
static PyObject *
Ofdm_getprop_sym_in_run (OfdmObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->sym_in_run);
}
static PyObject *
Ofdm_getprop_tail_len (OfdmObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->tail_len);
}

static PyGetSetDef Ofdm_getset[]
    = { { "run_index", (getter)Ofdm_getprop_run_index, NULL,
          "The run the next OFDM symbol belongs to; the number of runs once "
          "the layout is rendered.\n",
          NULL },
        { "sym_in_run", (getter)Ofdm_getprop_sym_in_run, NULL,
          "OFDM symbols already rendered of the run `run_index`.\n", NULL },
        { "tail_len", (getter)Ofdm_getprop_tail_len, NULL,
          "Samples of window tail held for the next OFDM symbol.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
OfdmObj_destroy (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_ofdm_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
OfdmObj_enter (OfdmObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
OfdmObj_exit (OfdmObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_ofdm_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef OfdmObj_methods[] = {
  { "reset", (PyCFunction)OfdmObj_reset, METH_NOARGS,
    "Return to the first OFDM symbol, keeping the layout.\n"
    "\n"
    "The next render starts from the post-seal state: the first run, no held\n"
    "tail. The layout, sealed or not, is unchanged.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o = Ofdm()\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 2)\n"
    "0\n"
    ">>> x = o.modulate(np.ones(12, np.complex64))\n"
    ">>> o.reset()\n"
    ">>> o.points_remaining()\n"
    "12\n" },

  { "add_class", (PyCFunction)(void *)OfdmObj_add_class,
    METH_VARARGS | METH_KEYWORDS,
    "add_class(n_fft, subcarrier_map, useful, cyclic_prefix, window, scale, "
    "n_phase) -> int\n"
    "\n"
    "Append one kind of OFDM symbol and return its index. `n_fft` is the "
    "transform length of this class; `subcarrier_map` has `n_fft` entries, "
    "index `k + n_fft/2`, 1 where the subcarrier may carry a constellation "
    "point and 0 where it carries nothing the caller supplies. `useful` is "
    "how many samples are read cyclically from the inverse transform (0 reads "
    "as `n_fft`: a training field of ten short periods reads 160 of 64). "
    "`cyclic_prefix` is the default prefix in samples, the last samples of "
    "the body copied ahead of it. `window` is the transition into and out of "
    "this class in samples. `scale` multiplies the transform output. "
    "`n_phase` is the period of the class's fixed pattern in OFDM symbols. A "
    "class's per-subcarrier gains and polarity sequence are added by "
    "`add_subcarrier_gain` and `add_polarity`.\n"
    "\n"
    "Copies subcarrier_map. The optional per-subcarrier gains and the\n"
    "polarity sequence are added to the class by\n"
    "dp_ofdm_add_subcarrier_gain() and dp_ofdm_add_polarity(). Refuses (a\n"
    "negative return, nothing changed) what cannot be honoured; a sealed\n"
    "layout is re-opened.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_fft : int\n"
    "    Transform length, 2 to OFDM_N_FFT_MAX.\n"
    "subcarrier_map : npt.NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    `n_fft` entries, index `k + n_fft/2`, each OFDM_DATA or OFDM_NULL.\n"
    "useful : int\n"
    "    Body samples read cyclically from the transform; 0 reads as n_fft.\n"
    "cyclic_prefix : int\n"
    "    Default prefix, samples; at most `useful`.\n"
    "window : int\n"
    "    Window transition, samples; at most the symbol length.\n"
    "scale : float\n"
    "    Multiplies the transform output.\n"
    "n_phase : int\n"
    "    Period of the fixed pattern, at least 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The class index, or a negative value if refused.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``cannot append a class: n_fft is under 2, subcarrier_map is not\n"
    "    n_fft long or holds a value other than 0 or 1, the cyclic prefix is\n"
    "    longer than the useful length, the window is longer than the\n"
    "    symbol, or n_phase is 0``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> o = Ofdm()\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_class(8, m, cyclic_prefix=2, window=1)\n"
    "1\n" },
  { "add_subcarrier_gain", (PyCFunction)(void *)OfdmObj_add_subcarrier_gain,
    METH_VARARGS | METH_KEYWORDS,
    "add_subcarrier_gain(class_index, gain) -> None\n"
    "\n"
    "Give a class a complex multiplier per subcarrier: `n_fft` values, index "
    "`k + n_fft/2`, applied to the whole grid, data and fixed entries alike. "
    "It carries a per-subband phase rotation or any per-subcarrier amplitude. "
    "Without it the gain is 1 everywhere. Refuses if the class already has "
    "gains.\n"
    "\n"
    "Applied to the whole grid (data and fixed entries alike): a per-subband\n"
    "phase rotation, or any per-subcarrier amplitude. Without it the gain is\n"
    "1 everywhere.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "class_index : int\n"
    "    An existing class without gains yet.\n"
    "gain : npt.NDArray[np.complex64]\n"
    "    `n_fft` multipliers, index `k + n_fft/2`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``cannot add gains: class_index names no class, the class already\n"
    "    has gains, or gain is not n_fft long``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o = Ofdm()\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 1)\n"
    "0\n"
    ">>> pts = np.ones(6, np.complex64)\n"
    ">>> one = o.modulate(pts)\n"
    ">>> o.reset()\n"
    ">>> o.add_subcarrier_gain(0, 2 * np.ones(8, np.complex64))\n"
    ">>> bool(np.allclose(o.modulate(pts), 2 * one))\n"
    "True\n" },
  { "add_polarity", (PyCFunction)(void *)OfdmObj_add_polarity,
    METH_VARARGS | METH_KEYWORDS,
    "add_polarity(class_index, polarity) -> None\n"
    "\n"
    "Give a class a polarity sequence: a complex scalar per OFDM symbol that "
    "multiplies the class's fixed entries, never its data. OFDM symbol `n` of "
    "a run uses element `(polarity_offset + n) mod len`. Without it the "
    "scalar is 1. Refuses if the class already has a sequence.\n"
    "\n"
    "OFDM symbol `n` of a run multiplies its fixed entries (never its data)\n"
    "by element `(polarity_offset + n) mod polarity_len`. Without it the\n"
    "scalar is 1.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "class_index : int\n"
    "    An existing class without a sequence yet.\n"
    "polarity : npt.NDArray[np.complex64]\n"
    "    The sequence.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``cannot add a polarity sequence: class_index names no class, the\n"
    "    class already has one, or it is empty``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> z = np.zeros(8, np.uint8)               # no data subcarriers at "
    "all\n"
    ">>> p = Ofdm(); p.add_class(8, z, cyclic_prefix=2)\n"
    "0\n"
    ">>> p.add_fixed(0, 2**32 - 1, 2, 1.0)      # one pilot, every phase\n"
    ">>> p.add_polarity(0, np.array([1, -1], np.complex64))\n"
    ">>> p.add_run(0, 2)\n"
    "0\n"
    ">>> x = p.modulate(np.empty(0, np.complex64))\n"
    ">>> bool(np.allclose(x[2:10], -x[12:20]))   # the pilot flips sign\n"
    "True\n" },
  { "add_fixed", (PyCFunction)(void *)OfdmObj_add_fixed,
    METH_VARARGS | METH_KEYWORDS,
    "add_fixed(class_index, phase, k, value) -> None\n"
    "\n"
    "Append one fixed entry to a class: a pilot, a training value, a boosted "
    "pilot. `phase` is the pattern phase it belongs to, or 2**32 - 1 "
    "(`OFDM_ALL_PHASES` in C) for every phase. `k` is the subcarrier, from "
    "`-n_fft/2` to `n_fft/2 - 1`. A fixed entry displaces data on its "
    "subcarrier in its phase, and is multiplied by the class's polarity "
    "element for the OFDM symbol.\n"
    "\n"
    "A fixed entry displaces data on its subcarrier in its phase (when the\n"
    "map has data there) and is multiplied by the class's polarity element.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "class_index : int\n"
    "    An existing class.\n"
    "phase : int\n"
    "    The pattern phase it belongs to, below the class's `n_phase`, or\n"
    "    OFDM_ALL_PHASES.\n"
    "k : int\n"
    "    Subcarrier, `-n_fft/2 .. n_fft/2 - 1`.\n"
    "value : complex\n"
    "    The value placed.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``cannot append a fixed entry: class_index names no class, phase is\n"
    "    not below n_phase, or k is outside -n_fft/2 .. n_fft/2 - 1``, with\n"
    "    the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o = Ofdm()\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 1)\n"
    "0\n"
    ">>> o.points_remaining()      # six data subcarriers\n"
    "6\n"
    ">>> o.add_fixed(0, 2**32 - 1, 3, 1j)   # a pilot on a data subcarrier\n"
    ">>> o.points_remaining()      # it displaces one\n"
    "5\n" },
  { "add_run", (PyCFunction)(void *)OfdmObj_add_run,
    METH_VARARGS | METH_KEYWORDS,
    "add_run(class_index, count, cyclic_prefix, polarity_offset, first_phase) "
    "-> int\n"
    "\n"
    "Append a run of `count` OFDM symbols of one class. `cyclic_prefix` "
    "overrides the class's prefix for this run (omit it, or give 4294967295 = "
    "2**32 - 1, `OFDM_CP_DEFAULT` in C, to keep the class's; a named default "
    "is just-buildit/just-makeit#2679); `polarity_offset` is the class's "
    "polarity element used by the run's first symbol; `first_phase` is the "
    "pattern phase of the run's first symbol. Returns the run's index.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "class_index : int\n"
    "    An existing class.\n"
    "count : int\n"
    "    OFDM symbols, at least 1.\n"
    "cyclic_prefix : int\n"
    "    Override of the class's prefix, or OFDM_CP_DEFAULT; at most the\n"
    "    class's `useful`.\n"
    "polarity_offset : int\n"
    "    Polarity element of the run's first symbol.\n"
    "first_phase : int\n"
    "    Pattern phase of the run's first symbol.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The run index, or a negative value if refused.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``cannot append a run: class_index names no class, count is 0,\n"
    "    first_phase is not below n_phase, or the prefix does not fit the\n"
    "    class's useful length``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o = Ofdm()\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 2)\n"
    "0\n"
    ">>> o.add_run(0, 3)\n"
    "1\n" },
  { "check", (PyCFunction)OfdmObj_check, METH_NOARGS,
    "check() -> None\n"
    "\n"
    "Check the whole layout and return 0 if it is usable; raise ValueError, "
    "with a sentence naming the condition, if not (no run, a window longer "
    "than an OFDM symbol of its class, two fixed entries on one subcarrier in "
    "one phase, or memory or a transform plan that could not be built). "
    "`modulate` checks on first use and raises `ValueError`, with the same "
    "sentence, for a layout that fails; call this first to learn it before "
    "any points exist. Adding a class, fixed entry or run to a checked layout "
    "re-opens it, keeping the position and the held window tail, so a signal "
    "can be extended while it is rendered; the next `modulate` checks the "
    "longer layout.\n"
    "\n"
    "Refuses a layout with no run, a window longer than an OFDM symbol of\n"
    "its class, or two fixed entries on one subcarrier in one phase. Derives\n"
    "what the render uses (the data count per phase, the window weights, one\n"
    "inverse transform per distinct length). Idempotent once it has passed.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    the reason the C call gives, or ``the layout is not usable``, with\n"
    "    the return code appended (gh-869), when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> o = Ofdm()\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3]] = 1\n"
    ">>> o.add_class(8, m)\n"
    "0\n"
    ">>> o.check()                # no run yet\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: the layout has no run\n"
    ">>> o.add_run(0, 1)\n"
    "0\n"
    ">>> o.check()                # usable: returns None\n" },
  { "points_remaining", (PyCFunction)OfdmObj_points_remaining, METH_NOARGS,
    "points_remaining() -> int\n"
    "\n"
    "Constellation points the rest of the layout consumes, from the current "
    "position. Pass exactly that many to `modulate` to render the rest in one "
    "call.\n"
    "\n"
    "Zero for a layout that does not seal.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Points from the current position to the end of the layout.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o = Ofdm()\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 3)\n"
    "0\n"
    ">>> o.points_remaining()\n"
    "18\n" },
  { "modulate", (PyCFunction)(void *)OfdmObj_modulate,
    METH_VARARGS | METH_KEYWORDS,
    "modulate(x, out) -> ndarray\n"
    "\n"
    "Render OFDM symbols from constellation points, in layout order, and "
    "return the samples. Each OFDM symbol takes its data count from the "
    "points: its data subcarriers less those a fixed entry displaces in its "
    "pattern phase. Rendering stops when the next OFDM symbol needs more "
    "points than remain; OFDM symbols that need none (training fields) "
    "render. A refused call (an unusable layout, a point count that ends "
    "inside an OFDM symbol or runs past the layout, a buffer too short) "
    "raises `ValueError` saying which, the modulator untouched. The window "
    "tail of the last OFDM symbol is held for the next call; `finish` returns "
    "it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Constellation points, consumed in layout order.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Receives the samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Samples written, which may be 0: no points at the end of the\n"
    "    layout, or zero points mid-layout when the next OFDM symbol needs\n"
    "    some. A refused call changes nothing and reports it by face: C\n"
    "    returns a negative value and sets why; the Python binding raises\n"
    "    ValueError with that sentence.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    the reason the C call gives, or ``modulate refused``, with the\n"
    "    return code appended (gh-869), when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> o = Ofdm()\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 3)\n"
    "0\n"
    ">>> o.points_remaining()\n"
    "18\n"
    ">>> x = o.modulate(np.ones(12, np.complex64))   # two of the three\n"
    ">>> x.size\n"
    "20\n"
    ">>> o.points_remaining()\n"
    "6\n"
    ">>> o.modulate(np.ones(5, np.complex64))        # inside a symbol\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: the points end inside an OFDM symbol: pass a count that ends "
    "where one does (see points_remaining and modulate_max_out)\n" },
  { "modulate_max_out", (PyCFunction)OfdmObj_modulate_max_out, METH_VARARGS,
    "modulate_max_out(n_in) -> int\n"
    "\n"
    "Samples a render of n_in points writes.\n"
    "\n"
    "Simulates the render from the current position: OFDM symbols are taken\n"
    "in layout order until the next one needs more points than remain. The\n"
    "result is zero if n_in ends inside an OFDM symbol or runs past the\n"
    "layout (that call is refused), for a layout that does not seal, and for\n"
    "no points when the next OFDM symbol needs some. Use it to size the\n"
    "output before the call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Constellation points.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Samples that call will write.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> o = Ofdm()\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o.add_class(8, m, cyclic_prefix=2)\n"
    "0\n"
    ">>> o.add_run(0, 3)\n"
    "0\n"
    ">>> o.modulate_max_out(12)   # two OFDM symbols of 10 samples\n"
    "20\n"
    ">>> o.modulate_max_out(5)    # ends inside a symbol\n"
    "0\n" },
  { "finish", (PyCFunction)(void *)OfdmObj_finish,
    METH_VARARGS | METH_KEYWORDS,
    "finish(count=..., out=None) -> ndarray\n"
    "\n"
    "Return the window tail still held after the last OFDM symbol (empty when "
    "the window is 0 or nothing has been rendered) and clear it.\n"
    "\n"
    "After the last OFDM symbol this is the final fall of the window; with a\n"
    "window of 0, or before anything is rendered, there is none.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Receives the tail.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Samples written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1\n"
    ">>> o = Ofdm()\n"
    ">>> o.add_class(8, m, cyclic_prefix=2, window=2)\n"
    "0\n"
    ">>> o.add_run(0, 2)\n"
    "0\n"
    ">>> x = o.modulate(np.ones(12, np.complex64))\n"
    ">>> o.finish().size          # the window's last fall\n"
    "2\n"
    ">>> o.finish().size          # and it is cleared\n"
    "0\n" },
  { "finish_max_out", (PyCFunction)OfdmObj_finish_max_out, METH_VARARGS,
    "finish_max_out(n) -> int\n"
    "\n"
    "Capacity dp_ofdm_finish() needs for n samples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Samples asked for.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The held tail's length, at most n.\n" },
  { "state_bytes", (PyCFunction)OfdmObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Ofdm has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)OfdmObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures the state that evolves as the object runs, including any value\n"
    "a setter changed after construction, so a blob taken now and restored\n"
    "later resumes from this point. Construction parameters are not\n"
    "restored: restore into an object built the same way. A core may check\n"
    "some of them and refuse a blob built differently.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the Ofdm has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)OfdmObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place, including any value a setter\n"
    "changed after construction. Construction parameters are not restored:\n"
    "restore into an object built the same way. Length is validated against\n"
    "`state_bytes()` before the blob is handed to the C core, which may\n"
    "reject it too: a core may check some construction parameters and refuse\n"
    "a blob built differently.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Ofdm has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)OfdmObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)OfdmObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Ofdm be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Ofdm\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)OfdmObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Ofdm.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never\n"
    "suppresses one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject OfdmObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.ofdm.Ofdm",
  .tp_basicsize                           = sizeof (OfdmObject),
  .tp_dealloc                             = (destructor)OfdmObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create an empty modulator (no classes, no runs).\n"
    "\n"
    "Raises\n"
    "------\n"
    "MemoryError\n"
    "    If construction fails. The exception message is ``Ofdm: out of\n"
    "    memory``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler.ofdm import Ofdm\n"
    ">>> obj = Ofdm()\n",
  .tp_methods = OfdmObj_methods,
  .tp_getset  = Ofdm_getset,
  .tp_new     = OfdmObj_new,
  .tp_init    = (initproc)OfdmObj_init,
};
