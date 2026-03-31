// na_fft
#define NPY_NO_DEPRECATED_API NPY_2_0_API_VERSION

#include <Python.h>
#include <complex.h>
#include <numpy/arrayobject.h>

#include "../../c/include/dp/fft.h"

/* ---------------- UTIL ---------------- */

static int
ensure_complex128 (PyArrayObject **arr, PyObject *obj, int writeable)
{
  *arr = (PyArrayObject *)PyArray_FROM_OTF (
      obj, NPY_COMPLEX128,
      writeable ? (NPY_ARRAY_ALIGNED | NPY_ARRAY_WRITEABLE)
                : NPY_ARRAY_ALIGNED);
  if (!*arr)
    {
      PyErr_SetString (PyExc_TypeError, "Expected complex128 array");
      return -1;
    }
  return 0;
}

/* ---------------- GLOBAL SETUP ---------------- */

static PyObject *
na_fft_global_setup (PyObject *self, PyObject *args)
{
  PyObject *shape_obj;
  int sign, nthreads;
  const char *planner;
  const char *wisdom;

  if (!PyArg_ParseTuple (args, "Oiiss", &shape_obj, &sign, &nthreads, &planner,
                         &wisdom))
    return NULL;

  if (!PyTuple_Check (shape_obj))
    {
      PyErr_SetString (PyExc_ValueError, "shape must be a tuple");
      return NULL;
    }

  size_t shape[2];
  size_t ndim = PyTuple_Size (shape_obj);

  if (ndim == 1)
    {
      shape[0] = (size_t)PyLong_AsSsize_t (PyTuple_GetItem (shape_obj, 0));
    }
  else if (ndim == 2)
    {
      shape[0] = (size_t)PyLong_AsSsize_t (PyTuple_GetItem (shape_obj, 0));
      shape[1] = (size_t)PyLong_AsSsize_t (PyTuple_GetItem (shape_obj, 1));
    }
  else
    {
      PyErr_SetString (PyExc_ValueError, "Only 1D or 2D FFTs supported");
      return NULL;
    }

  dp_fft_global_setup (shape, ndim, sign, nthreads, planner, wisdom);
  Py_RETURN_NONE;
}

/* ---------------- 1D EXECUTE ---------------- */

static PyObject *
na_fft1d_execute (PyObject *self, PyObject *args)
{
  PyArrayObject *arr_in;

  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr_in))
    return NULL;

  npy_intp n = PyArray_SIZE (arr_in);
  PyArrayObject *arr_out
      = (PyArrayObject *)PyArray_SimpleNew (1, &n, NPY_COMPLEX128);

  double complex *in = (double complex *)PyArray_DATA (arr_in);
  double complex *out = (double complex *)PyArray_DATA (arr_out);

  dp_fft1d_execute (in, out);
  return (PyObject *)arr_out;
}

static PyObject *
na_fft1d_execute_inplace (PyObject *self, PyObject *args)
{
  PyArrayObject *arr;

  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return NULL;

  double complex *data = (double complex *)PyArray_DATA (arr);
  dp_fft1d_execute_inplace (data);

  Py_RETURN_NONE;
}

/* ---------------- 2D EXECUTE ---------------- */

static PyObject *
na_fft2d_execute (PyObject *self, PyObject *args)
{
  PyArrayObject *arr_in;

  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr_in))
    return NULL;

  npy_intp *dims = PyArray_DIMS (arr_in);
  PyArrayObject *arr_out
      = (PyArrayObject *)PyArray_SimpleNew (2, dims, NPY_COMPLEX128);

  double complex *in = (double complex *)PyArray_DATA (arr_in);
  double complex *out = (double complex *)PyArray_DATA (arr_out);

  dp_fft2d_execute (in, out);
  return (PyObject *)arr_out;
}

static PyObject *
na_fft2d_execute_inplace (PyObject *self, PyObject *args)
{
  PyArrayObject *arr;

  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return NULL;

  double complex *data = (double complex *)PyArray_DATA (arr);
  dp_fft2d_execute_inplace (data);

  Py_RETURN_NONE;
}

/* ---------------- METHOD TABLE ---------------- */

static PyMethodDef DopplerMethods[]
    = { { "fft_global_setup", na_fft_global_setup, METH_VARARGS,
          "Global FFT setup" },
        { "fft1d_execute", na_fft1d_execute, METH_VARARGS, "Execute 1D FFT" },
        { "fft1d_execute_inplace", na_fft1d_execute_inplace, METH_VARARGS,
          "Execute 1D FFT inplace" },
        { "fft2d_execute", na_fft2d_execute, METH_VARARGS, "Execute 2D FFT" },
        { "fft2d_execute_inplace", na_fft2d_execute_inplace, METH_VARARGS,
          "Execute 2D FFT inplace" },
        { NULL, NULL, 0, NULL } };

static struct PyModuleDef moduledef
    = { PyModuleDef_HEAD_INIT, "_fft", "Doppler FFT module", -1,
        DopplerMethods };

PyMODINIT_FUNC
PyInit__fft (void)
{
  import_array ();
  return PyModule_Create (&moduledef);
}
