/*
 * wfm_reader_ext_wfm_reader.c — Reader type for the wfm_reader module.
 *
 * Included by wfm_reader_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_reader_ext.c is compiled.
 */
/* ======================================================== */
/* ReaderObject — wraps wfm_reader_state_t *       */
/* ======================================================== */

#include "wfm_reader/wfm_reader_core.h"

/* The stop predicate the Python face installs below. The CORE deliberately
   does not call dp_interrupted() itself -- that would put dp_interrupt.c on
   the link line of every C consumer of wfm_reader_core, which is why
   read_follow() takes an injected predicate at all (end-of-capture.md 4c).
   A binding is the layer that CAN link it, and doppler's own answer to "what
   should stop a follow read?" is the process interrupt, exactly as
   wfm_reader_core.h's own example says. */
#include "dp_interrupt.h"

typedef struct
{
  PyObject_HEAD wfm_reader_state_t *handle;
} ReaderObject;

static void
ReaderObj_dealloc (ReaderObject *self)
{
  if (self->handle)
    wfm_reader_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ReaderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ReaderObject *self = (ReaderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ReaderObj_init (ReaderObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]        = { "path", "sample_type", "endian", NULL };
  PyObject    *path            = NULL; /* fspath -> bytes */
  const char  *sample_type_str = "cf32";
  const char  *endian_str      = "le";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&|ss", kwlist,
                                    PyUnicode_FSConverter, &path,
                                    &sample_type_str, &endian_str))
    {
      Py_XDECREF (path);
      return -1;
    }
  int sample_type = 0;
  if (strcmp (sample_type_str, "cf32") == 0)
    sample_type = 0;
  else if (strcmp (sample_type_str, "cf64") == 0)
    sample_type = 1;
  else if (strcmp (sample_type_str, "ci32") == 0)
    sample_type = 2;
  else if (strcmp (sample_type_str, "ci16") == 0)
    sample_type = 3;
  else if (strcmp (sample_type_str, "ci8") == 0)
    sample_type = 4;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "sample_type must be one of \"cf32\", \"cf64\", \"ci32\", "
                    "\"ci16\", \"ci8\", got '%s'",
                    sample_type_str);
      Py_XDECREF (path);
      return -1;
    }
  int endian = 0;
  if (strcmp (endian_str, "le") == 0)
    endian = 0;
  else if (strcmp (endian_str, "be") == 0)
    endian = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "endian must be one of \"le\", \"be\", got '%s'",
                    endian_str);
      Py_XDECREF (path);
      return -1;
    }
  self->handle
      = wfm_reader_create (PyBytes_AS_STRING (path), sample_type, endian);
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "cannot open capture: no such file, unrecognised file "
                       "type, or an unsupported BLUE format mode (only S and "
                       "C are supported)");
      return -1;
    }
  /* Make Ctrl+C end a follow read. Without this the predicate is NULL and
     read_follow() has no escape at all -- both budgets default to "forever"
     on purpose (a stream with no rhythm we control turns any finite budget
     into a spurious ending), so "no stop predicate" means "waits until the
     writer closes, whatever happens".

     This is only true across modules because dp_interrupt_guard is
     `process_global` (doppler#976): the flag Interrupt() sets in
     doppler.interrupt is the same object this module reads. Before that fix
     it would have been a different variable and this line would have looked
     like it worked. */
  wfm_reader_set_stop_fn (self->handle, dp_interrupted);
  return 0;
}

static PyObject *
ReaderObj_reset (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  wfm_reader_reset (self->handle);
  Py_RETURN_NONE;
}

/* gh-519: strcmp for the enum lookup below. Python.h already
 * pulls in <string.h>, but the include is explicit so the block
 * stands on its own wherever it is spliced. */
