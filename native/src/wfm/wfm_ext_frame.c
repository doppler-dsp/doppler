/*
 * wfm_ext_frame.c — Frame type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* FrameObject — wraps dp_frame_state_t *       */
/* ======================================================== */

#include "doppler/frame/frame_core.h"

typedef struct
{
  PyObject_HEAD dp_frame_state_t *handle;
} FrameObject;

static void
FrameObj_dealloc (FrameObject *self)
{
  if (self->handle)
    dp_frame_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FrameObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FrameObject *self = (FrameObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FrameObj_init (FrameObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]     = { "preamble", "sync", "payload", "crc", NULL };
  PyObject    *preamble_obj = NULL;
  PyObject    *sync_obj     = NULL;
  PyObject    *payload_obj  = NULL;
  const char  *crc_str      = "none";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|OOOs", kwlist, &preamble_obj,
                                    &sync_obj, &payload_obj, &crc_str))
    return -1;
  int crc = 0;
  if (strcmp (crc_str, "none") == 0)
    crc = 0;
  else if (strcmp (crc_str, "crc16") == 0)
    crc = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "crc must be one of \"none\", \"crc16\", got '%s'",
                    crc_str);
      return -1;
    }
  PyArrayObject *preamble_arr = NULL;
  size_t         preamble_len = 0;
  if (preamble_obj && preamble_obj != Py_None)
    {
      preamble_arr = (PyArrayObject *)PyArray_FROM_OTF (
          preamble_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
      if (!preamble_arr)
        {
          return -1;
        }
      preamble_len = (size_t)PyArray_SIZE (preamble_arr);
    }
  PyArrayObject *sync_arr = NULL;
  size_t         sync_len = 0;
  if (sync_obj && sync_obj != Py_None)
    {
      sync_arr = (PyArrayObject *)PyArray_FROM_OTF (sync_obj, NPY_UINT8,
                                                    NPY_ARRAY_C_CONTIGUOUS);
      if (!sync_arr)
        {
          Py_XDECREF (preamble_arr);
          return -1;
        }
      sync_len = (size_t)PyArray_SIZE (sync_arr);
    }
  PyArrayObject *payload_arr = NULL;
  size_t         payload_len = 0;
  if (payload_obj && payload_obj != Py_None)
    {
      payload_arr = (PyArrayObject *)PyArray_FROM_OTF (payload_obj, NPY_UINT8,
                                                       NPY_ARRAY_C_CONTIGUOUS);
      if (!payload_arr)
        {
          Py_XDECREF (preamble_arr);
          Py_XDECREF (sync_arr);
          return -1;
        }
      payload_len = (size_t)PyArray_SIZE (payload_arr);
    }
  self->handle = dp_frame_create (
      preamble_arr ? (const uint8_t *)PyArray_DATA (preamble_arr) : NULL,
      preamble_len, sync_arr ? (const uint8_t *)PyArray_DATA (sync_arr) : NULL,
      sync_len,
      payload_arr ? (const uint8_t *)PyArray_DATA (payload_arr) : NULL,
      payload_len, crc);
  Py_XDECREF (preamble_arr);
  Py_XDECREF (sync_arr);
  Py_XDECREF (payload_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "frame geometry is empty, or a field holds an element "
                       "that is not a bit (0 or 1)");
      return -1;
    }
  return 0;
}

static PyObject *
FrameObj_bits_max_out (FrameObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  return PyLong_FromSize_t (dp_frame_bits_max_out (self->handle, (size_t)n));
}

static PyObject *
FrameObj_bits (FrameObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_frame_bits_max_out (self->handle, (size_t)n);
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
      uint8_t *_ng0 = (uint8_t *)PyArray_DATA (out_arr);
      size_t   n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_frame_bits (self->handle, (size_t)n, _ng0, _cap);
      Py_END_ALLOW_THREADS
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT8,
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
  size_t _need = (size_t)n;
  size_t _cap  = dp_frame_bits_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      return NULL;
    }
  uint8_t *_d0 = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  size_t n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_frame_bits (self->handle, (size_t)n, _d0, _cap);
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
FrameObj_crc_ok (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]   = { "rx_bits", NULL };
  PyObject    *rx_bits_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &rx_bits_obj))
    return NULL;
  PyArrayObject *rx_bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      rx_bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!rx_bits_arr)
    {
      return NULL;
    }
  const uint8_t *rx_bits     = (const uint8_t *)PyArray_DATA (rx_bits_arr);
  size_t         rx_bits_len = (size_t)PyArray_SIZE (rx_bits_arr);
  int            y = dp_frame_crc_ok (self->handle, rx_bits, rx_bits_len);
  Py_DECREF (rx_bits_arr);
  return PyLong_FromLong ((long)y);
}

static PyObject *
FrameObj_add_field (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "name", "bits", NULL };
  const char  *name      = NULL;
  PyObject    *bits_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sO", _kwlist, &name,
                                    &bits_obj))
    return NULL;
  PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!bits_arr)
    {
      return NULL;
    }
  const uint8_t *bits     = (const uint8_t *)PyArray_DATA (bits_arr);
  size_t         bits_len = (size_t)PyArray_SIZE (bits_arr);
  int            _rc = dp_frame_add_field (self->handle, name, bits, bits_len);
  Py_DECREF (bits_arr);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a field: the description is full or "
                    "already built, the name is taken, the bits are empty, "
                    "or an element is not a bit",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
