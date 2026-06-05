/*
 * ddc_ext_ddcr.c — DDCR type for the ddc module.
 *
 * Included by ddc_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only ddc_ext.c is compiled.
 */
/* ======================================================== */
/* DDCRObject — wraps ddcr_state_t *       */
/* ======================================================== */

#include "ddcr/ddcr_core.h"

typedef struct {
    PyObject_HEAD
    ddcr_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
    size_t _execute_buf_cap;  /* allocated capacity for execute */
} DDCRObject;

static void
DDCRObj_dealloc(DDCRObject *self)
{
    if (self->handle)
        ddcr_destroy(self->handle);
    free(self->_execute_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
DDCRObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DDCRObject *self = (DDCRObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
DDCRObj_init(DDCRObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"norm_freq", "rate", NULL};
    double norm_freq = 0.0;
    double rate = 0.25;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|dd", kwlist,
                                     &norm_freq, &rate))
        return -1;
    self->handle = ddcr_create(norm_freq, rate);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "ddcr_create returned NULL");
        return -1;
    }
    {
        size_t _max = ddcr_execute_max_out(self->handle);
        if (_max) {
        self->_execute_buf = malloc(_max * sizeof(float complex));
        if (!self->_execute_buf) { PyErr_NoMemory(); return -1; }
            self->_execute_buf_cap = _max;
        }
    }
    return 0;
}








static PyObject *
DDCRObj_execute(DDCRObject *self, PyObject *args)
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
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;
    size_t _need = (size_t)PyArray_SIZE(x_arr);
    if (!self->_execute_buf || self->_execute_buf_cap < _need) {
        size_t _max = ddcr_execute_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        float complex *_tmp = realloc(self->_execute_buf, _max * sizeof(float complex));
        if (!_tmp) { Py_DECREF(x_arr); PyErr_NoMemory(); return NULL; }
        self->_execute_buf = _tmp;
        self->_execute_buf_cap = _max;
    }
    size_t n_out = ddcr_execute(self->handle, (const float *)PyArray_DATA(x_arr), (size_t)PyArray_SIZE(x_arr), self->_execute_buf, self->_execute_buf_cap);
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
DDCRObj_reset(DDCRObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    ddcr_reset(self->handle);
    Py_RETURN_NONE;
}
static PyObject *
DDCR_getprop_norm_freq(DDCRObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(ddcr_get_norm_freq(self->handle));
}
static int
DDCR_setprop_norm_freq(DDCRObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    ddcr_set_norm_freq(self->handle, v);
    return 0;
}
static PyObject *
DDCR_getprop_rate(DDCRObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(ddcr_get_rate(self->handle));
}

static PyGetSetDef DDCR_getset[] = {
    { "norm_freq", (getter)DDCR_getprop_norm_freq, (setter)DDCR_setprop_norm_freq, NULL, NULL },
    { "rate", (getter)DDCR_getprop_rate, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
DDCRObj_destroy(DDCRObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        ddcr_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
DDCRObj_enter(DDCRObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
DDCRObj_exit(DDCRObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        ddcr_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef DDCRObj_methods[] = {

    {"execute", (PyCFunction)DDCRObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Halfband R2C decimate, LO mix, then rate-convert.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import DDCR\n"
     "    >>> obj = DDCR(0.0, 0.25)\n"
     "    >>> y = obj.execute(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"reset", (PyCFunction)DDCRObj_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "Zero halfband, LO phase, and filter history.\n"
     "\n"
     "    >>> from doppler import DDCR\n"
     "    >>> obj = DDCR(0.0, 0.25)\n"
     "    >>> obj.reset()\n"},
    {"destroy",  (PyCFunction)DDCRObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)DDCRObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)DDCRObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject DDCRObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "ddc.DDCR",
    .tp_basicsize = sizeof(DDCRObject),
    .tp_dealloc   = (destructor)DDCRObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "DDCR type.",
    .tp_methods   = DDCRObj_methods,
    .tp_getset    = DDCR_getset,
    .tp_new       = DDCRObj_new,
    .tp_init      = (initproc)DDCRObj_init,
};
