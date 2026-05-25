/*
 * source_ext_lo.c — LO type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* LOObject — wraps lo_state_t *       */
/* ======================================================== */

#include "lo/lo_core.h"

typedef struct {
    PyObject_HEAD
    lo_state_t *handle;
    float complex *_steps_buf;  /* pre-allocated output for steps */
    float complex *_steps_ctrl_buf;  /* pre-allocated output for steps_ctrl */
} LOObject;

static void
LOObj_dealloc(LOObject *self)
{
    if (self->handle)
        lo_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
LOObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    LOObject *self = (LOObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
LOObj_init(LOObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"norm_freq", NULL};
    double norm_freq = 0.0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|d", kwlist,
                                     &norm_freq))
        return -1;
    self->handle = lo_create(norm_freq);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "lo_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
LOObj_reset(LOObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    lo_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
LOObj_steps(LOObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = 1;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;
    if (!self->_steps_buf) {
        size_t _max = lo_steps_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_steps_buf = malloc(_max * sizeof(float complex));
        if (!self->_steps_buf) { PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = lo_steps(self->handle, (size_t)n, self->_steps_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_steps_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
LOObj_steps_ctrl(LOObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *ctrl_obj = NULL;
    PyArrayObject *ctrl_arr = NULL;
    if (!PyArg_ParseTuple(args, "O", &ctrl_obj))
        return NULL;
    ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF(
        ctrl_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!ctrl_arr) return NULL;
    if (!self->_steps_ctrl_buf) {
        size_t _max = lo_steps_ctrl_max_out(self->handle);
        if (!_max) _max = (size_t)PyArray_SIZE(ctrl_arr);
        self->_steps_ctrl_buf = malloc(_max * sizeof(float complex));
        if (!self->_steps_ctrl_buf) { Py_DECREF(ctrl_arr); PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = lo_steps_ctrl(self->handle, (const float *)PyArray_DATA(ctrl_arr), (size_t)PyArray_SIZE(ctrl_arr), self->_steps_ctrl_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_steps_ctrl_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(ctrl_arr);
    return arr;
}
static PyObject *
LO_getprop_norm_freq(LOObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(lo_get_norm_freq(self->handle));
}
static int
LO_setprop_norm_freq(LOObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    lo_set_norm_freq(self->handle, v);
    return 0;
}
static PyObject *
LO_getprop_phase(LOObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLong((unsigned long)lo_get_phase(self->handle));
}
static int
LO_setprop_phase(LOObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    unsigned long v_raw = 0UL;
    if (!PyArg_Parse(value, "k", &v_raw)) return -1;
    uint32_t v = (uint32_t)v_raw;
    lo_set_phase(self->handle, v);
    return 0;
}
static PyObject *
LO_getprop_phase_inc(LOObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLong((unsigned long)lo_get_phase_inc(self->handle));
}

static PyGetSetDef LO_getset[] = {
    { "norm_freq", (getter)LO_getprop_norm_freq, (setter)LO_setprop_norm_freq, NULL, NULL },
    { "phase", (getter)LO_getprop_phase, (setter)LO_setprop_phase, NULL, NULL },
    { "phase_inc", (getter)LO_getprop_phase_inc, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
LOObj_destroy(LOObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        lo_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
LOObj_enter(LOObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
LOObj_exit(LOObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        lo_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef LOObj_methods[] = {
    {"reset",    (PyCFunction)LOObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"steps", (PyCFunction)LOObj_steps, METH_VARARGS,
     "steps(n=1) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import LO\n"
     "    >>> obj = LO(0.0)\n"
     "    >>> y = obj.steps(4)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"steps_ctrl", (PyCFunction)LOObj_steps_ctrl, METH_VARARGS,
     "steps_ctrl(ctrl) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import LO\n"
     "    >>> obj = LO(0.0)\n"
     "    >>> y = obj.steps_ctrl(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)LOObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)LOObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)LOObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject LOObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "source.LO",
    .tp_basicsize = sizeof(LOObject),
    .tp_dealloc   = (destructor)LOObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "LO type.",
    .tp_methods   = LOObj_methods,
    .tp_getset    = LO_getset,
    .tp_new       = LOObj_new,
    .tp_init      = (initproc)LOObj_init,
};
