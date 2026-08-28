/*
 * dsss_ext_burst_demod.c — BurstDemod type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* BurstDemodObject — wraps burst_demod_state_t *       */
/* ======================================================== */

#include "burst_demod/burst_demod_core.h"

typedef struct
{
  PyObject_HEAD burst_demod_state_t *handle;
} BurstDemodObject;

static void
BurstDemodObj_dealloc (BurstDemodObject *self)
{
  if (self->handle)
    burst_demod_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
BurstDemodObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  BurstDemodObject *self = (BurstDemodObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
BurstDemodObj_init (BurstDemodObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "data_code", "spc",        "chip_rate",    "carrier_hz",
          "max_rate",  "frame_syms", "est_segments", NULL };
  PyObject          *data_code_obj    = NULL;
  unsigned long long spc_raw          = 4;
  double             chip_rate        = 1.0e6;
  double             carrier_hz       = 0.0;
  double             max_rate         = 0.0;
  unsigned long long frame_syms_raw   = 0;
  unsigned long long est_segments_raw = 10;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|KdddKK", kwlist, &data_code_obj, &spc_raw, &chip_rate,
          &carrier_hz, &max_rate, &frame_syms_raw, &est_segments_raw))
    return -1;
  size_t         spc           = (size_t)spc_raw;
  size_t         frame_syms    = (size_t)frame_syms_raw;
  size_t         est_segments  = (size_t)est_segments_raw;
  PyArrayObject *data_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      data_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!data_code_arr)
    {
      return -1;
    }
  size_t data_code_len = (size_t)PyArray_SIZE (data_code_arr);
  self->handle         = burst_demod_create (
      (const uint8_t *)PyArray_DATA (data_code_arr), data_code_len, spc,
      chip_rate, carrier_hz, max_rate, frame_syms, est_segments);
  Py_DECREF (data_code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "burst_demod_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
BurstDemodObj_reset (BurstDemodObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  burst_demod_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
BurstDemodObj_set_preamble (BurstDemodObject *self, PyObject *args,
                            PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "acq_code", "reps", NULL };
  PyObject          *acq_code_obj = NULL;
  unsigned long long reps_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OK", _kwlist, &acq_code_obj,
                                    &reps_raw))
    return NULL;
  size_t         reps         = (size_t)reps_raw;
  PyArrayObject *acq_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      acq_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!acq_code_arr)
    {
      return NULL;
    }
  const uint8_t *acq_code     = (const uint8_t *)PyArray_DATA (acq_code_arr);
  size_t         acq_code_len = (size_t)PyArray_SIZE (acq_code_arr);
  burst_demod_set_preamble (self->handle, acq_code, acq_code_len, reps);
  Py_DECREF (acq_code_arr);
  Py_RETURN_NONE;
}

static PyObject *
BurstDemodObj_set_sync (BurstDemodObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "sync", NULL };
  PyObject    *sync_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &sync_obj))
    return NULL;
  PyArrayObject *sync_arr = (PyArrayObject *)PyArray_FROM_OTF (
      sync_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!sync_arr)
    {
      return NULL;
    }
  const uint8_t *sync     = (const uint8_t *)PyArray_DATA (sync_arr);
  size_t         sync_len = (size_t)PyArray_SIZE (sync_arr);
  burst_demod_set_sync (self->handle, sync, sync_len);
  Py_DECREF (sync_arr);
  Py_RETURN_NONE;
}

static PyObject *
BurstDemodObj_llrs_max_out (BurstDemodObject *self, PyObject *args)
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
      burst_demod_llrs_max_out (self->handle, (size_t)n));
}

static PyObject *
BurstDemodObj_llrs (BurstDemodObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = burst_demod_llrs_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = burst_demod_llrs (self->handle, (size_t)n,
                                       (float *)PyArray_DATA (out_arr), _cap);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _cap  = burst_demod_llrs_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = burst_demod_llrs (self->handle, (size_t)n, _d0, _cap);
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
BurstDemodObj_set_prior (BurstDemodObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "f0_coarse", "start", NULL };
  double             f0_coarse = 0.0;
  unsigned long long start_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dK", _kwlist, &f0_coarse,
                                    &start_raw))
    return NULL;
  size_t start = (size_t)start_raw;
  burst_demod_set_prior (self->handle, f0_coarse, start);
  Py_RETURN_NONE;
}

