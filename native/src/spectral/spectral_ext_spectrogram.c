/* jm:generated spectral_ext_spectrogram.c */
/*
 * spectral_ext_spectrogram.c — Spectrogram type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in spectral_ext_spectrogram_extra.c.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* SpectrogramObject — wraps dp_spectrogram_state_t *       */
/* ======================================================== */

#include "doppler/spectrogram/spectrogram_core.h"

typedef struct
{
  PyObject_HEAD dp_spectrogram_state_t *handle;
} SpectrogramObject;

static void
SpectrogramObj_dealloc (SpectrogramObject *self)
{
  if (self->handle)
    dp_spectrogram_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SpectrogramObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  SpectrogramObject *self = (SpectrogramObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SpectrogramObj_init (SpectrogramObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "nfft", "hop", "window", "beta", "mode", NULL };
  unsigned long long nfft_raw   = 256;
  unsigned long long hop_raw    = 0;
  const char        *window_str = "hann";
  float              beta       = 0.0f;
  const char        *mode_str   = "power";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKsfs", kwlist, &nfft_raw,
                                    &hop_raw, &window_str, &beta, &mode_str))
    return -1;
  size_t nfft   = (size_t)nfft_raw;
  size_t hop    = (size_t)hop_raw;
  int    window = 0;
  if (strcmp (window_str, "hann") == 0)
    window = DP_PSD_WINDOW_HANN;
  else if (strcmp (window_str, "kaiser") == 0)
    window = DP_PSD_WINDOW_KAISER;
  else if (strcmp (window_str, "blackman-harris") == 0)
    window = DP_PSD_WINDOW_BLACKMAN_HARRIS;
  else if (strcmp (window_str, "rect") == 0)
    window = DP_PSD_WINDOW_RECT;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", "
                    "\"blackman-harris\", \"rect\", got '%s'",
                    window_str);
      return -1;
    }
  int mode = 0;
  if (strcmp (mode_str, "power") == 0)
    mode = DP_SPECTROGRAM_POWER;
  else if (strcmp (mode_str, "db") == 0)
    mode = DP_SPECTROGRAM_DB;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "mode must be one of \"power\", \"db\", got '%s'",
                    mode_str);
      return -1;
    }
  self->handle = dp_spectrogram_create (nfft, hop, window, beta, mode);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "Spectrogram: invalid parameter (need hop in 0..nfft, "
                       "0 meaning nfft; nfft within the largest transform, a "
                       "window the PSD accepts, and mode power or db)");
      return -1;
    }
  return 0;
}

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static const char *const _enum_Spectrogram_psd_window[] = {
  "hann", "kaiser", "blackman-harris", "rect", NULL,
};
static const int _enum_Spectrogram_psd_window_c[] = {
  DP_PSD_WINDOW_HANN,
  DP_PSD_WINDOW_KAISER,
  DP_PSD_WINDOW_BLACKMAN_HARRIS,
  DP_PSD_WINDOW_RECT,
};
static inline const char *
_enum_Spectrogram_psd_window_name (long v)
{
  for (int j = 0; j < 4; j++)
    if (_enum_Spectrogram_psd_window_c[j] == v)
      return _enum_Spectrogram_psd_window[j];
  return NULL;
}

static const char *const _enum_Spectrogram_spectrogram_mode[] = {
  "power",
  "db",
  NULL,
};
static const int _enum_Spectrogram_spectrogram_mode_c[] = {
  DP_SPECTROGRAM_POWER,
  DP_SPECTROGRAM_DB,
};
static inline const char *
_enum_Spectrogram_spectrogram_mode_name (long v)
{
  for (int j = 0; j < 2; j++)
    if (_enum_Spectrogram_spectrogram_mode_c[j] == v)
      return _enum_Spectrogram_spectrogram_mode[j];
  return NULL;
}

static PyObject *
SpectrogramObj_push_max_out (SpectrogramObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n_in = 0;
  if (!PyArg_ParseTuple (args, "n", &n_in))
    return NULL;
  return PyLong_FromSize_t (
      dp_spectrogram_push_max_out (self->handle, (size_t)n_in));
}

