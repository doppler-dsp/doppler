/* jm:generated wlan_ext_wlan.c */
/*
 * wlan_ext_wlan.c — Wlan type for the wlan module.
 *
 * Included by wlan_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in wlan_ext_wlan_extra.c.
 * Do NOT compile this file directly — only wlan_ext.c is compiled.
 */
/* ======================================================== */
/* WlanObject — wraps dp_wlan_state_t *       */
/* ======================================================== */

#include "doppler/wlan/wlan_core.h"

typedef struct
{
  PyObject_HEAD dp_wlan_state_t *handle;
} WlanObject;

static void
WlanObj_dealloc (WlanObject *self)
{
  if (self->handle)
    dp_wlan_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
WlanObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  WlanObject *self = (WlanObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
WlanObj_init (WlanObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "constellation", "code_rate", "window", "oversample", NULL };
  const char *constellation_str = "qam16";
  const char *code_rate_str     = "r12";
  long long   window_raw        = WLAN_WINDOW_STANDARD;
  long long   oversample_raw    = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ssLL", kwlist,
                                    &constellation_str, &code_rate_str,
                                    &window_raw, &oversample_raw))
    return -1;
  int constellation = 0;
  if (strcmp (constellation_str, "bpsk") == 0)
    constellation = 0;
  else if (strcmp (constellation_str, "qpsk") == 0)
    constellation = 1;
  else if (strcmp (constellation_str, "qam16") == 0)
    constellation = 2;
  else if (strcmp (constellation_str, "qam64") == 0)
    constellation = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "constellation must be one of \"bpsk\", \"qpsk\", "
                    "\"qam16\", \"qam64\", got '%s'",
                    constellation_str);
      return -1;
    }
  int code_rate = 0;
  if (strcmp (code_rate_str, "r12") == 0)
    code_rate = 0;
  else if (strcmp (code_rate_str, "r23") == 0)
    code_rate = 1;
  else if (strcmp (code_rate_str, "r34") == 0)
    code_rate = 2;
  else if (strcmp (code_rate_str, "r56") == 0)
    code_rate = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "code_rate must be one of \"r12\", \"r23\", \"r34\", "
                    "\"r56\", got '%s'",
                    code_rate_str);
      return -1;
    }
  if (window_raw < 0 || window_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "window: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)window_raw, (long long)0,
                    (long long)UINT32_MAX);
      return -1;
    }
  uint32_t window = (uint32_t)window_raw;
  if (oversample_raw < 0 || oversample_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "oversample: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)oversample_raw, (long long)0, (long long)UINT32_MAX);
      return -1;
    }
  uint32_t    oversample = (uint32_t)oversample_raw;
  const char *_why       = NULL;
  self->handle = dp_wlan_create_why (constellation, code_rate, window,
                                     oversample, &_why);
  if (!self->handle)
    {
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "constellation is not one of bpsk, qpsk, qam16, "
                         "qam64, code_rate is not one of r12, r23, r34, "
                         "r56, window is not 0 to 80 * oversample, or "
                         "oversample is not 1 to 128");
      return -1;
    }
  return 0;
}

static PyObject *
WlanObj_reset (WlanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_wlan_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
WlanObj_ppdu_max_out (WlanObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n_in = 0;
  if (!PyArg_ParseTuple (args, "n", &n_in))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_wlan_ppdu_max_out (self->handle, (size_t)n_in));
}

static PyObject *
WlanObj_ppdu (WlanObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]           = { "x", "scrambler_state", "out", NULL };
  PyObject    *x_obj               = NULL;
  long long    scrambler_state_raw = 0LL;
  PyObject    *out_obj             = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OL|O", _kwlist, &x_obj,
                                    &scrambler_state_raw, &out_obj))
    return NULL;
  if (scrambler_state_raw < 0 || scrambler_state_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "scrambler_state: %lld is out of range for uint32_t [%lld, %lld]",
          (long long)scrambler_state_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t       scrambler_state = (uint32_t)scrambler_state_raw;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const uint8_t *x     = (const uint8_t *)PyArray_DATA (x_arr);
  size_t         x_len = (size_t)PyArray_SIZE (x_arr);
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
          Py_DECREF (x_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_wlan_ppdu_max_out (self->handle, x_len);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      const char *_why = NULL;
      int64_t _rc = dp_wlan_ppdu_why (self->handle, x, x_len, scrambler_state,
                                      (float _Complex *)PyArray_DATA (out_arr),
                                      _cap, &_why);
      Py_DECREF (x_arr);
      if (_rc < 0)
        {
          Py_DECREF (out_arr);
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "ppdu refused",
                          (long long)_rc);
          return NULL;
        }
      size_t n_out = (size_t)_rc;
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Wlan.ppdu: wrote %zu elements into a buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
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
  if (!self->handle)
    {
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = x_len;
  size_t _cap  = dp_wlan_ppdu_max_out (self->handle, x_len);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Wlan.ppdu: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  int64_t _rc = dp_wlan_ppdu_why (self->handle, x, x_len, scrambler_state, _d0,
                                  _cap, &_why);
  Py_DECREF (x_arr);
  if (_rc < 0)
    {
      Py_DECREF (arr0);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "ppdu refused",
                      (long long)_rc);
      return NULL;
    }
  size_t n_out = (size_t)_rc;
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Wlan.ppdu: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
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
WlanObj_n_sym (WlanObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "psdu_len", NULL };
  PyObject          *psdu_len_obj = NULL;
  unsigned long long psdu_len_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &psdu_len_obj))
    return NULL;
  if (psdu_len_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (psdu_len_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      psdu_len_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((psdu_len_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || psdu_len_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "psdu_len: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t psdu_len = (size_t)psdu_len_raw;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_wlan_n_sym (self->handle, psdu_len);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}
static PyObject *
Wlan_getprop_n_bpsc (WlanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->n_bpsc);
}
static PyObject *
Wlan_getprop_n_cbps (WlanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->n_cbps);
}
static PyObject *
Wlan_getprop_n_dbps (WlanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->n_dbps);
}
static PyObject *
Wlan_getprop_rate_mbps (WlanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->rate_mbps);
}
static PyObject *
Wlan_getprop_window (WlanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->window);
}
static PyObject *
Wlan_getprop_oversample (WlanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->oversample);
}

