/*
 * accumulator_ext_acc_cf64.c — AccCf64 type for the accumulator module.
 *
 * Included by accumulator_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only accumulator_ext.c is compiled.
 */
/* ======================================================== */
/* AccCf64Object — wraps acc_cf64_state_t *       */
/* ======================================================== */

#include "acc_cf64/acc_cf64_core.h"

typedef struct {
    PyObject_HEAD
    acc_cf64_state_t *handle;
} AccCf64Object;

static void
AccCf64_dealloc(AccCf64Object *self)
{
    if (self->handle)
        acc_cf64_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
AccCf64_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AccCf64Object *self = (AccCf64Object *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
AccCf64_init(AccCf64Object *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"acc", NULL};
    Py_complex acc_raw = {0.0, 0.0};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|D", kwlist,
                                     &acc_raw))
        return -1;
    double _Complex acc = acc_raw.real + acc_raw.imag * I;
    self->handle = acc_cf64_create(acc);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "acc_cf64_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
AccCf64_reset(AccCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    acc_cf64_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_step(AccCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_complex x_raw = {0.0, 0.0};
    if (!PyArg_ParseTuple(args, "D", &x_raw))
        return NULL;
    double complex x = x_raw.real + x_raw.imag * I;
    acc_cf64_step(self->handle, x);
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_steps(AccCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;

    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr)
        return NULL;

    acc_cf64_steps(
        self->handle,
        (const double complex *)PyArray_DATA(in_arr),
        (size_t)PyArray_SIZE(in_arr));
    Py_DECREF(in_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_get_acc(
    AccCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyComplex_FromDoubles(creal(acc_cf64_get_acc(self->handle)), cimag(acc_cf64_get_acc(self->handle)));
}

static PyObject *
AccCf64_set_acc(
    AccCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_complex v_raw = {0.0, 0.0};
    if (!PyArg_ParseTuple(args, "D", &v_raw))
        return NULL;
    double _Complex v = v_raw.real + v_raw.imag * I;
    acc_cf64_set_acc(self->handle, v);
    Py_RETURN_NONE;
}
static PyObject *
AccCf64_get(AccCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    double complex y = acc_cf64_get(self->handle);
    return PyComplex_FromDoubles(creal(y), cimag(y));
}

static PyObject *
AccCf64_dump(AccCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    double complex y = acc_cf64_dump(self->handle);
    return PyComplex_FromDoubles(creal(y), cimag(y));
}

static PyObject *
AccCf64_madd(AccCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    PyObject *h_obj = NULL;
    if (!PyArg_ParseTuple(args, "OO", &x_obj, &h_obj))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const double complex *x = (const double complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!h_arr) { Py_DECREF(x_arr); return NULL; }
    const float *h = (const float *)PyArray_DATA(h_arr);
    size_t h_len = (size_t)PyArray_SIZE(h_arr);
    acc_cf64_madd(self->handle, x, x_len, h, h_len);
    Py_DECREF(x_arr);
    Py_DECREF(h_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_add2d(AccCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &x_obj))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const double complex *x = (const double complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    acc_cf64_add2d(self->handle, x, x_len);
    Py_DECREF(x_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_madd2d(AccCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    PyObject *h_obj = NULL;
    if (!PyArg_ParseTuple(args, "OO", &x_obj, &h_obj))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const double complex *x = (const double complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!h_arr) { Py_DECREF(x_arr); return NULL; }
    const float *h = (const float *)PyArray_DATA(h_arr);
    size_t h_len = (size_t)PyArray_SIZE(h_arr);
    acc_cf64_madd2d(self->handle, x, x_len, h, h_len);
    Py_DECREF(x_arr);
    Py_DECREF(h_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_destroy(AccCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        acc_cf64_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
AccCf64_enter(AccCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
AccCf64_exit(AccCf64Object *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        acc_cf64_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef AccCf64_methods[] = {
    {"reset",    (PyCFunction)AccCf64_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)AccCf64_step,     METH_VARARGS,
     "step(x) -> None\n"
     "\n"
     "Consume one input sample (sink; no output).\n"
     "\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> obj.step(1.0 + 0.0j)\n"},
    {"steps",    (PyCFunction)AccCf64_steps,    METH_VARARGS,
     "steps(x[, out]) -> ndarray\n"
     "\n"
     "Process a block of samples in batch.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> y = obj.steps(np.zeros(4, dtype=np.complex128))\n"},

    {"get_acc",
     (PyCFunction)AccCf64_get_acc, METH_NOARGS,
     "Get acc."},
    {"set_acc",
     (PyCFunction)AccCf64_set_acc, METH_VARARGS,
     "Set acc."},
    {"get", (PyCFunction)AccCf64_get, METH_NOARGS,
     "get() -> complex\n"
     "\n"
     "get.\n"
     "\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> obj.get()\n"
     "    0j\n"},
    {"dump", (PyCFunction)AccCf64_dump, METH_NOARGS,
     "dump() -> complex\n"
     "\n"
     "dump.\n"
     "\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> obj.dump()\n"
     "    0j\n"},
    {"madd", (PyCFunction)AccCf64_madd, METH_VARARGS,
     "madd(x, h) -> None\n"
     "\n"
     "Multiply-accumulate: acc += sum(x * h) over x_len samples.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> obj.madd(np.zeros(4, dtype=np.complex128), np.zeros(4, dtype=np.float32))\n"},
    {"add2d", (PyCFunction)AccCf64_add2d, METH_VARARGS,
     "add2d(x) -> None\n"
     "\n"
     "Accumulate a 2-D array: acc += sum of all elements in x.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> obj.add2d(np.zeros(4, dtype=np.complex128))\n"},
    {"madd2d", (PyCFunction)AccCf64_madd2d, METH_VARARGS,
     "madd2d(x, h) -> None\n"
     "\n"
     "2-D multiply-accumulate: acc += sum(x * h) over x_len elements.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccCf64\n"
     "    >>> obj = AccCf64(0j)\n"
     "    >>> obj.madd2d(np.zeros(4, dtype=np.complex128), np.zeros(4, dtype=np.float32))\n"},
    {"destroy",  (PyCFunction)AccCf64_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)AccCf64_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)AccCf64_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject AccCf64Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "accumulator.AccCf64",
    .tp_basicsize = sizeof(AccCf64Object),
    .tp_dealloc   = (destructor)AccCf64_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "AccCf64 type.\n",
    .tp_methods   = AccCf64_methods,
    .tp_new       = AccCf64_new,
    .tp_init      = (initproc)AccCf64_init,
};
