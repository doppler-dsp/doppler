/*
 * dp_window.c — Python C extension for dp/window.h
 *
 * Exposes two stateless functions:
 *   kaiser_window(n, beta) → float32 ndarray, shape (n,)
 *   kaiser_enbw(w)         → float  (ENBW in bins)
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <dp/window.h>

/* ------------------------------------------------------------------ */
/* kaiser_window(n, beta) → ndarray[float32, shape=(n,)]              */
/* ------------------------------------------------------------------ */

PyDoc_STRVAR (kaiser_window_doc,
              "kaiser_window(n, beta)\n"
              "\n"
              "Return a Kaiser window of length *n* with shape parameter "
              "*beta*.\n"
              "\n"
              "Parameters\n"
              "----------\n"
              "n : int\n"
              "    Window length (number of samples).  Must be >= 1.\n"
              "beta : float\n"
              "    Shape parameter.  0 gives a rectangular window;\n"
              "    larger values increase side-lobe suppression.\n"
              "    Convention: direct I0 argument, same as NumPy/SciPy.\n"
              "\n"
              "Returns\n"
              "-------\n"
              "w : numpy.ndarray, dtype=float32, shape=(n,)\n"
              "    Window coefficients, normalised so that the centre\n"
              "    sample equals 1.0.\n"
              "\n"
              "Examples\n"
              "--------\n"
              ">>> from doppler.window import kaiser_window\n"
              ">>> w = kaiser_window(64, 6.0)\n"
              ">>> w.dtype\n"
              "dtype('float32')\n"
              ">>> float(w[32])  # centre == 1.0\n"
              "1.0\n");

static PyObject *
py_kaiser_window (PyObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "n", "beta", NULL };
  Py_ssize_t n = 0;
  double beta = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "nd", kwlist, &n, &beta))
    return NULL;

  if (n < 1)
    {
      PyErr_SetString (PyExc_ValueError, "n must be >= 1");
      return NULL;
    }
  if (beta < 0.0)
    {
      PyErr_SetString (PyExc_ValueError, "beta must be >= 0");
      return NULL;
    }

  npy_intp dims[1] = { (npy_intp)n };
  PyObject *arr = PyArray_SimpleNew (1, dims, NPY_FLOAT32);
  if (!arr)
    return NULL;

  dp_kaiser_window ((float *)PyArray_DATA ((PyArrayObject *)arr), (size_t)n,
                    (float)beta);
  return arr;
}

/* ------------------------------------------------------------------ */
/* kaiser_enbw(w) → float                                             */
/* ------------------------------------------------------------------ */

PyDoc_STRVAR (kaiser_enbw_doc,
              "kaiser_enbw(w)\n"
              "\n"
              "Compute the equivalent noise bandwidth (ENBW) of a window.\n"
              "\n"
              "Parameters\n"
              "----------\n"
              "w : array-like, dtype=float32\n"
              "    Window coefficients (e.g., from :func:`kaiser_window`).\n"
              "\n"
              "Returns\n"
              "-------\n"
              "enbw : float\n"
              "    ENBW in FFT bins.  Multiply by ``fs / len(w)`` to get Hz.\n"
              "\n"
              "Examples\n"
              "--------\n"
              ">>> from doppler.window import kaiser_window, kaiser_enbw\n"
              ">>> w = kaiser_window(4096, 0.0)\n"
              ">>> round(kaiser_enbw(w), 3)  # rectangular → 1.0 bin\n"
              "1.0\n");

static PyObject *
py_kaiser_enbw (PyObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "w", NULL };
  PyObject *w_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &w_obj))
    return NULL;

  PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF (
      w_obj, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY);
  if (!w_arr)
    return NULL;

  if (PyArray_NDIM (w_arr) != 1)
    {
      Py_DECREF (w_arr);
      PyErr_SetString (PyExc_ValueError, "w must be a 1-D array");
      return NULL;
    }

  size_t n = (size_t)PyArray_SIZE (w_arr);
  float enbw = dp_kaiser_enbw ((const float *)PyArray_DATA (w_arr), n);
  Py_DECREF (w_arr);
  return PyFloat_FromDouble ((double)enbw);
}

/* ------------------------------------------------------------------ */
/* Module                                                             */
/* ------------------------------------------------------------------ */

static PyMethodDef window_methods[] = {
  { "kaiser_window", (PyCFunction)py_kaiser_window,
    METH_VARARGS | METH_KEYWORDS, kaiser_window_doc },
  { "kaiser_enbw", (PyCFunction)py_kaiser_enbw, METH_VARARGS | METH_KEYWORDS,
    kaiser_enbw_doc },
  { NULL, NULL, 0, NULL },
};

static struct PyModuleDef window_module = {
  PyModuleDef_HEAD_INIT,
  "_window",
  "Kaiser window and ENBW (C extension wrapping dp/window.h)",
  -1,
  window_methods,
};

PyMODINIT_FUNC
PyInit__window (void)
{
  import_array ();
  return PyModule_Create (&window_module);
}