static PyGetSetDef Wlan_getset[] = {
  { "n_bpsc", (getter)Wlan_getprop_n_bpsc, NULL,
    "Bits per subcarrier: 1 for BPSK, 2, 4 or 6.\n", NULL },
  { "n_cbps", (getter)Wlan_getprop_n_cbps, NULL,
    "Coded bits per OFDM symbol, 48 times `n_bpsc`.\n", NULL },
  { "n_dbps", (getter)Wlan_getprop_n_dbps, NULL,
    "Data bits per OFDM symbol: `n_cbps` times the code rate.\n", NULL },
  { "rate_mbps", (getter)Wlan_getprop_rate_mbps, NULL,
    "The data rate at 20 MHz in Mb/s, as Table 17-6 prints it: 6, 9, 12, 18, "
    "24, 36, 48 or 54, and 0 for the eight pairs the table does not list.\n",
    NULL },
  { "window", (getter)Wlan_getprop_window, NULL,
    "The window length in samples at the output rate.\n", NULL },
  { "oversample", (getter)Wlan_getprop_oversample, NULL,
    "`M`: the output rate over the 20 MHz clock.\n", NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
WlanObj_destroy (WlanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_wlan_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
WlanObj_enter (WlanObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
WlanObj_exit (WlanObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_wlan_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef WlanObj_methods[] = {
  { "reset", (PyCFunction)WlanObj_reset, METH_NOARGS,
    "Reset a rate holder: nothing to reset, the rate is configuration.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wlan import Wlan\n"
    ">>> w = Wlan(constellation=\"qpsk\", code_rate=\"r34\")\n"
    ">>> w.reset()                      # nothing runs, so nothing changes\n"
    ">>> w.n_dbps\n"
    "72\n" },

  { "ppdu", (PyCFunction)(void *)WlanObj_ppdu, METH_VARARGS | METH_KEYWORDS,
    "ppdu(x, scrambler_state, out) -> ndarray\n"
    "\n"
    "Render a PSDU to a whole PPDU (L-STF, L-LTF, SIGNAL, Data) at this rate, "
    "window and oversampling, as complex64 samples. Refuses with `ValueError` "
    "naming why.\n"
    "\n"
    "The standard's tables are the library's (dp_wlan_tables_standard());\n"
    "the PSDU is the caller's octets as given, FCS included. The result is\n"
    "the dp_wlan_ppdu() output for the same arguments, with the refusal's\n"
    "reason.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.uint8] | bytes | bytearray | memoryview\n"
    "    The PSDU octets.\n"
    "scrambler_state : int\n"
    "    The scrambler register's initial state, 1 to 127.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Receives dp_wlan_ppdu_max_out() samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    The samples written, or -1 if the rate has no SIGNAL RATE bits, the\n"
    "    PSDU length or the scrambler state is out of range, or max_out is\n"
    "    short; out is then untouched. On the Python face a short `out=` is\n"
    "    refused before the other arguments are, by the binding.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    the reason the C call gives, or ``ppdu refused``, with the return\n"
    "    code appended (gh-869), when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wlan import Wlan\n"
    ">>> w = Wlan(constellation=\"qam16\", code_rate=\"r34\")  # 36 Mb/s\n"
    ">>> psdu = np.arange(100, dtype=np.uint8)\n"
    ">>> x = w.ppdu(psdu, 93)\n"
    ">>> x.dtype, x.shape  # 160 + 160 + 80 + 80 * 6, plus the 2-sample "
    "window\n"
    "(dtype('complex64'), (882,))\n"
    ">>> w2 = Wlan(constellation=\"qam16\", code_rate=\"r34\", oversample=2)\n"
    ">>> w2.ppdu(psdu, 93).shape  # twice as fine, and a 4-sample window\n"
    "(1764,)\n"
    ">>> w.ppdu(psdu, 0)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: the scrambler state is 1 to 127\n" },
  { "ppdu_max_out", (PyCFunction)WlanObj_ppdu_max_out, METH_VARARGS,
    "ppdu_max_out(n_in) -> int\n"
    "\n"
    "The samples a PPDU of n_in PSDU octets occupies at this object's\n"
    "rate, window and oversampling: the capacity dp_wlan_ppdu_why() needs,\n"
    "exactly.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    PSDU octets, 1 to WLAN_PSDU_MAX.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Samples, or 0 for a length outside that range or for a rate with no\n"
    "    SIGNAL RATE bits (Table 17-6 lists eight of the sixteen pairs; the\n"
    "    others cannot be sent, and `rate_mbps` is 0 for them).\n" },
  { "n_sym", (PyCFunction)(void *)WlanObj_n_sym, METH_VARARGS | METH_KEYWORDS,
    "n_sym(psdu_len) -> int\n"
    "\n"
    "The Data OFDM symbols a PSDU of `psdu_len` octets needs at this rate, "
    "`ceil((16 + 8*psdu_len + 6) / n_dbps)`. 0 for a length of 0 or above "
    "4095.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "psdu_len : int\n"
    "    Octets, 1 to WLAN_PSDU_MAX.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The symbol count, or 0 for a length of 0 or above WLAN_PSDU_MAX.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wlan import Wlan\n"
    ">>> w = Wlan(constellation=\"qam16\", code_rate=\"r34\")\n"
    ">>> w.n_sym(100)\n"
    "6\n"
    ">>> w.n_sym(0)\n"
    "0\n" },
  { "destroy", (PyCFunction)WlanObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)WlanObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Wlan be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Wlan\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)WlanObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Wlan.\n"
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

static PyTypeObject WlanObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.wlan.Wlan",
  .tp_basicsize                           = sizeof (WlanObject),
  .tp_dealloc                             = (destructor)WlanObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "dp_wlan_create(), and the reason when it refuses. The binding calls "
    "this\n"
    "one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "constellation : Literal[\"bpsk\", \"qpsk\", \"qam16\", \"qam64\"], "
    "default \"qam16\"\n"
    "    The constellation: BPSK, QPSK, 16-QAM or 64-QAM, one to six bits "
    "per\n"
    "    subcarrier.\n"
    "    One of ``\"bpsk\"``, ``\"qpsk\"``, ``\"qam16\"``, ``\"qam64\"``.\n"
    "code_rate : Literal[\"r12\", \"r23\", \"r34\", \"r56\"], default "
    "\"r12\"\n"
    "    The BCC rate: `r12` is 1/2, `r23` is 2/3, `r34` is 3/4 and `r56` is\n"
    "    5/6.\n"
    "    One of ``\"r12\"``, ``\"r23\"``, ``\"r34\"``, ``\"r56\"``.\n"
    "window : int, default WLAN_WINDOW_STANDARD\n"
    "    The default is the standard's 100 ns transition, `2 * oversample`\n"
    "    samples; 0 is no window. Otherwise the window length in samples at "
    "the\n"
    "    OUTPUT rate, 0 to 80 times `oversample`: the cross-fade at each "
    "OFDM\n"
    "    symbol boundary of the PPDUs `ppdu` renders. Above the `16 *\n"
    "    oversample` guard interval the cross-fade reaches into the symbol "
    "body.\n"
    "oversample : int, default 1\n"
    "    `M`, 1 to 128: the output rate over the 20 MHz clock. The PPDU is\n"
    "    rendered by a transform `M` times longer, the same subcarriers, not\n"
    "    resampled.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is the reason the\n"
    "    constructor gives, or ``constellation is not one of bpsk, qpsk, "
    "qam16,\n"
    "    qam64, code_rate is not one of r12, r23, r34, r56, window is not 0 "
    "to\n"
    "    80 * oversample, or oversample is not 1 to 128`` when it gives "
    "none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wlan import Wlan\n"
    ">>> w = Wlan(constellation=\"qam16\", code_rate=\"r34\")  # 36 Mb/s at "
    "20 MHz\n"
    ">>> w.n_bpsc, w.n_cbps, w.n_dbps\n"
    "(4, 192, 144)\n"
    ">>> Wlan(constellation=\"qam256\")\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: constellation must be one of \"bpsk\", \"qpsk\", \"qam16\", "
    "\"qam64\", got 'qam256'\n",
  .tp_methods = WlanObj_methods,
  .tp_getset  = Wlan_getset,
  .tp_new     = WlanObj_new,
  .tp_init    = (initproc)WlanObj_init,
};
