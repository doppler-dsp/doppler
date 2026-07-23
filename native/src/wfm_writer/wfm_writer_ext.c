/*
 * wfm_writer_ext.c — Python extension module wfm_writer
 *
 * Objects: Writer
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>
#include <string.h>

#include "wfm_writer/wfm_writer_core.h"

#include "wfm_writer_ext_wfm_writer.c"

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index(const char *const *tab, const char *s)
{
    for (int i = 0; tab[i]; i++)
        if (strcmp(tab[i], s) == 0)
            return i;
    return -1;
}

static const char *const _enum_stype[] = {
    "cf32",
    "cf64",
    "ci32",
    "ci16",
    "ci8",
    NULL,
};

static const char *const _enum_endian[] = {
    "le",
    "be",
    NULL,
};


static PyObject *
_bind_write_blue_header(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"path", "sample_type", "endian", "fs", "fc", "data_start", "total", "detached", NULL};
    PyObject *path = NULL;  /* fspath -> bytes */
    const char *sample_type = "cf32";
    const char *endian = "le";
    double fs = 1e6;
    double fc = 0.0;
    double data_start = 0.0;
    unsigned long long total_raw = 0;
    int detached = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|ssdddKi",
            _kwlist, PyUnicode_FSConverter, &path, &sample_type, &endian, &fs, &fc, &data_start, &total_raw, &detached))
    {
        Py_XDECREF(path);
        return NULL;
    }
    int _arg_sample_type = _enum_index(_enum_stype, sample_type);
    if (_arg_sample_type < 0) {
        PyErr_Format(PyExc_ValueError, "invalid sample_type '%s'", sample_type); Py_XDECREF(path);
        return NULL;
    }
    int _arg_endian = _enum_index(_enum_endian, endian);
    if (_arg_endian < 0) {
        PyErr_Format(PyExc_ValueError, "invalid endian '%s'", endian); Py_XDECREF(path);
        return NULL;
    }
    size_t total = (size_t)total_raw;
    int _rc = write_blue_header(PyBytes_AS_STRING(path), _arg_sample_type, _arg_endian, fs, fc, data_start, total, detached);
    Py_XDECREF(path);
    if (_rc != 0) {
        PyErr_Format(PyExc_RuntimeError,
            "write_blue_header failed (rc=%d)", (int)_rc);
        return NULL;
    }
    Py_RETURN_NONE;
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef wfm_writer_module_methods[] = {
    {"write_blue_header", (PyCFunction)(void *)_bind_write_blue_header, METH_VARARGS | METH_KEYWORDS, "Write a standalone BLUE type-1000 HCB header (the detached .hdr): 512 bytes carrying the BLUE magic, byte order, data_size (total x bytes-per-sample), the type-1000 tag and xdelta = 1/fs. Pair it with a detached .det body of raw interleaved I/Q. Raises on a failed write."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef wfm_writer_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "wfm_writer",
    .m_doc     = "WfmWriter module.",
    .m_size    = -1,
    .m_methods = wfm_writer_module_methods,
};

PyMODINIT_FUNC
PyInit_wfm_writer(void)
{
    import_array();
    if (PyType_Ready(&WriterObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&wfm_writer_moduledef);
    if (!m) return NULL;
    Py_INCREF(&WriterObjType);
    if (PyModule_AddObject(m, "Writer", (PyObject *)&WriterObjType) < 0) {
        Py_DECREF(&WriterObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
