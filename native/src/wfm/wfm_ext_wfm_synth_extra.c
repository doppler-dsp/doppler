/*
 * wfm_ext_wfm_synth_extra.c -- hand-written _SynthEngine methods (jm hook).
 *
 * Included by wfm_ext_wfm_synth.c after the generated types, and registered
 * by the `[[wfm_synth.extra_methods]]` row in objects/wfm_synth.toml. jm
 * never creates or modifies this file.
 *
 * Why this one is hand-written: dp_wfm_synth_prod is
 * `(state, in, out, n) -> void`, the elementwise-operator shape every
 * in/out C kernel of the form `y[i] = f(x[i])` shares, and jm's block-method
 * forms are the count-returning `(state, in, n, out) -> size_t`. The glue is
 * only type conversion: validate, allocate the result, call the kernel.
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
                       (float _Complex *)PyArray_DATA ((PyArrayObject *)arr),
                       (size_t)dims[0]);
  Py_END_ALLOW_THREADS
  Py_DECREF (in);
  return arr;
}
