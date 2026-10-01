/*
 * agc_ext.c — Python extension module agc
 *
 * Objects: AGC
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/agc/agc_core.h"

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

#include "agc_ext_agc.c"

static PyObject *
_bind_settling_samples (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]
      = { "loop_bw", "alpha", "gain_err_db", "tol_db", NULL };
  double loop_bw     = 0.0;
  double alpha       = 0.0;
  double gain_err_db = 0.0;
  double tol_db      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dddd", _kwlist, &loop_bw,
                                    &alpha, &gain_err_db, &tol_db))
    return NULL;
  return PyLong_FromUnsignedLongLong ((unsigned long long)dp_settling_samples (
      loop_bw, alpha, gain_err_db, tol_db));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef agc_module_methods[] = {
  { "settling_samples", (PyCFunction)(void *)_bind_settling_samples,
    METH_VARARGS | METH_KEYWORDS,
    "How many samples this loop needs to settle -- the design query a caller "
    "sizing a warm-up budget, a burst preamble or an acquisition guard has to "
    "answer. 1/(4*loop_bw) is the loop FILTER's time constant and not the "
    "object's: the detector sits inside the loop and measures in power, so a "
    "quiet input settles more slowly. Measured, the multiplier runs from "
    "about 0.8 on a loud start to nearly 5 on a quiet one with a slow "
    "detector. This runs the real loop against a constant input and counts, "
    "so there is no fitted curve to go stale. gain_err_db is POSITIVE for a "
    "quiet input, which is the slow direction and the one to budget for. "
    "Returns 0 rather than a plausible guess when the arguments are invalid. "
    "Design-time only: it allocates and iterates.\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef agc_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "agc",
  .m_doc     = "Automatic gain control: a log-domain feedback AGC (AGC) that "
               "drives signal power toward a reference level.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> import numpy as np\n"
               ">>> from doppler.agc import AGC\n"
               ">>> y = AGC(ref_db=0.0, loop_bw=0.05).steps(0.1 * np.ones(200, "
               "np.complex64))\n"
               ">>> bool(abs(y[-1]) > abs(y[0]))\n"
               "True\n",
  .m_size    = -1,
  .m_methods = agc_module_methods,
};

PyMODINIT_FUNC
PyInit_agc (void)
{
  import_array ();
  if (PyType_Ready (&AGCObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&agc_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&AGCObjType);
  if (PyModule_AddObject (m, "AGC", (PyObject *)&AGCObjType) < 0)
    {
      Py_DECREF (&AGCObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
