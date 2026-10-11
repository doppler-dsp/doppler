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
  unsigned long long hop_raw    = 256;
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
    window = 0;
  else if (strcmp (window_str, "kaiser") == 0)
    window = 1;
  else if (strcmp (window_str, "blackman-harris") == 0)
    window = 2;
  else if (strcmp (window_str, "rect") == 0)
    window = 3;
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
    mode = 0;
  else if (strcmp (mode_str, "db") == 0)
    mode = 1;
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
                       "Spectrogram: invalid parameter (need 1 <= hop <= "
                       "nfft, nfft within the largest transform, a window "
                       "the PSD accepts, and mode power or db)");
      return -1;
    }
  return 0;
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
      size_t n_out = dp_spectrogram_push (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float *)PyArray_DATA (out_arr), _cap);
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
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_spectrogram_push (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
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
  float *row     = (float *)PyArray_DATA (row_arr);
  size_t row_len = (size_t)PyArray_SIZE (row_arr);
  size_t y       = dp_spectrogram_flush (self->handle, row, row_len);
  Py_DECREF (row_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
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
Spectrogram_getprop_consumed (SpectrogramObject *self,
                              void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_spectrogram_get_consumed (self->handle));
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
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_spectrogram_get_pending (self->handle));
}

static PyGetSetDef Spectrogram_getset[] = {
  { "consumed", (getter)Spectrogram_getprop_consumed, NULL,
    "Input samples the last push took (the `consumed` property).\n", NULL },
  { "pending", (getter)Spectrogram_getprop_pending, NULL,
    "Samples carried toward the next row (the `pending` property).\n", NULL },
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
    "Push input, write every whole row it completes that fits.\n"
    "\n"
    "Takes the input in order and writes row after row into out, each nfft\n"
    "floats, until either the input is used up or the next row is due and\n"
    "out has no room for it. Only whole rows are written: a max_out that is\n"
    "not a multiple of nfft uses floor(max_out / nfft) rows of it, and an\n"
    "out too small for one row writes nothing.\n"
    "\n"
    "A short out never loses input. A sample is taken unless taking it would\n"
    "complete a row out has no room for: the framer's feed contract\n"
    "(DECLARE_DP_BUFFER_FRAMES) applied to rows. dp_spectrogram_consumed()\n"
    "reports how many were taken, and the caller offers the rest again.\n"
    "Taken input that does not yet complete a row is the carry, held inside:\n"
    "fewer than nfft samples once this returns. So input that completes no\n"
    "row is always taken whole, even with max_out 0.\n"
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
    "    Floats written: a multiple of nfft, at most\n"
    "    dp_spectrogram_push_max_out(s, n_in). None when the C call writes\n"
    "    no output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "// nfft 8, hop 4, rectangular, power: a unit tone on bin 2\n"
    "dp_spectrogram_state_t *s\n"
    "    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "float _Complex x[16];\n"
    "for (int i = 0; i < 16; i++)\n"
    "  x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);\n"
    "float rows[3 * 8];\n"
    "size_t got = dp_spectrogram_push (s, x, 16, rows, 3 * 8);\n"
    "// 16 samples at hop 4 complete the rows starting at 0, 4 and 8\n"
    "if (got != 3 * 8 || dp_spectrogram_consumed (s) != 16)\n"
    "  return 1;\n"
    "if (fabsf (rows[4 + 2] - 1.0f) > 1e-4f) // bin 2 reads 1.0, at nfft/2 + "
    "2\n"
    "  return 1;\n"
    "dp_spectrogram_destroy (s);\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=8, window=\"rect\")\n"
    ">>> rows = sp.push(np.ones(20, np.complex64))\n"
    ">>> rows.shape\n"
    "(2, 8)\n"
    ">>> sp.consumed, sp.pending\n"
    "(20, 4)\n" },
  { "push_max_out", (PyCFunction)SpectrogramObj_push_max_out, METH_VARARGS,
    "push_max_out(n_in) -> int\n"
    "\n"
    "Floats one push of n_in samples writes when out has room:\n"
    "dp_spectrogram_rows_for(s, n_in) * nfft.\n"
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
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "dp_spectrogram_state_t *s\n"
    "    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "float _Complex x[16] = { 0 };\n"
    "float out[2 * 8];\n"
    "// 3 samples complete no row: no room is needed, and they are taken\n"
    "if (dp_spectrogram_push_max_out (s, 3) != 0)\n"
    "  return 1;\n"
    "dp_spectrogram_push (s, x, 3, out, 0);\n"
    "if (dp_spectrogram_consumed (s) != 3)\n"
    "  return 1;\n"
    "// 3 carried + 9 more = 12 samples: the rows starting at 0 and at 4\n"
    "size_t room = dp_spectrogram_push_max_out (s, 9);\n"
    "if (room != 2 * 8 || dp_spectrogram_push (s, x, 9, out, room) != room\n"
    "    || dp_spectrogram_consumed (s) != 9)\n"
    "  return 1;\n"
    "dp_spectrogram_destroy (s);\n" },
  { "flush", (PyCFunction)(void *)SpectrogramObj_flush,
    METH_VARARGS | METH_KEYWORDS,
    "flush(row) -> int\n"
    "\n"
    "End the stream: write the one zero-padded row it still owes, if any.\n"
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
    "    Floats written: nfft, or 0 if no row was owed or row_len is less\n"
    "    than nfft.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "dp_spectrogram_state_t *s\n"
    "    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "dp_spectrogram_state_t *t\n"
    "    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "if (!s || !t)\n"
    "  return 1;\n"
    "float _Complex x[10], tail[8] = { 0 };\n"
    "for (int i = 0; i < 10; i++)\n"
    "  x[i] = (float)(i + 1);\n"
    "float row[8], last[8], want[8];\n"
    "// 10 samples: one row, [0, 8); samples 8 and 9 no row has covered\n"
    "dp_spectrogram_push (s, x, 10, row, 8);\n"
    "if (dp_spectrogram_pending (s) != 2)\n"
    "  return 1;\n"
    "// the owed row starts on the hop grid, at 4 (not at 8): x[4..10), "
    "zeros\n"
    "if (dp_spectrogram_flush (s, last, 8) != 8)\n"
    "  return 1;\n"
    "memcpy (tail, x + 4, 6 * sizeof *x);      // the same frame, pushed "
    "whole\n"
    "if (dp_spectrogram_push (t, tail, 8, want, 8) != 8\n"
    "    || memcmp (last, want, sizeof want) != 0)\n"
    "  return 1;\n"
    "if (dp_spectrogram_flush (s, last, 8) != 0)  // the stream is over\n"
    "  return 1;\n"
    "dp_spectrogram_destroy (t);\n"
    "dp_spectrogram_destroy (s);\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=8, window=\"rect\")\n"
    ">>> _ = sp.push(np.ones(11, np.complex64))\n"
    ">>> row = np.empty(8, np.float32)\n"
    ">>> sp.flush(row)\n"
    "8\n"
    ">>> sp.flush(row)\n"
    "0\n" },
  { "rows_for", (PyCFunction)(void *)SpectrogramObj_rows_for,
    METH_VARARGS | METH_KEYWORDS,
    "rows_for(n_in) -> int\n"
    "\n"
    "Rows a push of n_in more samples completes, given the carry: exact,\n"
    "not an estimate.\n"
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
    "dp_spectrogram_state_t *s\n"
    "    = dp_spectrogram_create (8, 2, 3, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "if (dp_spectrogram_rows_for (s, 7) != 0      // less than a frame\n"
    "    || dp_spectrogram_rows_for (s, 8) != 1\n"
    "    || dp_spectrogram_rows_for (s, 100) != 47) // (100 - 8) / 2 + 1\n"
    "  return 1;\n"
    "dp_spectrogram_destroy (s);\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=2)\n"
    ">>> sp.rows_for(7), sp.rows_for(8), sp.rows_for(100)\n"
    "(0, 1, 47)\n" },
  { "reset", (PyCFunction)SpectrogramObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Forget the stream: drop the carry and restart at sample 0.\n"
    "\n"
    "The next row covers samples [0, nfft) of whatever is pushed next.\n"
    "dp_spectrogram_consumed() and dp_spectrogram_pending() read 0.\n"
    "Configuration is kept.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "dp_spectrogram_state_t *s\n"
    "    = dp_spectrogram_create (8, 8, 3, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "float _Complex x[5] = { 0 };\n"
    "float row[8];\n"
    "dp_spectrogram_push (s, x, 5, row, 8);   // 5 samples of carry, no row\n"
    "if (dp_spectrogram_pending (s) != 5)\n"
    "  return 1;\n"
    "dp_spectrogram_reset (s);                 // the carry is gone\n"
    "if (dp_spectrogram_pending (s) != 0)\n"
    "  return 1;\n"
    "dp_spectrogram_destroy (s);\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=8, hop=8)\n"
    ">>> _ = sp.push(np.ones(5, np.complex64))\n"
    ">>> sp.pending\n"
    "5\n"
    ">>> sp.reset()\n"
    ">>> sp.pending\n"
    "0\n" },
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
  = "Create a streaming spectrogram.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "nfft : int, default 256\n"
    "    Samples per frame and bins per row, which must be the same number: "
    "an\n"
    "    nfft the PSD would zero-pad to a longer transform (anything but a "
    "power\n"
    "    of two >= 2) is refused rather than given rows wider than its "
    "frames.\n"
    "hop : int, default 256\n"
    "    Samples between row starts, 1 <= hop <= nfft. hop == nfft tiles the\n"
    "    stream; hop < nfft overlaps the frames.\n"
    "window : Literal[\"hann\", \"kaiser\", \"blackman-harris\", \"rect\"], "
    "default \"hann\"\n"
    "    0 = Hann, 1 = Kaiser, 2 = Blackman-Harris, 3 = rectangular, as\n"
    "    dp_psd_create(), which also refuses a window that sums to zero at "
    "this\n"
    "    nfft (the symmetric Hann at nfft = 2).\n"
    "beta : float, default 0.0\n"
    "    Kaiser beta (ignored for the other windows).\n"
    "mode : Literal[\"power\", \"db\"], default \"power\"\n"
    "    DP_SPECTROGRAM_POWER (linear rows, the default, 0) or "
    "DP_SPECTROGRAM_DB\n"
    "    (dBFS rows, 1); any other value is refused. Pass it by name: `make\n"
    "    lint` refuses an integer literal here.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``Spectrogram: "
    "invalid\n"
    "    parameter (need 1 <= hop <= nfft, nfft within the largest transform, "
    "a\n"
    "    window the PSD accepts, and mode power or db)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "// nfft 1024, a row every 256 samples (75% overlap), Blackman-Harris,\n"
    "// power rows (the default), DC-centred\n"
    "dp_spectrogram_state_t *s\n"
    "    = dp_spectrogram_create (1024, 256, 2, 0.0f, DP_SPECTROGRAM_POWER);\n"
    "// the same rows in dBFS, asked for by name\n"
    "dp_spectrogram_state_t *d\n"
    "    = dp_spectrogram_create (1024, 256, 2, 0.0f, DP_SPECTROGRAM_DB);\n"
    "if (!s || !d)\n"
    "  return 1;\n"
    "// refused: 1000 is not a power of two, a hop may not exceed nfft, and\n"
    "// a mode is one of the two\n"
    "const int not_a_mode = DP_SPECTROGRAM_DB + 1;\n"
    "if (dp_spectrogram_create (1000, 256, 2, 0.0f, DP_SPECTROGRAM_POWER)\n"
    "    || dp_spectrogram_create (1024, 2048, 2, 0.0f, "
    "DP_SPECTROGRAM_POWER)\n"
    "    || dp_spectrogram_create (1024, 256, 2, 0.0f, not_a_mode))\n"
    "  return 1;\n"
    "dp_spectrogram_destroy (d);\n"
    "dp_spectrogram_destroy (s);\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import Spectrogram\n"
    ">>> sp = Spectrogram(nfft=1024, hop=256, window=\"blackman-harris\")\n"
    ">>> rows = sp.push(np.ones(1024 + 256, np.complex64))\n"
    ">>> rows.shape\n"
    "(2, 1024)\n",
  .tp_methods = SpectrogramObj_methods,
  .tp_getset  = Spectrogram_getset,
  .tp_new     = SpectrogramObj_new,
  .tp_init    = (initproc)SpectrogramObj_init,
};