static PyObject *
BurstDemodObj_demod_max_out (BurstDemodObject *self,
                             PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (burst_demod_demod_max_out (self->handle));
}

static PyObject *
BurstDemodObj_demod (BurstDemodObject *self, PyObject *args, PyObject *kwds)
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
          Py_DECREF (x_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = burst_demod_demod_max_out (self->handle);
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
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      uint8_t             *_ng2 = (uint8_t *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = burst_demod_demod (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
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
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = burst_demod_demod_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  uint8_t *_d0 = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = burst_demod_demod (self->handle, _ng0, _ng1, _d0, _cap);
  Py_END_ALLOW_THREADS
  Py_DECREF (x_arr);
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
BurstDemod_getprop_frame_offset (BurstDemodObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->frame_offset);
}
static PyObject *
BurstDemod_getprop_n_symbols (BurstDemodObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_symbols);
}
static PyObject *
BurstDemod_getprop_est_freq_hz (BurstDemodObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->est_freq_hz);
}
static PyObject *
BurstDemod_getprop_est_rate_hz (BurstDemodObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->est_rate_hz);
}
static PyObject *
BurstDemod_getprop_est_snr_db (BurstDemodObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->est_snr_db);
}
static PyObject *
BurstDemod_getprop_frame_syms (BurstDemodObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->frame_syms);
}

static PyGetSetDef BurstDemod_getset[]
    = { { "frame_offset", (getter)BurstDemod_getprop_frame_offset, NULL,
          "symbol offset of the sync word.\n", NULL },
        { "n_symbols", (getter)BurstDemod_getprop_n_symbols, NULL,
          "despread data symbols produced.\n", NULL },
        { "est_freq_hz", (getter)BurstDemod_getprop_est_freq_hz, NULL,
          "estimated residual Doppler (Hz).\n", NULL },
        { "est_rate_hz", (getter)BurstDemod_getprop_est_rate_hz, NULL,
          "estimated Doppler rate (Hz/s).\n", NULL },
        { "est_snr_db", (getter)BurstDemod_getprop_est_snr_db, NULL,
          "estimator confidence (dB).\n", NULL },
        { "frame_syms", (getter)BurstDemod_getprop_frame_syms, NULL,
          "symbols the frame occupies AFTER the sync word — a number the "
          "caller states. What they MEAN is the frame description's business, "
          "one layer up.\n",
          NULL },
        { NULL } };

