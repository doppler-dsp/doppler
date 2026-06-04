/*
 * resample_ext_Resampler.c — Resampler type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* ResamplerObject — wraps Resampler_state_t *       */
/* ======================================================== */

#include "Resampler/Resampler_core.h"

typedef struct {
    PyObject_HEAD
    Resampler_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
    size_t _execute_buf_cap;  /* allocated capacity for execute */
    float complex *_execute_ctrl_buf;  /* pre-allocated output for execute_ctrl */
    size_t _execute_ctrl_buf_cap;  /* allocated capacity for execute_ctrl */
} ResamplerObject;

static void
ResamplerObj_dealloc(ResamplerObject *self)
{
    if (self->handle)
        Resampler_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
ResamplerObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    ResamplerObject *self = (ResamplerObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
ResamplerObj_init(ResamplerObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"rate", "bank", NULL};
    double rate = 0.0;
    PyObject *bank_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|dO", kwlist,
                                     &rate, &bank_obj))
        return -1;

    if (bank_obj && bank_obj != Py_None) {
        PyArrayObject *bank_arr = (PyArrayObject *)PyArray_FROM_OTF(
            bank_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
        if (!bank_arr) return -1;
        if (PyArray_NDIM(bank_arr) != 2) {
            Py_DECREF(bank_arr);
            PyErr_SetString(PyExc_ValueError, "bank must be 2-D float32");
            return -1;
        }
        size_t num_phases = (size_t)PyArray_DIM(bank_arr, 0);
        size_t num_taps   = (size_t)PyArray_DIM(bank_arr, 1);
        self->handle = Resampler_create_custom(
            num_phases, num_taps,
            (const float *)PyArray_DATA(bank_arr), rate);
        Py_DECREF(bank_arr);
    } else {
        self->handle = Resampler_create(rate);
    }

    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "Resampler_create returned NULL");
        return -1;
    }
    return 0;
}








static PyObject *
ResamplerObj_execute(ResamplerObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    PyArrayObject *x_arr = NULL;
    if (!PyArg_ParseTuple(args, "O", &x_obj))
        return NULL;
    x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;
    if (!self->_execute_buf) {
        size_t _max = Resampler_execute_max_out(self->handle);
        if (!_max) _max = (size_t)PyArray_SIZE(x_arr);
        self->_execute_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_buf) { Py_DECREF(x_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = Resampler_execute(self->handle, (const float complex *)PyArray_DATA(x_arr), (size_t)PyArray_SIZE(x_arr), self->_execute_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_execute_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(x_arr);
    return arr;
}

static PyObject *
ResamplerObj_execute_ctrl(ResamplerObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    PyArrayObject *x_arr = NULL;
    PyObject *ctrl_obj = NULL;
    PyArrayObject *ctrl_arr = NULL;
    if (!PyArg_ParseTuple(args, "OO", &x_obj, &ctrl_obj))
        return NULL;
    x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;
    ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF(
        ctrl_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!ctrl_arr) { Py_DECREF(x_arr); return NULL; }
    if (PyArray_SIZE(ctrl_arr) < PyArray_SIZE(x_arr)) {
        Py_DECREF(x_arr); Py_DECREF(ctrl_arr);
        PyErr_SetString(PyExc_ValueError,
                        "ctrl must be at least as long as x");
        return NULL;
    }
    if (!self->_execute_ctrl_buf) {
        size_t _max = Resampler_execute_ctrl_max_out(self->handle);
        if (!_max) _max = (size_t)PyArray_SIZE(x_arr);
        self->_execute_ctrl_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_ctrl_buf) { Py_DECREF(x_arr); Py_DECREF(ctrl_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = Resampler_execute_ctrl(self->handle, (const float complex *)PyArray_DATA(x_arr), (size_t)PyArray_SIZE(x_arr), (const float complex *)PyArray_DATA(ctrl_arr), (size_t)PyArray_SIZE(ctrl_arr), self->_execute_ctrl_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_execute_ctrl_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(x_arr);
    Py_DECREF(ctrl_arr);
    return arr;
}

static PyObject *
ResamplerObj_reset(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Resampler_reset(self->handle);
    Py_RETURN_NONE;
}
static PyObject *
Resampler_getprop_rate(ResamplerObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(Resampler_get_rate(self->handle));
}
static int
Resampler_setprop_rate(ResamplerObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    Resampler_set_rate(self->handle, v);
    return 0;
}
static PyObject *
Resampler_getprop_num_phases(ResamplerObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLongLong((unsigned long long)Resampler_get_num_phases(self->handle));
}
static PyObject *
Resampler_getprop_num_taps(ResamplerObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLongLong((unsigned long long)Resampler_get_num_taps(self->handle));
}

static PyGetSetDef Resampler_getset[] = {
    { "rate", (getter)Resampler_getprop_rate, (setter)Resampler_setprop_rate, "Rate.\n", NULL },
    { "num_phases", (getter)Resampler_getprop_num_phases, NULL, "Num phases.\n", NULL },
    { "num_taps", (getter)Resampler_getprop_num_taps, NULL, "Num taps.\n", NULL },
    { NULL }
};

static PyObject *
ResamplerObj_destroy(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        Resampler_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
ResamplerObj_enter(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
ResamplerObj_exit(ResamplerObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        Resampler_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef ResamplerObj_methods[] = {

    {"execute", (PyCFunction)ResamplerObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Resample x(0..x_len-1) into out(0..n_out-1).\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Resampler\n"
     "    >>> obj = Resampler(0.0)\n"
     "    >>> y = obj.execute(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"execute_ctrl", (PyCFunction)ResamplerObj_execute_ctrl, METH_VARARGS,
     "execute_ctrl(x) -> ndarray\n"
     "\n"
     "Resample with per-sample rate deviations.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Resampler\n"
     "    >>> obj = Resampler(0.0)\n"
     "    >>> y = obj.execute_ctrl(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"reset", (PyCFunction)ResamplerObj_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "Zero delay line and phase accumulator.  Rate and bank preserved.\n"
     "\n"
     "    >>> from doppler import Resampler\n"
     "    >>> obj = Resampler(0.0)\n"
     "    >>> obj.reset()\n"},
    {"destroy",  (PyCFunction)ResamplerObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)ResamplerObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)ResamplerObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject ResamplerObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.Resampler",
    .tp_basicsize = sizeof(ResamplerObject),
    .tp_dealloc   = (destructor)ResamplerObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create a Resampler with the built-in 4096×19 Kaiser bank.\n",
    .tp_methods   = ResamplerObj_methods,
    .tp_getset    = Resampler_getset,
    .tp_new       = ResamplerObj_new,
    .tp_init      = (initproc)ResamplerObj_init,
};