FrameObj_add_stage (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "kind",     "first_field", "n_fields",  "depth",
                              "emit_num", "emit_den",    "unit_bits", NULL };
  int           kind      = 0;
  unsigned long first_field_raw = 0;
  unsigned long n_fields_raw    = 0;
  unsigned long depth_raw       = 0;
  unsigned long emit_num_raw    = 0;
  unsigned long emit_den_raw    = 0;
  unsigned long unit_bits_raw   = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ikkkkkk", _kwlist, &kind,
                                    &first_field_raw, &n_fields_raw,
                                    &depth_raw, &emit_num_raw, &emit_den_raw,
                                    &unit_bits_raw))
    return NULL;
  uint32_t first_field = (uint32_t)first_field_raw;
  uint32_t n_fields    = (uint32_t)n_fields_raw;
  uint32_t depth       = (uint32_t)depth_raw;
  uint32_t emit_num    = (uint32_t)emit_num_raw;
  uint32_t emit_den    = (uint32_t)emit_den_raw;
  uint32_t unit_bits   = (uint32_t)unit_bits_raw;
  int      _rc = dp_frame_add_stage (self->handle, kind, first_field, n_fields,
                                     depth, emit_num, emit_den, unit_bits);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a stage: the description is full or "
                    "already built",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
FrameObj_field_index (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "name", NULL };
  const char  *name      = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s", _kwlist, &name))
    return NULL;
  int y = dp_frame_field_index (self->handle, name);
  return PyLong_FromLong ((long)y);
}

static PyObject *
FrameObj_name_field (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "index", "name", NULL };
  unsigned long index_raw = 0UL;
  const char   *name      = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ks", _kwlist, &index_raw,
                                    &name))
    return NULL;
  uint32_t index = (uint32_t)index_raw;
  int      _rc   = dp_frame_name_field (self->handle, index, name);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot name a field: the index is out of range, another "
                    "field already carries the name, or the frame is already "
                    "built",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FrameObj_add_derived (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "name", "bits", NULL };
  const char        *name      = NULL;
  unsigned long long bits_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sK", _kwlist, &name,
                                    &bits_raw))
    return NULL;
  size_t bits = (size_t)bits_raw;
  int    _rc  = dp_frame_add_derived (self->handle, name, bits);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a derived field: the description is full "
                    "or already built",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
FrameObj_add_stage_over (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "kind", "first", "last", "depth", "unit_bits", NULL };
  int           kind          = 0;
  const char   *first         = NULL;
  const char   *last          = NULL;
  unsigned long depth_raw     = 0;
  unsigned long unit_bits_raw = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "iss|kk", _kwlist, &kind,
                                    &first, &last, &depth_raw, &unit_bits_raw))
    return NULL;
  uint32_t depth     = (uint32_t)depth_raw;
  uint32_t unit_bits = (uint32_t)unit_bits_raw;
  int _rc = dp_frame_add_stage_over (self->handle, kind, first, last, depth,
                                     unit_bits);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot append a stage: the description is full, already "
                    "built, or names a field the description does not carry "
                    "(and `last` must not precede `first`)",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
