/*
 * wfm_ext_frame.c — Frame type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* FrameObject — wraps frame_state_t *       */
/* ======================================================== */

#include "frame/frame_core.h"

typedef struct
{
  PyObject_HEAD frame_state_t *handle;
} FrameObject;

static void
FrameObj_dealloc (FrameObject *self)
{
  if (self->handle)
    frame_destroy (self->handle);
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
  static char       *kwlist[]              = { "preamble",
                                               "sync",
                                               "payload",
                                               "preamble_kind",
                                               "preamble_nbits",
                                               "preamble_reps",
                                               "preamble_poly",
                                               "preamble_seed",
                                               "preamble_reg_bits",
                                               "preamble_lfsr",
                                               "preamble_taps_a",
                                               "preamble_seed_a",
                                               "preamble_taps_b",
                                               "preamble_seed_b",
                                               "sync_kind",
                                               "sync_nbits",
                                               "sync_poly",
                                               "sync_seed",
                                               "sync_reg_bits",
                                               "sync_lfsr",
                                               "sync_taps_a",
                                               "sync_seed_a",
                                               "sync_taps_b",
                                               "sync_seed_b",
                                               "payload_kind",
                                               "payload_nbits",
                                               "payload_poly",
                                               "payload_seed",
                                               "payload_reg_bits",
                                               "payload_lfsr",
                                               "payload_taps_a",
                                               "payload_seed_a",
                                               "payload_taps_b",
                                               "payload_seed_b",
                                               "crc",
                                               NULL };
  PyObject          *preamble_obj          = NULL;
  PyObject          *sync_obj              = NULL;
  PyObject          *payload_obj           = NULL;
  const char        *preamble_kind_str     = "literal";
  unsigned long long preamble_nbits_raw    = 0;
  unsigned long long preamble_reps_raw     = 0;
  unsigned long long preamble_poly_raw     = 0;
  unsigned long long preamble_seed_raw     = 0;
  unsigned long      preamble_reg_bits_raw = 0;
  const char        *preamble_lfsr_str     = "galois";
  unsigned long long preamble_taps_a_raw   = 0;
  unsigned long long preamble_seed_a_raw   = 0;
  unsigned long long preamble_taps_b_raw   = 0;
  unsigned long long preamble_seed_b_raw   = 0;
  const char        *sync_kind_str         = "literal";
  unsigned long long sync_nbits_raw        = 0;
  unsigned long long sync_poly_raw         = 0;
  unsigned long long sync_seed_raw         = 0;
  unsigned long      sync_reg_bits_raw     = 0;
  const char        *sync_lfsr_str         = "galois";
  unsigned long long sync_taps_a_raw       = 0;
  unsigned long long sync_seed_a_raw       = 0;
  unsigned long long sync_taps_b_raw       = 0;
  unsigned long long sync_seed_b_raw       = 0;
  const char        *payload_kind_str      = "literal";
  unsigned long long payload_nbits_raw     = 0;
  unsigned long long payload_poly_raw      = 0;
  unsigned long long payload_seed_raw      = 0;
  unsigned long      payload_reg_bits_raw  = 0;
  const char        *payload_lfsr_str      = "galois";
  unsigned long long payload_taps_a_raw    = 0;
  unsigned long long payload_seed_a_raw    = 0;
  unsigned long long payload_taps_b_raw    = 0;
  unsigned long long payload_seed_b_raw    = 0;
  const char        *crc_str               = "none";

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "OOO|sKKKKksKKKKsKKKksKKKKsKKKksKKKKs", kwlist,
          &preamble_obj, &sync_obj, &payload_obj, &preamble_kind_str,
          &preamble_nbits_raw, &preamble_reps_raw, &preamble_poly_raw,
          &preamble_seed_raw, &preamble_reg_bits_raw, &preamble_lfsr_str,
          &preamble_taps_a_raw, &preamble_seed_a_raw, &preamble_taps_b_raw,
          &preamble_seed_b_raw, &sync_kind_str, &sync_nbits_raw,
          &sync_poly_raw, &sync_seed_raw, &sync_reg_bits_raw, &sync_lfsr_str,
          &sync_taps_a_raw, &sync_seed_a_raw, &sync_taps_b_raw,
          &sync_seed_b_raw, &payload_kind_str, &payload_nbits_raw,
          &payload_poly_raw, &payload_seed_raw, &payload_reg_bits_raw,
          &payload_lfsr_str, &payload_taps_a_raw, &payload_seed_a_raw,
          &payload_taps_b_raw, &payload_seed_b_raw, &crc_str))
    return -1;
  int preamble_kind = 0;
  if (strcmp (preamble_kind_str, "literal") == 0)
    preamble_kind = 0;
  else if (strcmp (preamble_kind_str, "pn") == 0)
    preamble_kind = 1;
  else if (strcmp (preamble_kind_str, "gold") == 0)
    preamble_kind = 2;
  else if (strcmp (preamble_kind_str, "dotted") == 0)
    preamble_kind = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "preamble_kind must be one of \"literal\", \"pn\", "
                    "\"gold\", \"dotted\", got '%s'",
                    preamble_kind_str);
      return -1;
    }
  size_t   preamble_nbits    = (size_t)preamble_nbits_raw;
  size_t   preamble_reps     = (size_t)preamble_reps_raw;
  uint64_t preamble_poly     = (uint64_t)preamble_poly_raw;
  uint64_t preamble_seed     = (uint64_t)preamble_seed_raw;
  uint32_t preamble_reg_bits = (uint32_t)preamble_reg_bits_raw;
  int      preamble_lfsr     = 0;
  if (strcmp (preamble_lfsr_str, "galois") == 0)
    preamble_lfsr = 0;
  else if (strcmp (preamble_lfsr_str, "fibonacci") == 0)
    preamble_lfsr = 1;
  else
    {
      PyErr_Format (
          PyExc_ValueError,
          "preamble_lfsr must be one of \"galois\", \"fibonacci\", got '%s'",
          preamble_lfsr_str);
      return -1;
    }
  uint64_t preamble_taps_a = (uint64_t)preamble_taps_a_raw;
  uint64_t preamble_seed_a = (uint64_t)preamble_seed_a_raw;
  uint64_t preamble_taps_b = (uint64_t)preamble_taps_b_raw;
  uint64_t preamble_seed_b = (uint64_t)preamble_seed_b_raw;
  int      sync_kind       = 0;
  if (strcmp (sync_kind_str, "literal") == 0)
    sync_kind = 0;
  else if (strcmp (sync_kind_str, "pn") == 0)
    sync_kind = 1;
  else if (strcmp (sync_kind_str, "gold") == 0)
    sync_kind = 2;
  else if (strcmp (sync_kind_str, "dotted") == 0)
    sync_kind = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "sync_kind must be one of \"literal\", \"pn\", \"gold\", "
                    "\"dotted\", got '%s'",
                    sync_kind_str);
      return -1;
    }
  size_t   sync_nbits    = (size_t)sync_nbits_raw;
  uint64_t sync_poly     = (uint64_t)sync_poly_raw;
  uint64_t sync_seed     = (uint64_t)sync_seed_raw;
  uint32_t sync_reg_bits = (uint32_t)sync_reg_bits_raw;
  int      sync_lfsr     = 0;
  if (strcmp (sync_lfsr_str, "galois") == 0)
    sync_lfsr = 0;
  else if (strcmp (sync_lfsr_str, "fibonacci") == 0)
    sync_lfsr = 1;
  else
    {
      PyErr_Format (
          PyExc_ValueError,
          "sync_lfsr must be one of \"galois\", \"fibonacci\", got '%s'",
          sync_lfsr_str);
      return -1;
    }
  uint64_t sync_taps_a  = (uint64_t)sync_taps_a_raw;
  uint64_t sync_seed_a  = (uint64_t)sync_seed_a_raw;
  uint64_t sync_taps_b  = (uint64_t)sync_taps_b_raw;
  uint64_t sync_seed_b  = (uint64_t)sync_seed_b_raw;
  int      payload_kind = 0;
  if (strcmp (payload_kind_str, "literal") == 0)
    payload_kind = 0;
  else if (strcmp (payload_kind_str, "pn") == 0)
    payload_kind = 1;
  else if (strcmp (payload_kind_str, "gold") == 0)
    payload_kind = 2;
  else if (strcmp (payload_kind_str, "dotted") == 0)
    payload_kind = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "payload_kind must be one of \"literal\", \"pn\", "
                    "\"gold\", \"dotted\", got '%s'",
                    payload_kind_str);
      return -1;
    }
  size_t   payload_nbits    = (size_t)payload_nbits_raw;
  uint64_t payload_poly     = (uint64_t)payload_poly_raw;
  uint64_t payload_seed     = (uint64_t)payload_seed_raw;
  uint32_t payload_reg_bits = (uint32_t)payload_reg_bits_raw;
  int      payload_lfsr     = 0;
  if (strcmp (payload_lfsr_str, "galois") == 0)
    payload_lfsr = 0;
  else if (strcmp (payload_lfsr_str, "fibonacci") == 0)
    payload_lfsr = 1;
  else
    {
      PyErr_Format (
          PyExc_ValueError,
          "payload_lfsr must be one of \"galois\", \"fibonacci\", got '%s'",
          payload_lfsr_str);
      return -1;
    }
  uint64_t payload_taps_a = (uint64_t)payload_taps_a_raw;
  uint64_t payload_seed_a = (uint64_t)payload_seed_a_raw;
  uint64_t payload_taps_b = (uint64_t)payload_taps_b_raw;
  uint64_t payload_seed_b = (uint64_t)payload_seed_b_raw;
  int      crc            = 0;
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
  PyArrayObject *preamble_arr = (PyArrayObject *)PyArray_FROM_OTF (
      preamble_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!preamble_arr)
    {
      return -1;
    }
  size_t         preamble_len = (size_t)PyArray_SIZE (preamble_arr);
  PyArrayObject *sync_arr     = (PyArrayObject *)PyArray_FROM_OTF (
      sync_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!sync_arr)
    {
      Py_DECREF (preamble_arr);
      return -1;
    }
  size_t         sync_len    = (size_t)PyArray_SIZE (sync_arr);
  PyArrayObject *payload_arr = (PyArrayObject *)PyArray_FROM_OTF (
      payload_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!payload_arr)
    {
      Py_DECREF (preamble_arr);
      Py_DECREF (sync_arr);
      return -1;
    }
  size_t payload_len = (size_t)PyArray_SIZE (payload_arr);
  self->handle       = frame_create (
      preamble_kind, (const uint8_t *)PyArray_DATA (preamble_arr),
      preamble_len, preamble_nbits, preamble_reps, preamble_poly,
      preamble_seed, preamble_reg_bits, preamble_lfsr, preamble_taps_a,
      preamble_seed_a, preamble_taps_b, preamble_seed_b, sync_kind,
      (const uint8_t *)PyArray_DATA (sync_arr), sync_len, sync_nbits,
      sync_poly, sync_seed, sync_reg_bits, sync_lfsr, sync_taps_a, sync_seed_a,
      sync_taps_b, sync_seed_b, payload_kind,
      (const uint8_t *)PyArray_DATA (payload_arr), payload_len, payload_nbits,
      payload_poly, payload_seed, payload_reg_bits, payload_lfsr,
      payload_taps_a, payload_seed_a, payload_taps_b, payload_seed_b, crc);
  Py_DECREF (preamble_arr);
  Py_DECREF (sync_arr);
  Py_DECREF (payload_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "frame geometry is empty or a field is unbuildable (a "
                       "literal with no array, or a generated field with no "
                       "register width)");
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
  return PyLong_FromSize_t (frame_bits_max_out (self->handle, (size_t)n));
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
      size_t _omax    = frame_bits_max_out (self->handle, (size_t)n);
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
        n_out = frame_bits (self->handle, (size_t)n, _ng0, _cap);
      Py_END_ALLOW_THREADS
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT8,
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
  size_t _cap  = frame_bits_max_out (self->handle, (size_t)n);
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
    n_out = frame_bits (self->handle, (size_t)n, _d0, _cap);
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

