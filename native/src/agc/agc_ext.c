/*
 * agc_ext.c — Python extension module agc
 *
 * Objects: AGC
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "agc_ext_agc.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef agc_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "agc",
    .m_doc     = "Automatic gain control: a log-domain feedback AGC (AGC) that drives signal power toward a reference level.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> import numpy as np\n"
     ">>> from doppler.agc import AGC\n"
     ">>> y = AGC(ref_db=0.0, loop_bw=0.05).steps(0.1 * np.ones(200, np.complex64))\n"
     ">>> bool(abs(y[-1]) > abs(y[0]))\n"
     "True\n",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_agc(void)
{
    import_array();
    if (PyType_Ready(&AGCObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&agc_moduledef);
    if (!m) return NULL;
    Py_INCREF(&AGCObjType);
    if (PyModule_AddObject(m, "AGC", (PyObject *)&AGCObjType) < 0) {
        Py_DECREF(&AGCObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
