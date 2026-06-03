/*
 * arith_ext_acc_q15.c — AccQ15 type for the arith module.
 *
 * Included by arith_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only arith_ext.c is compiled.
 */
/* ======================================================== */
/* AccQ15Object — wraps acc_q15_state_t *       */
/* ======================================================== */

#include "acc_q15/acc_q15_core.h"

typedef struct {
    PyObject_HEAD
    acc_q15_state_t *handle;
} AccQ15Object;

static void
AccQ15_dealloc(AccQ15Object *self)
{
    if (self->handle)
        acc_q15_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
AccQ15_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AccQ15Object *self = (AccQ15Object *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
AccQ15_init(AccQ15Object *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"acc", NULL};
    long long acc_raw = 0LL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|L", kwlist,
                                     &acc_raw))
        return -1;
    int64_t acc = (int64_t)acc_raw;
    self->handle = acc_q15_create(acc);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "acc_q15_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
AccQ15_reset(AccQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    acc_q15_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
AccQ15_step(AccQ15Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    int x_raw = 0;
    if (!PyArg_ParseTuple(args, "i", &x_raw))
        return NULL;
    int16_t x = (int16_t)x_raw;
    acc_q15_step(self->handle, x);
    Py_RETURN_NONE;
}

static PyObject *
AccQ15_steps(AccQ15Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;

    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr)
        return NULL;

    acc_q15_steps(
        self->handle,
        (const int16_t *)PyArray_DATA(in_arr),
        (size_t)PyArray_SIZE(in_arr));
    Py_DECREF(in_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccQ15_get_acc(
    AccQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLongLong((long long)acc_q15_get_acc(self->handle));
}

static PyObject *
AccQ15_set_acc(
    AccQ15Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    long long v_raw = 0LL;
    if (!PyArg_ParseTuple(args, "L", &v_raw))
        return NULL;
    int64_t v = (int64_t)v_raw;
    acc_q15_set_acc(self->handle, v);
    Py_RETURN_NONE;
}
static PyObject *
AccQ15_get(AccQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    int64_t y = acc_q15_get(self->handle);
    return PyLong_FromLongLong((long long)y);
}

static PyObject *
AccQ15_dump(AccQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    int64_t y = acc_q15_dump(self->handle);
    return PyLong_FromLongLong((long long)y);
}

static PyObject *
AccQ15_madd(AccQ15Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *a_obj = NULL;
    PyObject *b_obj = NULL;
    if (!PyArg_ParseTuple(args, "OO", &a_obj, &b_obj))
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
    acc_q15_madd(self->handle, a, a_len, b, b_len);
    Py_DECREF(a_arr);
    Py_DECREF(b_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccQ15_destroy(AccQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        acc_q15_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
AccQ15_enter(AccQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
AccQ15_exit(AccQ15Object *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        acc_q15_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef AccQ15_methods[] = {
    {"reset",    (PyCFunction)AccQ15_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)AccQ15_step,     METH_VARARGS,
     "step(x) -> None\n"
     "\n"
     "Consume one input sample (sink; no output).\n"
     "\n"
     "    >>> from doppler import AccQ15\n"
     "    >>> obj = AccQ15(0)\n"
     "    >>> obj.step(1)\n"},
    {"steps",    (PyCFunction)AccQ15_steps,    METH_VARARGS,
     "steps(x[, out]) -> ndarray\n"
     "\n"
     "Process a block of samples in batch.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccQ15\n"
     "    >>> obj = AccQ15(0)\n"
     "    >>> y = obj.steps(np.zeros(4, dtype=np.int16))\n"},

    {"get_acc",
     (PyCFunction)AccQ15_get_acc, METH_NOARGS,
     "Get acc."},
    {"set_acc",
     (PyCFunction)AccQ15_set_acc, METH_VARARGS,
     "Set acc."},
    {"get", (PyCFunction)AccQ15_get, METH_NOARGS,
     "get() -> int\n"
     "\n"
     "get.\n"
     "\n"
     "    >>> from doppler import AccQ15\n"
     "    >>> obj = AccQ15(0)\n"
     "    >>> obj.get()\n"
     "    0\n"},
    {"dump", (PyCFunction)AccQ15_dump, METH_NOARGS,
     "dump() -> int\n"
     "\n"
     "dump.\n"
     "\n"
     "    >>> from doppler import AccQ15\n"
     "    >>> obj = AccQ15(0)\n"
     "    >>> obj.dump()\n"
     "    0\n"},
    {"madd", (PyCFunction)AccQ15_madd, METH_VARARGS,
     "madd(a, b) -> None\n"
     "\n"
     "madd.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccQ15\n"
     "    >>> obj = AccQ15(0)\n"
     "    >>> obj.madd(np.zeros(4, dtype=np.int16), np.zeros(4, dtype=np.int16))\n"},
    {"destroy",  (PyCFunction)AccQ15_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)AccQ15_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)AccQ15_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject AccQ15Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "arith.AccQ15",
    .tp_basicsize = sizeof(AccQ15Object),
    .tp_dealloc   = (destructor)AccQ15_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "AccQ15 type.",
    .tp_methods   = AccQ15_methods,
    .tp_new       = AccQ15_new,
    .tp_init      = (initproc)AccQ15_init,
};