static PyStructSequence_Field FrameObj_layout_fields[] = {
  { "preamble_off", NULL },
  { "preamble_bits", NULL },
  { "sync_off", NULL },
  { "sync_bits", NULL },
  { "payload_off", NULL },
  { "payload_bits", NULL },
  { "crc_off", NULL },
  { "crc_bits", "16, or 0 when crc is unset or the payload is empty — a CRC "
                "over nothing protects nothing" },
  { "total_bits", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc FrameObj_layout_desc
    = { "doppler.wfm.FrameLayout",
        "Where each field lands, in bits from the start of the frame. The "
        "offsets a receiver needs to slice a capture -- computed once, by the "
        "same code the generator laid the frame out with.",
        FrameObj_layout_fields, 9 };
static PyTypeObject *FrameObj_layout_type = NULL;

static PyObject *
FrameObj_layout (FrameObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!FrameObj_layout_type)
    {
      FrameObj_layout_type = PyStructSequence_NewType (&FrameObj_layout_desc);
      if (!FrameObj_layout_type)
        return NULL;
    }
  wfm_frame_layout_t _r = frame_layout (self->handle);
  PyObject          *_o = PyStructSequence_New (FrameObj_layout_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (
      _o, 0,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.preamble_off));
  PyStructSequence_SET_ITEM (
      _o, 1,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.preamble_bits));
  PyStructSequence_SET_ITEM (
      _o, 2, PyLong_FromUnsignedLongLong ((unsigned long long)_r.sync_off));
  PyStructSequence_SET_ITEM (
      _o, 3, PyLong_FromUnsignedLongLong ((unsigned long long)_r.sync_bits));
  PyStructSequence_SET_ITEM (
      _o, 4, PyLong_FromUnsignedLongLong ((unsigned long long)_r.payload_off));
  PyStructSequence_SET_ITEM (
      _o, 5,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.payload_bits));
  PyStructSequence_SET_ITEM (
      _o, 6, PyLong_FromUnsignedLongLong ((unsigned long long)_r.crc_off));
  PyStructSequence_SET_ITEM (
      _o, 7, PyLong_FromUnsignedLongLong ((unsigned long long)_r.crc_bits));
  PyStructSequence_SET_ITEM (
      _o, 8, PyLong_FromUnsignedLongLong ((unsigned long long)_r.total_bits));
  return _o;
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
  int            y = frame_crc_ok (self->handle, rx_bits, rx_bits_len);
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
  static char *_kwlist[]
      = { "lit",    "kind",     "gen_len",    "reps",         "poly",
          "seed",   "reg_bits", "lfsr",       "taps_a",       "seed_a",
          "taps_b", "seed_b",   "derived_by", "derived_bits", NULL };
  PyObject          *lit_obj          = NULL;
  int                kind             = 0;
  unsigned long long gen_len_raw      = 0;
  unsigned long long reps_raw         = 0;
  unsigned long long poly_raw         = 0;
  unsigned long long seed_raw         = 0;
  unsigned long      reg_bits_raw     = 0;
  int                lfsr             = 0;
  unsigned long long taps_a_raw       = 0;
  unsigned long long seed_a_raw       = 0;
  unsigned long long taps_b_raw       = 0;
  unsigned long long seed_b_raw       = 0;
  unsigned long      derived_by_raw   = 0;
  unsigned long long derived_bits_raw = 0;
  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|iKKKKkiKKKKkK", _kwlist, &lit_obj, &kind,
          &gen_len_raw, &reps_raw, &poly_raw, &seed_raw, &reg_bits_raw, &lfsr,
          &taps_a_raw, &seed_a_raw, &taps_b_raw, &seed_b_raw, &derived_by_raw,
          &derived_bits_raw))
    return NULL;
  size_t         gen_len      = (size_t)gen_len_raw;
  size_t         reps         = (size_t)reps_raw;
  uint64_t       poly         = (uint64_t)poly_raw;
  uint64_t       seed         = (uint64_t)seed_raw;
  uint32_t       reg_bits     = (uint32_t)reg_bits_raw;
  uint64_t       taps_a       = (uint64_t)taps_a_raw;
  uint64_t       seed_a       = (uint64_t)seed_a_raw;
  uint64_t       taps_b       = (uint64_t)taps_b_raw;
  uint64_t       seed_b       = (uint64_t)seed_b_raw;
  uint32_t       derived_by   = (uint32_t)derived_by_raw;
  size_t         derived_bits = (size_t)derived_bits_raw;
  PyArrayObject *lit_arr      = (PyArrayObject *)PyArray_FROM_OTF (
      lit_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!lit_arr)
    {
      return NULL;
    }
  const uint8_t *lit     = (const uint8_t *)PyArray_DATA (lit_arr);
  size_t         lit_len = (size_t)PyArray_SIZE (lit_arr);
  int y = frame_add_field (self->handle, lit, lit_len, kind, gen_len, reps,
                           poly, seed, reg_bits, lfsr, taps_a, seed_a, taps_b,
                           seed_b, derived_by, derived_bits);
  Py_DECREF (lit_arr);
  return PyLong_FromLong ((long)y);
}

