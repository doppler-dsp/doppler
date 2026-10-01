/*
 * snr_ext.c — Python extension module snr
 *
 * Objects:
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/snr/snr_core.h"

#ifndef JM_ARRAY_ARG_DEFINED
#define JM_ARRAY_ARG_DEFINED
/* Convert a Python argument for an array parameter to an ndarray of
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, less the two inputs
 * it reads as text (gh-1700): a str is refused, never parsed as a number,
 * and for a one-byte element type a byte buffer (bytes, bytearray,
 * memoryview) is its bytes, one element per byte. `name` is the parameter,
 * for the message, and `hint` (NULL for none) is appended to a str's
 * refusal. Returns a new reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  if (PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj)))
    {
      /* `hint` (gh-1756) says where text goes instead: a str only. */
      int say = hint && PyUnicode_Check (obj);
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
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

static PyObject *
_bind_snr_data_aided_db (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]     = { "soft", "sign_bits", NULL };
  PyObject    *soft_obj      = NULL;
  PyObject    *sign_bits_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &soft_obj,
                                    &sign_bits_obj))
    return NULL;
  PyArrayObject *soft_arr = (PyArrayObject *)jm_array_arg (
      soft_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "soft");
  if (!soft_arr)
    {
      return NULL;
    }
  const float _Complex *soft = (const float _Complex *)PyArray_DATA (soft_arr);
  size_t                soft_len      = (size_t)PyArray_SIZE (soft_arr);
  PyArrayObject        *sign_bits_arr = (PyArrayObject *)jm_array_arg (
      sign_bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "sign_bits");
  if (!sign_bits_arr)
    {
      Py_DECREF (soft_arr);
      return NULL;
    }
  const uint8_t *sign_bits     = (const uint8_t *)PyArray_DATA (sign_bits_arr);
  size_t         sign_bits_len = (size_t)PyArray_SIZE (sign_bits_arr);
  double _r = dp_snr_data_aided_db (soft, soft_len, sign_bits, sign_bits_len);
  Py_DECREF (soft_arr);
  Py_DECREF (sign_bits_arr);
  return PyFloat_FromDouble (_r);
}

static PyObject *
_bind_snr_m2m4_db (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "x", NULL };
  PyObject    *x_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &x_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)jm_array_arg (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  double                _r    = dp_snr_m2m4_db (x, x_len);
  Py_DECREF (x_arr);
  return PyFloat_FromDouble (_r);
}

static PyObject *
_bind_snr_data_aided_db_series (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[]     = { "soft", "sign_bits", "window", NULL };
  PyObject          *soft_obj      = NULL;
  PyObject          *sign_bits_obj = NULL;
  unsigned long long window_raw    = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOK", _kwlist, &soft_obj,
                                    &sign_bits_obj, &window_raw))
    return NULL;
  size_t         window   = (size_t)window_raw;
  PyArrayObject *soft_arr = (PyArrayObject *)jm_array_arg (
      soft_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "soft");
  if (!soft_arr)
    {
      return NULL;
    }
  const float _Complex *soft = (const float _Complex *)PyArray_DATA (soft_arr);
  size_t                soft_len      = (size_t)PyArray_SIZE (soft_arr);
  PyArrayObject        *sign_bits_arr = (PyArrayObject *)jm_array_arg (
      sign_bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "sign_bits");
  if (!sign_bits_arr)
    {
      Py_DECREF (soft_arr);
      return NULL;
    }
  const uint8_t *sign_bits     = (const uint8_t *)PyArray_DATA (sign_bits_arr);
  size_t         sign_bits_len = (size_t)PyArray_SIZE (sign_bits_arr);
  size_t         _dim_need     = (size_t)(soft_len);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (soft_arr);
      Py_DECREF (sign_bits_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "snr_data_aided_db_series: output of %zu elements is too large",
          _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_DOUBLE, 0);
  if (!_out)
    {
      Py_DECREF (soft_arr);
      Py_DECREF (sign_bits_arr);
      return NULL;
    }
  dp_snr_data_aided_db_series (soft, soft_len, sign_bits, sign_bits_len,
                               window,
                               (double *)PyArray_DATA ((PyArrayObject *)_out));
  Py_DECREF (soft_arr);
  Py_DECREF (sign_bits_arr);
  return _out;
}

