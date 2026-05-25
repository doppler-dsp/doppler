/*
 * source_ext_nco.c — NCO type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* NCOObject — wraps nco_state_t *       */
/* ======================================================== */

#include "nco/nco_core.h"

typedef struct {
    PyObject_HEAD
    nco_state_t *handle;
    uint32_t *_steps_u32_buf;  /* pre-allocated output for steps_u32 */
    uint32_t *_steps_u32_scaled_buf;  /* pre-allocated output for steps_u32_scaled */
    uint32_t *_steps_u32_ovf_buf;  /* pre-allocated output for steps_u32_ovf */
    uint8_t *_steps_u32_ovf_buf_1;  /* pre-allocated output for steps_u32_ovf */
} NCOObject;

static void
NCOObj_dealloc(NCOObject *self)
{
    if (self->handle)
        nco_destroy(self->handle);
    free(self->_steps_u32_buf);
    free(self->_steps_u32_scaled_buf);
    free(self->_steps_u32_ovf_buf);
    free(self->_steps_u32_ovf_buf_1);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
NCOObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    NCOObject *self = (NCOObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
NCOObj_init(NCOObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"norm_freq", "nmax", NULL};
    double norm_freq = 0.0;
    unsigned long nmax_raw = 0UL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|dk", kwlist,
                                     &norm_freq, &nmax_raw))
        return -1;
    uint32_t nmax = (uint32_t)nmax_raw;
    self->handle = nco_create(norm_freq, nmax);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "nco_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
NCOObj_reset(NCOObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    nco_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
NCOObj_steps_u32(NCOObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = 1;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;
    if (!self->_steps_u32_buf) {
        size_t _max = nco_steps_u32_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_steps_u32_buf = malloc(_max * sizeof(uint32_t));
        if (!self->_steps_u32_buf) { PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = nco_steps_u32(self->handle, (size_t)n, self->_steps_u32_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_UINT32, self->_steps_u32_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
NCOObj_steps_u32_scaled(NCOObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = 1;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;
    if (!self->_steps_u32_scaled_buf) {
        size_t _max = nco_steps_u32_scaled_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_steps_u32_scaled_buf = malloc(_max * sizeof(uint32_t));
        if (!self->_steps_u32_scaled_buf) { PyErr_NoMemory(); return NULL; }
    }
    size_t n_out = nco_steps_u32_scaled(self->handle, (size_t)n, self->_steps_u32_scaled_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_UINT32, self->_steps_u32_scaled_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
NCOObj_steps_u32_ovf(NCOObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = 1;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;
    if (!self->_steps_u32_ovf_buf) {
        size_t _max = nco_steps_u32_ovf_max_out(self->handle);
        if (!_max) _max = (size_t)n;
        self->_steps_u32_ovf_buf = malloc(_max * sizeof(uint32_t));
        if (!self->_steps_u32_ovf_buf) { PyErr_NoMemory(); return NULL; }
        self->_steps_u32_ovf_buf_1 = malloc(_max * sizeof(uint8_t));
        if (!self->_steps_u32_ovf_buf_1) {
            free(self->_steps_u32_ovf_buf);
            self->_steps_u32_ovf_buf = NULL;
            PyErr_NoMemory(); return NULL;
        }
    }
    size_t n_out = nco_steps_u32_ovf(self->handle, (size_t)n, self->_steps_u32_ovf_buf, self->_steps_u32_ovf_buf_1);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr0 = PyArray_SimpleNewFromData(
        1, &dim, NPY_UINT32, self->_steps_u32_ovf_buf);
    PyObject *arr1 = PyArray_SimpleNewFromData(
        1, &dim, NPY_UINT8, self->_steps_u32_ovf_buf_1);
    if (!arr0 || !arr1) {
        Py_XDECREF(arr0); Py_XDECREF(arr1); return NULL;
    }
    PyArray_SetBaseObject((PyArrayObject *)arr0, (PyObject *)self); Py_INCREF(self);
    PyArray_SetBaseObject((PyArrayObject *)arr1, (PyObject *)self); Py_INCREF(self);
    PyObject *result = PyTuple_Pack(2, arr0, arr1);
    Py_DECREF(arr0);
    Py_DECREF(arr1);
    return result;
}
static PyObject *
NCO_getprop_norm_freq(NCOObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(nco_get_norm_freq(self->handle));
}
static int
NCO_setprop_norm_freq(NCOObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    nco_set_norm_freq(self->handle, v);
    return 0;
}
static PyObject *
NCO_getprop_phase(NCOObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLong((unsigned long)nco_get_phase(self->handle));
}
static int
NCO_setprop_phase(NCOObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    unsigned long v_raw = 0UL;
    if (!PyArg_Parse(value, "k", &v_raw)) return -1;
    uint32_t v = (uint32_t)v_raw;
    nco_set_phase(self->handle, v);
    return 0;
}
static PyObject *
NCO_getprop_phase_inc(NCOObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLong((unsigned long)nco_get_phase_inc(self->handle));
}

static PyGetSetDef NCO_getset[] = {
    { "norm_freq", (getter)NCO_getprop_norm_freq, (setter)NCO_setprop_norm_freq, NULL, NULL },
    { "phase", (getter)NCO_getprop_phase, (setter)NCO_setprop_phase, NULL, NULL },
    { "phase_inc", (getter)NCO_getprop_phase_inc, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
NCOObj_destroy(NCOObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        nco_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
NCOObj_enter(NCOObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
NCOObj_exit(NCOObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        nco_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef NCOObj_methods[] = {
    {"reset",    (PyCFunction)NCOObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"steps_u32", (PyCFunction)NCOObj_steps_u32, METH_VARARGS,
     "steps_u32(n=1) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import NCO\n"
     "    >>> obj = NCO(0.0, 0)\n"
     "    >>> y = obj.steps_u32(4)\n"
     "    >>> y.dtype\n"
     "    dtype('uint32')\n"},
    {"steps_u32_scaled", (PyCFunction)NCOObj_steps_u32_scaled, METH_VARARGS,
     "steps_u32_scaled(n=1) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import NCO\n"
     "    >>> obj = NCO(0.0, 0)\n"
     "    >>> y = obj.steps_u32_scaled(4)\n"
     "    >>> y.dtype\n"
     "    dtype('uint32')\n"},
    {"steps_u32_ovf", (PyCFunction)NCOObj_steps_u32_ovf, METH_VARARGS,
     "steps_u32_ovf(n=1) -> tuple[ndarray, ndarray]\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import NCO\n"
     "    >>> obj = NCO(0.0, 0)\n"
     "    >>> y = obj.steps_u32_ovf(4)\n"
     "    >>> y[0].dtype\n"
     "    dtype('uint32')\n"},
    {"destroy",  (PyCFunction)NCOObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)NCOObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)NCOObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject NCOObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "source.NCO",
    .tp_basicsize = sizeof(NCOObject),
    .tp_dealloc   = (destructor)NCOObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "NCO type.",
    .tp_methods   = NCOObj_methods,
    .tp_getset    = NCO_getset,
    .tp_new       = NCOObj_new,
    .tp_init      = (initproc)NCOObj_init,
};