static PyObject *
SpectrogramObj_push (SpectrogramObject *self, PyObject *args, PyObject *kwds)
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
  Py_ssize_t              n     = PyArray_SIZE (in_arr);
  dp_spectrogram_state_t *state = self->handle;
  size_t                  _cols = (size_t)(state->nfft);
  if (_cols == 0)
    {
      PyErr_SetString (PyExc_RuntimeError,
                       "Spectrogram.push: out_cols evaluated to 0");
      Py_DECREF (in_arr);
      return NULL;
    }
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      if (PyArray_NDIM (out_arr) != 2
          || (size_t)PyArray_DIM (out_arr, 1) != _cols)
        {
          PyErr_Format (
              PyExc_ValueError,
              "out must be a 2-D array with %zu columns, shape (rows, %zu)",
              _cols, _cols);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_spectrogram_push_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float *_ng1 = (float *)PyArray_DATA (out_arr);
      size_t n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out
            = dp_spectrogram_push (self->handle, _ng0, (size_t)n, _ng1, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (in_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Spectrogram.push: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp _odim = (npy_intp)n_out;
      if ((size_t)_odim % _cols != 0)
        {
          PyErr_Format (PyExc_RuntimeError,
                        "Spectrogram.push: kernel returned %zu elements, not "
                        "a whole number of %zu-wide rows",
                        (size_t)_odim, _cols);
          Py_DECREF (out_arr);
          return NULL;
        }
      npy_intp  _odims[2] = { _odim / (npy_intp)_cols, (npy_intp)_cols };
      PyObject *_oview    = PyArray_SimpleNewFromData (2, _odims, NPY_FLOAT,
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
  size_t _cap  = dp_spectrogram_push_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Spectrogram.push: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_spectrogram_push (self->handle, _ng0, (size_t)n, _d0, _cap);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Spectrogram.push: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
    }
  npy_intp _odim = (npy_intp)n_out;
  if ((size_t)_odim % _cols != 0)
    {
      PyErr_Format (PyExc_RuntimeError,
                    "Spectrogram.push: kernel returned %zu elements, not a "
                    "whole number of %zu-wide rows",
                    (size_t)_odim, _cols);
      Py_DECREF (arr0);
      return NULL;
    }
  npy_intp     _odims[2] = { _odim / (npy_intp)_cols, (npy_intp)_cols };
  PyArray_Dims _rs0      = { _odims, 2 };
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
SpectrogramObj_flush (SpectrogramObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "row", NULL };
  PyObject    *row_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &row_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (row_obj)
      || PyArray_TYPE ((PyArrayObject *)row_obj) != NPY_FLOAT
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)row_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)row_obj))
    {
      PyErr_SetString (PyExc_TypeError, "row must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *row_arr = jm_array_arg (
      row_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "row");
  if (!row_arr)
    {
      return NULL;
    }
  float  *row     = (float *)PyArray_DATA (row_arr);
  size_t  row_len = (size_t)PyArray_SIZE (row_arr);
  int64_t _rc     = dp_spectrogram_flush (self->handle, row, row_len);
  Py_DECREF (row_arr);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "flush: row must hold exactly nfft floats",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLongLong ((long long)_rc);
}

static PyObject *
SpectrogramObj_rows_for (SpectrogramObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n_in", NULL };
  unsigned long long n_in_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_in_raw))
    return NULL;
  size_t n_in = (size_t)n_in_raw;
  size_t y    = dp_spectrogram_rows_for (self->handle, n_in);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
SpectrogramObj_reset (SpectrogramObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_spectrogram_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
SpectrogramObj_state_bytes (SpectrogramObject *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_spectrogram_state_bytes (self->handle));
}

