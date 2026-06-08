/*
 * accumulator_ext_acc_f32.c — AccF32 type for the accumulator module.
 *
 * Included by accumulator_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only accumulator_ext.c is compiled.
 */
/* ======================================================== */
/* AccF32Object — wraps acc_f32_state_t *       */
/* ======================================================== */

#include "acc_f32/acc_f32_core.h"

typedef struct {
    PyObject_HEAD
    acc_f32_state_t *handle;
} AccF32Object;

static void
AccF32_dealloc(AccF32Object *self)
{
    if (self->handle)
        acc_f32_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
AccF32_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AccF32Object *self = (AccF32Object *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
AccF32_init(AccF32Object *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"acc", NULL};
    float acc = 0.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", kwlist,
                                     &acc))
        return -1;
    self->handle = acc_f32_create(acc);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "acc_f32_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
AccF32_reset(AccF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    acc_f32_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
AccF32_step(AccF32Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float x;
    if (!PyArg_ParseTuple(args, "f", &x))
        return NULL;
    acc_f32_step(self->handle, x);
    Py_RETURN_NONE;
}

static PyObject *
AccF32_steps(AccF32Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;

    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr)
        return NULL;

    acc_f32_steps(
        self->handle,
        (const float *)PyArray_DATA(in_arr),
        (size_t)PyArray_SIZE(in_arr));
    Py_DECREF(in_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccF32_get_acc(
    AccF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble((double)acc_f32_get_acc(self->handle));
}

static PyObject *
AccF32_set_acc(
    AccF32Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float v = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &v))
        return NULL;
    acc_f32_set_acc(self->handle, v);
    Py_RETURN_NONE;
}
static PyObject *
AccF32_get(AccF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float y = acc_f32_get(self->handle);
    return PyFloat_FromDouble((double)y);
}

static PyObject *
AccF32_dump(AccF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float y = acc_f32_dump(self->handle);
    return PyFloat_FromDouble((double)y);
}

static PyObject *
AccF32_madd(AccF32Object *self, PyObject *args)
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
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float *x = (const float *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!h_arr) { Py_DECREF(x_arr); return NULL; }
    const float *h = (const float *)PyArray_DATA(h_arr);
    size_t h_len = (size_t)PyArray_SIZE(h_arr);
    acc_f32_madd(self->handle, x, x_len, h, h_len);
    Py_DECREF(x_arr);
    Py_DECREF(h_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccF32_add2d(AccF32Object *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &x_obj))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float *x = (const float *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    acc_f32_add2d(self->handle, x, x_len);
    Py_DECREF(x_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccF32_madd2d(AccF32Object *self, PyObject *args)
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
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float *x = (const float *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!h_arr) { Py_DECREF(x_arr); return NULL; }
    const float *h = (const float *)PyArray_DATA(h_arr);
    size_t h_len = (size_t)PyArray_SIZE(h_arr);
    acc_f32_madd2d(self->handle, x, x_len, h, h_len);
    Py_DECREF(x_arr);
    Py_DECREF(h_arr);
    Py_RETURN_NONE;
}

static PyObject *
AccF32_destroy(AccF32Object *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        acc_f32_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
AccF32_enter(AccF32Object *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
AccF32_exit(AccF32Object *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        acc_f32_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef AccF32_methods[] = {
    {"reset",    (PyCFunction)AccF32_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)AccF32_step,     METH_VARARGS,
     "step(x) -> None\n"
     "\n"
     "Consume one input sample (sink; no output).\n"
     "\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> obj.step(1.0)\n"},
    {"steps",    (PyCFunction)AccF32_steps,    METH_VARARGS,
     "steps(x[, out]) -> ndarray\n"
     "\n"
     "Add all samples in ``input`` to the running sum. Equivalent to calling ``acc_f32_step`` for each element, but SIMD-vectorised on platforms that provide it (AVX-512 / AVX2 / SSE2). The loop uses JM_RESTRICT so the compiler can assume no aliasing between ``state`` and ``input``.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> y = obj.steps(np.zeros(4, dtype=np.float32))\n"},

    {"get_acc",
     (PyCFunction)AccF32_get_acc, METH_NOARGS,
     "Get acc."},
    {"set_acc",
     (PyCFunction)AccF32_set_acc, METH_VARARGS,
     "Set acc."},
    {"get", (PyCFunction)AccF32_get, METH_NOARGS,
     "get() -> float\n"
     "\n"
     "Return the current accumulated sum without resetting state. Identical to reading the ``acc`` property directly; retained as an explicit method so call sites that need the value can be uniform with ``dump`` without a conditional.\n"
     "\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> obj.get()\n"
     "    0.0\n"},
    {"dump", (PyCFunction)AccF32_dump, METH_NOARGS,
     "dump() -> float\n"
     "\n"
     "Return the accumulated sum and atomically reset it to zero. This is the canonical \"drain\" primitive: read the period total, then start a fresh accumulation interval without a separate ``reset`` call. The zero-reset is unconditional and always writes 0.0f.\n"
     "\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> obj.dump()\n"
     "    0.0\n"},
    {"madd", (PyCFunction)AccF32_madd, METH_VARARGS,
     "madd(x, h) -> None\n"
     "\n"
     "Multiply-accumulate: acc += sum(x * h) over x_len samples.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> obj.madd(np.zeros(4, dtype=np.float32), np.zeros(4, dtype=np.float32))\n"},
    {"add2d", (PyCFunction)AccF32_add2d, METH_VARARGS,
     "add2d(x) -> None\n"
     "\n"
     "Accumulate a 2-D array: acc += sum of all elements in x.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> obj.add2d(np.zeros(4, dtype=np.float32))\n"},
    {"madd2d", (PyCFunction)AccF32_madd2d, METH_VARARGS,
     "madd2d(x, h) -> None\n"
     "\n"
     "2-D multiply-accumulate: acc += sum(x * h) over x_len elements.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AccF32\n"
     "    >>> obj = AccF32(0.0)\n"
     "    >>> obj.madd2d(np.zeros(4, dtype=np.float32), np.zeros(4, dtype=np.float32))\n"},
    {"destroy",  (PyCFunction)AccF32_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)AccF32_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)AccF32_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject AccF32Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "accumulator.AccF32",
    .tp_basicsize = sizeof(AccF32Object),
    .tp_dealloc   = (destructor)AccF32_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Single-precision floating-point scalar accumulator. Maintains one running sum (``acc``) that persists across calls to ``step``, ``steps``, ``madd``, ``add2d``, and ``madd2d``. Use ``get`` to read without side-effects or ``dump`` to read and atomically zero in a single call.\n",
    .tp_methods   = AccF32_methods,
    .tp_new       = AccF32_new,
    .tp_init      = (initproc)AccF32_init,
};
