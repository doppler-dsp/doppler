/*
 * util_ext.c — Python extension module util
 *
 * Objects:
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "util/util_core.h"
static PyObject *
_bind_square_clip (PyObject *self, PyObject *args)
{
  (void)self;
  Py_complex y_raw = { 0.0, 0.0 };
  float lin = 0.0f;
  if (!PyArg_ParseTuple (args, "Df", &y_raw, &lin))
    return NULL;
  float complex y = (float)y_raw.real + (float)y_raw.imag * I;
  return PyComplex_FromDoubles ((double)crealf (square_clip (y, lin)),
                                (double)cimagf (square_clip (y, lin)));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Util_methods[]
    = { { "square_clip", _bind_square_clip, METH_VARARGS,
          "Square-clip a complex sample: clip the real and imaginary parts "
          "independently to [-lin, lin] (a square region in the IQ plane)." },
        { NULL, NULL, 0, NULL } };

static PyModuleDef util_moduledef = {
  PyModuleDef_HEAD_INIT, .m_name = "util",          .m_doc = "Util module.",
  .m_size = -1,          .m_methods = Util_methods,
};

PyMODINIT_FUNC
PyInit_util (void)
{
  import_array ();

  PyObject *m = PyModule_Create (&util_moduledef);
  if (!m)
    return NULL;

  return m;
}
