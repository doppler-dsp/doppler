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
Synth_dealloc(SynthObject *self)
{
    if (self->handle)
        synth_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Synth_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    SynthObject *self = (SynthObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Synth_init(SynthObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"type", "snr_mode", "fs", "freq", "snr", "seed", "sps", "pn_length", "pn_poly", "lfsr", NULL};
    const char *type_str = "tone";
    const char *snr_mode_str = "auto";
    double fs = 1000000.0;
    double freq = 0.0;
    double snr = 100.0;
    unsigned long seed_raw = 0UL;
    int sps = 8;
    int pn_length = 7;
    unsigned long long pn_poly_raw = 0ULL;
    const char *lfsr_str = "galois";

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ssdddkiiKs", kwlist,
                                     &type_str, &snr_mode_str, &fs, &freq, &snr, &seed_raw, &sps, &pn_length, &pn_poly_raw, &lfsr_str))
        return -1;
    int type = 0;
    if (strcmp(type_str, "tone") == 0) type = 0;
    else if (strcmp(type_str, "noise") == 0) type = 1;
    else if (strcmp(type_str, "pn") == 0) type = 2;
    else if (strcmp(type_str, "bpsk") == 0) type = 3;
    else if (strcmp(type_str, "qpsk") == 0) type = 4;
    else {
        PyErr_Format(PyExc_ValueError, "type must be one of \"tone\", \"noise\", \"pn\", \"bpsk\", \"qpsk\", got '%s'", type_str);
        return -1;
    }
    int snr_mode = 0;
    if (strcmp(snr_mode_str, "auto") == 0) snr_mode = 0;
    else if (strcmp(snr_mode_str, "fs") == 0) snr_mode = 1;
    else if (strcmp(snr_mode_str, "ebno") == 0) snr_mode = 2;
    else if (strcmp(snr_mode_str, "esno") == 0) snr_mode = 3;
    else {
        PyErr_Format(PyExc_ValueError, "snr_mode must be one of \"auto\", \"fs\", \"ebno\", \"esno\", got '%s'", snr_mode_str);
        return -1;
    }
    int lfsr = 0;
    if (strcmp(lfsr_str, "galois") == 0) lfsr = 0;
    else if (strcmp(lfsr_str, "fibonacci") == 0) lfsr = 1;
    else {
        PyErr_Format(PyExc_ValueError, "lfsr must be \"galois\" or \"fibonacci\", got '%s'", lfsr_str);
        return -1;
    }
    uint32_t seed = (uint32_t)seed_raw;
    uint64_t pn_poly = (uint64_t)pn_poly_raw;
    self->handle = synth_create(type, fs, freq, snr, snr_mode, seed, sps, pn_length, pn_poly, lfsr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "synth_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
Synth_reset(SynthObject *self, PyObject *Py_UNUSED(ignored))
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
Synth_get_wtype(
    SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLong((long)synth_get_wtype(self->handle));
}

static PyObject *
Synth_set_wtype(
    SynthObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    int v = 0;
    if (!PyArg_ParseTuple(args, "i", &v))
        return NULL;
    synth_set_wtype(self->handle, v);
    Py_RETURN_NONE;
}

static PyObject *
Synth_get_nsps(
    SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLong((long)synth_get_nsps(self->handle));
}

static PyObject *
Synth_set_nsps(
    SynthObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    int v = 0;
    if (!PyArg_ParseTuple(args, "i", &v))
        return NULL;
    synth_set_nsps(self->handle, v);
    Py_RETURN_NONE;
}

static PyObject *
Synth_get_sym_pos(
    SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromLong((long)synth_get_sym_pos(self->handle));
}

static PyObject *
Synth_set_sym_pos(
    SynthObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    int v = 0;
    if (!PyArg_ParseTuple(args, "i", &v))
        return NULL;
    synth_set_sym_pos(self->handle, v);
    Py_RETURN_NONE;
}

static PyObject *
Synth_get_cur_re(
    SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble((double)synth_get_cur_re(self->handle));
}

static PyObject *
Synth_set_cur_re(
    SynthObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float v = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &v))
        return NULL;
    synth_set_cur_re(self->handle, v);
    Py_RETURN_NONE;
}

static PyObject *
Synth_get_cur_im(
    SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble((double)synth_get_cur_im(self->handle));
}

static PyObject *
Synth_set_cur_im(
    SynthObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    float v = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &v))
        return NULL;
    synth_set_cur_im(self->handle, v);
    Py_RETURN_NONE;
}


static PyObject *
Synth_destroy(SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        synth_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Synth_enter(SynthObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Synth_exit(SynthObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        synth_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Synth_methods[] = {
    {"reset",    (PyCFunction)Synth_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},
    {"step",     (PyCFunction)Synth_step,     METH_NOARGS,
     "step() -> float complex\n"
     "\n"
     "Generate one output sample from internal state.\n"
     "\n"
     "    >>> from doppler import Synth\n"
     "    >>> obj = Synth(\"tone\", \"auto\", 1000000.0, 0.0, 100.0, 1, 8, 7, 0)\n"
     "    >>> obj.step()\n"
     "    0j\n"},
    {"steps",    (PyCFunction)Synth_steps,    METH_VARARGS,
     "steps(n=1) -> ndarray\n"
     "\n"
     "Generate n output samples.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Synth\n"
     "    >>> obj = Synth(\"tone\", \"auto\", 1000000.0, 0.0, 100.0, 1, 8, 7, 0)\n"
     "    >>> y = obj.steps(4)\n"
     "    >>> y.shape\n"
     "    (4,)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},

    {"get_wtype",
     (PyCFunction)Synth_get_wtype, METH_NOARGS,
     "Get wtype."},
    {"set_wtype",
     (PyCFunction)Synth_set_wtype, METH_VARARGS,
     "Set wtype."},
    {"get_nsps",
     (PyCFunction)Synth_get_nsps, METH_NOARGS,
     "Get nsps."},
    {"set_nsps",
     (PyCFunction)Synth_set_nsps, METH_VARARGS,
     "Set nsps."},
    {"get_sym_pos",
     (PyCFunction)Synth_get_sym_pos, METH_NOARGS,
     "Get sym_pos."},
    {"set_sym_pos",
     (PyCFunction)Synth_set_sym_pos, METH_VARARGS,
     "Set sym_pos."},
    {"get_cur_re",
     (PyCFunction)Synth_get_cur_re, METH_NOARGS,
     "Get cur_re."},
    {"set_cur_re",
     (PyCFunction)Synth_set_cur_re, METH_VARARGS,
     "Set cur_re."},
    {"get_cur_im",
     (PyCFunction)Synth_get_cur_im, METH_NOARGS,
     "Get cur_im."},
    {"set_cur_im",
     (PyCFunction)Synth_set_cur_im, METH_VARARGS,
     "Set cur_im."},
    {"destroy",  (PyCFunction)Synth_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Synth_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Synth_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject SynthType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "wfmgen.Synth",
    .tp_basicsize = sizeof(SynthObject),
    .tp_dealloc   = (destructor)Synth_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source.  One call to synth_step() or synth_steps() advances all sub-components in lock-step. SNR >= SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead.  When ``snr_mode`` is \"auto\" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN.\n",
    .tp_methods   = Synth_methods,
    .tp_new       = Synth_new,
    .tp_init      = (initproc)Synth_init,
};
