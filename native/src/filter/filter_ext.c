/*
 * filter_ext.c — Python extension module filter
 *
 * Objects: FIR, MovingAverage
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "filter/filter_core.h"

#include "filter_ext_fir.c"
#include "filter_ext_boxcar.c"

static PyObject *
_bind_design_lowpass(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"fpass", "fstop", "atten_db", NULL};
    double fpass = 0.4;
    double fstop = 0.6;
    double atten_db = 60.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ddd",
            _kwlist, &fpass, &fstop, &atten_db))
        return NULL;
    npy_intp _dim = (npy_intp)(kaiser_num_taps(1, atten_db, fpass / 2.0, fstop / 2.0) | 1);
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_FLOAT, 0);
    if (!_out) { return NULL; }
    design_lowpass(fpass, fstop, atten_db, (float *)PyArray_DATA((PyArrayObject *)_out));
    return _out;
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef filter_module_methods[] = {
    {"design_lowpass", (PyCFunction)(void *)_bind_design_lowpass, METH_VARARGS | METH_KEYWORDS, "Kaiser-windowed-sinc lowpass FIR taps, auto-sized by kaiser_num_taps (Nyquist-normalised fpass/fstop band edges, unit-DC-gain float32 taps)."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef filter_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "filter",
    .m_doc     = "Filter module.",
    .m_size    = -1,
    .m_methods = filter_module_methods,
};

PyMODINIT_FUNC
PyInit_filter(void)
{
    import_array();
    if (PyType_Ready(&FIRObjType) < 0) return NULL;
    if (PyType_Ready(&MovingAverageObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&filter_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FIRObjType);
    if (PyModule_AddObject(m, "FIR", (PyObject *)&FIRObjType) < 0) {
        Py_DECREF(&FIRObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&MovingAverageObjType);
    if (PyModule_AddObject(m, "MovingAverage", (PyObject *)&MovingAverageObjType) < 0) {
        Py_DECREF(&MovingAverageObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
