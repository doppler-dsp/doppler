/*
 * arith_ext.c — Python extension module arith
 *
 * Objects: AccQ15, AccQ8
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "arith/arith_core.h"

#include "arith_ext_acc_q15.c"
#include "arith_ext_acc_q8.c"

static PyObject *
_bind_add_q15(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int16_t *a = (const int16_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int16_t *b = (const int16_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT16, 0);
    if (!_out) {Py_DECREF(a_arr); Py_DECREF(b_arr); return NULL; }
    add_q15(a, a_len, b, b_len, (int16_t *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return _out;
}

static PyObject *
_bind_sub_q15(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int16_t *a = (const int16_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int16_t *b = (const int16_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT16, 0);
    if (!_out) {Py_DECREF(a_arr); Py_DECREF(b_arr); return NULL; }
    sub_q15(a, a_len, b, b_len, (int16_t *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return _out;
}

static PyObject *
_bind_mul_q15(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int16_t *a = (const int16_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int16_t *b = (const int16_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT16, 0);
    if (!_out) {Py_DECREF(a_arr); Py_DECREF(b_arr); return NULL; }
    mul_q15(a, a_len, b, b_len, (int16_t *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return _out;
}

static PyObject *
_bind_dot_q15(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int16_t *a = (const int16_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int16_t *b = (const int16_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return PyLong_FromLongLong((long long)dot_q15(a, a_len, b, b_len));
}

static PyObject *
_bind_shl_q15(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "n", NULL};
    PyObject *a_obj = NULL;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi",
            _kwlist, &a_obj, &n))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int16_t *a = (const int16_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT16, 0);
    if (!_out) {Py_DECREF(a_arr); return NULL; }
    shl_q15(a, a_len, (int16_t *)PyArray_DATA((PyArrayObject *)_out), n);
    Py_DECREF(a_arr);
    return _out;
}

static PyObject *
_bind_shr_q15(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "n", NULL};
    PyObject *a_obj = NULL;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi",
            _kwlist, &a_obj, &n))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int16_t *a = (const int16_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT16, 0);
    if (!_out) {Py_DECREF(a_arr); return NULL; }
    shr_q15(a, a_len, (int16_t *)PyArray_DATA((PyArrayObject *)_out), n);
    Py_DECREF(a_arr);
    return _out;
}

static PyObject *
_bind_add_q8(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int8_t *a = (const int8_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int8_t *b = (const int8_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT8, 0);
    if (!_out) {Py_DECREF(a_arr); Py_DECREF(b_arr); return NULL; }
    add_q8(a, a_len, b, b_len, (int8_t *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return _out;
}

static PyObject *
_bind_sub_q8(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int8_t *a = (const int8_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int8_t *b = (const int8_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT8, 0);
    if (!_out) {Py_DECREF(a_arr); Py_DECREF(b_arr); return NULL; }
    sub_q8(a, a_len, b, b_len, (int8_t *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return _out;
}

static PyObject *
_bind_mul_q8(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int8_t *a = (const int8_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int8_t *b = (const int8_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT8, 0);
    if (!_out) {Py_DECREF(a_arr); Py_DECREF(b_arr); return NULL; }
    mul_q8(a, a_len, b, b_len, (int8_t *)PyArray_DATA((PyArrayObject *)_out));
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return _out;
}

static PyObject *
_bind_dot_q8(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "b", NULL};
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO",
            _kwlist, &a_obj, &b_obj))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int8_t *a = (const int8_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    PyArrayObject *b_arr = (PyArrayObject *)PyArray_FROM_OTF(
        b_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!b_arr) { Py_DECREF(a_arr); return NULL; }
    const int8_t *b = (const int8_t *)PyArray_DATA(b_arr);
    size_t b_len = (size_t)PyArray_SIZE(b_arr);
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    return PyLong_FromLong((long)dot_q8(a, a_len, b, b_len));
}

static PyObject *
_bind_shl_q8(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "n", NULL};
    PyObject *a_obj = NULL;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi",
            _kwlist, &a_obj, &n))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int8_t *a = (const int8_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT8, 0);
    if (!_out) {Py_DECREF(a_arr); return NULL; }
    shl_q8(a, a_len, (int8_t *)PyArray_DATA((PyArrayObject *)_out), n);
    Py_DECREF(a_arr);
    return _out;
}

static PyObject *
_bind_shr_q8(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "n", NULL};
    PyObject *a_obj = NULL;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi",
            _kwlist, &a_obj, &n))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int8_t *a = (const int8_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT8, 0);
    if (!_out) {Py_DECREF(a_arr); return NULL; }
    shr_q8(a, a_len, (int8_t *)PyArray_DATA((PyArrayObject *)_out), n);
    Py_DECREF(a_arr);
    return _out;
}

static PyObject *
_bind_shl_i64(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "n", NULL};
    PyObject *a_obj = NULL;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi",
            _kwlist, &a_obj, &n))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT64, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int64_t *a = (const int64_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT64, 0);
    if (!_out) {Py_DECREF(a_arr); return NULL; }
    shl_i64(a, a_len, (int64_t *)PyArray_DATA((PyArrayObject *)_out), n);
    Py_DECREF(a_arr);
    return _out;
}

static PyObject *
_bind_shr_i64(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"a", "n", NULL};
    PyObject *a_obj = NULL;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi",
            _kwlist, &a_obj, &n))
        return NULL;
    PyArrayObject *a_arr = (PyArrayObject *)PyArray_FROM_OTF(
        a_obj, NPY_INT64, NPY_ARRAY_C_CONTIGUOUS);
    if (!a_arr) { return NULL; }
    const int64_t *a = (const int64_t *)PyArray_DATA(a_arr);
    size_t a_len = (size_t)PyArray_SIZE(a_arr);
    npy_intp _dim = (npy_intp)a_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_INT64, 0);
    if (!_out) {Py_DECREF(a_arr); return NULL; }
    shr_i64(a, a_len, (int64_t *)PyArray_DATA((PyArrayObject *)_out), n);
    Py_DECREF(a_arr);
    return _out;
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef arith_module_methods[] = {
    {"add_q15", (PyCFunction)(void *)_bind_add_q15, METH_VARARGS | METH_KEYWORDS, "Elementwise saturating two's complement add of two Q15 arrays."},
    {"sub_q15", (PyCFunction)(void *)_bind_sub_q15, METH_VARARGS | METH_KEYWORDS, "Elementwise saturating two's complement subtract of two Q15 arrays."},
    {"mul_q15", (PyCFunction)(void *)_bind_mul_q15, METH_VARARGS | METH_KEYWORDS, "Elementwise Q15 multiply with round-half-up: out[i] = sat16((a[i]*b[i] + 16384) >> 15)."},
    {"dot_q15", (PyCFunction)(void *)_bind_dot_q15, METH_VARARGS | METH_KEYWORDS, "Inner product of two Q15 arrays. Returns the raw Q30 accumulation as int64_t. Shift right 15 to get a Q15 scalar."},
    {"shl_q15", (PyCFunction)(void *)_bind_shl_q15, METH_VARARGS | METH_KEYWORDS, "Elementwise arithmetic left shift of a Q15 array with saturation. Equivalent to multiplying by 2^n in fixed-point."},
    {"shr_q15", (PyCFunction)(void *)_bind_shr_q15, METH_VARARGS | METH_KEYWORDS, "Elementwise arithmetic right shift of a Q15 array with round-half-up. Equivalent to dividing by 2^n."},
    {"add_q8", (PyCFunction)(void *)_bind_add_q8, METH_VARARGS | METH_KEYWORDS, "Elementwise saturating two's complement add of two Q8 arrays."},
    {"sub_q8", (PyCFunction)(void *)_bind_sub_q8, METH_VARARGS | METH_KEYWORDS, "Elementwise saturating two's complement subtract of two Q8 arrays."},
    {"mul_q8", (PyCFunction)(void *)_bind_mul_q8, METH_VARARGS | METH_KEYWORDS, "Elementwise Q8 multiply with round-half-up: out[i] = sat8((a[i]*b[i] + 64) >> 7)."},
    {"dot_q8", (PyCFunction)(void *)_bind_dot_q8, METH_VARARGS | METH_KEYWORDS, "Inner product of two Q8 arrays. Returns the raw Q14 accumulation as int32_t."},
    {"shl_q8", (PyCFunction)(void *)_bind_shl_q8, METH_VARARGS | METH_KEYWORDS, "Elementwise arithmetic left shift of a Q8 array with saturation."},
    {"shr_q8", (PyCFunction)(void *)_bind_shr_q8, METH_VARARGS | METH_KEYWORDS, "Elementwise arithmetic right shift of a Q8 array with round-half-up."},
    {"shl_i64", (PyCFunction)(void *)_bind_shl_i64, METH_VARARGS | METH_KEYWORDS, "Elementwise logical left shift of an int64_t array. No saturation (caller ensures no overflow)."},
    {"shr_i64", (PyCFunction)(void *)_bind_shr_i64, METH_VARARGS | METH_KEYWORDS, "Elementwise arithmetic right shift of an int64_t array with round-half-up. Useful for normalising dot_q15 Q30 results back to Q15."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef arith_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "arith",
    .m_doc     = "Arith module.",
    .m_size    = -1,
    .m_methods = arith_module_methods,
};

PyMODINIT_FUNC
PyInit_arith(void)
{
    import_array();
    if (PyType_Ready(&AccQ15Type) < 0) return NULL;
    if (PyType_Ready(&AccQ8Type) < 0) return NULL;
    PyObject *m = PyModule_Create(&arith_moduledef);
    if (!m) return NULL;
    Py_INCREF(&AccQ15Type);
    if (PyModule_AddObject(m, "AccQ15", (PyObject *)&AccQ15Type) < 0) {
        Py_DECREF(&AccQ15Type); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&AccQ8Type);
    if (PyModule_AddObject(m, "AccQ8", (PyObject *)&AccQ8Type) < 0) {
        Py_DECREF(&AccQ8Type); Py_DECREF(m); return NULL;
    }
    return m;
}
