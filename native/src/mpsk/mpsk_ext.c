/*
 * mpsk_ext.c — Python extension module mpsk
 *
 * Objects:
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/mpsk/mpsk_core.h"

#ifndef JM_ARRAY_ARG_DEFINED
#define JM_ARRAY_ARG_DEFINED
/* Convert a Python argument for an array parameter to an ndarray of
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, except that for a
 * one-byte element type a byte buffer (bytes, bytearray, memoryview) is its
 * bytes, one element per byte (gh-1700). `name` is the parameter, for the
 * message. `hint` is its declared str_hint, or NULL. Declaring one is the
 * opt-in to refusing text (gh-1824): a str, or a bytes numpy would parse as
 * a number, and a str's refusal ends with the hint (gh-1756). With NULL,
 * numpy converts a str as it converts anything else. Returns a new
 * reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  int text     = PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj));
  if (hint && text)
    {
      /* The hint says where text goes instead: a str's refusal only. */
      int say = PyUnicode_Check (obj);
      PyErr_Format (PyExc_TypeError,
                    "%s must be an array of numbers, not %.200s%s%s", name,
                    Py_TYPE (obj)->tp_name, say ? ": " : "", say ? hint : "");
      return NULL;
    }
  if (one_byte && !PyArray_Check (obj) && PyObject_CheckBuffer (obj))
    {
      PyObject *view = PyMemoryView_FromObject (obj);
      if (!view)
        return NULL;
      if (PyMemoryView_GET_BUFFER (view)->itemsize == 1)
        {
          PyObject *raw = PyArray_FromBuffer (
              view, PyArray_DescrFromType (typenum), -1, 0);
          Py_DECREF (view);
          if (!raw)
            return NULL;
          PyObject *arr = PyArray_FROM_OTF (raw, typenum, requirements);
          Py_DECREF (raw);
          return (PyArrayObject *)arr;
        }
      Py_DECREF (view);
    }
  return (PyArrayObject *)PyArray_FROM_OTF (obj, typenum, requirements);
}
/* `unused` (gh-1747): emitted into every extension translation unit,
 * including one that takes no array, or calls only jm_array_arg_hint above
 * -- which needs no mark, since this wrapper always calls it. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

static PyObject *
_bind_mpsk_map (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "sym", "m", NULL };
  PyObject    *sym_obj   = NULL;
  int          m         = 4;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|i", _kwlist, &sym_obj, &m))
    return NULL;
  PyArrayObject *sym_arr = (PyArrayObject *)jm_array_arg (
      sym_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "sym");
  if (!sym_arr)
    {
      return NULL;
    }
  const uint8_t *sym       = (const uint8_t *)PyArray_DATA (sym_arr);
  size_t         sym_len   = (size_t)PyArray_SIZE (sym_arr);
  size_t         _dim_need = (size_t)(sym_len);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (sym_arr);
      PyErr_Format (PyExc_OverflowError,
                    "mpsk_map: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_COMPLEX64, 0);
  if (!_out)
    {
      Py_DECREF (sym_arr);
      return NULL;
    }
  dp_mpsk_map (sym, sym_len,
               (float _Complex *)PyArray_DATA ((PyArrayObject *)_out), m);
  Py_DECREF (sym_arr);
  return _out;
}

static PyObject *
_bind_mpsk_demap (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "x", "m", NULL };
  PyObject    *x_obj     = NULL;
  int          m         = 4;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|i", _kwlist, &x_obj, &m))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)jm_array_arg (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  size_t                _dim_need = (size_t)(x_len);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (PyExc_OverflowError,
                    "mpsk_demap: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_UINT8, 0);
  if (!_out)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  dp_mpsk_demap (x, x_len, (uint8_t *)PyArray_DATA ((PyArrayObject *)_out), m);
  Py_DECREF (x_arr);
  return _out;
}

static PyObject *
_bind_mpsk_diff_map (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "sym", "m", NULL };
  PyObject    *sym_obj   = NULL;
  int          m         = 4;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|i", _kwlist, &sym_obj, &m))
    return NULL;
  PyArrayObject *sym_arr = (PyArrayObject *)jm_array_arg (
      sym_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "sym");
  if (!sym_arr)
    {
      return NULL;
    }
  const uint8_t *sym       = (const uint8_t *)PyArray_DATA (sym_arr);
  size_t         sym_len   = (size_t)PyArray_SIZE (sym_arr);
  size_t         _dim_need = (size_t)(sym_len);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (sym_arr);
      PyErr_Format (PyExc_OverflowError,
                    "mpsk_diff_map: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_COMPLEX64, 0);
  if (!_out)
    {
      Py_DECREF (sym_arr);
      return NULL;
    }
  dp_mpsk_diff_map (sym, sym_len,
                    (float _Complex *)PyArray_DATA ((PyArrayObject *)_out), m);
  Py_DECREF (sym_arr);
  return _out;
}

static PyObject *
_bind_mpsk_diff_demap (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "x", "m", NULL };
  PyObject    *x_obj     = NULL;
  int          m         = 4;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|i", _kwlist, &x_obj, &m))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)jm_array_arg (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  size_t                _dim_need = (size_t)(x_len);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (PyExc_OverflowError,
                    "mpsk_diff_demap: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_UINT8, 0);
  if (!_out)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  dp_mpsk_diff_demap (x, x_len,
                      (uint8_t *)PyArray_DATA ((PyArrayObject *)_out), m);
  Py_DECREF (x_arr);
  return _out;
}

static PyObject *
_bind_mpsk_soft_demap (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "x", "llr", "m", "n0", NULL };
  PyObject    *x_obj     = NULL;
  PyObject    *llr_obj   = NULL;
  int          m         = 4;
  float        n0        = 1.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO|if", _kwlist, &x_obj,
                                    &llr_obj, &m, &n0))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)jm_array_arg (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (llr_obj)
      || PyArray_TYPE ((PyArrayObject *)llr_obj) != NPY_FLOAT
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)llr_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)llr_obj))
    {
      PyErr_SetString (PyExc_TypeError, "llr must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      Py_DECREF (x_arr);
      return NULL;
    }
  PyArrayObject *llr_arr = (PyArrayObject *)jm_array_arg (
      llr_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "llr");
  if (!llr_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float *llr     = (float *)PyArray_DATA (llr_arr);
  size_t llr_len = (size_t)PyArray_SIZE (llr_arr);
  dp_mpsk_soft_demap (x, x_len, llr, llr_len, m, n0);
  Py_DECREF (x_arr);
  Py_DECREF (llr_arr);
  Py_RETURN_NONE;
}

static PyObject *
_bind_mpsk_bits_per_symbol (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "m", NULL };
  int          m         = 4;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", _kwlist, &m))
    return NULL;
  return PyLong_FromLong ((long)dp_mpsk_bits_per_symbol (m));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef mpsk_module_methods[] = {
  { "mpsk_map", (PyCFunction)(void *)_bind_mpsk_map,
    METH_VARARGS | METH_KEYWORDS,
    "Map Gray-coded M-PSK labels to unit-amplitude constellation points.\n"
    "\n"
    "Element-wise inverse of dp_mpsk_demap(): each input byte is one\n"
    "symbol's log2(M) Gray-coded bits (0..M-1), each output is its cf32\n"
    "point. Memoryless (absolute phase). out must hold sym_len points.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sym : npt.NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    Gray label bytes (0..M-1), one per symbol.\n"
    "m : int\n"
    "    M in {2,4,8}.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_map, mpsk_demap\n"
    ">>> sym = np.array([0, 1, 2, 3], dtype=np.uint8)   # QPSK labels\n"
    ">>> pts = mpsk_map(sym, 4)\n"
    ">>> np.round(np.abs(pts), 5)\n"
    "array([1., 1., 1., 1.], dtype=float32)\n"
    ">>> np.array_equal(mpsk_demap(pts, 4), sym)\n"
    "True\n" },
  { "mpsk_demap", (PyCFunction)(void *)_bind_mpsk_demap,
    METH_VARARGS | METH_KEYWORDS,
    "Hard-decide M-PSK symbols to their Gray-coded label bytes.\n"
    "\n"
    "Element-wise inverse of dp_mpsk_map(): each cf32 symbol is sliced to\n"
    "the nearest constellation point and its Gray label (0..M-1) is written\n"
    "out. A slip to an adjacent point flips exactly one bit (Gray). out must\n"
    "hold x_len bytes.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Received symbols (any amplitude; phase only).\n"
    "m : int\n"
    "    M in {2,4,8}.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_demap\n"
    ">>> x = np.array([1+0j, 1j, -1+0j, -1j], dtype=np.complex64)  # 8PSK\n"
    ">>> mpsk_demap(x, 8).tolist()   # Gray labels of indices 0, 2, 4, 6\n"
    "[0, 3, 6, 5]\n" },
  { "mpsk_diff_map", (PyCFunction)(void *)_bind_mpsk_diff_map,
    METH_VARARGS | METH_KEYWORDS,
    "Differential M-PSK map: the label selects a phase INCREMENT.\n"
    "\n"
    "Information rides on phase *differences*: the running constellation\n"
    "index accumulates `gray_decode(label)` each symbol (starting from an\n"
    "implicit zero-phase reference), so an unknown constant carrier phase\n"
    "cancels at the receiver (mpsk_diff_demap) — resolving the M-fold\n"
    "ambiguity. Sequential over the array.\n"
    "\n"
    "The cost is up to **2x the symbol-error rate** of coherent map(). That\n"
    "factor is a high-SNR asymptote, not a constant: measured, BPSK and QPSK\n"
    "reach it by ~8 dB Es/N0 while 8PSK pays only 1.44x at 4 dB and 2.03x by\n"
    "14 dB. A caller sizing a link at low Es/N0 is charged less than the\n"
    "round number suggests (native/validation/mpsk_diff_penalty.c).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sym : npt.NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    Gray label bytes (0..M-1), one per symbol.\n"
    "m : int\n"
    "    M in {2,4,8}.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_diff_map, mpsk_diff_demap\n"
    ">>> sym = np.array([1, 0, 3, 2, 1], dtype=np.uint8)\n"
    ">>> pts = mpsk_diff_map(sym, 4)\n"
    ">>> np.array_equal(mpsk_diff_demap(pts, 4), sym)   # exact round-trip\n"
    "True\n"
    ">>> rot = (pts * np.exp(1j * np.pi / 2)).astype(np.complex64)  # slip\n"
    ">>> np.array_equal(mpsk_diff_demap(rot, 4)[1:], sym[1:])  # invariant\n"
    "True\n" },
  { "mpsk_diff_demap", (PyCFunction)(void *)_bind_mpsk_diff_demap,
    METH_VARARGS | METH_KEYWORDS,
    "Differential M-PSK demap: decide from the phase DIFFERENCE.\n"
    "\n"
    "Inverse of dp_mpsk_diff_map(): the Gray label of each symbol is decided\n"
    "from the phase difference between consecutive sliced indices (the first\n"
    "references an implicit zero-phase start). Invariant to an unknown\n"
    "constant carrier phase.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Received symbols (any amplitude; phase only).\n"
    "m : int\n"
    "    M in {2,4,8}.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_diff_demap, mpsk_diff_map\n"
    ">>> sym = np.array([2, 2, 1, 0], dtype=np.uint8)\n"
    ">>> np.array_equal(mpsk_diff_demap(mpsk_diff_map(sym, 8), 8), sym)\n"
    "True\n" },
  { "mpsk_soft_demap", (PyCFunction)(void *)_bind_mpsk_soft_demap,
    METH_VARARGS | METH_KEYWORDS,
    "Soft-demap M-PSK symbols to per-bit log-likelihood ratios.\n"
    "\n"
    "The soft counterpart of dp_mpsk_demap(): instead of one label byte per\n"
    "symbol it writes `log2(M)` LLRs, one per bit, which is what a\n"
    "soft-input decoder (a Viterbi, for the CCSDS inner code) needs. A hard\n"
    "decision throws away roughly 2 dB of the coding gain such a decoder\n"
    "exists to deliver.\n"
    "\n"
    "The convention, which every consumer has to agree with:\n"
    "\n"
    "L_i = log( P(bit i = 0 | y) / P(bit i = 1 | y) )\n"
    "\n"
    "so **positive means bit 0** and the hard decision is `L < 0`. That is\n"
    "not a separate rule: `dp_mpsk_demap()` is what this reproduces, and the\n"
    "sign agreeing with it at every M and every SNR is asserted in\n"
    "test_mpsk_core.c rather than assumed. The repository has ONE decision\n"
    "rule; this is a second view of it, not a second copy.\n"
    "\n"
    "Bits are LSB-first within a symbol, matching how the Gray label packs\n"
    "them, and symbols run in order: `llr[i * log2(M) + b]` is bit b of\n"
    "symbol i.\n"
    "\n"
    "Computed by the max-log rule over the constellation `L_i = (min_{b_i=1}\n"
    "|y-a|^2 - min_{b_i=0} |y-a|^2) / n0`. For BPSK and QPSK this is EXACT —\n"
    "QPSK's `phi0 = pi/4` grid is axis-separable, so its two bits are\n"
    "independent BPSK decisions and each subset holds one point. Only 8PSK\n"
    "is an approximation; what that costs in dB is not measured yet and is\n"
    "therefore not claimed here (docs/design/mpsk.md §9.7).\n"
    "\n"
    "n0 is the noise power `E[|n|^2]` for unit-amplitude symbols, and it\n"
    "scales the output exactly: `L(n0) = L(1) / n0`. A **Viterbi is\n"
    "invariant to it**, since scaling every branch metric by a positive\n"
    "constant cannot move the maximum-likelihood path — so a caller with no\n"
    "SNR estimate may pass 1.0 and get correctly ordered, unscaled soft\n"
    "values.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Received symbols (amplitude matters here — unlike the hard path,\n"
    "    which uses phase only).\n"
    "llr : npt.NDArray[np.float32]\n"
    "    Out: x_len * log2(M) LLRs.\n"
    "m : int\n"
    "    M in {2,4,8}.\n"
    "n0 : float\n"
    "    Noise power `E[|n|^2]`; must be positive.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_soft_demap, mpsk_demap\n"
    ">>> x = np.array([0.9+0.1j, -0.8-0.2j], dtype=np.complex64)   # BPSK\n"
    ">>> llr = np.empty(2, dtype=np.float32)\n"
    ">>> mpsk_soft_demap(x, llr, 2, 1.0)\n"
    ">>> np.round(llr, 3)                       # 4*Re(y)/n0\n"
    "array([ 3.6, -3.2], dtype=float32)\n"
    ">>> np.array_equal((llr < 0).astype(np.uint8), mpsk_demap(x, 2))\n"
    "True\n" },
  { "mpsk_bits_per_symbol", (PyCFunction)(void *)_bind_mpsk_bits_per_symbol,
    METH_VARARGS | METH_KEYWORDS,
    "Bits per M-PSK symbol = log2(M).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "m : int\n"
    "    M in {2,4,8}.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    1, 2, or 3 (0 for an unsupported M).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.mpsk import mpsk_bits_per_symbol\n"
    ">>> [mpsk_bits_per_symbol(m) for m in (2, 4, 8)]\n"
    "[1, 2, 3]\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef mpsk_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "mpsk",
  .m_doc  = "M-ary PSK mapping: hard and soft, including differential, symbol "
            "map/demap for BPSK, QPSK, and 8-PSK constellations.\n"
            "\n"
            "Examples\n"
            "--------\n"
            ">>> import numpy as np\n"
            ">>> from doppler.mpsk import mpsk_map, mpsk_demap\n"
            ">>> bits = np.array([0, 1, 1, 0], np.uint8)\n"
            ">>> np.array_equal(mpsk_demap(mpsk_map(bits, 4), 4), bits)\n"
            "True\n",
  .m_size = -1,
  .m_methods = mpsk_module_methods,
};

PyMODINIT_FUNC
PyInit_mpsk (void)
{
  import_array ();

  PyObject *m = PyModule_Create (&mpsk_moduledef);
  if (!m)
    return NULL;

  return m;
}
