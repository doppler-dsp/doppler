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

static int
ensure_complex64 (PyArrayObject **arr, PyObject *obj, int writeable)
{
  *arr = (PyArrayObject *)PyArray_FROM_OTF (
      obj, NPY_COMPLEX64,
      writeable ? (NPY_ARRAY_ALIGNED | NPY_ARRAY_WRITEABLE)
                : NPY_ARRAY_ALIGNED);
  if (!*arr)
    {
      PyErr_SetString (PyExc_TypeError, "Expected complex64 array");
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
  PyObject *obj;

  if (!PyArg_ParseTuple (args, "O", &obj))
    return NULL;

  PyArrayObject *arr_in;
  if (!PyArray_Check (obj))
    {
      PyErr_SetString (PyExc_TypeError, "Expected ndarray");
      return NULL;
    }

  int dtype = PyArray_TYPE ((PyArrayObject *)obj);

  if (dtype == NPY_COMPLEX64)
    {
      if (ensure_complex64 (&arr_in, obj, 0) < 0)
        return NULL;
      npy_intp n = PyArray_SIZE (arr_in);
      PyArrayObject *arr_out
          = (PyArrayObject *)PyArray_SimpleNew (1, &n, NPY_COMPLEX64);
      dp_fft1d_execute_cf32 ((const float complex *)PyArray_DATA (arr_in),
                              (float complex *)PyArray_DATA (arr_out));
      Py_DECREF (arr_in);
      return (PyObject *)arr_out;
    }

  if (ensure_complex128 (&arr_in, obj, 0) < 0)
    return NULL;
  npy_intp n = PyArray_SIZE (arr_in);
  PyArrayObject *arr_out
      = (PyArrayObject *)PyArray_SimpleNew (1, &n, NPY_COMPLEX128);
  dp_fft1d_execute ((const double complex *)PyArray_DATA (arr_in),
                    (double complex *)PyArray_DATA (arr_out));
  Py_DECREF (arr_in);
  return (PyObject *)arr_out;
}

static PyObject *
na_fft1d_execute_inplace (PyObject *self, PyObject *args)
{
  PyObject *obj;

  if (!PyArg_ParseTuple (args, "O", &obj))
    return NULL;

  if (!PyArray_Check (obj))
    {
      PyErr_SetString (PyExc_TypeError, "Expected ndarray");
      return NULL;
    }

  int dtype = PyArray_TYPE ((PyArrayObject *)obj);
  PyArrayObject *arr;

  if (dtype == NPY_COMPLEX64)
    {
      if (ensure_complex64 (&arr, obj, 1) < 0)
        return NULL;
      dp_fft1d_execute_inplace_cf32 ((float complex *)PyArray_DATA (arr));
      Py_DECREF (arr);
      Py_RETURN_NONE;
    }

  if (ensure_complex128 (&arr, obj, 1) < 0)
    return NULL;
  dp_fft1d_execute_inplace ((double complex *)PyArray_DATA (arr));
  Py_DECREF (arr);
  Py_RETURN_NONE;
}

/* ---------------- 2D EXECUTE ---------------- */

static PyObject *
na_fft2d_execute (PyObject *self, PyObject *args)
{
  PyObject *obj;

  if (!PyArg_ParseTuple (args, "O", &obj))
    return NULL;

  if (!PyArray_Check (obj))
    {
      PyErr_SetString (PyExc_TypeError, "Expected ndarray");
      return NULL;
    }

  int dtype = PyArray_TYPE ((PyArrayObject *)obj);
  PyArrayObject *arr_in;

  if (dtype == NPY_COMPLEX64)
    {
      if (ensure_complex64 (&arr_in, obj, 0) < 0)
        return NULL;
      npy_intp *dims = PyArray_DIMS (arr_in);
      PyArrayObject *arr_out
          = (PyArrayObject *)PyArray_SimpleNew (2, dims, NPY_COMPLEX64);
      dp_fft2d_execute_cf32 ((const float complex *)PyArray_DATA (arr_in),
                              (float complex *)PyArray_DATA (arr_out));
      Py_DECREF (arr_in);
      return (PyObject *)arr_out;
    }

  if (ensure_complex128 (&arr_in, obj, 0) < 0)
    return NULL;
  npy_intp *dims = PyArray_DIMS (arr_in);
  PyArrayObject *arr_out
      = (PyArrayObject *)PyArray_SimpleNew (2, dims, NPY_COMPLEX128);
  dp_fft2d_execute ((const double complex *)PyArray_DATA (arr_in),
                    (double complex *)PyArray_DATA (arr_out));
  Py_DECREF (arr_in);
  return (PyObject *)arr_out;
}

static PyObject *
na_fft2d_execute_inplace (PyObject *self, PyObject *args)
{
  PyObject *obj;

  if (!PyArg_ParseTuple (args, "O", &obj))
    return NULL;

  if (!PyArray_Check (obj))
    {
      PyErr_SetString (PyExc_TypeError, "Expected ndarray");
      return NULL;
    }

  int dtype = PyArray_TYPE ((PyArrayObject *)obj);
  PyArrayObject *arr;

  if (dtype == NPY_COMPLEX64)
    {
      if (ensure_complex64 (&arr, obj, 1) < 0)
        return NULL;
      dp_fft2d_execute_inplace_cf32 ((float complex *)PyArray_DATA (arr));
      Py_DECREF (arr);
      Py_RETURN_NONE;
    }

  if (ensure_complex128 (&arr, obj, 1) < 0)
    return NULL;
  dp_fft2d_execute_inplace ((double complex *)PyArray_DATA (arr));
  Py_DECREF (arr);
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
