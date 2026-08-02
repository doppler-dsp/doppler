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
    {"add_q15", (PyCFunction)(void *)_bind_add_q15, METH_VARARGS | METH_KEYWORDS,
     "Elementwise saturating two's complement add of two Q15 arrays.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int16]\n"
     "    First input array (int16_t).\n"
     "b : NDArray[np.int16]\n"
     "    Second input array (int16_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int16]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import add_q15\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([100, 20000, -20000], dtype=np.int16)\n"
     ">>> b = np.array([50,  20000, -20000], dtype=np.int16)\n"
     ">>> add_q15(a, b).tolist()\n"
     "[150, 32767, -32768]\n"},
    {"sub_q15", (PyCFunction)(void *)_bind_sub_q15, METH_VARARGS | METH_KEYWORDS,
     "Elementwise saturating two's complement subtract of two Q15 arrays.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int16]\n"
     "    Minuend array (int16_t).\n"
     "b : NDArray[np.int16]\n"
     "    Subtrahend array (int16_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int16]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import sub_q15\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([100,  0, -32768], dtype=np.int16)\n"
     ">>> b = np.array([50,   0,     10], dtype=np.int16)\n"
     ">>> sub_q15(a, b).tolist()\n"
     "[50, 0, -32768]\n"},
    {"mul_q15", (PyCFunction)(void *)_bind_mul_q15, METH_VARARGS | METH_KEYWORDS,
     "Elementwise Q15 multiply with round-half-up: out[i] = sat16((a[i]*b[i] + 16384) >> 15).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int16]\n"
     "    First input array (int16_t).\n"
     "b : NDArray[np.int16]\n"
     "    Second input array (int16_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int16]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import mul_q15\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([16384, 16384, 32767], dtype=np.int16)\n"
     ">>> b = np.array([16384, -16384, 32767], dtype=np.int16)\n"
     ">>> mul_q15(a, b).tolist()\n"
     "[8192, -8192, 32766]\n"},
    {"dot_q15", (PyCFunction)(void *)_bind_dot_q15, METH_VARARGS | METH_KEYWORDS,
     "Inner product of two Q15 arrays. Returns the raw Q30 accumulation as int64_t. Shift right 15 to get a Q15 scalar.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int16]\n"
     "    First input array (int16_t).\n"
     "b : NDArray[np.int16]\n"
     "    Second input array (int16_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Raw Q30 accumulation (int64_t).\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import dot_q15\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([100, 200, 300], dtype=np.int16)\n"
     ">>> b = np.array([1, 2, 3], dtype=np.int16)\n"
     ">>> dot_q15(a, b)\n"
     "1400\n"},
    {"shl_q15", (PyCFunction)(void *)_bind_shl_q15, METH_VARARGS | METH_KEYWORDS,
     "Elementwise arithmetic left shift of a Q15 array with saturation. Equivalent to multiplying by 2^n in fixed-point.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int16]\n"
     "    Input array (int16_t).\n"
     "n : int\n"
     "    Shift count (non-negative integer).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int16]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import shl_q15\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([8192, 16384, 20000], dtype=np.int16)\n"
     ">>> shl_q15(a, 1).tolist()\n"
     "[16384, 32767, 32767]\n"},
    {"shr_q15", (PyCFunction)(void *)_bind_shr_q15, METH_VARARGS | METH_KEYWORDS,
     "Elementwise arithmetic right shift of a Q15 array with round-half-up. Equivalent to dividing by 2^n.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int16]\n"
     "    Input array (int16_t).\n"
     "n : int\n"
     "    Shift count (non-negative integer).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int16]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import shr_q15\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([100, 101, 102, -100], dtype=np.int16)\n"
     ">>> shr_q15(a, 2).tolist()\n"
     "[25, 25, 26, -25]\n"},
    {"add_q8", (PyCFunction)(void *)_bind_add_q8, METH_VARARGS | METH_KEYWORDS,
     "Elementwise saturating two's complement add of two Q8 arrays.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int8]\n"
     "    First input array (int8_t).\n"
     "b : NDArray[np.int8]\n"
     "    Second input array (int8_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int8]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import add_q8\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([50, 100, -100], dtype=np.int8)\n"
     ">>> b = np.array([50,  30,  -50], dtype=np.int8)\n"
     ">>> add_q8(a, b).tolist()\n"
     "[100, 127, -128]\n"},
    {"sub_q8", (PyCFunction)(void *)_bind_sub_q8, METH_VARARGS | METH_KEYWORDS,
     "Elementwise saturating two's complement subtract of two Q8 arrays.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int8]\n"
     "    Minuend array (int8_t).\n"
     "b : NDArray[np.int8]\n"
     "    Subtrahend array (int8_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int8]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import sub_q8\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([50,   0, -128], dtype=np.int8)\n"
     ">>> b = np.array([30,   0,   10], dtype=np.int8)\n"
     ">>> sub_q8(a, b).tolist()\n"
     "[20, 0, -128]\n"},
    {"mul_q8", (PyCFunction)(void *)_bind_mul_q8, METH_VARARGS | METH_KEYWORDS,
     "Elementwise Q8 multiply with round-half-up: out[i] = sat8((a[i]*b[i] + 64) >> 7).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int8]\n"
     "    First input array (int8_t).\n"
     "b : NDArray[np.int8]\n"
     "    Second input array (int8_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int8]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import mul_q8\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([64,  64, -64], dtype=np.int8)\n"
     ">>> b = np.array([64, -64,  64], dtype=np.int8)\n"
     ">>> mul_q8(a, b).tolist()\n"
     "[32, -32, -32]\n"},
    {"dot_q8", (PyCFunction)(void *)_bind_dot_q8, METH_VARARGS | METH_KEYWORDS,
     "Inner product of two Q8 arrays. Returns the raw Q14 accumulation as int32_t.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int8]\n"
     "    First input array (int8_t).\n"
     "b : NDArray[np.int8]\n"
     "    Second input array (int8_t), same length as a.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Raw Q14 accumulation (int32_t).\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import dot_q8\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([10, 20, 30], dtype=np.int8)\n"
     ">>> b = np.array([1, 2, 3], dtype=np.int8)\n"
     ">>> dot_q8(a, b)\n"
     "140\n"},
    {"shl_q8", (PyCFunction)(void *)_bind_shl_q8, METH_VARARGS | METH_KEYWORDS,
     "Elementwise arithmetic left shift of a Q8 array with saturation.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int8]\n"
     "    Input array (int8_t).\n"
     "n : int\n"
     "    Shift count (non-negative integer).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int8]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import shl_q8\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([10, 50, 64], dtype=np.int8)\n"
     ">>> shl_q8(a, 1).tolist()\n"
     "[20, 100, 127]\n"},
    {"shr_q8", (PyCFunction)(void *)_bind_shr_q8, METH_VARARGS | METH_KEYWORDS,
     "Elementwise arithmetic right shift of a Q8 array with round-half-up.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int8]\n"
     "    Input array (int8_t).\n"
     "n : int\n"
     "    Shift count (non-negative integer).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int8]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import shr_q8\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([10, 11, 12, -10], dtype=np.int8)\n"
     ">>> shr_q8(a, 2).tolist()\n"
     "[3, 3, 3, -2]\n"},
    {"shl_i64", (PyCFunction)(void *)_bind_shl_i64, METH_VARARGS | METH_KEYWORDS,
     "Elementwise logical left shift of an int64_t array. No saturation (caller ensures no overflow).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int64]\n"
     "    Input array (int64_t).\n"
     "n : int\n"
     "    Shift count (non-negative integer; >= 63 yields 0).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int64]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import shl_i64\n"
     ">>> import numpy as np\n"
     ">>> a = np.array([100, 200, -200], dtype=np.int64)\n"
     ">>> shl_i64(a, 3).tolist()\n"
     "[800, 1600, -1600]\n"},
    {"shr_i64", (PyCFunction)(void *)_bind_shr_i64, METH_VARARGS | METH_KEYWORDS,
     "Elementwise arithmetic right shift of an int64_t array with round-half-up. Useful for normalising dot_q15 Q30 results back to Q15.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "a : NDArray[np.int64]\n"
     "    Input array (int64_t).\n"
     "n : int\n"
     "    Shift count (non-negative integer; >= 63 is clamped to 63).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.int64]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import dot_q15, shr_i64\n"
     ">>> import numpy as np\n"
     ">>> raw = dot_q15(\n"
     "...     np.array([16384, 16384], dtype=np.int16),\n"
     "...     np.array([16384, 16384], dtype=np.int16),\n"
     "... )\n"
     ">>> shr_i64(np.array([raw], dtype=np.int64), 15).tolist()\n"
     "[16384]\n"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef arith_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "arith",
    .m_doc     = "Fixed-point accumulators: Q8 and Q15 scalar sums (AccQ8, AccQ15) with exact integer arithmetic for bit-true pipelines.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.arith import AccQ15\n"
     ">>> a = AccQ15()\n"
     ">>> a.step(100); a.step(50)\n"
     ">>> a.get()\n"
     "150\n",
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
