/*
 * agc_ext_agc.c — AGC type for the agc module.
 *
 * Included by agc_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only agc_ext.c is compiled.
 */
/* ======================================================== */
/* AGCObject — wraps agc_state_t *       */
/* ======================================================== */

#include "agc/agc_core.h"

typedef struct {
    PyObject_HEAD
    agc_state_t *handle;
} AGCObject;

static void
AGCObj_dealloc(AGCObject *self)
{
    if (self->handle)
        agc_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
AGCObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AGCObject *self = (AGCObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
AGCObj_init(AGCObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"ref_db", "loop_bw", "alpha", NULL};
    double ref_db = 0.0;
    double loop_bw = 0.0025;
    double alpha = 0.05;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ddd", kwlist,
                                     &ref_db, &loop_bw, &alpha))
        return -1;
    self->handle = agc_create(ref_db, loop_bw, alpha);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "agc_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
AGCObj_reset(AGCObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    agc_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
AGC_step(AGCObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_complex x_raw = {0.0, 0.0};
    if (!PyArg_ParseTuple(args, "D", &x_raw))
        return NULL;
    float complex x = (float)x_raw.real + (float)x_raw.imag * I;
    float complex y = agc_step(self->handle, x);
    return PyComplex_FromDoubles((double)crealf(y), (double)cimagf(y));
}

static PyObject *
AGC_steps(AGCObject *self, PyObject *args)
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
        in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr)
        return NULL;

    Py_ssize_t n = PyArray_SIZE(in_arr);

    if (out_obj && out_obj != Py_None) {
        PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
            out_obj, NPY_COMPLEX64,
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
        agc_steps(
            self->handle,
            (const float complex *)PyArray_DATA(in_arr),
            (float complex *)PyArray_DATA(out_arr),
            (size_t)n);
        Py_DECREF(in_arr);
        return (PyObject *)out_arr;
    }

    npy_intp dims[] = {n};
    PyObject *out_arr = PyArray_SimpleNew(1, dims, NPY_COMPLEX64);
    if (!out_arr) {
        Py_DECREF(in_arr);
        return NULL;
    }

    agc_steps(
        self->handle,
        (const float complex *)PyArray_DATA(in_arr),
        (float complex *)PyArray_DATA((PyArrayObject *)out_arr),
        (size_t)n);

    Py_DECREF(in_arr);
    return out_arr;
}



static PyObject *
AGC_getprop_gain_db(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->gain_db);
}
static PyObject *
AGC_getprop_applied_gain_db(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(agc_get_applied_gain_db(self->handle));
}
static PyObject *
AGC_getprop_ref_db(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->ref_db);
}
static int
AGC_setprop_ref_db(AGCObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    self->handle->ref_db = v;
    return 0;
}
static PyObject *
AGC_getprop_loop_bw(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->loop_bw);
}
static int
AGC_setprop_loop_bw(AGCObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    self->handle->loop_bw = v;
    return 0;
}
static PyObject *
AGC_getprop_alpha(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->alpha);
}
static int
AGC_setprop_alpha(AGCObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    self->handle->alpha = v;
    return 0;
}
static PyObject *
AGC_getprop_decim(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->decim);
}
static int
AGC_setprop_decim(AGCObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    unsigned long long v_raw = 0ULL;
    if (!PyArg_Parse(value, "K", &v_raw)) return -1;
    size_t v = (size_t)v_raw;
    self->handle->decim = v;
    return 0;
}
static PyObject *
AGC_getprop_clip_db(AGCObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(self->handle->clip_db);
}
static int
AGC_setprop_clip_db(AGCObject *self, PyObject *value, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return -1;
    }
    double v = 0.0;
    if (!PyArg_Parse(value, "d", &v)) return -1;
    self->handle->clip_db = v;
    return 0;
}

static PyGetSetDef AGC_getset[] = {
    { "gain_db", (getter)AGC_getprop_gain_db, NULL, "Gain db.\n", NULL },
    { "applied_gain_db", (getter)AGC_getprop_applied_gain_db, NULL, "Gain actually applied to the most recent sample, in dB.\n", NULL },
    { "ref_db", (getter)AGC_getprop_ref_db, (setter)AGC_setprop_ref_db, "Ref db.\n", NULL },
    { "loop_bw", (getter)AGC_getprop_loop_bw, (setter)AGC_setprop_loop_bw, "Loop bw.\n", NULL },
    { "alpha", (getter)AGC_getprop_alpha, (setter)AGC_setprop_alpha, "Alpha.\n", NULL },
    { "decim", (getter)AGC_getprop_decim, (setter)AGC_setprop_decim, "Decim.\n", NULL },
    { "clip_db", (getter)AGC_getprop_clip_db, (setter)AGC_setprop_clip_db, "Clip db.\n", NULL },
    { NULL }
};

static PyObject *
AGCObj_destroy(AGCObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        agc_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
AGCObj_enter(AGCObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
AGCObj_exit(AGCObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        agc_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef AGCObj_methods[] = {
    {"reset",    (PyCFunction)AGCObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)AGC_step,     METH_VARARGS,
     "step(x) -> float complex\n"
     "\n"
     "Process one input sample.\n"
     "\n"
     "    >>> from doppler import AGC\n"
     "    >>> obj = AGC(0.0, 0.0025, 0.05)\n"
     "    >>> obj.step(1.0 + 0.0j)\n"
     "    0j\n"},
    {"steps",    (PyCFunction)AGC_steps,    METH_VARARGS,
     "steps(x[, out]) -> ndarray\n"
     "\n"
     "Process a block of samples in batch.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import AGC\n"
     "    >>> obj = AGC(0.0, 0.0025, 0.05)\n"
     "    >>> y = obj.steps(np.zeros(4, dtype=np.complex64))\n"
     "    >>> y.shape\n"
     "    (4,)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},

    {"destroy",  (PyCFunction)AGCObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)AGCObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)AGCObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject AGCObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "agc.AGC",
    .tp_basicsize = sizeof(AGCObject),
    .tp_dealloc   = (destructor)AGCObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Create an AGC instance.\n",
    .tp_methods   = AGCObj_methods,
    .tp_getset    = AGC_getset,
    .tp_new       = AGCObj_new,
    .tp_init      = (initproc)AGCObj_init,
};