FrameObj_build (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_frame_build (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "cannot build: the description is empty, unbuildable, "
                    "names a stage no kernel here covers, or was already "
                    "built",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FrameObj_deframe_max_out (FrameObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t rx_bits_len = 0;
  if (!PyArg_ParseTuple (args, "n", &rx_bits_len))
    return NULL;
  return PyLong_FromSize_t (
      dp_frame_deframe_max_out (self->handle, (size_t)rx_bits_len));
}

static PyObject *
FrameObj_deframe (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[]   = { "rx_bits", "out", NULL };
  PyObject      *rx_bits_obj = NULL;
  PyArrayObject *rx_bits_arr = NULL;
  PyObject      *out_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &rx_bits_obj,
                                    &out_obj))
    return NULL;
  rx_bits_arr = (PyArrayObject *)PyArray_FROM_OTF (rx_bits_obj, NPY_UINT8,
                                                   NPY_ARRAY_C_CONTIGUOUS);
  if (!rx_bits_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (rx_bits_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (rx_bits_arr);
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = dp_frame_deframe_max_out (
          self->handle, (size_t)PyArray_SIZE (rx_bits_arr));
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (rx_bits_arr);
          return NULL;
        }
      size_t n_out = dp_frame_deframe (
          self->handle, (const uint8_t *)PyArray_DATA (rx_bits_arr),
          (size_t)PyArray_SIZE (rx_bits_arr),
          (uint8_t *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (rx_bits_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT8,
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
  size_t _need = (size_t)PyArray_SIZE (rx_bits_arr);
  size_t _cap  = dp_frame_deframe_max_out (self->handle,
                                           (size_t)PyArray_SIZE (rx_bits_arr));
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (rx_bits_arr);
      return NULL;
    }
  uint8_t *_d0   = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t   n_out = dp_frame_deframe (
      self->handle, (const uint8_t *)PyArray_DATA (rx_bits_arr),
      (size_t)PyArray_SIZE (rx_bits_arr), _d0, _cap);
  Py_DECREF (rx_bits_arr);
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

static PyStructSequence_Field FrameObj_check_fields[] = {
  { "passed",
    "Every check good: 1 yes, 0 no. Also 0 when nothing was checked -- see "
    "`checked`. Named `passed` rather than `pass` because the obvious name is "
    "a Python keyword and `r.pass` will not parse." },
  { "stages", "Stages in the description." },
  { "checked", "How many were reversed here. 0 means the description carries "
               "no reversible stage, which is why `pass` is 0: carrying no "
               "check is not the same answer as passing one." },
  { "units", "Checks performed: one for a CRC, one per codeword for an "
             "interleaved outer code." },
  { "ok", "How many came out good -- clean or repaired." },
  { "corrected", "How many needed and received repair." },
  { "symbols", "Symbol errors repaired across the frame." },
  { NULL, NULL },
};
static PyStructSequence_Desc FrameObj_check_desc
    = { "doppler.wfm.FrameCheck",
        "What checking one received frame found. `ok == units` is the "
        "verdict; `symbols` is what it cost, which is margin being spent and "
        "is visible before it is lost.",
        FrameObj_check_fields, 7 };
static PyTypeObject *FrameObj_check_type = NULL;

static PyObject *
FrameObj_check (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]   = { "rx_bits", NULL };
  PyObject    *rx_bits_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &rx_bits_obj))
    return NULL;
  PyArrayObject *rx_bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      rx_bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!rx_bits_arr)
    {
      return NULL;
    }
  const uint8_t *rx_bits     = (const uint8_t *)PyArray_DATA (rx_bits_arr);
  size_t         rx_bits_len = (size_t)PyArray_SIZE (rx_bits_arr);
  if (!FrameObj_check_type)
    {
      FrameObj_check_type = PyStructSequence_NewType (&FrameObj_check_desc);
      if (!FrameObj_check_type)
        {
          Py_DECREF (rx_bits_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  frame_check_t _r;
  Py_BEGIN_ALLOW_THREADS
    _r = dp_frame_check (self->handle, rx_bits, rx_bits_len);
  Py_END_ALLOW_THREADS
  Py_DECREF (rx_bits_arr);
  PyObject *_o = PyStructSequence_New (FrameObj_check_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyLong_FromLong ((long)_r.passed));
  PyStructSequence_SET_ITEM (
      _o, 1, PyLong_FromUnsignedLong ((unsigned long)_r.stages));
  PyStructSequence_SET_ITEM (
      _o, 2, PyLong_FromUnsignedLong ((unsigned long)_r.checked));
  PyStructSequence_SET_ITEM (
      _o, 3, PyLong_FromUnsignedLong ((unsigned long)_r.units));
  PyStructSequence_SET_ITEM (_o, 4,
                             PyLong_FromUnsignedLong ((unsigned long)_r.ok));
  PyStructSequence_SET_ITEM (
      _o, 5, PyLong_FromUnsignedLong ((unsigned long)_r.corrected));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLong ((unsigned long)_r.symbols));
  return _o;
}

static PyObject *
FrameObj_n_fields (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_frame_n_fields (self->handle);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
FrameObj_n_stages (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_frame_n_stages (self->handle);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
FrameObj_field_off (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "i", NULL };
  unsigned long long i_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &i_raw))
    return NULL;
  size_t i = (size_t)i_raw;
  size_t y = dp_frame_field_off (self->handle, i);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
FrameObj_field_bits (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "i", NULL };
  unsigned long long i_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &i_raw))
    return NULL;
  size_t i = (size_t)i_raw;
  size_t y = dp_frame_field_bits (self->handle, i);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
FrameObj_stage_first (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "i", NULL };
  unsigned long long i_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &i_raw))
    return NULL;
  size_t i = (size_t)i_raw;
  size_t y = dp_frame_stage_first (self->handle, i);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
FrameObj_stage_bits (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "i", NULL };
  unsigned long long i_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &i_raw))
    return NULL;
  size_t i = (size_t)i_raw;
  size_t y = dp_frame_stage_bits (self->handle, i);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}
static PyObject *
Frame_getprop_rx_ok (FrameObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->rx_ok);
}
static PyObject *
Frame_getprop_rx_units (FrameObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->rx_units);
}
static PyObject *
Frame_getprop_rx_checked (FrameObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->rx_checked);
}
static PyObject *
Frame_getprop_rx_symbols (FrameObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->rx_symbols);
}
static PyObject *
Frame_getprop_nbits (FrameObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nbits);
}

static PyGetSetDef Frame_getset[] = {
  { "rx_ok", (getter)Frame_getprop_rx_ok, NULL,
    "Checks that came out good in the last deframe() -- one per CRC, one per "
    "outer-code codeword. `rx_ok == rx_units` is the verdict.\n",
    NULL },
  { "rx_units", (getter)Frame_getprop_rx_units, NULL,
    "Checks the last deframe() performed across every stage it reversed.\n",
    NULL },
  { "rx_checked", (getter)Frame_getprop_rx_checked, NULL,
    "Stages the last deframe() actually reversed. 0 means the description "
    "carries no reversible stage at all -- which is why `rx_ok` is 0 too, and "
    "is a different fact from a check that failed. An FER conflating them "
    "scores every unprotected frame as an error.\n",
    NULL },
  { "rx_symbols", (getter)Frame_getprop_rx_symbols, NULL,
    "Symbol errors the last deframe() repaired. Margin being spent, visible "
    "before it is lost -- what an outer code reports and a CRC cannot.\n",
    NULL },
  { "nbits", (getter)Frame_getprop_nbits, NULL, "Nbits.\n", NULL },
  { NULL }
};