static PyObject *
BurstDemodObj_destroy (BurstDemodObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      burst_demod_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BurstDemodObj_enter (BurstDemodObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
BurstDemodObj_exit (BurstDemodObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      burst_demod_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef BurstDemodObj_methods[] = {
  { "reset", (PyCFunction)BurstDemodObj_reset, METH_NOARGS,
    "Clear the per-burst read-backs, leaving the configuration intact.\n"
    "\n"
    "Zeros the after-demod fields (frame_offset, n_symbols, and the est_*\n"
    "estimates) so a stale result cannot be mistaken for a fresh one. The\n"
    "spreading codes, sync word, and prior set up before the first burst are\n"
    "preserved, so the object is immediately ready to demodulate the next\n"
    "burst.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> dcode = (np.arange(50) & 1).astype(np.uint8)\n"
    ">>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)\n"
    ">>> d.reset()          # clears the estimates, keeps the config\n"
    ">>> d.frame_offset\n"
    "0\n" },

  { "set_preamble", (PyCFunction)(void *)BurstDemodObj_set_preamble,
    METH_VARARGS | METH_KEYWORDS,
    "set_preamble(acq_code, reps) -> None\n"
    "\n"
    "Set the (unmodulated) acquisition preamble code + repetition count\n"
    "used for the feedforward (f0, rate) estimate.\n"
    "\n"
    "The preamble is the acq spreading code transmitted reps times with no\n"
    "data modulation; demod() segment-despreads it into partial correlations\n"
    "and feeds those to the polynomial-phase estimator to recover the coarse\n"
    "(frequency, chirp-rate). Call once after construction; the code is\n"
    "copied.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "acq_code : NDArray[np.uint8]\n"
    "    Acq preamble spreading code, one 0/1 chip per element; copied into\n"
    "    the object.\n"
    "reps : int\n"
    "    Number of preamble repetitions in the burst.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> dcode = (np.arange(50) & 1).astype(np.uint8)\n"
    ">>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)\n"
    ">>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated\n"
    ">>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit\n" },
  { "set_sync", (PyCFunction)(void *)BurstDemodObj_set_sync,
    METH_VARARGS | METH_KEYWORDS,
    "set_sync(sync) -> None\n"
    "\n"
    "Set the known frame-sync word (0/1 BPSK symbols) used for frame\n"
    "alignment and phase/sign resolution. The ONLY thing this object is told\n"
    "about the frame's content, and for a physical-layer reason: without the\n"
    "sign the slicer would be a coin toss. Where the payload sits, which\n"
    "stages cover what and whether a check passed all need the frame's\n"
    "description and belong one layer up (doppler#1022).\n"
    "\n"
    "After the data section is despread to soft BPSK symbols, demod()\n"
    "correlates them against this word; the complex correlation peak locates\n"
    "the frame (its frame_offset) and its phase resolves the residual\n"
    "carrier rotation and the BPSK sign ambiguity before slicing. Pass the\n"
    "word as 0/1 symbols; it is copied and stored internally as +/-1.\n"
    "\n"
    "This is the ONLY thing this object is told about the frame's content,\n"
    "and it is told it for a physical-layer reason: without the sign the\n"
    "slicer would be a coin toss. Everything else — where the payload sits,\n"
    "which stages cover what, whether a check passed — needs the frame's\n"
    "description and belongs one layer up (doppler#1022).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sync : NDArray[np.uint8]\n"
    "    Frame-sync word, one 0/1 symbol per element; copied.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> dcode = (np.arange(50) & 1).astype(np.uint8)\n"
    ">>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)\n"
    ">>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)\n"
    ">>> d.set_sync(sync)   # Barker-13: frame align + phase/sign fix\n" },
  { "llrs", (PyCFunction)(void *)BurstDemodObj_llrs,
    METH_VARARGS | METH_KEYWORDS,
    "llrs(count=1) -> ndarray\n"
    "\n"
    "The soft bits of the last demod() — one LLR per FRAME bit, in\n"
    "`mpsk_soft_demap`'s convention: positive means bit 0, so `L < 0`\n"
    "reproduces exactly the bits demod() returned. `Re(sym * derot)` IS the\n"
    "log-likelihood ratio up to a scale and used to be computed, sliced to\n"
    "one bit and freed; a hard decision costs roughly 2 dB of the coding\n"
    "gain a soft-input decoder exists to deliver. Spans the whole frame\n"
    "rather than the payload alone, because a code covers what its\n"
    "description says it covers. Scaled by `est_n0`, the burst's own noise\n"
    "estimate, so LLRs from different bursts are comparable — a Viterbi\n"
    "would not care, but combining across bursts does.\n"
    "\n"
    "`crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and it\n"
    "was computed, sliced to one bit and freed on every burst. A hard\n"
    "decision throws away roughly 2 dB of the coding gain a soft-input\n"
    "decoder exists to deliver (`mpsk_soft_demap`'s own docstring), so this\n"
    "is what makes a coded burst worth coding.\n"
    "\n"
    "**The convention is not a new one**: `mpsk_soft_demap`'s, which is\n"
    "`mpsk_demap`'s decision rule seen a second way. Positive means bit 0,\n"
    "so `L < 0` reproduces exactly the bits demod() returned — asserted in\n"
    "the tests rather than assumed.\n"
    "\n"
    "Spans the WHOLE frame, not just the payload, because a code covers what\n"
    "its description says it covers and a decoder needs the bits the code\n"
    "protects. The payload's own span is `field_off`/`field_bits` of the\n"
    "layout.\n"
    "\n"
    "Scaled by est_n0 rather than left raw: a Viterbi is invariant to a\n"
    "positive scale, but LLRs from different bursts are not comparable\n"
    "without one, and combining across bursts needs them to be.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[np.float32] | None\n"
    "    Receives the LLRs, one per frame bit.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    LLRs written — `min(frame bits, max_out)`, or 0 if the last demod()\n"
    "    produced no frame.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> dcode = (np.arange(50) & 1).astype(np.uint8)\n"
    ">>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)\n"
    ">>> d.set_sync(np.zeros(13, dtype=np.uint8))\n"
    ">>> d.llrs_max_out(1)          # one per frame symbol\n"
    "93\n" },
  { "llrs_max_out", (PyCFunction)BurstDemodObj_llrs_max_out, METH_VARARGS,
    "llrs_max_out(n) -> int\n"
    "\n"
    "Max LLRs burst_demod_llrs() writes: the frame's length in bits.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Ignored — the count is the last demod()'s frame.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "set_prior", (PyCFunction)(void *)BurstDemodObj_set_prior,
    METH_VARARGS | METH_KEYWORDS,
    "set_prior(f0_coarse, start) -> None\n"
    "\n"
    "Seed from acquisition: coarse Doppler (cycles/sample at the input\n"
    "rate) and the preamble start sample.\n"
    "\n"
    "These come from the upstream acquisition stage: f0_coarse centres the\n"
    "feedforward frequency search near the true Doppler, and start tells\n"
    "demod() where the preamble begins within the burst so it despreads the\n"
    "right samples. Call once per burst before demod().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "f0_coarse : float\n"
    "    Coarse Doppler prior (cycles/sample at the input rate).\n"
    "start : int\n"
    "    Preamble start sample index within the burst.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> dcode = (np.arange(50) & 1).astype(np.uint8)\n"
    ">>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)\n"
    ">>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq\n" },
  { "demod", (PyCFunction)(void *)BurstDemodObj_demod,
    METH_VARARGS | METH_KEYWORDS,
    "demod(x, out) -> ndarray\n"
    "\n"
    "Demodulate a burst (preamble + frame); return the payload bits.\n"
    "Read-back properties report the estimates + CRC validity.\n"
    "\n"
    "Runs the whole feedforward chain on the supplied samples: estimate the\n"
    "(frequency, chirp-rate) from the preamble, dechirp, despread the data\n"
    "section to soft symbols, sync-align and derotate, and slice\n"
    "`frame_syms` symbols to bits. It writes the frame as received — sync\n"
    "word first — and makes no claim about what those bits are for: undoing\n"
    "the frame needs a description, and that is a caller's, not this\n"
    "object's. The soft twin of the same decisions is burst_demod_llrs().\n"
    "\n"
    "On return the read-back fields report the outcome — frame_offset,\n"
    "n_symbols, and the est_freq_hz / est_rate_hz / est_snr_db estimates.\n"
    "The templates and prior must already be set via set_preamble(),\n"
    "set_sync(), set_prior().\n"
    "\n"
    "The C function returns the number of bits written; the Python binding\n"
    "returns those bits as an array (a view into a reused buffer unless an\n"
    "out buffer is supplied).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Burst samples (complex baseband at spc*chip_rate).\n"
    "out : NDArray[np.uint8] | None\n"
    "    Caller-provided output buffer for the frame's bits.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Number of frame bits written (0 on failure / too-short burst).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50\n"
    ">>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)\n"
    ">>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(\n"
    "...     np.uint8)\n"
    ">>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)\n"
    ">>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)\n"
    ">>> def crc16(bits):\n"
    "...     c = 0xFFFF\n"
    "...     for b in bits:\n"
    "...         c ^= (int(b) & 1) << 15\n"
    "...         c = (((c << 1) ^ 0x1021) & 0xFFFF\n"
    "...              if c & 0x8000 else (c << 1) & 0xFFFF)\n"
    "...     return c\n"
    ">>> crc = crc16(payload)\n"
    ">>> crc_bits = np.array(\n"
    "...     [(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)\n"
    ">>> frame = np.concatenate([sync, payload, crc_bits])\n"
    ">>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)\n"
    ">>> chips = ([np.tile(csign(acode), reps)]\n"
    "...          + [csign(b) * csign(dcode) for b in frame])\n"
    ">>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)\n"
    ">>> n = np.arange(len(bb))\n"
    ">>> f0 = 0.012\n"
    ">>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)\n"
    ">>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, frame_syms=93)\n"
    ">>> d.set_preamble(acode, reps)\n"
    ">>> d.set_sync(sync)\n"
    ">>> d.set_prior(f0, 0)\n"
    ">>> bits = d.demod(x)\n"
    ">>> bool(np.array_equal(bits, frame))     # sync | payload | CRC, as "
    "sent\n"
    "True\n"
    ">>> from doppler.wfm import crc16\n"
    ">>> int(crc16(bits[13:77])) == crc        # the CHECK is the caller's\n"
    "True\n" },
  { "demod_max_out", (PyCFunction)BurstDemodObj_demod_max_out, METH_NOARGS,
    "demod_max_out() -> int\n"
    "\n"
    "Max output bits = frame_syms (caller sizes the buffer).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "destroy", (PyCFunction)BurstDemodObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)BurstDemodObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a BurstDemod be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BurstDemod\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)BurstDemodObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the BurstDemod.\n"
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

static PyTypeObject BurstDemodObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.BurstDemod",
  .tp_basicsize                           = sizeof (BurstDemodObject),
  .tp_dealloc                             = (destructor)BurstDemodObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a feedforward BPSK DSSS burst demodulator.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "data_code : NDArray[np.uint8]\n"
    "    Data spreading code, one 0/1 chip per element; copied into the "
    "object\n"
    "    (its length is the data spreading factor, chips/symbol).\n"
    "spc : int, default 4\n"
    "    Samples per chip (front-end oversample).\n"
    "chip_rate : float, default 1.0e6\n"
    "    Chip rate (Hz); sets the sample rate as spc*chip_rate.\n"
    "carrier_hz : float, default 0.0\n"
    "    RF carrier (Hz) for code-Doppler scaling; 0 = ignore.\n"
    "max_rate : float, default 0.0\n"
    "    Chirp-rate search half-span (cycles/sample^2 at the input rate); 0 "
    "=\n"
    "    Doppler only (no rate search).\n"
    "frame_syms : int, default 0\n"
    "    Symbols the frame occupies after the sync word — how many bits "
    "demod()\n"
    "    hands back per burst. What they mean is a frame description's "
    "business.\n"
    "est_segments : int, default 10\n"
    "    Partial correlations per acq period (segmentation for the "
    "feedforward\n"
    "    estimate; larger tolerates more rate).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstDemod\n"
    ">>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50\n"
    ">>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)\n"
    ">>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(\n"
    "...     np.uint8)\n"
    ">>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)\n"
    ">>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)\n"
    ">>> def crc16(bits):\n"
    "...     c = 0xFFFF\n"
    "...     for b in bits:\n"
    "...         c ^= (int(b) & 1) << 15\n"
    "...         c = (((c << 1) ^ 0x1021) & 0xFFFF\n"
    "...              if c & 0x8000 else (c << 1) & 0xFFFF)\n"
    "...     return c\n"
    ">>> crc = crc16(payload)\n"
    ">>> crc_bits = np.array(\n"
    "...     [(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)\n"
    ">>> frame = np.concatenate([sync, payload, crc_bits])\n"
    ">>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)\n"
    ">>> chips = ([np.tile(csign(acode), reps)]\n"
    "...          + [csign(b) * csign(dcode) for b in frame])\n"
    ">>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)\n"
    ">>> n = np.arange(len(bb))\n"
    ">>> f0 = 0.012\n"
    ">>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)\n"
    ">>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, "
    "frame_syms=len(frame))\n"
    ">>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble\n"
    ">>> d.set_sync(sync)              # Barker-13: frame align + sign fix\n"
    ">>> d.set_prior(f0, 0)            # coarse Doppler + preamble start\n"
    ">>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice\n"
    ">>> bool(np.array_equal(bits, frame))   # the FRAME, not the payload\n"
    "True\n",
  .tp_methods = BurstDemodObj_methods,
  .tp_getset  = BurstDemod_getset,
  .tp_new     = BurstDemodObj_new,
  .tp_init    = (initproc)BurstDemodObj_init,
};
