/*
 * wfm_ext_wfm_synth_extra.c -- hand-written _SynthEngine.prod (jm hook).
 *
 * Included by the aggregator wfm_ext.c after the generated types, and
 * registered by the `[[wfm_synth.extra_methods]]` row in
 * objects/wfm_synth.toml. jm never creates or modifies this file.
 *
 * Why it is hand-written, and for how long: dp_wfm_synth_prod is
 * `void (state, in, n, out)`, exactly the form a `batch` method declares. jm
 * 0.104.0's batch has three bugs -- no .pyi member, `nogil` ignored, a wrong
 * runtime doc (just-makeit#2259) -- so until jm 0.105.0 ships the fixes this
 * glue stays, and then this file is deleted for a `batch = true` method. The
 * glue is only type conversion: validate, allocate the result, call the
 * kernel.
 */

static PyObject *
_SynthEngine_prod (PyObject *self_, PyObject *x)
{
  _SynthEngineObject *self = (_SynthEngineObject *)self_;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!PyArray_Check (x) || PyArray_TYPE ((PyArrayObject *)x) != NPY_COMPLEX64)
    {
      PyErr_Format (PyExc_TypeError,
                    "x must be an ndarray of dtype np.complex64 (got %R)",
                    PyArray_Check (x)
                        ? (PyObject *)PyArray_DESCR ((PyArrayObject *)x)
                        : (PyObject *)Py_TYPE (x));
      return NULL;
    }
  if (PyArray_NDIM ((PyArrayObject *)x) != 1)
    {
      PyErr_Format (PyExc_ValueError, "x must be 1-D (got %d-D)",
                    PyArray_NDIM ((PyArrayObject *)x));
      return NULL;
    }
  /* A contiguous view of x (x itself when it already is): the kernel reads
   * a flat buffer, and x is never written. */
  PyArrayObject *in
      = (PyArrayObject *)PyArray_GETCONTIGUOUS ((PyArrayObject *)x);
  if (!in)
    return NULL;
  npy_intp  dims[1] = { PyArray_DIM (in, 0) };
  PyObject *arr     = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!arr)
    {
      Py_DECREF (in);
      return NULL;
    }
  Py_BEGIN_ALLOW_THREADS
    dp_wfm_synth_prod (self->handle, (const float _Complex *)PyArray_DATA (in),
                       (size_t)dims[0],
                       (float _Complex *)PyArray_DATA ((PyArrayObject *)arr));
  Py_END_ALLOW_THREADS
  Py_DECREF (in);
  return arr;
}