static PyObject *
SpectrogramObj_get_state (SpectrogramObject *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_spectrogram_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_spectrogram_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
SpectrogramObj_set_state (SpectrogramObject *self, PyObject *arg)
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
      != dp_spectrogram_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_spectrogram_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Spectrogram_getprop_nfft (SpectrogramObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
Spectrogram_getprop_hop (SpectrogramObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->hop);
}
static PyObject *
Spectrogram_getprop_window (SpectrogramObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long        _v = (long)(self->handle->window);
  const char *_s = _enum_Spectrogram_psd_window_name (_v);
  if (!_s)
    {
      PyErr_Format (PyExc_ValueError,
                    "window holds psd_window value %ld, which is none of"
                    " its choices",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_s);
}
static PyObject *
Spectrogram_getprop_beta (SpectrogramObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->beta);
}
static PyObject *
Spectrogram_getprop_mode (SpectrogramObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long        _v = (long)(self->handle->mode);
  const char *_s = _enum_Spectrogram_spectrogram_mode_name (_v);
  if (!_s)
    {
      PyErr_Format (PyExc_ValueError,
                    "mode holds spectrogram_mode value %ld, which is none of"
                    " its choices",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_s);
}
static PyObject *
Spectrogram_getprop_consumed (SpectrogramObject *self,
                              void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->consumed);
}
static PyObject *
Spectrogram_getprop_pending (SpectrogramObject *self,
                             void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_spectrogram_pending (self->handle)));
}