static PyObject *
_bind_snr_m2m4_db_series (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[]  = { "x", "window", NULL };
  PyObject          *x_obj      = NULL;
  unsigned long long window_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OK", _kwlist, &x_obj,
                                    &window_raw))
    return NULL;
  size_t         window = (size_t)window_raw;
  PyArrayObject *x_arr  = (PyArrayObject *)jm_array_arg (
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
                    "snr_m2m4_db_series: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_DOUBLE, 0);
  if (!_out)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  dp_snr_m2m4_db_series (x, x_len, window,
                         (double *)PyArray_DATA ((PyArrayObject *)_out));
  Py_DECREF (x_arr);
  return _out;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef snr_module_methods[] = {
  { "snr_data_aided_db", (PyCFunction)(void *)_bind_snr_data_aided_db,
    METH_VARARGS | METH_KEYWORDS,
    "Data-aided Es/N0 (dB): strip the known sign, Es/N0 = a^2 / "
    "mean(|z-a|^2).\n"
    "\n"
    "Strips the known transmitted sign (``soft[i] * (sign_bits[i] ? -1 :\n"
    "1)``), then Es/N0 = (mean signal amplitude)^2 / (mean residual power).\n"
    "Scale-invariant (works regardless of the caller's symbol normalization)\n"
    "and polarity-invariant (a global sign flip in ``soft`` changes nothing,\n"
    "since the amplitude is squared) -- so it needs no resolution of an\n"
    "absolute-phase ambiguity a tracking loop may carry.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "soft : NDArray[np.complex64]\n"
    "    Despread complex symbols.\n"
    "sign_bits : NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    Known transmitted bits (0/1; 0 -> +1, 1 -> -1).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Es/N0 in dB over ``min(soft_len, sign_bits_len)`` paired samples,\n"
    "    or NaN if that count is 0 or the residual power is exactly 0.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.snr import snr_data_aided_db\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> bits = (rng.random(2000) > 0.5).astype(np.uint8)\n"
    ">>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)\n"
    ">>> noise = (0.1 * (rng.standard_normal(2000)\n"
    "...          + 1j * rng.standard_normal(2000))).astype(np.complex64)\n"
    ">>> soft = (sign + noise).astype(np.complex64)\n"
    ">>> round(float(snr_data_aided_db(soft, bits)), 1)\n"
    "17.1\n" },
  { "snr_m2m4_db", (PyCFunction)(void *)_bind_snr_m2m4_db,
    METH_VARARGS | METH_KEYWORDS,
    "Non-data-aided moment-based (M2M4) Es/N0 (dB) for a constant-modulus "
    "signal in AWGN.\n"
    "\n"
    "M2M4 estimator (Pauluzzi & Beaulieu 2000) for a constant-modulus signal\n"
    "(BPSK/QPSK/M-PSK) in circular complex AWGN: no known symbols required.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Complex baseband samples (post-carrier-lock; residual phase does\n"
    "    not bias the moment-based estimate).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Es/N0 in dB, 0-linear for pure noise, +inf for a noiseless\n"
    "    constant-modulus signal, or NaN if x_len is 0 or the block has zero\n"
    "    power.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.snr import snr_m2m4_db\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> bits = (rng.random(2000) > 0.5).astype(np.uint8)\n"
    ">>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)\n"
    ">>> noise = (0.1 * (rng.standard_normal(2000)\n"
    "...          + 1j * rng.standard_normal(2000))).astype(np.complex64)\n"
    ">>> x = (sign + noise).astype(np.complex64)\n"
    ">>> round(float(snr_m2m4_db(x)), 1)\n"
    "17.1\n" },
  { "snr_data_aided_db_series",
    (PyCFunction)(void *)_bind_snr_data_aided_db_series,
    METH_VARARGS | METH_KEYWORDS,
    "Sliding-window data-aided Es/N0 (dB) vs index, for visualizing drift.\n"
    "\n"
    "Same estimator as dp_snr_data_aided_db(), applied to a ``[i - window/2,\n"
    "i + window/2]`` window centered (clamped at the edges) on each output\n"
    "index -- for visualizing SNR drift vs time/index rather than reading\n"
    "one block-average scalar.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "soft : NDArray[np.complex64]\n"
    "    Despread complex symbols.\n"
    "sign_bits : NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    Known transmitted bits (0/1).\n"
    "window : int\n"
    "    Window width in samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float64]\n"
    "    Output.\n" },
  { "snr_m2m4_db_series", (PyCFunction)(void *)_bind_snr_m2m4_db_series,
    METH_VARARGS | METH_KEYWORDS,
    "Sliding-window blind (M2M4) Es/N0 (dB) vs index, for visualizing drift.\n"
    "\n"
    "Same estimator as dp_snr_m2m4_db(), applied to a ``[i - window/2, i +\n"
    "window/2]`` window centered (clamped at the edges) on each output\n"
    "index.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Complex baseband samples.\n"
    "window : int\n"
    "    Window width in samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float64]\n"
    "    Output.\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef snr_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "snr",
  .m_doc
  = "Signal-to-noise estimation: data-aided and moment-based (M2M4) SNR / "
    "Es-N0 estimators for a recovered symbol stream.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.snr import snr_m2m4_db\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> sym = (2 * rng.integers(0, 2, 20000) - 1).astype(np.complex64)\n"
    ">>> sym += 0.1 * (rng.standard_normal(20000)\n"
    "...              + 1j * "
    "rng.standard_normal(20000)).astype(np.complex64)\n"
    ">>> bool(15 < snr_m2m4_db(sym) < 25)\n"
    "True\n",
  .m_size    = -1,
  .m_methods = snr_module_methods,
};

PyMODINIT_FUNC
PyInit_snr (void)
{
  import_array ();

  PyObject *m = PyModule_Create (&snr_moduledef);
  if (!m)
    return NULL;

  return m;
}
