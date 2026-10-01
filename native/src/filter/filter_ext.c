/*
 * filter_ext.c — Python extension module filter
 *
 * Objects: FIR, MovingAverage
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/filter/filter_core.h"

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

#include "filter_ext_boxcar.c"
#include "filter_ext_fir.c"

static PyObject *
_bind_design_lowpass (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "fpass", "fstop", "atten_db", NULL };
  double       fpass     = 0.4;
  double       fstop     = 0.6;
  double       atten_db  = 60.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddd", _kwlist, &fpass,
                                    &fstop, &atten_db))
    return NULL;
  size_t _dim_need
      = (size_t)(dp_kaiser_num_taps (1, atten_db, fpass / 2.0, fstop / 2.0)
                 | 1);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "design_lowpass: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_FLOAT, 0);
  if (!_out)
    {
      return NULL;
    }
  dp_design_lowpass (fpass, fstop, atten_db,
                     (float *)PyArray_DATA ((PyArrayObject *)_out));
  return _out;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef filter_module_methods[]
    = { { "design_lowpass", (PyCFunction)(void *)_bind_design_lowpass,
          METH_VARARGS | METH_KEYWORDS,
          "Kaiser-windowed-sinc lowpass FIR taps, auto-sized by "
          "kaiser_num_taps (Nyquist-normalised fpass/fstop band edges, "
          "unit-DC-gain float32 taps).\n" },
        { NULL, NULL, 0, NULL } };

static PyModuleDef filter_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "filter",
  .m_doc
  = "FIR filtering: a direct-form complex or real FIR (FIR) and an O(1) "
    "boxcar moving average (MovingAverage).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.filter import MovingAverage\n"
    ">>> MovingAverage(2).steps(np.ones(3, np.complex64)).real.tolist()\n"
    "[0.5, 1.0, 1.0]\n",
  .m_size    = -1,
  .m_methods = filter_module_methods,
};

PyMODINIT_FUNC
PyInit_filter (void)
{
  import_array ();
  if (PyType_Ready (&FIRObjType) < 0)
    return NULL;
  if (PyType_Ready (&MovingAverageObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&filter_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&FIRObjType);
  if (PyModule_AddObject (m, "FIR", (PyObject *)&FIRObjType) < 0)
    {
      Py_DECREF (&FIRObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&MovingAverageObjType);
  if (PyModule_AddObject (m, "MovingAverage",
                          (PyObject *)&MovingAverageObjType)
      < 0)
    {
      Py_DECREF (&MovingAverageObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
