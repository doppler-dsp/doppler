/*
 * mpsk_ext.c — Python extension module mpsk
 *
 * Objects: 
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "mpsk/mpsk_core.h"



static PyObject *
_bind_mpsk_map(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"sym", "m", NULL};
    PyObject *sym_obj = NULL;
    int m = 4;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|i",
            _kwlist, &sym_obj, &m))
        return NULL;
    PyArrayObject *sym_arr = (PyArrayObject *)PyArray_FROM_OTF(
        sym_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!sym_arr) { return NULL; }
    const uint8_t *sym = (const uint8_t *)PyArray_DATA(sym_arr);
    size_t sym_len = (size_t)PyArray_SIZE(sym_arr);
    npy_intp _dim = (npy_intp)sym_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_COMPLEX64, 0);
    if (!_out) {Py_DECREF(sym_arr); return NULL; }
    mpsk_map(sym, sym_len, (float complex *)PyArray_DATA((PyArrayObject *)_out), m);
    Py_DECREF(sym_arr);
    return _out;
}

static PyObject *
_bind_mpsk_demap(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"x", "m", NULL};
    PyObject *x_obj = NULL;
    int m = 4;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|i",
            _kwlist, &x_obj, &m))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float complex *x = (const float complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    npy_intp _dim = (npy_intp)x_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_UINT8, 0);
    if (!_out) {Py_DECREF(x_arr); return NULL; }
    mpsk_demap(x, x_len, (uint8_t *)PyArray_DATA((PyArrayObject *)_out), m);
    Py_DECREF(x_arr);
    return _out;
}

static PyObject *
_bind_mpsk_diff_map(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"sym", "m", NULL};
    PyObject *sym_obj = NULL;
    int m = 4;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|i",
            _kwlist, &sym_obj, &m))
        return NULL;
    PyArrayObject *sym_arr = (PyArrayObject *)PyArray_FROM_OTF(
        sym_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!sym_arr) { return NULL; }
    const uint8_t *sym = (const uint8_t *)PyArray_DATA(sym_arr);
    size_t sym_len = (size_t)PyArray_SIZE(sym_arr);
    npy_intp _dim = (npy_intp)sym_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_COMPLEX64, 0);
    if (!_out) {Py_DECREF(sym_arr); return NULL; }
    mpsk_diff_map(sym, sym_len, (float complex *)PyArray_DATA((PyArrayObject *)_out), m);
    Py_DECREF(sym_arr);
    return _out;
}

static PyObject *
_bind_mpsk_diff_demap(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"x", "m", NULL};
    PyObject *x_obj = NULL;
    int m = 4;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|i",
            _kwlist, &x_obj, &m))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float complex *x = (const float complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    npy_intp _dim = (npy_intp)x_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_UINT8, 0);
    if (!_out) {Py_DECREF(x_arr); return NULL; }
    mpsk_diff_demap(x, x_len, (uint8_t *)PyArray_DATA((PyArrayObject *)_out), m);
    Py_DECREF(x_arr);
    return _out;
}

static PyObject *
_bind_mpsk_bits_per_symbol(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"m", NULL};
    int m = 4;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i",
            _kwlist, &m))
        return NULL;
    return PyLong_FromLong((long)mpsk_bits_per_symbol(m));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef mpsk_module_methods[] = {
    {"mpsk_map", (PyCFunction)(void *)_bind_mpsk_map, METH_VARARGS | METH_KEYWORDS, "mpsk_map."},
    {"mpsk_demap", (PyCFunction)(void *)_bind_mpsk_demap, METH_VARARGS | METH_KEYWORDS, "mpsk_demap."},
    {"mpsk_diff_map", (PyCFunction)(void *)_bind_mpsk_diff_map, METH_VARARGS | METH_KEYWORDS, "mpsk_diff_map."},
    {"mpsk_diff_demap", (PyCFunction)(void *)_bind_mpsk_diff_demap, METH_VARARGS | METH_KEYWORDS, "mpsk_diff_demap."},
    {"mpsk_bits_per_symbol", (PyCFunction)(void *)_bind_mpsk_bits_per_symbol, METH_VARARGS | METH_KEYWORDS, "mpsk_bits_per_symbol."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef mpsk_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "mpsk",
    .m_doc     = "Mpsk module.",
    .m_size    = -1,
    .m_methods = mpsk_module_methods,
};

PyMODINIT_FUNC
PyInit_mpsk(void)
{
    import_array();

    PyObject *m = PyModule_Create(&mpsk_moduledef);
    if (!m) return NULL;

    return m;
}
