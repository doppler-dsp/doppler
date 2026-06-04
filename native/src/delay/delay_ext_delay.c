/*
 * delay_ext_delay.c — DelayCf64 type for the delay module.
 *
 * Included by delay_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only delay_ext.c is compiled.
 */
/* ======================================================== */
/* DelayCf64Object — wraps delay_state_t *       */
/* ======================================================== */

#include "delay/delay_core.h"

typedef struct {
    PyObject_HEAD
    delay_state_t *handle;
    double complex *_ptr_buf;  /* pre-allocated output for ptr */
    size_t _ptr_buf_cap;  /* allocated capacity for ptr */
    double complex *_push_ptr_buf;  /* pre-allocated output for push_ptr */
    size_t _push_ptr_buf_cap;  /* allocated capacity for push_ptr */
} DelayCf64Object;

static void
DelayCf64Obj_dealloc(DelayCf64Object *self)
{
    if (self->handle)
        delay_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
DelayCf64Obj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DelayCf64Object *self = (DelayCf64Object *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
DelayCf64Obj_init(DelayCf64Object *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"num_taps", NULL};
    unsigned long long num_taps_raw = 0ULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|K", kwlist,
                                     &num_taps_raw))
        return -1;
    size_t num_taps = (size_t)num_taps_raw;
    self->handle = delay_create(num_taps);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "delay_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
DelayCf64Obj_reset(DelayCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    delay_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
DelayCf64Obj_push(DelayCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_complex x_raw = {0.0, 0.0};
    if (!PyArg_ParseTuple(args, "D", &x_raw))
        return NULL;
    double complex x = x_raw.real + x_raw.imag * I;
    delay_push(self->handle, x);
    Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_ptr(DelayCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = (Py_ssize_t)self->handle->num_taps;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;
    if (!self->_ptr_buf) {
        size_t _max = delay_ptr_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_ptr_buf = malloc(_max * sizeof(double complex));
        if (!self->_ptr_buf) { PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = delay_ptr(self->handle, (size_t)n, self->_ptr_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX128, self->_ptr_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
DelayCf64Obj_push_ptr(DelayCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_complex x_raw = {0.0, 0.0};
    if (!PyArg_ParseTuple(args, "D", &x_raw))
        return NULL;
    double complex x = x_raw.real + x_raw.imag * I;
    if (!self->_push_ptr_buf) {
        size_t _max = delay_push_ptr_max_out(self->handle);
        if (!_max) _max = 1;
        self->_push_ptr_buf = malloc(_max * sizeof(double complex));
        if (!self->_push_ptr_buf) { PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = delay_push_ptr(self->handle, x, self->_push_ptr_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX128, self->_push_ptr_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
DelayCf64Obj_write(DelayCf64Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_complex x_raw = {0.0, 0.0};
    if (!PyArg_ParseTuple(args, "D", &x_raw))
        return NULL;
    double complex x = x_raw.real + x_raw.imag * I;
    delay_write(self->handle, x);
    Py_RETURN_NONE;
}
static PyObject *
DelayCf64_getprop_num_taps(DelayCf64Object *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->num_taps);
}
static PyObject *
DelayCf64_getprop_capacity(DelayCf64Object *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->capacity);
}

static PyGetSetDef DelayCf64_getset[] = {
    { "num_taps", (getter)DelayCf64_getprop_num_taps, NULL, "Num taps.\n", NULL },
    { "capacity", (getter)DelayCf64_getprop_capacity, NULL, "Capacity.\n", NULL },
    { NULL }
};

static PyObject *
DelayCf64Obj_destroy(DelayCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        delay_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
DelayCf64Obj_enter(DelayCf64Object *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
DelayCf64Obj_exit(DelayCf64Object *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        delay_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef DelayCf64Obj_methods[] = {
    {"reset",    (PyCFunction)DelayCf64Obj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"push", (PyCFunction)DelayCf64Obj_push, METH_VARARGS,
     "push(x) -> None\n"
     "\n"
     "push.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import DelayCf64\n"
     "    >>> obj = DelayCf64(1)\n"
     "    >>> obj.push(0j)\n"},
    {"ptr", (PyCFunction)DelayCf64Obj_ptr, METH_VARARGS,
     "ptr(n=1) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import DelayCf64\n"
     "    >>> obj = DelayCf64(1)\n"
     "    >>> y = obj.ptr(4)\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"push_ptr", (PyCFunction)DelayCf64Obj_push_ptr, METH_VARARGS,
     "push_ptr(n=1) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import DelayCf64\n"
     "    >>> obj = DelayCf64(1)\n"
     "    >>> y = obj.push_ptr(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex128')\n"},
    {"write", (PyCFunction)DelayCf64Obj_write, METH_VARARGS,
     "write(x) -> None\n"
     "\n"
     "write.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import DelayCf64\n"
     "    >>> obj = DelayCf64(1)\n"
     "    >>> obj.write(1.0 + 0.0j)\n"},
    {"destroy",  (PyCFunction)DelayCf64Obj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)DelayCf64Obj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)DelayCf64Obj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject DelayCf64ObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "delay.DelayCf64",
    .tp_basicsize = sizeof(DelayCf64Object),
    .tp_dealloc   = (destructor)DelayCf64Obj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "DelayCf64 type.\n",
    .tp_methods   = DelayCf64Obj_methods,
    .tp_getset    = DelayCf64_getset,
    .tp_new       = DelayCf64Obj_new,
    .tp_init      = (initproc)DelayCf64Obj_init,
};
