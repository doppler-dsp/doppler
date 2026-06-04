/*
 * cvt_ext_f32_to_uq15.c — F32ToUQ15 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* F32ToUQ15Object — wraps f32_to_uq15_state_t *       */
/* ======================================================== */

#include "f32_to_uq15/f32_to_uq15_core.h"

typedef struct {
    PyObject_HEAD
    f32_to_uq15_state_t *handle;
} F32ToUQ15Object;

static void
F32ToUQ15Obj_dealloc(F32ToUQ15Object *self)
{
    if (self->handle)
        f32_to_uq15_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
F32ToUQ15Obj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    F32ToUQ15Object *self = (F32ToUQ15Object *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
F32ToUQ15Obj_init(F32ToUQ15Object *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"scale", NULL};
    float scale = 32768.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", kwlist,
                                     &scale))
        return -1;
    self->handle = f32_to_uq15_create(scale);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "f32_to_uq15_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
F32ToUQ15Obj_reset(F32ToUQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    f32_to_uq15_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
F32ToUQ15_step(F32ToUQ15Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float x;
    if (!PyArg_ParseTuple(args, "f", &x))
        return NULL;
    /* cast away const: step mutates the clipped flag */
    uint16_t y = f32_to_uq15_step(self->handle, x);
    return PyLong_FromUnsignedLong((unsigned long)y);
}

static PyObject *
F32ToUQ15_steps(F32ToUQ15Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj  = NULL;
    PyObject *out_obj = NULL;
    if (!PyArg_ParseTuple(args, "O|O", &in_obj, &out_obj))
        return NULL;

    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr)
        return NULL;

    Py_ssize_t n = PyArray_SIZE(in_arr);

    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_UINT16,
            NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
        if (!out_arr) { Py_DECREF(in_arr); return NULL; }
        if (PyArray_SIZE(out_arr) != n) {
            PyErr_Format(PyExc_ValueError,
                "out length %zd != input length %zd",
                (Py_ssize_t)PyArray_SIZE(out_arr), (Py_ssize_t)n);
            Py_DECREF(out_arr);
            Py_DECREF(in_arr);
            return NULL;
        }
        f32_to_uq15_steps(
            self->handle,
            (const float *)PyArray_DATA(in_arr),
            (uint16_t *)PyArray_DATA(out_arr),
            (size_t)n);
        Py_DECREF(in_arr);
        return (PyObject *)out_arr;
    }

    npy_intp dims[] = {n};
    PyObject *out_arr = PyArray_SimpleNew(1, dims, NPY_UINT16);
    if (!out_arr) {
        Py_DECREF(in_arr);
        return NULL;
    }

    f32_to_uq15_steps(
        self->handle,
        (const float *)PyArray_DATA(in_arr),
        (uint16_t *)PyArray_DATA((PyArrayObject *)out_arr),
        (size_t)n);

    Py_DECREF(in_arr);
    return out_arr;
}




static PyObject *
F32ToUQ15Obj_destroy(F32ToUQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        f32_to_uq15_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
F32ToUQ15Obj_enter(F32ToUQ15Object *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
F32ToUQ15Obj_exit(F32ToUQ15Object *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        f32_to_uq15_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef F32ToUQ15Obj_methods[] = {
    {"reset",    (PyCFunction)F32ToUQ15Obj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)F32ToUQ15_step,     METH_VARARGS,
     "step(x) -> uint16_t\n"
     "\n"
     "Process one input sample.\n"
     "\n"
     "    >>> from doppler import F32ToUQ15\n"
     "    >>> obj = F32ToUQ15(32768.0)\n"
     "    >>> obj.step(1.0)\n"
     "    0\n"},
    {"steps",    (PyCFunction)F32ToUQ15_steps,    METH_VARARGS,
     "steps(x[, out]) -> ndarray\n"
     "\n"
     "Process a block of samples in batch.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import F32ToUQ15\n"
     "    >>> obj = F32ToUQ15(32768.0)\n"
     "    >>> y = obj.steps(np.zeros(4, dtype=np.float32))\n"
     "    >>> y.shape\n"
     "    (4,)\n"
     "    >>> y.dtype\n"
     "    dtype('uint16')\n"},

    {"destroy",  (PyCFunction)F32ToUQ15Obj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)F32ToUQ15Obj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)F32ToUQ15Obj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject F32ToUQ15ObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "cvt.F32ToUQ15",
    .tp_basicsize = sizeof(F32ToUQ15Object),
    .tp_dealloc   = (destructor)F32ToUQ15Obj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "F32ToUQ15 type.\n",
    .tp_methods   = F32ToUQ15Obj_methods,
    .tp_new       = F32ToUQ15Obj_new,
    .tp_init      = (initproc)F32ToUQ15Obj_init,
};
