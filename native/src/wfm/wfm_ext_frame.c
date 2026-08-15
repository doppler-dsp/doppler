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
  { "preamble_off", NULL }, { "preamble_bits", NULL },
  { "sync_off", NULL },     { "sync_bits", NULL },
  { "payload_off", NULL },  { "payload_bits", NULL },
  { "crc_off", NULL },      { "crc_bits", NULL },
  { "total_bits", NULL },   { NULL, NULL },
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
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Bits written.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Frame\n"
    "    >>> obj = Frame(preamble=np.zeros(1, dtype=np.uint8), "
    "sync=np.zeros(1, dtype=np.uint8), payload=np.zeros(1, dtype=np.uint8), "
    "preamble_kind=\"literal\", preamble_nbits=0, preamble_reps=0, "
    "preamble_poly=0, preamble_seed=0, preamble_reg_bits=0, "
    "preamble_lfsr=\"galois\", preamble_taps_a=0, preamble_seed_a=0, "
    "preamble_taps_b=0, preamble_seed_b=0, sync_kind=\"literal\", "
    "sync_nbits=0, sync_poly=0, sync_seed=0, sync_reg_bits=0, "
    "sync_lfsr=\"galois\", sync_taps_a=0, sync_seed_a=0, sync_taps_b=0, "
    "sync_seed_b=0, payload_kind=\"literal\", payload_nbits=0, "
    "payload_poly=0, payload_seed=0, payload_reg_bits=0, "
    "payload_lfsr=\"galois\", payload_taps_a=0, payload_seed_a=0, "
    "payload_taps_b=0, payload_seed_b=0, crc=\"none\")\n"
    "    >>> y = obj.bits(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
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
    "sync_bits, payload_off, payload_bits, crc_off, crc_bits, total_bits)." },
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
    "    >>> import numpy as np\n"
    "    >>> from doppler import Frame\n"
    "    >>> obj = Frame(preamble=np.zeros(1, dtype=np.uint8), "
    "sync=np.zeros(1, dtype=np.uint8), payload=np.zeros(1, dtype=np.uint8), "
    "preamble_kind=\"literal\", preamble_nbits=0, preamble_reps=0, "
    "preamble_poly=0, preamble_seed=0, preamble_reg_bits=0, "
    "preamble_lfsr=\"galois\", preamble_taps_a=0, preamble_seed_a=0, "
    "preamble_taps_b=0, preamble_seed_b=0, sync_kind=\"literal\", "
    "sync_nbits=0, sync_poly=0, sync_seed=0, sync_reg_bits=0, "
    "sync_lfsr=\"galois\", sync_taps_a=0, sync_seed_a=0, sync_taps_b=0, "
    "sync_seed_b=0, payload_kind=\"literal\", payload_nbits=0, "
    "payload_poly=0, payload_seed=0, payload_reg_bits=0, "
    "payload_lfsr=\"galois\", payload_taps_a=0, payload_seed_a=0, "
    "payload_taps_b=0, payload_seed_b=0, crc=\"none\")\n"
    "    >>> obj.crc_ok(np.zeros(4, dtype=np.uint8))\n"
    "    0\n" },
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
  = "Frame component.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "preamble_kind : Literal[\"literal\", \"pn\", \"gold\", \"dotted\"], "
    "default \"literal\"\n"
    "    preamble_kind constructor parameter.\n"
    "preamble : NDArray[np.uint8]\n"
    "    preamble constructor parameter.\n"
    "preamble_nbits : int, default 0\n"
    "    Output bits for a GENERATED preamble kind. A literal takes its "
    "length\n"
    "    from the `preamble` array instead; `wfm_seq_t` names these apart "
    "(len\n"
    "    vs reg_bits) for the same reason.\n"
    "preamble_reps : int, default 0\n"
    "    preamble_reps constructor parameter.\n"
    "preamble_poly : int, default 0\n"
    "    preamble_poly constructor parameter.\n"
    "preamble_seed : int, default 0\n"
    "    preamble_seed constructor parameter.\n"
    "preamble_reg_bits : int, default 0\n"
    "    preamble_reg_bits constructor parameter.\n"
    "preamble_lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    preamble_lfsr constructor parameter.\n"
    "preamble_taps_a : int, default 0\n"
    "    preamble_taps_a constructor parameter.\n"
    "preamble_seed_a : int, default 0\n"
    "    preamble_seed_a constructor parameter.\n"
    "preamble_taps_b : int, default 0\n"
    "    preamble_taps_b constructor parameter.\n"
    "preamble_seed_b : int, default 0\n"
    "    preamble_seed_b constructor parameter.\n"
    "sync_kind : Literal[\"literal\", \"pn\", \"gold\", \"dotted\"], default "
    "\"literal\"\n"
    "    sync_kind constructor parameter.\n"
    "sync : NDArray[np.uint8]\n"
    "    sync constructor parameter.\n"
    "sync_nbits : int, default 0\n"
    "    sync_nbits constructor parameter.\n"
    "sync_poly : int, default 0\n"
    "    sync_poly constructor parameter.\n"
    "sync_seed : int, default 0\n"
    "    sync_seed constructor parameter.\n"
    "sync_reg_bits : int, default 0\n"
    "    sync_reg_bits constructor parameter.\n"
    "sync_lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    sync_lfsr constructor parameter.\n"
    "sync_taps_a : int, default 0\n"
    "    sync_taps_a constructor parameter.\n"
    "sync_seed_a : int, default 0\n"
    "    sync_seed_a constructor parameter.\n"
    "sync_taps_b : int, default 0\n"
    "    sync_taps_b constructor parameter.\n"
    "sync_seed_b : int, default 0\n"
    "    sync_seed_b constructor parameter.\n"
    "payload_kind : Literal[\"literal\", \"pn\", \"gold\", \"dotted\"], "
    "default \"literal\"\n"
    "    payload_kind constructor parameter.\n"
    "payload : NDArray[np.uint8]\n"
    "    payload constructor parameter.\n"
    "payload_nbits : int, default 0\n"
    "    payload_nbits constructor parameter.\n"
    "payload_poly : int, default 0\n"
    "    payload_poly constructor parameter.\n"
    "payload_seed : int, default 0\n"
    "    payload_seed constructor parameter.\n"
    "payload_reg_bits : int, default 0\n"
    "    payload_reg_bits constructor parameter.\n"
    "payload_lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    payload_lfsr constructor parameter.\n"
    "payload_taps_a : int, default 0\n"
    "    payload_taps_a constructor parameter.\n"
    "payload_seed_a : int, default 0\n"
    "    payload_seed_a constructor parameter.\n"
    "payload_taps_b : int, default 0\n"
    "    payload_taps_b constructor parameter.\n"
    "payload_seed_b : int, default 0\n"
    "    payload_seed_b constructor parameter.\n"
    "crc : Literal[\"none\", \"crc16\"], default \"none\"\n"
    "    crc constructor parameter.\n"
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
    "Create with defaults:\n"
    "\n"
    ">>> from doppler import Frame\n"
    ">>> obj = Frame(\n"
    "...     preamble=np.zeros(1, dtype=np.uint8),\n"
    "...     sync=np.zeros(1, dtype=np.uint8),\n"
    "...     payload=np.zeros(1, dtype=np.uint8),\n"
    "...     preamble_kind=\"literal\",\n"
    "...     preamble_nbits=0,\n"
    "...     preamble_reps=0,\n"
    "...     preamble_poly=0,\n"
    "...     preamble_seed=0,\n"
    "...     preamble_reg_bits=0,\n"
    "...     preamble_lfsr=\"galois\",\n"
    "...     preamble_taps_a=0,\n"
    "...     preamble_seed_a=0,\n"
    "...     preamble_taps_b=0,\n"
    "...     preamble_seed_b=0,\n"
    "...     sync_kind=\"literal\",\n"
    "...     sync_nbits=0,\n"
    "...     sync_poly=0,\n"
    "...     sync_seed=0,\n"
    "...     sync_reg_bits=0,\n"
    "...     sync_lfsr=\"galois\",\n"
    "...     sync_taps_a=0,\n"
    "...     sync_seed_a=0,\n"
    "...     sync_taps_b=0,\n"
    "...     sync_seed_b=0,\n"
    "...     payload_kind=\"literal\",\n"
    "...     payload_nbits=0,\n"
    "...     payload_poly=0,\n"
    "...     payload_seed=0,\n"
    "...     payload_reg_bits=0,\n"
    "...     payload_lfsr=\"galois\",\n"
    "...     payload_taps_a=0,\n"
    "...     payload_seed_a=0,\n"
    "...     payload_taps_b=0,\n"
    "...     payload_seed_b=0,\n"
    "...     crc=\"none\",\n"
    "... )\n",
  .tp_methods = FrameObj_methods,
  .tp_getset  = Frame_getset,
  .tp_new     = FrameObj_new,
  .tp_init    = (initproc)FrameObj_init,
};