static PyGetSetDef Spectrogram_getset[] = {
  { "nfft", (getter)Spectrogram_getprop_nfft, NULL,
    "Samples per frame, and bins per row: one number, because a frame the PSD "
    "would zero-pad to a longer transform is refused at construction.\n",
    NULL },
  { "hop", (getter)Spectrogram_getprop_hop, NULL,
    "Samples between row starts, 1 to nfft: nfft tiles the stream, anything "
    "less overlaps the frames. A hop of 0 at construction means nfft, and "
    "reads back as nfft.\n",
    NULL },
  { "window", (getter)Spectrogram_getprop_window, NULL,
    "The window every frame is shaped by: \"hann\", \"kaiser\", "
    "\"blackman-harris\" or \"rect\".\n",
    NULL },
  { "beta", (getter)Spectrogram_getprop_beta, NULL,
    "The Kaiser beta the Spectrogram was made with, as the float32 it keeps: "
    "0.1 reads back 0.10000000149011612. It shapes the Kaiser window only; "
    "under the others it changes no row.\n",
    NULL },
  { "mode", (getter)Spectrogram_getprop_mode, NULL,
    "The row units: \"power\" (linear against full scale, the default) or "
    "\"db\" (dBFS).\n",
    NULL },
  { "consumed", (getter)Spectrogram_getprop_consumed, NULL,
    "Input samples the last push took. From Python it is always the chunk's "
    "length: the binding sizes its output for every row the chunk completes, "
    "so no input is held back.\n",
    NULL },
  { "pending", (getter)Spectrogram_getprop_pending, NULL,
    "Samples pushed that no row has covered yet: what flush() would turn into "
    "a row. 0 means the stream is complete as it stands.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
SpectrogramObj_destroy (SpectrogramObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_spectrogram_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SpectrogramObj_enter (SpectrogramObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
SpectrogramObj_exit (SpectrogramObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_spectrogram_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef SpectrogramObj_methods[] = {

  { "push", (PyCFunction)(void *)SpectrogramObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x, out) -> ndarray | None\n"
    "\n"
    "Push a chunk of samples, and return the rows it completes.\n"
    "\n"
    "The rows come back as a (rows, nfft) float32 matrix, or None when the "
    "chunk\n"
    "completes none. A partial frame is carried to the next push. consumed "
    "equals\n"
    "the input length: the binding allocates room for every row the chunk "
    "can\n"
    "complete, so no input is held back. To bound memory, push smaller "
    "chunks,\n"
    "sized with rows_for(n).\n"
    "\n"
    "An object is not thread-safe: one Spectrogram must not be pushed from "
    "two\n"
    "threads at once, because the kernel uses the object's scratch. push "
    "releases\n"
    "the GIL, so other Python threads run meanwhile, but not on this object.\n"
    "\n"
    "An out array, if given, must be a float32 array of at least\n"
    "push_max_out(len(x)) elements. A smaller out raises ValueError; it is "
    "not\n"
    "taken as a short one.\n"
    "\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=8, window=\"rect\")\n"
    ">>> rows = sp.push(np.ones(20, np.complex64))\n"
    ">>> rows.shape\n"
    "(2, 8)\n"
    ">>> sp.consumed, sp.pending\n"
    "(20, 4)\n"
    "\n"
    "Takes the input in order and produces each row as soon as a frame of\n"
    "nfft samples is complete, at every hop. A partial frame is carried to\n"
    "the next call, so the rows do not depend on how the input was split\n"
    "into calls. Input that completes no row is always taken whole.\n"
    "\n"
    "Only whole rows are written. A C caller bounds the write with max_out\n"
    "and takes the rows that fit; the consumed count reports the input those\n"
    "rows used, and the caller offers the rest again. The Python face has no\n"
    "short buffer: its binding sizes the output for the whole chunk (see the\n"
    "Python method doc).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Rows, row-major, nfft floats each.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    Floats written, a multiple of nfft. From Python, the completed rows\n"
    "    as a (rows, nfft) matrix, or None when none is completed. None when\n"
    "    the C call writes no output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import Spectrogram\n"
    "    >>> obj = Spectrogram(nfft=256, hop=0, window=\"hann\", beta=0.0, "
    "mode=\"power\")\n"
    "    >>> y = obj.push(np.zeros(4, dtype=np.complex64))\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "push_max_out", (PyCFunction)SpectrogramObj_push_max_out, METH_VARARGS,
    "push_max_out(n_in) -> int\n"
    "\n"
    "Floats one push of n_in samples writes when out has room: the\n"
    "rows_for(n_in) count times nfft.\n"
    "\n"
    "The capacity that makes a push take ALL of its input. Saturates at\n"
    "SIZE_MAX rather than wrapping.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Samples about to be pushed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "flush", (PyCFunction)(void *)SpectrogramObj_flush,
    METH_VARARGS | METH_KEYWORDS,
    "flush(row) -> int\n"
    "\n"
    "End the stream: write the row a pending partial frame is owed into row.\n"
    "\n"
    "row must be a writable float32 array of exactly nfft elements; any other "
    "size\n"
    "raises ValueError, and the owed frame stays owed. Returns nfft when a "
    "frame was\n"
    "owed and was written, 0 when none was owed. A flush restarts the "
    "stream.\n"
    "\n"
    "The example uses hop 4, so the owed row starts on the hop grid, at "
    "sample 4:\n"
    "\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=4, window=\"rect\")\n"
    ">>> _ = sp.push(np.ones(11, np.complex64))\n"
    ">>> row = np.empty(8, np.float32)\n"
    ">>> sp.flush(row)\n"
    "8\n"
    ">>> sp.flush(row)\n"
    "0\n"
    "\n"
    "The row sits on the hop grid: it starts at the next row start k*hop,\n"
    "never at the first sample no row covered, so it is the row a one-shot\n"
    "push of the input zero-padded to that row's end would have written. It\n"
    "is written if and only if the stream holds a sample no earlier row\n"
    "covered. Either way the spectrogram then restarts at sample 0, as after\n"
    "reset, so a second flush writes nothing.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "row : npt.NDArray[np.float32]\n"
    "    Room for nfft floats.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Floats written: nfft, or 0 if no row was owed; -1 if the row is not\n"
    "    exactly nfft floats long.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``flush: row must hold exactly nfft floats``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import Spectrogram\n"
    "    >>> obj = Spectrogram(nfft=256, hop=0, window=\"hann\", beta=0.0, "
    "mode=\"power\")\n"
    "    >>> obj.flush(np.zeros(4, dtype=np.float32))\n"
    "    0\n" },
  { "rows_for", (PyCFunction)(void *)SpectrogramObj_rows_for,
    METH_VARARGS | METH_KEYWORDS,
    "rows_for(n_in) -> int\n"
    "\n"
    "How many rows a push of n_in more samples would complete, from the "
    "current carry.\n"
    "\n"
    "Takes no input. Use it to size chunks that bound memory.\n"
    "\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=2)\n"
    ">>> sp.rows_for(7), sp.rows_for(8), sp.rows_for(100)\n"
    "(0, 1, 47)\n"
    "\n"
    "The carry plus n_in samples, cut into frames of nfft at the hop. A push\n"
    "with room for this many rows writes exactly this many and takes all of\n"
    "n_in.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Samples about to be pushed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.spectral import Spectrogram\n"
    "    >>> obj = Spectrogram(nfft=256, hop=0, window=\"hann\", beta=0.0, "
    "mode=\"power\")\n"
    "    >>> obj.rows_for(0)\n"
    "    0\n" },
  { "reset", (PyCFunction)SpectrogramObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Forget the stream: drop the carry and restart at sample 0.\n"
    "\n"
    "The configuration is kept. consumed and pending read 0 afterwards.\n"
    "\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=8)\n"
    ">>> _ = sp.push(np.ones(5, np.complex64))\n"
    ">>> sp.pending\n"
    "5\n"
    ">>> sp.reset()\n"
    ">>> sp.pending\n"
    "0\n"
    "\n"
    "The next row covers samples [0, nfft) of whatever is pushed next. The\n"
    "consumed count and the pending count read 0. Configuration is kept.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from doppler.spectral import Spectrogram\n"
    "    >>> obj = Spectrogram(nfft=256, hop=0, window=\"hann\", beta=0.0, "
    "mode=\"power\")\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)SpectrogramObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Spectrogram has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)SpectrogramObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the Spectrogram has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)SpectrogramObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Spectrogram has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)SpectrogramObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)SpectrogramObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Spectrogram be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Spectrogram\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)SpectrogramObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Spectrogram.\n"
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

static PyTypeObject SpectrogramObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.spectral.Spectrogram",
  .tp_basicsize                           = sizeof (SpectrogramObject),
  .tp_dealloc                             = (destructor)SpectrogramObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "A streaming short-time power spectrum: a stream in chunks of any size, "
    "one row of nfft bins per hop samples, each row the PSD of its frame.\n"
    "\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=8, window=\"rect\")\n"
    ">>> rows = sp.push(np.ones(16, np.complex64))\n"
    ">>> rows.shape\n"
    "(2, 8)\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "nfft : int, default 256\n"
    "    Samples per frame and bins per row, which must be the same number: "
    "an\n"
    "    nfft that would be zero-padded to a longer transform (anything but "
    "a\n"
    "    power of two >= 2) is refused rather than given rows wider than its\n"
    "    frames.\n"
    "hop : int, default 0\n"
    "    Samples between row starts, 1 to nfft. 0 means nfft, which tiles "
    "the\n"
    "    stream; the default. Less than nfft overlaps the frames. A hop "
    "above\n"
    "    nfft would leave samples no row covers, so it is refused.\n"
    "window : Literal[\"hann\", \"kaiser\", \"blackman-harris\", \"rect\"], "
    "default \"hann\"\n"
    "    The window: \"hann\", \"kaiser\", \"blackman-harris\" or \"rect\", "
    "as PSD takes\n"
    "    them. It also refuses a window that sums to zero at this nfft (the\n"
    "    symmetric Hann at nfft = 2).\n"
    "\n"
    "    - ``\"hann\"`` — Hann: the default.\n"
    "    - ``\"kaiser\"`` — Kaiser, shaped by `beta`.\n"
    "    - ``\"blackman-harris\"`` — 4-term Blackman-Harris.\n"
    "    - ``\"rect\"`` — Rectangular: no taper, best resolution, worst "
    "leakage.\n"
    "beta : float, default 0.0\n"
    "    Kaiser beta (ignored for the other windows).\n"
    "mode : Literal[\"power\", \"db\"], default \"power\"\n"
    "    Row units: \"power\" (linear rows, the default) or \"db\" (dBFS "
    "rows, asked\n"
    "    for by name). Any other value is refused.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``Spectrogram: "
    "invalid\n"
    "    parameter (need hop in 0..nfft, 0 meaning nfft; nfft within the "
    "largest\n"
    "    transform, a window the PSD accepts, and mode power or db)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> obj = Spectrogram(\n"
    "...     nfft=256,\n"
    "...     hop=0,\n"
    "...     window=\"hann\",\n"
    "...     beta=0.0,\n"
    "...     mode=\"power\",\n"
    "... )\n",
  .tp_methods = SpectrogramObj_methods,
  .tp_getset  = Spectrogram_getset,
  .tp_new     = SpectrogramObj_new,
  .tp_init    = (initproc)SpectrogramObj_init,
};
