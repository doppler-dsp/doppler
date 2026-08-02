/*
 * util_ext.c — Python extension module util
 *
 * Objects: 
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "util/util_core.h"



static PyObject *
_bind_square_clip(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"y", "lin", NULL};
    Py_complex y_raw = {0.0, 0.0};
    float lin = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Df",
            _kwlist, &y_raw, &lin))
        return NULL;
    float complex y = (float)y_raw.real + (float)y_raw.imag * I;
    return PyComplex_FromDoubles((double)crealf(square_clip(y, lin)), (double)cimagf(square_clip(y, lin)));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef util_module_methods[] = {
    {"square_clip", (PyCFunction)(void *)_bind_square_clip, METH_VARARGS | METH_KEYWORDS,
     "Square-clip a complex sample: clip the real and imaginary parts independently to [-lin, lin] (a square region in the IQ plane).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "y : complex\n"
     "    Complex CF32 input sample.\n"
     "lin : float\n"
     "    Per-component clip threshold (linear amplitude, >= 0). Values\n"
     "    outside `[-lin, lin]` are clamped; values on the boundary are\n"
     "    preserved exactly.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "complex\n"
     "    Sample with each component limited to `[-lin, lin]`.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.util import square_clip\n"
     ">>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through\n"
     "(0.5+0.25j)\n"
     ">>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged\n"
     "(1+0.5j)\n"
     ">>> square_clip(3.0-4.0j, 1.0)    # both components clipped\n"
     "(1-1j)\n"
     ">>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both\n"
     "(0.25+0.25j)\n"
     ">>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped\n"
     "(-1+0j)\n"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef util_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "util",
    .m_doc     = "Shared numeric utilities used across the doppler modules.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.util import square_clip\n"
     ">>> square_clip(2 + 0j, 1.0)\n"
     "(1+0j)\n",
    .m_size    = -1,
    .m_methods = util_module_methods,
};

PyMODINIT_FUNC
PyInit_util(void)
{
    import_array();

    PyObject *m = PyModule_Create(&util_moduledef);
    if (!m) return NULL;

    return m;
}