static PyObject *
FrameObj_add_stage (FrameObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "kind",     "first_field", "n_fields", "depth",
                              "emit_num", "emit_den",    NULL };
  int           kind      = 0;
  unsigned long first_field_raw = 0;
  unsigned long n_fields_raw    = 0;
  unsigned long depth_raw       = 0;
  unsigned long emit_num_raw    = 0;
  unsigned long emit_den_raw    = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ikkkkk", _kwlist, &kind,
                                    &first_field_raw, &n_fields_raw,
                                    &depth_raw, &emit_num_raw, &emit_den_raw))
    return NULL;
  uint32_t first_field = (uint32_t)first_field_raw;
  uint32_t n_fields    = (uint32_t)n_fields_raw;
  uint32_t depth       = (uint32_t)depth_raw;
  uint32_t emit_num    = (uint32_t)emit_num_raw;
  uint32_t emit_den    = (uint32_t)emit_den_raw;
  int y = frame_add_stage (self->handle, kind, first_field, n_fields, depth,
                           emit_num, emit_den);
  return PyLong_FromLong ((long)y);
}

static PyObject *
FrameObj_build (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = frame_build (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "build failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
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
   * object per stream). */
  frame_check_t _r;
  Py_BEGIN_ALLOW_THREADS
    _r = frame_check (self->handle, rx_bits, rx_bits_len);
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
  size_t y = frame_n_fields (self->handle);
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
  size_t y = frame_n_stages (self->handle);
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
  size_t y = frame_field_off (self->handle, i);
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
  size_t y = frame_field_bits (self->handle, i);
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
  size_t y = frame_stage_first (self->handle, i);
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
  size_t y = frame_stage_bits (self->handle, i);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
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

static PyGetSetDef Frame_getset[]
    = { { "nbits", (getter)Frame_getprop_nbits, NULL, "Nbits.\n", NULL },
        { NULL } };

static PyObject *
FrameObj_destroy (FrameObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      frame_destroy (self->handle);
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
      frame_destroy (self->handle);
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> len(d.bits())        # one frame: 13 + 16 + 16\n"
    "45\n"
    ">>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is\n"
    "90\n" },
  { "bits_max_out", (PyCFunction)FrameObj_bits_max_out, METH_VARARGS,
    "bits_max_out(n) -> int\n"
    "\n"
    "Bits frame_bits will write for n frames — `n * nbits`.\n"
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
  { "layout", (PyCFunction)FrameObj_layout, METH_VARARGS,
    "layout() -> FrameLayout record (preamble_off, preamble_bits, sync_off, "
    "sync_bits, payload_off, payload_bits, crc_off, crc_bits, total_bits)\n"
    "\n"
    "Where each field lands, in bits from the start of the frame.\n"
    "\n"
    "The offsets a receiver needs to slice a capture, computed by the same\n"
    "code the generator laid the frame out with.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "FrameLayout\n"
    "    Where each named field lands.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Frame\n"
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> lay = Frame(empty, sync, payload, crc=\"crc16\").layout()\n"
    ">>> lay.sync_off, lay.payload_off, lay.crc_off\n"
    "(0, 13, 29)\n"
    ">>> lay.total_bits\n"
    "45\n"
    "\n"
    "This is the NAMED view, so it reports the four fields a `Frame` is "
    "built\n"
    "from. A description assembled with `add_field` reports zeros here and "
    "is\n"
    "read with `field_off()` / `field_bits()` instead.\n" },
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.crc_ok(d.bits())           # its own bits are its own truth\n"
    "1\n"
    ">>> rx = np.asarray(d.bits()).copy()\n"
    ">>> rx[d.field_off(2)] ^= 1      # flip one payload bit\n"
    ">>> d.crc_ok(rx)\n"
    "0\n" },
  { "add_field", (PyCFunction)(void *)FrameObj_add_field,
    METH_VARARGS | METH_KEYWORDS,
    "add_field(lit, kind, gen_len, reps, poly, seed, reg_bits, lfsr, taps_a, "
    "seed_a, taps_b, seed_b, derived_by, derived_bits) -> int\n"
    "\n"
    "Append one field to a description (see `FrameDesc`). `kind` is a\n"
    "`wfm_seq_kind_t` index -- 0 literal, 1 pn, 2 gold, 3 dotted -- and\n"
    "`lfsr` a `wfm_lfsr` one (0 galois, 1 fibonacci); they are ints rather\n"
    "than the strings the constructor takes because a method parameter\n"
    "cannot yet be a string enum (jm gh-1021), and the C enum is the SSOT\n"
    "either way. Either the caller supplies the bits (`lit`, or a generated\n"
    "`kind`) or a stage derives them (`derived_by` non-zero) -- both are\n"
    "fields, because both are on the wire. Returns the new field's index,\n"
    "which is what `derived_by` and a stage's `first_field` are counted in.\n"
    "Refuses once the frame is built.\n"
    "\n"
    "Either the caller supplies the bits (lit, or a generated kind) or a\n"
    "stage derives them (derived_by non-zero). Both are fields, because both\n"
    "are on the wire.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "lit : NDArray[np.uint8]\n"
    "    Literal bits, copied here so the description outlives the call; may\n"
    "    be NULL.\n"
    "kind : int\n"
    "    wfm_seq_kind_t index; 0=literal…3=dotted.\n"
    "gen_len : int\n"
    "    Output bits for a GENERATED kind.\n"
    "reps : int\n"
    "    Repetitions of the field, verbatim; 0 means one.\n"
    "poly : int\n"
    "    PN feedback polynomial; 0 selects the maximal-length.\n"
    "seed : int\n"
    "    PN seed; 0 selects 1.\n"
    "reg_bits : int\n"
    "    PN/Gold register width.\n"
    "lfsr : int\n"
    "    0=galois, 1=fibonacci.\n"
    "taps_a : int\n"
    "    Gold: first register's taps.\n"
    "seed_a : int\n"
    "    Gold: first register's seed.\n"
    "taps_b : int\n"
    "    Gold: second register's taps.\n"
    "seed_b : int\n"
    "    Gold: second register's seed.\n"
    "derived_by : int\n"
    "    0 when the caller supplies this field; otherwise the index of the\n"
    "    producing stage, PLUS ONE.\n"
    "derived_bits : int\n"
    "    Length of a derived field, in bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The new field's index, or -1 if the description is full, already\n"
    "    built, or the literal could not be copied.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc, ccsds_asm_bits\n"
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> asm = ccsds_asm_bits()\n"
    ">>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],\n"
    "...                   np.uint8)\n"
    ">>> data = np.unpackbits(octets).astype(np.uint8)\n"
    ">>> d = FrameDesc(empty, empty, empty)   # begin from nothing\n"
    ">>> d.add_field(asm)                     # the attached sync marker\n"
    "0\n"
    ">>> d.add_field(data)                    # the transfer frame\n"
    "1\n"
    "\n"
    "A field the CALLER does not supply is still a field, because it is "
    "still\n"
    "on the wire -- `derived_by` names the stage that fills it, PLUS ONE:\n"
    "\n"
    ">>> d.add_field(empty, derived_by=1, derived_bits=32 * 8)\n"
    "2\n" },
  { "add_stage", (PyCFunction)(void *)FrameObj_add_stage,
    METH_VARARGS | METH_KEYWORDS,
    "add_stage(kind, first_field, n_fields, depth, emit_num, emit_den) -> "
    "int\n"
    "\n"
    "Append one transform and -- the load-bearing part -- the span of\n"
    "fields it covers. `kind` is a `wfm_stage_kind_t` index: 0 crc16, 1 rs,\n"
    "2 randomise, 3 conv (jm gh-1021 -- a method parameter cannot yet be a\n"
    "string enum). `n_fields = 0` means the stage does not run. A stage that\n"
    "inherited whatever ran before it is the representation that cannot\n"
    "express a CCSDS CADU, where the marker is covered by the inner code and\n"
    "by neither the outer code nor the randomiser.\n"
    "\n"
    "n_fields is the load-bearing part and 0 means the stage does not run. A\n"
    "stage that inherited \"everything before me\" instead of declaring its\n"
    "cover is the representation that cannot express a CCSDS CADU — see\n"
    "`wfm/wfm_frame.h`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "kind : int\n"
    "    wfm_stage_kind_t index; 0=crc16…3=conv.\n"
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
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The new stage's index, or -1 if the description is full or already\n"
    "    built.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc, ccsds_asm_bits\n"
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> asm = ccsds_asm_bits()\n"
    ">>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],\n"
    "...                   np.uint8)\n"
    ">>> data = np.unpackbits(octets).astype(np.uint8)\n"
    ">>> d = FrameDesc(empty, empty, empty)\n"
    ">>> _ = d.add_field(asm), d.add_field(data)\n"
    ">>> _ = d.add_field(empty, derived_by=1, derived_bits=32 * 8)\n"
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
  { "build", (PyCFunction)FrameObj_build, METH_NOARGS,
    "build() -> None\n"
    "\n"
    "Lay out and materialise a description. Where a description is\n"
    "checked: one that cannot produce its own bits is not a frame. Separate\n"
    "from the constructor only because the description arrives over several\n"
    "calls and there is no earlier moment at which it is complete. Raises if\n"
    "it is empty, unbuildable, names a stage no kernel here covers, or was\n"
    "already built.\n"
    "\n"
    "The point at which a description is checked, which for frame_create\n"
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
    "is a transmitter's job and lives in `ccsds_tm_frame_encode`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``build failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import FrameDesc\n"
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.nbits                     # 13 + 16 + 16, laid out by build()\n"
    "45\n"
    "\n"
    "A description that cannot produce bits is not a frame, and is refused\n"
    "rather than half-built:\n"
    "\n"
    ">>> FrameDesc(empty, empty, empty).build()\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: build failed (rc=-1)\n" },
  { "check", (PyCFunction)(void *)FrameObj_check, METH_VARARGS | METH_KEYWORDS,
    "check(rx_bits) -> FrameCheck record (passed, stages, checked, units, ok, "
    "corrected, symbols)\n"
    "\n"
    "Undo the description's stages over a received frame and report what\n"
    "was found -- the receive mirror of `bits()`, reading the same\n"
    "description, so a transmitter and a receiver holding the same `Frame`\n"
    "cannot disagree about which stage covered what. This is the truth-free\n"
    "frame error rate on a CODED link: it needs no payload truth, so it\n"
    "works on a real capture, and an outer code is a strictly better\n"
    "detector than a CRC because it reports how much repair it took rather\n"
    "than one bit of right-or-wrong. `checked` is smaller than `stages` when\n"
    "the description names a stage the receiver does not reverse here -- the\n"
    "inner code is the case, being undone before frame synchronisation --\n"
    "and such a stage is reported as not checked, never as passed.\n"
    "\n"
    "The receive mirror of frame_bits, reading the same description — so a\n"
    "transmitter and a receiver holding the same `Frame` cannot disagree\n"
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> r = d.check(d.bits(1))\n"
    ">>> r.passed, r.ok, r.units\n"
    "(1, 1, 1)\n"
    "\n"
    "Flip a bit the CRC covers and the verdict turns over:\n"
    "\n"
    ">>> rx = np.asarray(d.bits(1)).copy()\n"
    ">>> rx[d.field_off(2)] ^= 1\n"
    ">>> d.check(rx).passed\n"
    "0\n"
    "\n"
    "Carrying no check is NOT passing one -- both are reported, separately:\n"
    "\n"
    ">>> n = FrameDesc(empty, sync, payload, crc=\"none\")\n"
    ">>> n.build()\n"
    ">>> c = n.check(n.bits(1))\n"
    ">>> c.passed, c.checked\n"
    "(0, 0)\n" },
  { "n_fields", (PyCFunction)FrameObj_n_fields, METH_NOARGS,
    "n_fields() -> int\n"
    "\n"
    "Fields in the description. A `Frame` built the four-field way\n"
    "reports 4 -- `wfm_frame_t` IS a configuration of the general\n"
    "description, so the indexed view below reads it too.\n"
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.n_fields()          # the four named fields, absent ones included\n"
    "4\n" },
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.field_off(1), d.field_off(2), d.field_off(3)\n"
    "(0, 13, 29)\n"
    "\n"
    "Field 0 is the absent preamble: an empty field still HAS an index, so "
    "the\n"
    "indices a caller passed to `add_field` keep meaning what they meant.\n"
    "\n"
    ">>> d.field_off(0), d.field_bits(0)\n"
    "(0, 0)\n" },
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.field_bits(1), d.field_bits(2), d.field_bits(3)\n"
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
    ">>> d.build()\n"
    ">>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0\n"
    "13\n" },
  { "stage_bits", (PyCFunction)(void *)FrameObj_stage_bits,
    METH_VARARGS | METH_KEYWORDS,
    "stage_bits(i) -> int\n"
    "\n"
    "Bits stage `i` covers; 0 for a stage that did not run -- which is\n"
    "how an optional stage is spelled, and why `first` is 0 there too.\n"
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
    ">>> empty = np.empty(0, np.uint8)\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> d = FrameDesc(empty, sync, payload, crc=\"crc16\")\n"
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
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm.Frame",
  .tp_basicsize                           = sizeof (FrameObject),
  .tp_dealloc                             = (destructor)FrameObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a frame instance.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "preamble_kind : Literal[\"literal\", \"pn\", \"gold\", \"dotted\"], "
    "default \"literal\"\n"
    "    Enum index; 0=literal…3=dotted.\n"
    "preamble : NDArray[np.uint8]\n"
    "    Literal preamble bits, one per element. Pass an EMPTY array when "
    "the\n"
    "    field is absent or generated -- `wfm_seq_t` already spells absence "
    "as a\n"
    "    zero length, so this is that convention reaching Python rather than "
    "a\n"
    "    placeholder. (An omittable array init-param is a jm gap; see the "
    "module\n"
    "    docs.)\n"
    "preamble_nbits : int, default 0\n"
    "    Output bits for a GENERATED preamble kind. A literal takes its "
    "length\n"
    "    from the `preamble` array instead; `wfm_seq_t` names these apart "
    "(len\n"
    "    vs reg_bits) for the same reason.\n"
    "preamble_reps : int, default 0\n"
    "    Repetitions of the preamble; 0 = no preamble (default: 0).\n"
    "preamble_poly : int, default 0\n"
    "    PN feedback polynomial; 0 selects the maximal-length one (default: "
    "0).\n"
    "preamble_seed : int, default 0\n"
    "    PN seed; 0 selects 1, since an all-zero register is a fixed point\n"
    "    (default: 0).\n"
    "preamble_reg_bits : int, default 0\n"
    "    PN/Gold register width, 1..64 (default: 0).\n"
    "preamble_lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    Enum index; 0=galois…1=fibonacci.\n"
    "preamble_taps_a : int, default 0\n"
    "    Gold: first register's taps (default: 0).\n"
    "preamble_seed_a : int, default 0\n"
    "    Gold: first register's seed (default: 0).\n"
    "preamble_taps_b : int, default 0\n"
    "    Gold: second register's taps (default: 0).\n"
    "preamble_seed_b : int, default 0\n"
    "    Gold: second register's seed (default: 0).\n"
    "sync_kind : Literal[\"literal\", \"pn\", \"gold\", \"dotted\"], default "
    "\"literal\"\n"
    "    Enum index; 0=literal…3=dotted.\n"
    "sync : NDArray[np.uint8]\n"
    "    Literal sync word bits, one per element. Pass an EMPTY array when "
    "the\n"
    "    field is absent or generated -- `wfm_seq_t` already spells absence "
    "as a\n"
    "    zero length, so this is that convention reaching Python rather than "
    "a\n"
    "    placeholder. (An omittable array init-param is a jm gap; see the "
    "module\n"
    "    docs.)\n"
    "sync_nbits : int, default 0\n"
    "    Output bits for a GENERATED sync kind (default: 0).\n"
    "sync_poly : int, default 0\n"
    "    PN feedback polynomial; 0 selects the maximal-length one (default: "
    "0).\n"
    "sync_seed : int, default 0\n"
    "    PN seed; 0 selects 1 (default: 0).\n"
    "sync_reg_bits : int, default 0\n"
    "    PN/Gold register width, 1..64 (default: 0).\n"
    "sync_lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    Enum index; 0=galois…1=fibonacci.\n"
    "sync_taps_a : int, default 0\n"
    "    Gold: first register's taps (default: 0).\n"
    "sync_seed_a : int, default 0\n"
    "    Gold: first register's seed (default: 0).\n"
    "sync_taps_b : int, default 0\n"
    "    Gold: second register's taps (default: 0).\n"
    "sync_seed_b : int, default 0\n"
    "    Gold: second register's seed (default: 0).\n"
    "payload_kind : Literal[\"literal\", \"pn\", \"gold\", \"dotted\"], "
    "default \"literal\"\n"
    "    Enum index; 0=literal…3=dotted.\n"
    "payload : NDArray[np.uint8]\n"
    "    Literal payload bits, one per element. Pass an EMPTY array when the\n"
    "    field is absent or generated -- `wfm_seq_t` already spells absence "
    "as a\n"
    "    zero length, so this is that convention reaching Python rather than "
    "a\n"
    "    placeholder. (An omittable array init-param is a jm gap; see the "
    "module\n"
    "    docs.)\n"
    "payload_nbits : int, default 0\n"
    "    Output bits for a GENERATED payload kind (default: 0).\n"
    "payload_poly : int, default 0\n"
    "    PN feedback polynomial; 0 selects the maximal-length one (default: "
    "0).\n"
    "payload_seed : int, default 0\n"
    "    PN seed; 0 selects 1 (default: 0).\n"
    "payload_reg_bits : int, default 0\n"
    "    PN/Gold register width, 1..64 (default: 0).\n"
    "payload_lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    Enum index; 0=galois…1=fibonacci.\n"
    "payload_taps_a : int, default 0\n"
    "    Gold: first register's taps (default: 0).\n"
    "payload_seed_a : int, default 0\n"
    "    Gold: first register's seed (default: 0).\n"
    "payload_taps_b : int, default 0\n"
    "    Gold: second register's taps (default: 0).\n"
    "payload_seed_b : int, default 0\n"
    "    Gold: second register's seed (default: 0).\n"
    "crc : Literal[\"none\", \"crc16\"], default \"none\"\n"
    "    Enum index; 0=none…1=crc16.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``frame geometry is\n"
    "    empty or a field is unbuildable (a literal with no array, or a\n"
    "    generated field with no register width)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Frame\n"
    ">>> empty = np.empty(0, np.uint8)                    # an absent field\n"
    ">>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # "
    "Barker-13\n"
    ">>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)\n"
    ">>> f = Frame(empty, sync, payload, crc=\"crc16\")\n"
    ">>> f.nbits                                          # 13 + 16 + 16\n"
    "45\n"
    ">>> f.layout().payload_off\n"
    "13\n"
    ">>> f.crc_ok(f.bits())        # its own bits are its own truth\n"
    "1\n"
    "\n"
    "A payload a receiver can REGENERATE, rather than one it must be handed:\n"
    "\n"
    ">>> g = Frame(empty, sync, empty, payload_kind=\"pn\",\n"
    "...           payload_nbits=1024, payload_reg_bits=10, crc=\"crc16\")\n"
    ">>> g.nbits\n"
    "1053\n",
  .tp_methods = FrameObj_methods,
  .tp_getset  = Frame_getset,
  .tp_new     = FrameObj_new,
  .tp_init    = (initproc)FrameObj_init,
};