#include <string.h>

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index_Reader (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_Reader_follow_end[] = {
  "none", "eof", "timeout", "interrupted", NULL,
};

static const char *const _enum_Reader_ftype[] = {
  "raw", "csv", "blue", "sigmf", NULL,
};

static const char *const _enum_Reader_stype[] = {
  "cf32", "cf64", "ci32", "ci16", "ci8", NULL,
};

static const char *const _enum_Reader_sample_mode[] = {
  "complex",
  "scalar",
  NULL,
};

static const char *const _enum_Reader_endian[] = {
  "le",
  "be",
  NULL,
};

static const char *const _enum_Reader_fs_source[] = {
  "none",
  "xdelta",
  "core:sample_rate",
  NULL,
};

static const char *const _enum_Reader_t0_source[] = {
  "none",
  "timecode",
  NULL,
};

static const char *const _enum_Reader_fc_source[] = {
  "none", "FREQ", "RF_FREQ", "CENTER_FREQ", "F_C", "core:frequency", NULL,
};

static PyObject *
ReaderObj_read_max_out (ReaderObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  return PyLong_FromSize_t (wfm_reader_read_max_out (self->handle, (size_t)n));
}

static PyObject *
ReaderObj_read (ReaderObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = wfm_reader_read_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out
          = wfm_reader_read (self->handle, (size_t)n,
                             (float complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _need = (size_t)n;
  size_t _cap  = wfm_reader_read_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = wfm_reader_read (self->handle, (size_t)n, _d0, _cap);
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
ReaderObj_read_follow_max_out (ReaderObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  return PyLong_FromSize_t (
      wfm_reader_read_follow_max_out (self->handle, (size_t)n));
}

static PyObject *
ReaderObj_read_follow (ReaderObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = wfm_reader_read_follow_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      float complex *_ng0 = (float complex *)PyArray_DATA (out_arr);
      size_t         n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = wfm_reader_read_follow (self->handle, (size_t)n, _ng0, _cap);
      Py_END_ALLOW_THREADS
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
  size_t _need = (size_t)n;
  size_t _cap  = wfm_reader_read_follow_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  size_t n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = wfm_reader_read_follow (self->handle, (size_t)n, _d0, _cap);
  Py_END_ALLOW_THREADS
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
Reader_getprop_follow_timeout_ms (ReaderObject *self,
                                  void         *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong (
      (unsigned long)wfm_reader_get_follow_timeout_ms (self->handle));
}
static int
Reader_setprop_follow_timeout_ms (ReaderObject *self, PyObject *value,
                                  void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  unsigned long v_raw = 0UL;
  if (!PyArg_Parse (value, "k", &v_raw))
    return -1;
  uint32_t v = (uint32_t)v_raw;
  wfm_reader_set_follow_timeout_ms (self->handle, v);
  return 0;
}
static PyObject *
Reader_getprop_follow_grace_ms (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong (
      (unsigned long)wfm_reader_get_follow_grace_ms (self->handle));
}
static int
Reader_setprop_follow_grace_ms (ReaderObject *self, PyObject *value,
                                void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  unsigned long v_raw = 0UL;
  if (!PyArg_Parse (value, "k", &v_raw))
    return -1;
  uint32_t v = (uint32_t)v_raw;
  wfm_reader_set_follow_grace_ms (self->handle, v);
  return 0;
}
static PyObject *
Reader_getprop_ending (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_ending (self->handle));
  if (_v < 0 || _v >= 4)
    {
      PyErr_Format (PyExc_ValueError,
                    "ending holds out-of-range follow_end value %ld"
                    " (valid: 0..3)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_follow_end[_v]);
}
static PyObject *
Reader_getprop_file_type (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_file_type (self->handle));
  if (_v < 0 || _v >= 4)
    {
      PyErr_Format (PyExc_ValueError,
                    "file_type holds out-of-range ftype value %ld"
                    " (valid: 0..3)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_ftype[_v]);
}
static PyObject *
Reader_getprop_sample_type (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_sample_type (self->handle));
  if (_v < 0 || _v >= 5)
    {
      PyErr_Format (PyExc_ValueError,
                    "sample_type holds out-of-range stype value %ld"
                    " (valid: 0..4)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_stype[_v]);
}
static PyObject *
Reader_getprop_mode (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_mode (self->handle));
  if (_v < 0 || _v >= 2)
    {
      PyErr_Format (PyExc_ValueError,
                    "mode holds out-of-range sample_mode value %ld"
                    " (valid: 0..1)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_sample_mode[_v]);
}
static PyObject *
Reader_getprop_endian (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_endian (self->handle));
  if (_v < 0 || _v >= 2)
    {
      PyErr_Format (PyExc_ValueError,
                    "endian holds out-of-range endian value %ld"
                    " (valid: 0..1)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_endian[_v]);
}
static PyObject *
Reader_getprop_fs (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_reader_get_fs (self->handle));
}
static PyObject *
Reader_getprop_fc (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_reader_get_fc (self->handle));
}
static PyObject *
Reader_getprop_fs_source (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_fs_source (self->handle));
  if (_v < 0 || _v >= 3)
    {
      PyErr_Format (PyExc_ValueError,
                    "fs_source holds out-of-range fs_source value %ld"
                    " (valid: 0..2)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_fs_source[_v]);
}
static PyObject *
Reader_getprop_t0 (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_reader_get_t0 (self->handle));
}
static PyObject *
Reader_getprop_t0_source (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_t0_source (self->handle));
  if (_v < 0 || _v >= 2)
    {
      PyErr_Format (PyExc_ValueError,
                    "t0_source holds out-of-range t0_source value %ld"
                    " (valid: 0..1)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_t0_source[_v]);
}
static PyObject *
Reader_getprop_num_samples (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)wfm_reader_get_num_samples (self->handle));
}
static PyObject *
Reader_getprop_fc_source (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(wfm_reader_get_fc_source (self->handle));
  if (_v < 0 || _v >= 6)
    {
      PyErr_Format (PyExc_ValueError,
                    "fc_source holds out-of-range fc_source value %ld"
                    " (valid: 0..5)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_fc_source[_v]);
}
static PyObject *
Reader_getprop_trailing_bytes (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)wfm_reader_get_trailing_bytes (self->handle));
}
static PyObject *
Reader_decode_keywords (const wfm_keyword_t *_e)
{
  size_t _esz      = 0;
  int    _is_float = 0, _is_bytes = 0;
  switch (_e->type)
    {
    case 'A':
      _is_bytes = 1;
      break;
    case 'B':
      _esz = sizeof (int8_t);
      break;
    case 'I':
      _esz = sizeof (int16_t);
      break;
    case 'L':
      _esz = sizeof (int32_t);
      break;
    case 'T':
      _esz = sizeof (int32_t);
      break;
    case 'X':
      _esz = sizeof (int64_t);
      break;
    case 'F':
      _esz      = sizeof (float);
      _is_float = 1;
      break;
    case 'D':
      _esz      = sizeof (double);
      _is_float = 1;
      break;
    default:
      PyErr_Format (PyExc_ValueError, "unknown code '%c'", _e->type);
      return NULL;
    }
  if (_is_bytes)
    return PyUnicode_FromStringAndSize ((const char *)_e->value,
                                        (Py_ssize_t)_e->count);
  PyObject *_lst = PyList_New ((Py_ssize_t)_e->count);
  if (!_lst)
    return NULL;
  for (size_t _k = 0; _k < _e->count; _k++)
    {
      const uint8_t *_p  = (const uint8_t *)_e->value + _k * _esz;
      PyObject      *_it = NULL;
      if (_is_float)
        {
          switch (_e->type)
            {
            case 'F':
              {
                float _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyFloat_FromDouble ((double)_v);
                break;
              }
            case 'D':
              {
                double _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyFloat_FromDouble ((double)_v);
                break;
              }
            default:
              break;
            }
        }
      else
        {
          switch (_e->type)
            {
            case 'B':
              {
                int8_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'I':
              {
                int16_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'L':
              {
                int32_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'T':
              {
                int32_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'X':
              {
                int64_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            default:
              break;
            }
        }
      if (!_it)
        {
          Py_DECREF (_lst);
          return NULL;
        }
      PyList_SET_ITEM (_lst, (Py_ssize_t)_k, _it);
    }
  if (_e->count == 1)
    {
      PyObject *_s = PyList_GET_ITEM (_lst, 0);
      Py_INCREF (_s);
      Py_DECREF (_lst);
      return _s;
    }
  return _lst;
}

static PyObject *
Reader_getprop_keywords (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = wfm_reader_num_keywords (self->handle);
  PyObject *_c = PyDict_New ();
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      const char *_k = wfm_reader_keyword_tag (self->handle, _i);
      if (!_k)
        {
          PyErr_Format (
              PyExc_RuntimeError,
              "keywords: wfm_reader_keyword_tag returned NULL at index %zu",
              _i);
          Py_DECREF (_c);
          return NULL;
        }
      PyObject *_v
          = Reader_decode_keywords (wfm_reader_keyword (self->handle, _i));
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      if (PyDict_SetItemString (_c, _k, _v) != 0)
        {
          Py_DECREF (_v);
          Py_DECREF (_c);
          return NULL;
        }
      Py_DECREF (_v);
    }
  return _c;
}
static PyObject *
Reader_decode_header (const wfm_keyword_t *_e)
{
  size_t _esz      = 0;
  int    _is_float = 0, _is_bytes = 0;
  switch (_e->type)
    {
    case 'A':
      _is_bytes = 1;
      break;
    case 'B':
      _esz = sizeof (int8_t);
      break;
    case 'I':
      _esz = sizeof (int16_t);
      break;
    case 'L':
      _esz = sizeof (int32_t);
      break;
    case 'T':
      _esz = sizeof (int32_t);
      break;
    case 'X':
      _esz = sizeof (int64_t);
      break;
    case 'F':
      _esz      = sizeof (float);
      _is_float = 1;
      break;
    case 'D':
      _esz      = sizeof (double);
      _is_float = 1;
      break;
    default:
      PyErr_Format (PyExc_ValueError, "unknown code '%c'", _e->type);
      return NULL;
    }
  if (_is_bytes)
    return PyUnicode_FromStringAndSize ((const char *)_e->value,
                                        (Py_ssize_t)_e->count);
  PyObject *_lst = PyList_New ((Py_ssize_t)_e->count);
  if (!_lst)
    return NULL;
  for (size_t _k = 0; _k < _e->count; _k++)
    {
      const uint8_t *_p  = (const uint8_t *)_e->value + _k * _esz;
      PyObject      *_it = NULL;
      if (_is_float)
        {
          switch (_e->type)
            {
            case 'F':
              {
                float _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyFloat_FromDouble ((double)_v);
                break;
              }
            case 'D':
              {
                double _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyFloat_FromDouble ((double)_v);
                break;
              }
            default:
              break;
            }
        }
      else
        {
          switch (_e->type)
            {
            case 'B':
              {
                int8_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'I':
              {
                int16_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'L':
              {
                int32_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'T':
              {
                int32_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            case 'X':
              {
                int64_t _v;
                memcpy (&_v, _p, sizeof _v);
                _it = PyLong_FromLongLong ((long long)_v);
                break;
              }
            default:
              break;
            }
        }
      if (!_it)
        {
          Py_DECREF (_lst);
          return NULL;
        }
      PyList_SET_ITEM (_lst, (Py_ssize_t)_k, _it);
    }
  if (_e->count == 1)
    {
      PyObject *_s = PyList_GET_ITEM (_lst, 0);
      Py_INCREF (_s);
      Py_DECREF (_lst);
      return _s;
    }
  return _lst;
}

static PyObject *
Reader_getprop_header (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = wfm_reader_num_header_fields (self->handle);
  PyObject *_c = PyDict_New ();
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      const char *_k = wfm_reader_header_tag (self->handle, _i);
      if (!_k)
        {
          PyErr_Format (
              PyExc_RuntimeError,
              "header: wfm_reader_header_tag returned NULL at index %zu", _i);
          Py_DECREF (_c);
          return NULL;
        }
      PyObject *_v
          = Reader_decode_header (wfm_reader_header_field (self->handle, _i));
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      if (PyDict_SetItemString (_c, _k, _v) != 0)
        {
          Py_DECREF (_v);
          Py_DECREF (_c);
          return NULL;
        }
      Py_DECREF (_v);
    }
  return _c;
}

static PyGetSetDef Reader_getset[] = {
  { "follow_timeout_ms", (getter)Reader_getprop_follow_timeout_ms,
    (setter)Reader_setprop_follow_timeout_ms,
    "How long `read_follow()` waits for samples to arrive, in milliseconds; "
    "**0 (the default) waits forever**, which is the right answer for a "
    "stream with no end -- any finite budget fires during an ordinary quiet "
    "patch and reports an ending that has not happened. Set it only if the "
    "caller has its own reason to give up. A bounded wait that expires leaves "
    "`ending` at `\"timeout\"`.\n",
    NULL },
  { "follow_grace_ms", (getter)Reader_getprop_follow_grace_ms,
    (setter)Reader_setprop_follow_grace_ms,
    "How long `read_follow()` keeps waiting for the writer's end-of-capture "
    "marker after a stop has been requested, in milliseconds; **0 (the "
    "default) waits forever**. The writer needs a moment to flush and patch "
    "its header, and expiring this budget before it does costs you the tail "
    "-- so the default trades an unlikely hang against a certain loss. A "
    "bounded grace that expires leaves `ending` at `\"interrupted\"`.\n",
    NULL },
  { "ending", (getter)Reader_getprop_ending, NULL,
    "Why the last `read_follow()` came back empty -- `\"none\"` while the "
    "capture is still live, `\"eof\"` once the writer closed and said so, "
    "`\"timeout\"` if a bounded `timeout_ms` expired, `\"interrupted\"` if a "
    "stop was requested and a bounded `grace_ms` expired before the writer's "
    "marker arrived. With the default unbounded budgets only `\"none\"` and "
    "`\"eof\"` are reachable, which is why an empty result needs no check in "
    "the common case. The values are doppler's own return codes (`DP_OK`, "
    "`DP_ERR_EOF`, `DP_ERR_TIMEOUT`, `DP_ERR_INTERRUPTED`), so a C caller "
    "reads the same vocabulary every other transport uses.\n",
    NULL },
  { "file_type", (getter)Reader_getprop_file_type, NULL,
    "Which file type the capture turned out to be -- `\"raw\"`, `\"csv\"`, "
    "`\"blue\"` or `\"sigmf\"`. Detected from the file's CONTENT, not its "
    "name, so a CSV called `capture.dat` reports `\"csv\"` and a BLUE file "
    "called `capture.csv` reports `\"blue\"`. `\"raw\"` is also the fallback "
    "for a file nothing else recognised, so it means \"headerless interleaved "
    "I/Q at the sample_type you passed\" rather than a positive "
    "identification.\n",
    NULL },
  { "sample_type", (getter)Reader_getprop_sample_type, NULL,
    "The wire sample type the samples are being decoded FROM -- `\"cf32\"`, "
    "`\"cf64\"`, `\"ci32\"`, `\"ci16\"` or `\"ci8\"`. For BLUE and SigMF this "
    "was read from the file's metadata and is authoritative; for raw and CSV "
    "it is simply the hint passed to the constructor, echoed back. `read()` "
    "returns `complex64` at unit scale regardless.\n",
    NULL },
  { "mode", (getter)Reader_getprop_mode, NULL,
    "Components per wire sample: `\"complex\"` for interleaved I/Q, "
    "`\"scalar\"` for a real capture. Only BLUE carries this (its `format` "
    "field's mode designator, `C` or `S`); every other file type is complex. "
    "A scalar capture still reads back as `complex64` -- the imaginary part "
    "is exactly 0, so a real signal lands on the real axis.\n",
    NULL },
  { "endian", (getter)Reader_getprop_endian, NULL,
    "Byte order of the samples on the wire, `\"le\"` or `\"be\"`. Read from "
    "the metadata for BLUE (the HCB's `head_rep`) and SigMF (the `_be`/`_le` "
    "datatype suffix); for raw it is the constructor hint echoed back. CSV is "
    "text and ignores it.\n",
    NULL },
  { "fs", (getter)Reader_getprop_fs, NULL,
    "Sample rate in Hz, or 0.0 when the file type does not carry one. BLUE "
    "derives it from the header's `xdelta` (fs = 1/xdelta); SigMF reads "
    "`core:sample_rate`. Raw and CSV have nowhere to record a rate, so they "
    "always report 0.0 -- whatever rate the capture was taken at has to "
    "travel with it by other means.\n",
    NULL },
  { "fc", (getter)Reader_getprop_fc, NULL,
    "Centre frequency in Hz, or 0.0 when nothing in the capture declares one. "
    "**0.0 is ambiguous on its own** -- a genuine baseband capture and a "
    "capture whose frequency could not be found report the same number -- so "
    "read `fc_source` alongside it: `\"none\"` there is what distinguishes "
    "them. SigMF takes it from `captures[0][\"core:frequency\"]`; BLUE from a "
    "`FREQ` keyword (see `fc_source` for the tags tried), in either the ASCII "
    "HCB keyword area or the typed extended header. Raw and CSV carry no "
    "metadata at all.\n",
    NULL },
  { "fs_source", (getter)Reader_getprop_fs_source, NULL,
    "Which metadata `fs` was read from -- `\"xdelta\"` for BLUE (the "
    "type-1000 adjunct, as 1/xdelta), `\"core:sample_rate\"` for SigMF, or "
    "`\"none\"` when nothing carried a rate. Raw and CSV always report "
    "`\"none\"`: they have nowhere to record one, so whatever rate the "
    "capture was taken at has to travel with it by other means.\n",
    NULL },
  { "t0", (getter)Reader_getprop_t0, NULL,
    "Capture start time in seconds since the UNIX epoch, or 0.0 when the "
    "capture does not declare one. **0.0 does not mean 1970** -- read "
    "`t0_source` alongside it, exactly as with `fc`/`fc_source`. This is the "
    "`t0` of `t = t0 + n/fs`: hand it to a `SampleClock` via `track()` and a "
    "replayed capture's timeline lands where the samples were taken, not "
    "where they are being replayed. BLUE carries it as a J1950 `timecode` in "
    "the header, converted here to the UNIX epoch.\n",
    NULL },
  { "t0_source", (getter)Reader_getprop_t0_source, NULL,
    "Where `t0` was read from -- `\"timecode\"` for a BLUE header that "
    "declares one, or `\"none\"`. **`\"none\"` is the common answer and the "
    "one that matters**: a zero BLUE timecode means the field was never set, "
    "not 1950-01-01, and doppler's own writer leaves it zero -- so a caller "
    "that skips this check dates every doppler-written capture to 1950. "
    "SigMF's `core:datetime` is an ISO 8601 string this reader does not parse "
    "yet, so a SigMF capture also reports `\"none\"` rather than a guess.\n",
    NULL },
  { "num_samples", (getter)Reader_getprop_num_samples, NULL,
    "Total samples in the capture, or 0 when the file type cannot say. BLUE "
    "takes it from the header's `data_size`; raw and SigMF divide the file "
    "length by the sample stride. A CSV has to be counted, so the first read "
    "of this property scans the file once (the scan is exact -- it parses "
    "rows the same way `read` does -- and leaves the read position alone); "
    "every later read is free.\n",
    NULL },
  { "fc_source", (getter)Reader_getprop_fc_source, NULL,
    "Which piece of metadata `fc` was read from -- the keyword's own tag "
    "(`\"FREQ\"`, `\"RF_FREQ\"`, `\"CENTER_FREQ\"`, `\"F_C\"`), "
    "`\"core:frequency\"` for SigMF, or `\"none\"` when nothing carried it. "
    "Check this before trusting `fc == 0.0`: `\"none\"` means not found, "
    "anything else means the capture really does say 0 Hz. BLUE type-1000 has "
    "no header field for centre frequency, so an RF capture conveys it as a "
    "keyword; `FREQ` in the HCB keyword area is the X-Midas convention and is "
    "tried first.\n",
    NULL },
  { "trailing_bytes", (getter)Reader_getprop_trailing_bytes, NULL,
    "Payload bytes left over after the last whole sample; 0 for a capture "
    "whose declared sample type and mode match its content, and always 0 for "
    "CSV. Non-zero means either the `sample_type`/`endian` hint is wrong for "
    "a headerless file type or the capture is truncated -- the reader cannot "
    "tell which, and stops at the last complete sample either way. This is "
    "the only signal available for a raw file: a wrong hint does not fail, it "
    "returns plausible garbage at the wrong stride.\n",
    NULL },
  { "keywords", (getter)Reader_getprop_keywords, NULL,
    "The BLUE extended header as a {tag: value} dict, in file order; empty "
    "when the capture carries no extended header. Values follow the keyword "
    "type: a str for A, an int/float for a single-element numeric keyword, a "
    "list for a multi-element one. For a detached capture these come from the "
    "HEADER file.\n",
    NULL },
  { "header", (getter)Reader_getprop_header, NULL,
    "The BLUE header control block as a {field: value} dict, under the names "
    "the format itself uses -- `version`, `head_rep`, `data_rep`, `detached`, "
    "`protected`, `pipe`, `ext_start`, `ext_size`, `data_start`, `data_size`, "
    "`type`, `format`, `flagmask`, `timecode`, `inlet`, `outlets`, `outmask`, "
    "`pipeloc`, `pipesize`, `in_byte`, `out_byte`, `outbytes`, `keylength`, "
    "and the type-1000 adjunct `xstart`, `xdelta`, `xunits`. Empty for a "
    "non-BLUE file type. Nothing is renamed or omitted, so what you see is "
    "what the file holds; the decoded keywords are in `keywords`.\n",
    NULL },
  { NULL }
};

static PyObject *
ReaderObj_destroy (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      wfm_reader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ReaderObj_enter (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ReaderObj_exit (ReaderObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      wfm_reader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ReaderObj_methods[] = {
  { "reset", (PyCFunction)ReaderObj_reset, METH_NOARGS,
    "Rewind to the first sample of the capture.\n"
    "\n"
    "Seeks back to where the payload starts — 512 bytes into an attached\n"
    "BLUE file, byte 0 of a `.det` or a raw/SigMF payload — and restores the\n"
    "remaining-sample count, so the capture reads again from the top. The\n"
    "file's metadata and decoded keywords are unaffected: they came from the\n"
    "header and do not change.\n" },

  { "read", (PyCFunction)(void *)ReaderObj_read, METH_VARARGS | METH_KEYWORDS,
    "read(count=1) -> ndarray\n"
    "\n"
    "Read up to count samples, returning them as `complex64`.\n"
    "\n"
    "Samples come out at unit scale whatever the wire type was: a float type\n"
    "is reinterpreted, an integer type is divided by its full scale. Returns\n"
    "fewer than asked at the end of the capture, and 0 once it is exhausted,\n"
    "so a `while` over the result terminates. Never returns more than the\n"
    "file's declared payload — trailing bytes past `data_size` (an extended\n"
    "header, X-Midas slack) are not samples.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[np.complex64] | None\n"
    "    destination, at least max_out samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import pathlib, tempfile\n"
    ">>> from doppler.wfm import Composer, Reader, Segment, Writer\n"
    ">>> tmp = tempfile.TemporaryDirectory()\n"
    ">>> p = pathlib.Path(tmp.name) / \"capture.blue\"\n"
    ">>> x = Composer([Segment(\"qpsk\", sps=8, "
    "num_samples=1024)]).compose()\n"
    ">>> with Writer(p, file_type=\"blue\", sample_type=\"ci16\",\n"
    "...             fs=2.4e6, fc=1.2e9) as w:\n"
    "...     _ = w.write(x)\n"
    ">>> r = Reader(p)\n"
    ">>> r.file_type, r.sample_type, r.endian\n"
    "('blue', 'ci16', 'le')\n"
    ">>> r.fs, r.fc, r.fc_source\n"
    "(2400000.0, 1200000000.0, 'FREQ')\n"
    ">>> total = 0\n"
    ">>> while len(block := r.read(256)):\n"
    "...     total += len(block)\n"
    ">>> total\n"
    "1024\n"
    ">>> r.close()\n"
    ">>> tmp.cleanup()   # directory and contents removed\n" },
  { "read_max_out", (PyCFunction)ReaderObj_read_max_out, METH_VARARGS,
    "read_max_out(n) -> int\n"
    "\n"
    "Maximum samples one read(n) yields: n (fewer at EOF).\n"
    "\n"
    "A reader streams, so a read of n produces at most n samples; the\n"
    "binding sizes its buffer to this per-call bound (gh-607) and resizes\n"
    "down to the actual count, never pre-allocating the whole capture.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "read_follow", (PyCFunction)(void *)ReaderObj_read_follow,
    METH_VARARGS | METH_KEYWORDS,
    "read_follow(count=1) -> ndarray\n"
    "\n"
    "Read whatever whole samples have arrived in a capture that is still\n"
    "being written, blocking until at least one does. Unlike `read()`, a\n"
    "short or empty result does not mean end-of-file: the reader waits. **0\n"
    "means wait forever** for both budgets, which is the default and the\n"
    "right answer for a stream with no end -- any finite budget fires during\n"
    "an ordinary quiet patch. An empty result therefore means the capture\n"
    "ENDED; read `ending` for which way. `timeout_ms` bounds the wait for\n"
    "data; `grace_ms` bounds how long to keep waiting for the writer's\n"
    "marker after a stop has been requested, and expiring it may cost you\n"
    "the tail. Never consumes a partial sample, so a writer flushing\n"
    "mid-sample cannot desynchronise the stream.\n"
    "\n"
    "Blocks until whole samples arrive. A short or empty result does not\n"
    "mean end-of-file the way ::wfm_reader_read's does -- the reader waits.\n"
    "**Zero means the capture ENDED**, because with the default unbounded\n"
    "budgets the call does not come back for \"not yet\";\n"
    "::wfm_reader_get_ending says which way it ended.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[np.complex64] | None\n"
    "    Optional pre-allocated output buffer. When given, the result is\n"
    "    written into it and the returned array is a view of exactly the\n"
    "    samples produced; when omitted, a fresh array is allocated.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import pathlib, tempfile\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Reader, Writer\n"
    ">>> tmp = tempfile.TemporaryDirectory()\n"
    ">>> p = pathlib.Path(tmp.name) / \"capture.blue\"\n"
    ">>> x = np.zeros(8, dtype=np.complex64)\n"
    ">>> with Writer(p, file_type=\"blue\", sample_type=\"ci16\", fs=2.4e6) "
    "as w:\n"
    "...     _ = w.write(x)\n"
    ">>> r = Reader(p)\n"
    ">>> total = 0\n"
    ">>> while len(block := r.read_follow(4)):   # 0 only when the capture "
    "ends\n"
    "...     total += len(block)\n"
    ">>> total, r.ending\n"
    "(8, 'eof')\n"
    ">>> r.close()\n"
    ">>> tmp.cleanup()\n" },
  { "read_follow_max_out", (PyCFunction)ReaderObj_read_follow_max_out,
    METH_VARARGS,
    "read_follow_max_out(n) -> int\n"
    "\n"
    "Largest number of samples read_follow() can return for n inputs.\n"
    "\n"
    "Size an `out=` buffer with this before calling read_follow(), or use it\n"
    "to allocate one up front. The bound is this object's own: what it\n"
    "depends on is a property of the algorithm, so a header block on\n"
    "read_follow_max_out() replaces this text.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Number of input samples read_follow() will be given.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Upper bound on the output length; the actual call may return "
    "fewer.\n" },
  { "close", (PyCFunction)ReaderObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "destroy", (PyCFunction)ReaderObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)ReaderObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Reader be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Reader\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ReaderObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Reader.\n"
    "\n"
    "Equivalent to calling `close()`. Returns ``None``, so an exception\n"
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
  { NULL }
};

static PyTypeObject ReaderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm_reader.Reader",
  .tp_basicsize                           = sizeof (ReaderObject),
  .tp_dealloc                             = (destructor)ReaderObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Open a capture, auto-detecting its file type from its content.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "path : str | os.PathLike\n"
    "    file to read -- a `str` or any `os.PathLike` from Python. For a\n"
    "    DETACHED BLUE capture this is normally the HEADER file -- "
    "`<base>.tmp`\n"
    "    or `<base>.prm` per BLUE 3.1.1.4 (this library's own writer emits\n"
    "    `<base>.hdr`) -- whose HCB `detached` field points at the "
    "collocated\n"
    "    `<base>.det` payload; the extension does not decide, `detached` "
    "does.\n"
    "    Passing the `<base>.det` directly also works (its header sibling is\n"
    "    resolved). A SigMF `.sigmf-data` file resolves its `.sigmf-meta`\n"
    "    sidecar the same way.\n"
    "sample_type : Literal[\"cf32\", \"cf64\", \"ci32\", \"ci16\", \"ci8\"], "
    "default \"cf32\"\n"
    "    the wire sample type, used only as a HINT for the headerless file "
    "types\n"
    "    (raw, CSV) -- BLUE and SigMF carry their own and ignore it. "
    "`\"cf32\"`,\n"
    "    `\"cf64\"`, `\"ci32\"`, `\"ci16\"` or `\"ci8\"` from Python; the "
    "matching 0..4\n"
    "    from C. A wrong hint does not fail; see\n"
    "    ::wfm_reader_get_trailing_bytes.\n"
    "endian : Literal[\"le\", \"be\"], default \"le\"\n"
    "    byte order, likewise a hint that only headerless raw uses; `\"le\"` "
    "or\n"
    "    `\"be\"` from Python, 0 or 1 from C.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``cannot open "
    "capture:\n"
    "    no such file, unrecognised file type, or an unsupported BLUE format\n"
    "    mode (only S and C are supported)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import pathlib, tempfile\n"
    ">>> from doppler.wfm import Composer, Reader, Segment, Writer\n"
    ">>> tmp = tempfile.TemporaryDirectory()\n"
    ">>> p = pathlib.Path(tmp.name) / \"capture.blue\"\n"
    ">>> x = Composer([Segment(\"qpsk\", sps=8, "
    "num_samples=1024)]).compose()\n"
    ">>> w = Writer(p, file_type=\"blue\", sample_type=\"ci16\", fs=2.4e6)\n"
    ">>> w.add_keyword(\"NAME\", \"A\", \"demo\")   # tag the header\n"
    ">>> _ = w.write(x)\n"
    ">>> w.close()\n"
    ">>> r = Reader(p)                         # file type auto-detected\n"
    ">>> r.file_type, r.sample_type, r.fs\n"
    "('blue', 'ci16', 2400000.0)\n"
    ">>> r.keywords[\"NAME\"]                    # keyword round-trips\n"
    "'demo'\n"
    ">>> total = 0\n"
    ">>> while len(block := r.read(256)):      # read returns 0 at EOF\n"
    "...     total += len(block)\n"
    ">>> total == r.num_samples == 1024\n"
    "True\n"
    ">>> r.close()\n"
    ">>> tmp.cleanup()\n",
  .tp_methods = ReaderObj_methods,
  .tp_getset  = Reader_getset,
  .tp_new     = ReaderObj_new,
  .tp_init    = (initproc)ReaderObj_init,
};
