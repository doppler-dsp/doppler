/*
 * cvt_ext_uq15_to_f32.c — UQ15ToF32 type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* UQ15ToF32Object — wraps uq15_to_f32_state_t *       */
/* ======================================================== */

#include "uq15_to_f32/uq15_to_f32_core.h"

typedef struct {
    PyObject_HEAD
    uq15_to_f32_state_t *handle;
} UQ15ToF32Object;

static void
UQ15ToF32Obj_dealloc(UQ15ToF32Object *self)
{
    if (self->handle)
        uq15_to_f32_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
UQ15ToF32Obj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    UQ15ToF32Object *self = (UQ15ToF32Object *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
UQ15ToF32Obj_init(UQ15ToF32Object *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"scale", NULL};
    float scale = 32768.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", kwlist,
                                     &scale))
        return -1;
    self->handle = uq15_to_f32_create(scale);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "uq15_to_f32_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
UQ15ToF32Obj_reset(UQ15ToF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    uq15_to_f32_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
UQ15ToF32_step(UQ15ToF32Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    unsigned int x_raw = 0U;
    if (!PyArg_ParseTuple(args, "I", &x_raw))
        return NULL;
    uint16_t x = (uint16_t)x_raw;
    float y = uq15_to_f32_step(self->handle, x);
    return PyFloat_FromDouble((double)y);
}

static PyObject *
UQ15ToF32_steps(UQ15ToF32Object *self, PyObject *args)
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
        in_obj, NPY_UINT16, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr)
        return NULL;

    Py_ssize_t n = PyArray_SIZE(in_arr);

    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_FLOAT,
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
        uq15_to_f32_steps(
            self->handle,
            (const uint16_t *)PyArray_DATA(in_arr),
            (float *)PyArray_DATA(out_arr),
            (size_t)n);
        Py_DECREF(in_arr);
        return (PyObject *)out_arr;
    }

    npy_intp dims[] = {n};
    PyObject *out_arr = PyArray_SimpleNew(1, dims, NPY_FLOAT);
    if (!out_arr) {
        Py_DECREF(in_arr);
        return NULL;
    }

    uq15_to_f32_steps(
        self->handle,
        (const uint16_t *)PyArray_DATA(in_arr),
        (float *)PyArray_DATA((PyArrayObject *)out_arr),
        (size_t)n);

    Py_DECREF(in_arr);
    return out_arr;
}




static PyObject *
UQ15ToF32Obj_destroy(UQ15ToF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        uq15_to_f32_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
UQ15ToF32Obj_enter(UQ15ToF32Object *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
UQ15ToF32Obj_exit(UQ15ToF32Object *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        uq15_to_f32_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef UQ15ToF32Obj_methods[] = {
    {"reset",    (PyCFunction)UQ15ToF32Obj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)UQ15ToF32_step,     METH_VARARGS,
     "step(x) -> float\n"
     "\n"
     "Process one input sample.\n"
     "\n"
     "Decodes UQ15 offset-binary: 0 -> -1.0, 32768 -> 0.0, 65535 -> ~+1.0.\n"
     "\n"
     "    >>> from doppler.cvt import UQ15ToF32\n"
     "    >>> obj = UQ15ToF32()\n"
     "    >>> obj.step(32768)\n"
     "    0.0\n"
     "    >>> obj.step(0)\n"
     "    -1.0\n"},
    {"steps",    (PyCFunction)UQ15ToF32_steps,    METH_VARARGS,
     "steps(x[, out]) -> ndarray\n"
     "\n"
     "Process a block of samples in batch.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler.cvt import UQ15ToF32\n"
     "    >>> obj = UQ15ToF32()\n"
     "    >>> y = obj.steps(np.full(4, 32768, dtype=np.uint16))\n"
     "    >>> y.shape\n"
     "    (4,)\n"
     "    >>> y.dtype\n"
     "    dtype('float32')\n"},

    {"destroy",  (PyCFunction)UQ15ToF32Obj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)UQ15ToF32Obj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)UQ15ToF32Obj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject UQ15ToF32ObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "cvt.UQ15ToF32",
    .tp_basicsize = sizeof(UQ15ToF32Object),
    .tp_dealloc   = (destructor)UQ15ToF32Obj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "UQ15ToF32 type.",
    .tp_methods   = UQ15ToF32Obj_methods,
    .tp_new       = UQ15ToF32Obj_new,
    .tp_init      = (initproc)UQ15ToF32Obj_init,
};
