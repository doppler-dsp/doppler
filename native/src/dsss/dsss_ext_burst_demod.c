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
  uint8_t *_demod_buf;     /* pre-allocated output for demod */
  size_t   _demod_buf_cap; /* allocated capacity for demod */
  void   **_demod_retired; /* gh-219 deferred free */
  size_t   _demod_retired_n;
  size_t   _demod_retired_cap;
} BurstDemodObject;

static void
BurstDemodObj_dealloc (BurstDemodObject *self)
{
  if (self->handle)
    burst_demod_destroy (self->handle);
  free (self->_demod_buf);
  for (size_t _i = 0; _i < self->_demod_retired_n; _i++)
    free (self->_demod_retired[_i]);
  free (self->_demod_retired);
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
      = { "data_code", "spc",         "chip_rate",    "carrier_hz",
          "max_rate",  "payload_len", "est_segments", NULL };
  PyObject          *data_code_obj    = NULL;
  unsigned long long spc_raw          = 4;
  double             chip_rate        = 1.0e6;
  double             carrier_hz       = 0.0;
  double             max_rate         = 0.0;
  unsigned long long payload_len_raw  = 0;
  unsigned long long est_segments_raw = 10;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|KdddKK", kwlist, &data_code_obj, &spc_raw, &chip_rate,
          &carrier_hz, &max_rate, &payload_len_raw, &est_segments_raw))
    return -1;
  size_t         spc           = (size_t)spc_raw;
  size_t         payload_len   = (size_t)payload_len_raw;
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
      chip_rate, carrier_hz, max_rate, payload_len, est_segments);
  Py_DECREF (data_code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "burst_demod_create returned NULL");
      return -1;
    }
  {
    size_t _max = burst_demod_demod_max_out (self->handle);
    if (_max)
      {
        self->_demod_buf = malloc (_max * sizeof (uint8_t));
        if (!self->_demod_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_demod_buf_cap = _max;
      }
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
BurstDemodObj_demod (BurstDemodObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_demod_buf || self->_demod_buf_cap < _need)
    {
      size_t _max = burst_demod_demod_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_demod_buf
          && self->_demod_retired_n == self->_demod_retired_cap)
        {
          size_t _rcap
              = self->_demod_retired_cap ? self->_demod_retired_cap * 2 : 4;
          void **_rt = realloc (self->_demod_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_demod_retired     = _rt;
          self->_demod_retired_cap = _rcap;
        }
      uint8_t *_tmp = malloc (_max * sizeof (uint8_t));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_demod_buf)
        self->_demod_retired[self->_demod_retired_n++] = self->_demod_buf;
      self->_demod_buf     = _tmp;
      self->_demod_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = burst_demod_demod (self->handle, _ng0, _ng1, self->_demod_buf,
                               self->_demod_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_UINT8, self->_demod_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}
static PyObject *
BurstDemod_getprop_frame_valid (BurstDemodObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->frame_valid);
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

static PyGetSetDef BurstDemod_getset[]
    = { { "frame_valid", (getter)BurstDemod_getprop_frame_valid, NULL,
          "Frame valid.\n", NULL },
        { "frame_offset", (getter)BurstDemod_getprop_frame_offset, NULL,
          "Frame offset.\n", NULL },
        { "n_symbols", (getter)BurstDemod_getprop_n_symbols, NULL,
          "N symbols.\n", NULL },
        { "est_freq_hz", (getter)BurstDemod_getprop_est_freq_hz, NULL,
          "Est freq hz.\n", NULL },
        { "est_rate_hz", (getter)BurstDemod_getprop_est_rate_hz, NULL,
          "Est rate hz.\n", NULL },
        { "est_snr_db", (getter)BurstDemod_getprop_est_snr_db, NULL,
          "Est snr db.\n", NULL },
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

static PyMethodDef BurstDemodObj_methods[]
    = { { "reset", (PyCFunction)BurstDemodObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "set_preamble", (PyCFunction)(void *)BurstDemodObj_set_preamble,
          METH_VARARGS | METH_KEYWORDS,
          "set_preamble(acq_code, reps) -> None\n"
          "\n"
          "Set the (unmodulated) acquisition preamble code + repetition count "
          "used for the feedforward (f0, rate) estimate.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import BurstDemod\n"
          "    >>> obj = BurstDemod(np.zeros(1, dtype=np.uint8), 4, 1.0e6, "
          "0.0, 0.0, 0, 10)\n"
          "    >>> obj.set_preamble(np.zeros(4, dtype=np.uint8), 0)\n" },
        { "set_sync", (PyCFunction)(void *)BurstDemodObj_set_sync,
          METH_VARARGS | METH_KEYWORDS,
          "set_sync(sync) -> None\n"
          "\n"
          "Set the known frame-sync word (0/1 BPSK symbols) used for frame "
          "alignment + phase/sign resolution.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import BurstDemod\n"
          "    >>> obj = BurstDemod(np.zeros(1, dtype=np.uint8), 4, 1.0e6, "
          "0.0, 0.0, 0, 10)\n"
          "    >>> obj.set_sync(np.zeros(4, dtype=np.uint8))\n" },
        { "set_prior", (PyCFunction)(void *)BurstDemodObj_set_prior,
          METH_VARARGS | METH_KEYWORDS,
          "set_prior(f0_coarse, start) -> None\n"
          "\n"
          "Seed from acquisition: coarse Doppler (cycles/sample at the input "
          "rate) and the preamble start sample.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import BurstDemod\n"
          "    >>> obj = BurstDemod(np.zeros(1, dtype=np.uint8), 4, 1.0e6, "
          "0.0, 0.0, 0, 10)\n"
          "    >>> obj.set_prior(0.0, 0)\n" },
        { "demod", (PyCFunction)BurstDemodObj_demod, METH_VARARGS,
          "demod(x) -> ndarray\n"
          "\n"
          "Demodulate a burst (preamble + frame); return the payload bits. "
          "Read-back properties report the estimates + CRC validity.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import BurstDemod\n"
          "    >>> obj = BurstDemod(np.zeros(1, dtype=np.uint8), 4, 1.0e6, "
          "0.0, 0.0, 0, 10)\n"
          "    >>> y = obj.demod(np.zeros(4))\n"
          "    >>> y.dtype\n"
          "    dtype('uint8')\n" },
        { "destroy", (PyCFunction)BurstDemodObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)BurstDemodObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)BurstDemodObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject BurstDemodObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.BurstDemod",
  .tp_basicsize                           = sizeof (BurstDemodObject),
  .tp_dealloc                             = (destructor)BurstDemodObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create a burst demodulator.\n",
  .tp_methods                             = BurstDemodObj_methods,
  .tp_getset                              = BurstDemod_getset,
  .tp_new                                 = BurstDemodObj_new,
  .tp_init                                = (initproc)BurstDemodObj_init,
};
