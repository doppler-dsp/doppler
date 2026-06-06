/*
 * wfmgen_ext_synth.c — Synth type for the wfmgen module.
 *
 * Included by wfmgen_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfmgen_ext.c is compiled.
 */
/* ======================================================== */
/* SynthObject — wraps synth_state_t *       */
/* ======================================================== */

#include "synth/synth_core.h"

typedef struct {
    PyObject_HEAD
    synth_state_t *handle;
} SynthObject;

static void
SynthObj_dealloc(SynthObject *self)
{
    if (self->handle)
        synth_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
SynthObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    SynthObject *self = (SynthObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
SynthObj_init(SynthObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"type", "fs", "freq_offset", "snr_db", "seed", NULL};
    int type = 0;
    double fs = 1000000.0;
    double freq_offset = 0.0;
    double snr_db = 100.0;
    unsigned long seed_raw = 0UL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|idddk", kwlist,
                                     &type, &fs, &freq_offset, &snr_db, &seed_raw))
        return -1;
    uint32_t seed = (uint32_t)seed_raw;
    self->handle = synth_create(type, fs, freq_offset, snr_db, seed);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "synth_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
SynthObj_reset(SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    synth_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
Synth_step(SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float complex y = synth_step(self->handle);
    return PyComplex_FromDoubles((double)crealf(y), (double)cimagf(y));
}

static PyObject *
Synth_steps(SynthObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = 1;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;

    npy_intp dims[] = {n};
    PyObject *out_arr = PyArray_SimpleNew(1, dims, NPY_COMPLEX64);
    if (!out_arr)
        return NULL;

    synth_steps(
        self->handle,
        (float complex *)PyArray_DATA((PyArrayObject *)out_arr),
        (size_t)n);

    return out_arr;
}




static PyObject *
SynthObj_destroy(SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        synth_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
SynthObj_enter(SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
SynthObj_exit(SynthObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        synth_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef SynthObj_methods[] = {
    {"reset",    (PyCFunction)SynthObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)Synth_step,     METH_NOARGS,
     "step() -> float complex\n"
     "\n"
     "Generate one output sample from internal state.\n"
     "\n"
     "    >>> from doppler import Synth\n"
     "    >>> obj = Synth(0, 1000000.0, 0.0, 100.0, 1)\n"
     "    >>> obj.step()\n"
     "    0j\n"},
    {"steps",    (PyCFunction)Synth_steps,    METH_VARARGS,
     "steps(n=1) -> ndarray\n"
     "\n"
     "Generate n output samples.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Synth\n"
     "    >>> obj = Synth(0, 1000000.0, 0.0, 100.0, 1)\n"
     "    >>> y = obj.steps(4)\n"
     "    >>> y.shape\n"
     "    (4,)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},

    {"destroy",  (PyCFunction)SynthObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)SynthObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)SynthObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject SynthObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "wfmgen.Synth",
    .tp_basicsize = sizeof(SynthObject),
    .tp_dealloc   = (destructor)SynthObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Synth type.\n",
    .tp_methods   = SynthObj_methods,
    .tp_new       = SynthObj_new,
    .tp_init      = (initproc)SynthObj_init,
};
