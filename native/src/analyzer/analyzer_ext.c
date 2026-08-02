/*
 * analyzer_ext.c — Python extension module analyzer
 *
 * Objects: Specan
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "analyzer_ext_specan.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef analyzer_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "analyzer",
    .m_doc     = "Spectrum analysis: Specan renders a windowed, averaged power spectral density (dBFS) over a chosen span and resolution bandwidth, mirroring a hardware spectrum analyser's span / RBW / averaging controls.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> import numpy as np\n"
     ">>> from doppler.analyzer import Specan\n"
     ">>> sp = Specan(fs=1.024e6, span=200e3, rbw=2e3, src_center=0.0,\n"
     "...             center=0.0, offset_db=0.0, full_scale=1.0, bits=0,\n"
     "...             window=\"kaiser\", navg=1)\n"
     ">>> sp.execute(np.ones(4096, np.complex64)).shape\n"
     "(201,)\n",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_analyzer(void)
{
    import_array();
    if (PyType_Ready(&SpecanObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&analyzer_moduledef);
    if (!m) return NULL;
    Py_INCREF(&SpecanObjType);
    if (PyModule_AddObject(m, "Specan", (PyObject *)&SpecanObjType) < 0) {
        Py_DECREF(&SpecanObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
