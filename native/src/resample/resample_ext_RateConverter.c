/*
 * resample_ext_RateConverter.c — RateConverter type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* RateConverterObject — wraps RateConverter_state_t *       */
/* ======================================================== */

#include "RateConverter/RateConverter_core.h"

typedef struct {
    PyObject_HEAD
    RateConverter_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
    size_t _execute_buf_cap;  /* allocated capacity for execute */
} RateConverterObject;

static void
RateConverterObj_dealloc(RateConverterObject *self)
{
    if (self->handle)
        RateConverter_destroy(self->handle);
    free(self->_execute_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
RateConverterObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    RateConverterObject *self =
        (RateConverterObject *)type->tp_alloc(type, 0);
    if (self) {
        self->handle          = NULL;
        self->_execute_buf     = NULL;
        self->_execute_buf_cap = 0;
    }
    return (PyObject *)self;
}

static int
RateConverterObj_init(RateConverterObject *self, PyObject *args,
                      PyObject *kwds)
{
    static char *kwlist[] = {"rate", "compensate", NULL};
    double rate       = 1.0;
    int    compensate = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|di", kwlist,
                                     &rate, &compensate))
        return -1;
    if (self->handle) {
        RateConverter_destroy(self->handle);
        self->handle = NULL;
    }
    self->handle = RateConverter_create(rate, compensate);
    if (!self->handle) {
        PyErr_SetString(PyExc_ValueError,
                        "RateConverter_create returned NULL"
                        " (rate must be > 0)");
        return -1;
    }
    return 0;
}








static PyObject *
RateConverterObj_execute(RateConverterObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &x_obj))
        return NULL;

    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr)
        return NULL;

    size_t n_in = (size_t)PyArray_SIZE(x_arr);

    /* Compute required output capacity for this block size. */
    double rate  = RateConverter_get_rate(self->handle);
    double ratio = (rate > 1.0) ? rate : 1.0;
    size_t need  = (size_t)(n_in * ratio) + 4;

    if (need > self->_execute_buf_cap) {
        free(self->_execute_buf);
        self->_execute_buf = malloc(need * sizeof(float complex));
        if (!self->_execute_buf) {
            self->_execute_buf_cap = 0;
            Py_DECREF(x_arr);
            PyErr_NoMemory();
            return NULL;
        }
        self->_execute_buf_cap = need;
    }

    size_t n_out = RateConverter_execute(
        self->handle,
        (const float complex *)PyArray_DATA(x_arr),
        n_in,
        self->_execute_buf,
        self->_execute_buf_cap);

    npy_intp dim = (npy_intp)n_out;
    PyObject *out = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_execute_buf);
    if (!out) { Py_DECREF(x_arr); return NULL; }

    /* Keep self alive as long as the returned array holds a view into
     * _execute_buf — prevents use-after-free if the caller drops self. */
    PyArray_SetBaseObject((PyArrayObject *)out, (PyObject *)self);
    Py_INCREF(self);
    Py_DECREF(x_arr);
    return out;
}

static PyObject *
RateConverterObj_reset(RateConverterObject *self,
                       PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    RateConverter_reset(self->handle);
    Py_RETURN_NONE;
}
static PyObject *
RateConverter_getprop_rate(RateConverterObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(RateConverter_get_rate(self->handle));
}
static int
RateConverter_setprop_rate(RateConverterObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    RateConverter_set_rate(self->handle, v);
    return 0;
}

static PyGetSetDef RateConverter_getset[] = {
    { "rate", (getter)RateConverter_getprop_rate, (setter)RateConverter_setprop_rate, "Return the current rate ratio.\n", NULL },
    { NULL }
};

static PyObject *
RateConverterObj_destroy(RateConverterObject *self,
                         PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        RateConverter_destroy(self->handle);
        self->handle = NULL;
    }
    free(self->_execute_buf);
    self->_execute_buf     = NULL;
    self->_execute_buf_cap = 0;
    Py_RETURN_NONE;
}

static PyObject *
RateConverterObj_enter(RateConverterObject *self,
                       PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
RateConverterObj_exit(RateConverterObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        RateConverter_destroy(self->handle);
        self->handle = NULL;
    }
    free(self->_execute_buf);
    self->_execute_buf     = NULL;
    self->_execute_buf_cap = 0;
    Py_RETURN_NONE;
}

static PyMethodDef RateConverterObj_methods[] = {

    {"execute", (PyCFunction)RateConverterObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Convert n_in samples and write results to out.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import RateConverter\n"
     "    >>> obj = RateConverter(1.0, 0)\n"
     "    >>> y = obj.execute(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"reset", (PyCFunction)RateConverterObj_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "Zero all sub-stage filter memories.\n"
     "\n"
     "    >>> from doppler import RateConverter\n"
     "    >>> obj = RateConverter(1.0, 0)\n"
     "    >>> obj.reset()\n"},
    {"destroy",  (PyCFunction)RateConverterObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)RateConverterObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)RateConverterObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject RateConverterObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.RateConverter",
    .tp_basicsize = sizeof(RateConverterObject),
    .tp_dealloc   = (destructor)RateConverterObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create a rate converter for the given output/input rate ratio.\n",
    .tp_methods   = RateConverterObj_methods,
    .tp_getset    = RateConverter_getset,
    .tp_new       = RateConverterObj_new,
    .tp_init      = (initproc)RateConverterObj_init,
};
