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
  static char *kwlist[]          = { "constellation", "code_rate", NULL };
  const char  *constellation_str = "qam16";
  const char  *code_rate_str     = "r12";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ss", kwlist,
                                    &constellation_str, &code_rate_str))
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
  self->handle = dp_wlan_create (constellation, code_rate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "constellation is not one of bpsk, qpsk, qam16, "
                       "qam64, code_rate is not one of r12, r23, r34, r56, "
                       "or the pair's data bits per OFDM symbol are not a "
                       "whole number");
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

static PyGetSetDef Wlan_getset[]
    = { { "n_bpsc", (getter)Wlan_getprop_n_bpsc, NULL,
          "Bits per subcarrier: 1 for BPSK, 2, 4 or 6.\n", NULL },
        { "n_cbps", (getter)Wlan_getprop_n_cbps, NULL,
          "Coded bits per OFDM symbol, 48 times `n_bpsc`.\n", NULL },
        { "n_dbps", (getter)Wlan_getprop_n_dbps, NULL,
          "Data bits per OFDM symbol: `n_cbps` times the code rate.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

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
  = "Create a rate holder, by the index of the constellation and of the code\n"
    "rate.\n"
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
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``constellation is "
    "not\n"
    "    one of bpsk, qpsk, qam16, qam64, code_rate is not one of r12, r23, "
    "r34,\n"
    "    r56, or the pair's data bits per OFDM symbol are not a whole "
    "number``.\n"
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