static PyObject *
FrameObj_destroy (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_frame_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FrameObj_enter (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FrameObj_exit (FrameObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_frame_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FrameObj_methods[] = {

  { "bits", (PyCFunction)(void *)FrameObj_bits, METH_VARARGS | METH_KEYWORDS,
    "bits(count=1) -> ndarray\n"
    "\n"
    "Materialise n consecutive frames, one bit per byte.\n"
    "\n"
    "n counts FRAMES, not bits: a descriptor describes one frame, and a\n"
    "capture holds many. Repeating here rather than making the caller tile\n"
    "it is what matches the generator, whose framed source cycles the same\n"
    "frame to fill whatever length was asked for — so a stream compared\n"
    "against this lines up with the one that was transmitted.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[np.uint8] | None\n"
    "    Output, one bit per byte.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Bits written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> len(d.bits())        # one frame: 13 + 16 + 16\n"
    "45\n"
    ">>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is\n"
    "90\n" },
  { "bits_max_out", (PyCFunction)FrameObj_bits_max_out, METH_VARARGS,
    "bits_max_out(n) -> int\n"
    "\n"
    "Bits dp_frame_bits will write for n frames — `n * nbits`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Frame repetitions.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "crc_ok", (PyCFunction)(void *)FrameObj_crc_ok,
    METH_VARARGS | METH_KEYWORDS,
    "crc_ok(rx_bits) -> int\n"
    "\n"
    "Check one received frame's CRC.\n"
    "\n"
    "**This is what makes a truth-free frame error rate possible.** It needs\n"
    "no payload truth at all, so it works on a real capture, and unlike a\n"
    "self-referenced EVM or a blind M2M4 it still catches a false lock — a\n"
    "rotated constellation fails the check rather than looking clean.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rx_bits : NDArray[np.uint8]\n"
    "    Received bits, one per byte.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    1 pass, 0 fail, -1 if the frame carries no CRC or rx_bits is\n"
    "    shorter than one frame.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.crc_ok(d.bits())           # its own bits are its own truth\n"
    "1\n"
    ">>> rx = np.asarray(d.bits()).copy()\n"
    ">>> rx[d.field_off(d.field_index(\"payload\"))] ^= 1   # one payload "
    "bit\n"
    ">>> d.crc_ok(rx)\n"
    "0\n" },
  { "add_field", (PyCFunction)(void *)FrameObj_add_field,
    METH_VARARGS | METH_KEYWORDS,
    "add_field(name, bits) -> int\n"
    "\n"
    "Append one named field to a description (see `FrameDesc`): bits and "
    "nothing else, one per element, each 0 or 1. Text reaches it through "
    "`field_bits()`, hex and packed octets through `cvt`. A field a STAGE "
    "fills is `add_derived` instead, because the caller has no bits for it. "
    "`name` may be empty for an anonymous field; a name another field carries "
    "is refused. Returns the new field's index. Refuses once the frame is "
    "built.\n"
    "\n"
    "The field is bits and nothing else, copied here so the description\n"
    "outlives the call. A field a STAGE fills is appended with\n"
    "dp_frame_add_derived instead, because the caller has no bits for it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    The field's name, or NULL/\"\" for anonymous; a name another field\n"
    "    carries is refused.\n"
    "bits : NDArray[np.uint8]\n"
    "    The bits, one per element, each 0 or 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The new field's index, or -1 if the description is full or already\n"
    "    built, the name is taken, or an element is not a bit.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``cannot append a field: the description is full or already built,\n"
    "    the name is taken, the bits are empty, or an element is not a\n"
    "    bit``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],\n"
    "...                   np.uint8)\n"
    ">>> d = FrameDesc()                      # begin from nothing\n"
    ">>> d.add_field(\"asm\", asm_bits())       # the attached sync marker\n"
    "0\n"
    ">>> d.add_field(\"data\", np.unpackbits(octets))   # the transfer frame\n"
    "1\n"
    ">>> d.field_index(\"data\")\n"
    "1\n" },
  { "add_stage", (PyCFunction)(void *)FrameObj_add_stage,
    METH_VARARGS | METH_KEYWORDS,
    "add_stage(kind, first_field, n_fields, depth, emit_num, emit_den, "
    "unit_bits) -> int\n"
    "\n"
    "Append one transform and -- the load-bearing part -- the span of fields "
    "it covers. `kind` is a stage kind: `STAGE_CRC16`, `STAGE_RS`, "
    "`STAGE_RANDOMISE`, `STAGE_CONV`, `STAGE_INTERLEAVE` from `doppler.wfm`, "
    "or a caller's own from `STAGE_USER` up. It stays an INT rather than a "
    "name because the kind is an open `uint32_t` a caller extends -- the "
    "constants are generated from the C enum, so there is nothing to "
    "transcribe. `n_fields = 0` means the stage does not run. A stage that "
    "inherited whatever ran before it is the representation that cannot "
    "express a CCSDS CADU, where the marker is covered by the inner code and "
    "by neither the outer code nor the randomiser. `unit_bits` applies to "
    "`interleave` alone and is the bits per permuted unit (0 reads as 1); its "
    "ROW count is `depth` and its column count is derived from the span the "
    "stage covers.\n"
    "\n"
    "n_fields is the load-bearing part and 0 means the stage does not run. A\n"
    "stage that inherited \"everything before me\" instead of declaring its\n"
    "cover is the representation that cannot express a CCSDS CADU — see\n"
    "`wfm/wfm_frame.h`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "kind : int\n"
    "    stage kind: a wfm_stage_kind_t value (0=crc16…4=interleave), or a\n"
    "    caller's own from `WFM_STAGE_USER` (0x1000) up, whose kernel then\n"
    "    has to reach the assembler through its ops table.\n"
    "first_field : int\n"
    "    First field covered.\n"
    "n_fields : int\n"
    "    Fields covered; 0 = the stage does not run.\n"
    "depth : int\n"
    "    Interleaving depth, for an outer code.\n"
    "emit_num : int\n"
    "    Expansion numerator for a stage that emits a NEW stream; 0 when the\n"
    "    stage stays inside the frame.\n"
    "emit_den : int\n"
    "    Expansion denominator.\n"
    "unit_bits : int\n"
    "    INTERLEAVE only: bits per interleaved unit; 0 reads as 1. Match it\n"
    "    to the outer code's symbol — permuting octets is what spreads a\n"
    "    burst across the codewords of a code over GF(256), and permuting\n"
    "    bits inside one spreads a burst within a symbol that is already\n"
    "    wrong.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The new stage's index, or -1 if the description is full or already\n"
    "    built. The Python binding raises `ValueError` rather than handing\n"
    "    back the -1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``cannot append a stage: the description is full or already\n"
    "    built``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],\n"
    "...                   np.uint8)\n"
    ">>> d = FrameDesc()\n"
    ">>> _ = d.add_field(\"asm\", asm_bits())\n"
    ">>> _ = d.add_field(\"data\", np.unpackbits(octets))\n"
    ">>> _ = d.add_derived(\"parity\", 32 * 8)   # the outer code fills it\n"
    ">>> d.add_stage(1, first_field=1, n_fields=2, depth=1)   # RS(255,223)\n"
    "0\n"
    ">>> d.add_stage(2, first_field=1, n_fields=2)            # randomiser\n"
    "1\n"
    "\n"
    "Both start at field 1, so both skip the marker -- the cover is "
    "DECLARED,\n"
    "which is the whole reason a CADU is describable here:\n"
    "\n"
    ">>> d.build()\n"
    ">>> d.stage_first(0), d.stage_bits(0)\n"
    "(32, 2040)\n" },
  { "field_index", (PyCFunction)(void *)FrameObj_field_index,
    METH_VARARGS | METH_KEYWORDS,
    "field_index(name) -> int\n"
    "\n"
    "Index of the field called `name`, or -1 -- the one verb whose sentinel "
    "survives into Python, because a name that matches nothing is an ANSWER "
    "rather than a refusal. The one lookup that resolves a name, so every "
    "index-taking method keeps working and a rename can only be wrong once. "
    "An unnamed field is ANONYMOUS rather than named \"\", so the empty name "
    "matches nothing.\n"
    "\n"
    "The one lookup that resolves a name, so every index-taking entry point\n"
    "keeps working unchanged and a rename can only be wrong once. An unnamed\n"
    "field is ANONYMOUS rather than named `\"\"`, so the empty name matches\n"
    "nothing — including a field that has no name.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    the field name.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    the index, or -1 on NULL or a name no field carries. This is the\n"
    "    one verb whose -1 survives into Python: a name that matches nothing\n"
    "    is an ANSWER, not a refusal, so there is nothing to raise about.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> d = FrameDesc()\n"
    ">>> d.add_field(\"sync\", np.array([1,0,1,0,1,0,1,1,1,1,0,0], "
    "np.uint8))\n"
    "0\n"
    ">>> d.field_index(\"sync\")\n"
    "0\n"
    ">>> d.field_index(\"absent\")\n"
    "-1\n" },
  { "name_field", (PyCFunction)(void *)FrameObj_name_field,
    METH_VARARGS | METH_KEYWORDS,
    "name_field(index, name) -> None\n"
    "\n"
    "Give an already-appended field a name, or clear it with \"\". Refuses a "
    "name another field already carries, because `field_index` would then "
    "answer with whichever it reached first. Refuses once the frame is "
    "built.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "index : int\n"
    "    the field to name.\n"
    "name : str\n"
    "    the new name; truncated at `WFM_FRAME_NAME_MAX - 1`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``cannot name a field: the index is out of range, another field\n"
    "    already carries the name, or the frame is already built``, with the\n"
    "    return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> d = FrameDesc()\n"
    ">>> d.add_field(\"\", np.array([1, 0, 1, 0], np.uint8))   # anonymous\n"
    "0\n"
    ">>> d.name_field(0, \"payload\")\n"
    ">>> d.field_index(\"payload\")\n"
    "0\n" },
  { "add_derived", (PyCFunction)(void *)FrameObj_add_derived,
    METH_VARARGS | METH_KEYWORDS,
    "add_derived(name, bits) -> int\n"
    "\n"
    "Append a named field a STAGE will fill -- a CRC trailer, a block of "
    "check symbols. Its producer is not named here because no stage exists "
    "yet when the field it derives is appended; `add_stage_over` wires it. "
    "Returns the new field's index; a refusal raises `ValueError`.\n"
    "\n"
    "A field with a declared length and no source: a CRC trailer, a block of\n"
    "check symbols. Its producer is wired by dp_frame_add_stage_over rather\n"
    "than named here, because no stage exists yet when the field it derives\n"
    "is appended — fields are ordered by POSITION and stages by APPLICATION.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    the field's name, or NULL for anonymous.\n"
    "bits : int\n"
    "    its length, which its stage decides and the caller states.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``cannot append a derived field: the description is full or already\n"
    "    built``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> d = FrameDesc()\n"
    ">>> d.add_field(\"payload\", np.array([1, 0, 1, 0], np.uint8))\n"
    "0\n"
    ">>> d.add_derived(\"crc\", 16)          # a stage will fill it\n"
    "1\n" },
  { "add_stage_over", (PyCFunction)(void *)FrameObj_add_stage_over,
    METH_VARARGS | METH_KEYWORDS,
    "add_stage_over(kind, first, last, depth, unit_bits) -> int\n"
    "\n"
    "Append a stage covering `[first .. last]` BY NAME -- "
    "`add_stage_over(STAGE_CRC16, \"payload\", \"crc\")` says what three "
    "integers used to. It wires a derived field's producer for you, which "
    "applies the invariant the layout already enforces: a field with a "
    "declared length and no source sitting at the end of a cover has exactly "
    "one possible producer. `kind` is a stage kind, as for `add_stage`. "
    "Returns the new stage's index; a refusal raises `ValueError`.\n"
    "\n"
    "The cover is the load-bearing part of the representation and this is\n"
    "the form that reads. It wires a derived field's producer for you, which\n"
    "applies the invariant the layout already enforces rather than adding\n"
    "one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "kind : int\n"
    "    a stage kind — `doppler.wfm.STAGE_CRC16` and its siblings, or a\n"
    "    caller's own from `STAGE_USER` up.\n"
    "first : str\n"
    "    name of the first field covered.\n"
    "last : str\n"
    "    name of the last field covered; may equal first.\n"
    "depth : int\n"
    "    RS / interleave depth; 0 when unused.\n"
    "unit_bits : int\n"
    "    interleave unit; 0 reads as 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    the new stage's index, or -1 on NULL, a full description, a name\n"
    "    neither field carries, last before first, or once built. The Python\n"
    "    binding raises `ValueError` rather than handing back the -1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``cannot append a stage: the description is full, already built, or\n"
    "    names a field the description does not carry (and `last` must not\n"
    "    precede `first`)``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> d = FrameDesc()\n"
    ">>> d.add_field(\"payload\", np.array([0, 1, 1, 0, 1, 0, 0, 1], "
    "np.uint8))\n"
    "0\n"
    ">>> d.add_derived(\"crc\", 16)\n"
    "1\n"
    ">>> d.add_stage_over(0, \"payload\", \"crc\")   # 0 = crc16\n"
    "0\n"
    ">>> d.build()\n"
    ">>> d.crc_ok(d.bits())                # its own bits are its own truth\n"
    "1\n" },
  { "build", (PyCFunction)FrameObj_build, METH_NOARGS,
    "build() -> None\n"
    "\n"
    "Lay out and materialise a description. Where a description is checked: "
    "one that cannot produce its own bits is not a frame. Separate from the "
    "constructor only because the description arrives over several calls and "
    "there is no earlier moment at which it is complete. Raises if it is "
    "empty, unbuildable, names a stage no kernel here covers, or was already "
    "built.\n"
    "\n"
    "The point at which a description is checked, which for dp_frame_create\n"
    "happens inside the constructor: a description that cannot produce its\n"
    "own bits is not a frame. It is separate here only because the\n"
    "description arrives over several calls and there is no earlier moment\n"
    "at which it is complete.\n"
    "\n"
    "The CRC, the outer code, the randomiser and the inner code are all\n"
    "runnable: `ccsds_tm` has no Python binding and is not getting one, so\n"
    "this object is where a caller meets them. A stage naming a kernel\n"
    "nothing here carries is refused rather than skipped, because a stage\n"
    "that quietly did not run produces a frame that still assembles and\n"
    "syncs to nothing.\n"
    "\n"
    "The inner encoder starts from the all-zero register on every build: a\n"
    "description describes ONE frame. A stream of CADUs sharing one register\n"
    "is a transmitter's job and lives in `dp_ccsds_tm_frame_encode`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``cannot build: the description is empty, unbuildable, names a\n"
    "    stage no kernel here covers, or was already built``, with the\n"
    "    return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.nbits                     # 13 + 16 + 16, laid out by build()\n"
    "45\n"
    "\n"
    "A description that cannot produce bits is not a frame, and is refused\n"
    "rather than half-built:\n"
    "\n"
    ">>> FrameDesc().build()\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: cannot build: the description is empty, unbuildable, ...\n" },
  { "deframe", (PyCFunction)(void *)FrameObj_deframe,
    METH_VARARGS | METH_KEYWORDS,
    "deframe(rx_bits, out) -> ndarray\n"
    "\n"
    "Undo the description's stages over a received frame and hand back the "
    "CORRECTED bits — the layer a receiver stops short of (doppler#1022).\n"
    "\n"
    "The receive counterpart of building one, and the layer a receiver stops\n"
    "short of: `DsssBurstReceiver` and friends hand back hard and soft\n"
    "decisions for a frame's symbols and make no claim about what they mean,\n"
    "because knowing that needs a description — this one (doppler#1022).\n"
    "\n"
    "Returns the frame with every reversible stage undone, in place order: a\n"
    "randomiser XORed back, an outer code's repairs APPLIED, a CRC checked.\n"
    "The payload is then a slice, at dp_frame_field_off of the payload field\n"
    "— which is the caller's arithmetic because a description does not\n"
    "privilege one field over another.\n"
    "\n"
    "The verdict comes back as read-backs (`ok`, `units`, `checked`,\n"
    "`symbols`), not as a return value, since the return is the bits. Read\n"
    "them exactly as frame_check_t's, including the distinction that matters\n"
    "most: `checked == 0` says the description carries no reversible stage\n"
    "at all, which is a different fact from a check that failed.\n"
    "\n"
    "A stage with no `undo` kernel — a convolutional inner code, which a\n"
    "receiver cannot even frame-sync through — is reported as not checked\n"
    "rather than as passed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rx_bits : NDArray[np.uint8]\n"
    "    Received bits, `frame_bits` of them; treated as a capture and never\n"
    "    modified.\n"
    "out : NDArray[np.uint8] | None\n"
    "    Receives the corrected frame.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Bits written — the frame's length — or 0 if the description is\n"
    "    empty or either buffer is too small.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Frame\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], "
    "dtype=np.uint8)\n"
    ">>> f = Frame(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> rx = np.asarray(f.bits())          # a clean capture of its own "
    "frame\n"
    ">>> got = np.asarray(f.deframe(rx))\n"
    ">>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed\n"
    "(1, 1, 1)\n"
    ">>> off = f.field_off(f.field_index(\"payload\"))   # a SLICE\n"
    ">>> bool(np.array_equal(got[off:off + 16], payload))\n"
    "True\n"
    ">>> rx[off] ^= 1                       # one bit flipped in flight\n"
    ">>> _ = f.deframe(rx)\n"
    ">>> f.rx_ok, f.rx_units                # the check notices\n"
    "(0, 1)\n" },
  { "deframe_max_out", (PyCFunction)FrameObj_deframe_max_out, METH_VARARGS,
    "deframe_max_out(rx_bits_len) -> int\n"
    "\n"
    "Max bits dp_frame_deframe() writes: the frame's own length.\n"
    "\n"
    "Size a `deframe()` buffer with this. The bound is the DESCRIPTION's,\n"
    "not the input's: a frame is as long as its fields say, so how many bits\n"
    "were received does not change how many come back.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rx_bits_len : int\n"
    "    How many bits are on offer. Ignored, for the reason above; it is in\n"
    "    the signature because the binding's capacity call passes the\n"
    "    input's length.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The frame's length in bits, or 0 for an empty description.\n" },
  { "check", (PyCFunction)(void *)FrameObj_check, METH_VARARGS | METH_KEYWORDS,
    "check(rx_bits) -> FrameCheck record (passed, stages, checked, units, ok, "
    "corrected, symbols)\n"
    "\n"
    "Undo the description's stages over a received frame and report what was "
    "found -- the receive mirror of `bits()`, reading the same description, "
    "so a transmitter and a receiver holding the same `Frame` cannot disagree "
    "about which stage covered what. This is the truth-free frame error rate "
    "on a CODED link: it needs no payload truth, so it works on a real "
    "capture, and an outer code is a strictly better detector than a CRC "
    "because it reports how much repair it took rather than one bit of "
    "right-or-wrong. `checked` is smaller than `stages` when the description "
    "names a stage the receiver does not reverse here -- the inner code is "
    "the case, being undone before frame synchronisation -- and such a stage "
    "is reported as not checked, never as passed.\n"
    "\n"
    "The receive mirror of dp_frame_bits, reading the same description — so\n"
    "a transmitter and a receiver holding the same `Frame` cannot disagree\n"
    "about which stage covered what.\n"
    "\n"
    "**This is the truth-free frame error rate on a coded link.** It needs\n"
    "the description and the received bits and no payload truth at all, so\n"
    "it works on a real capture, and unlike a self-referenced EVM it still\n"
    "catches a false lock.\n"
    "\n"
    "checked is smaller than stages when the description names a stage the\n"
    "receiver does not reverse here — the inner code is the case, since it\n"
    "is undone before frame synchronisation and a frame checker never sees\n"
    "channel symbols. Such a stage is reported as not checked, never as\n"
    "passed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rx_bits : NDArray[np.uint8]\n"
    "    Received bits, one per byte. Copied, not modified.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "FrameCheck\n"
    "    The outcome. passed is 0 and checked is 0 when the description\n"
    "    carries no reversible stage at all — \"carries no check\" is not "
    "\"the\n"
    "    check passed\", and an FER conflating them would score every\n"
    "    unprotected frame as perfect.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> r = d.check(d.bits(1))\n"
    ">>> r.passed, r.ok, r.units\n"
    "(1, 1, 1)\n"
    "\n"
    "Flip a bit the CRC covers and the verdict turns over:\n"
    "\n"
    ">>> rx = np.asarray(d.bits(1)).copy()\n"
    ">>> rx[d.field_off(d.field_index(\"payload\"))] ^= 1\n"
    ">>> d.check(rx).passed\n"
    "0\n"
    "\n"
    "Carrying no check is NOT passing one -- both are reported, separately:\n"
    "\n"
    ">>> n = FrameDesc(sync=sync, payload=payload, crc=\"none\")\n"
    ">>> n.build()\n"
    ">>> c = n.check(n.bits(1))\n"
    ">>> c.passed, c.checked\n"
    "(0, 0)\n" },
  { "n_fields", (PyCFunction)FrameObj_n_fields, METH_NOARGS,
    "n_fields() -> int\n"
    "\n"
    "Fields in the description. A `Frame` counts only the fields it was given "
    "-- `Frame(sync=..., payload=..., crc=\"crc16\")` is 3 -- so read a field "
    "by name with `field_index`, not by position.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    How many fields the description carries.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.n_fields()          # sync, payload, crc -- no preamble was given\n"
    "3\n" },
  { "n_stages", (PyCFunction)FrameObj_n_stages, METH_NOARGS,
    "n_stages() -> int\n"
    "\n"
    "Stages in the description.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    How many stages the description carries.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.n_stages()         # the CRC is a stage like any other\n"
    "1\n" },
  { "field_off", (PyCFunction)(void *)FrameObj_field_off,
    METH_VARARGS | METH_KEYWORDS,
    "field_off(i) -> int\n"
    "\n"
    "Bit offset of field `i`, or 0 if there is no such field.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "i : int\n"
    "    Field index.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Bits from the start of the frame.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.field_off(0), d.field_off(1), d.field_off(2)\n"
    "(0, 13, 29)\n"
    "\n"
    "An absent field has no index: no preamble was given, so field 0 is the\n"
    "sync word. Ask for a field by name rather than by position, and an "
    "index\n"
    "past the end is 0.\n"
    "\n"
    ">>> d.field_off(d.field_index(\"crc\")), d.field_off(7)\n"
    "(29, 0)\n" },
  { "field_bits", (PyCFunction)(void *)FrameObj_field_bits,
    METH_VARARGS | METH_KEYWORDS,
    "field_bits(i) -> int\n"
    "\n"
    "Bits in field `i`, or 0 if there is no such field.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "i : int\n"
    "    Field index.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The field's length in bits.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.field_bits(0), d.field_bits(1), d.field_bits(2)\n"
    "(13, 16, 16)\n" },
  { "stage_first", (PyCFunction)(void *)FrameObj_stage_first,
    METH_VARARGS | METH_KEYWORDS,
    "stage_first(i) -> int\n"
    "\n"
    "First frame bit stage `i` covers; 0 for a stage that did not run.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "i : int\n"
    "    Stage index.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Bits from the start of the frame.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0\n"
    "13\n" },
  { "stage_bits", (PyCFunction)(void *)FrameObj_stage_bits,
    METH_VARARGS | METH_KEYWORDS,
    "stage_bits(i) -> int\n"
    "\n"
    "Bits stage `i` covers; 0 for a stage that did not run -- which is how an "
    "optional stage is spelled, and why `first` is 0 there too.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "i : int\n"
    "    Stage index.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The covered span, in bits.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.stage_bits(0)      # payload+CRC: what crc16 covered\n"
    "32\n" },
  { "destroy", (PyCFunction)FrameObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)FrameObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Frame be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Frame\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)FrameObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Frame.\n"
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
  { NULL }
};

static PyTypeObject FrameObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.wfm.Frame",
  .tp_basicsize                           = sizeof (FrameObject),
  .tp_dealloc                             = (destructor)FrameObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a frame instance.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "preamble : NDArray[np.uint8], default ...\n"
    "    Preamble bits, one per element, each 0 or 1. Omitted, there is no\n"
    "    preamble. A repeated preamble is repeated in its bits:\n"
    "    `field_bits(\"pn:31:5*4\")`.\n"
    "sync : NDArray[np.uint8], default ...\n"
    "    Sync-word bits, one per element, each 0 or 1. Omitted, the frame is\n"
    "    unsynced.\n"
    "payload : NDArray[np.uint8], default ...\n"
    "    Payload bits, one per element, each 0 or 1. Omitted, the frame "
    "carries\n"
    "    none.\n"
    "crc : Literal[\"none\", \"crc16\"], default \"none\"\n"
    "    Enum index; 0=none, 1=crc16 over the payload.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``frame geometry is\n"
    "    empty, or a field holds an element that is not a bit (0 or 1)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Frame\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # "
    "Barker-13\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> f = Frame(sync=sync, payload=payload, crc=\"crc16\")\n"
    ">>> f.nbits                                          # 13 + 16 + 16\n"
    "45\n"
    ">>> f.field_off(f.field_index(\"payload\"))\n"
    "13\n"
    ">>> f.crc_ok(f.bits())        # its own bits are its own truth\n"
    "1\n",
  .tp_methods = FrameObj_methods,
  .tp_getset  = Frame_getset,
  .tp_new     = FrameObj_new,
  .tp_init    = (initproc)FrameObj_init,
};
