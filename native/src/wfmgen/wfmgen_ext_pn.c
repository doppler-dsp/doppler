/*
 * wfmgen_ext_pn.c — PN type for the wfmgen module.
 *
 * Included by wfmgen_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfmgen_ext.c is compiled.
 */
/* ======================================================== */
/* PNObject — wraps pn_state_t *       */
/* ======================================================== */

#include "pn/pn_core.h"

typedef struct {
    PyObject_HEAD
    pn_state_t *handle;
    uint8_t *_generate_buf;  /* pre-allocated output for generate */
    size_t _generate_buf_cap;  /* allocated capacity for generate */
} PNObject;

static void
PNObj_dealloc(PNObject *self)
{
    if (self->handle)
        pn_destroy(self->handle);
    free(self->_generate_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
PNObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PNObject *self = (PNObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
PNObj_init(PNObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"poly", "seed", "length", "lfsr", NULL};
    unsigned long long poly_raw = 0ULL;
    unsigned long long seed_raw = 0ULL;
    unsigned long length_raw = 0UL;
    const char *lfsr_str = "galois";

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|KKks", kwlist,
                                     &poly_raw, &seed_raw, &length_raw, &lfsr_str))
        return -1;
    int lfsr = 0;
    if (strcmp(lfsr_str, "galois") == 0) lfsr = 0;
    else if (strcmp(lfsr_str, "fibonacci") == 0) lfsr = 1;
    else {
        PyErr_Format(PyExc_ValueError, "lfsr must be \"galois\" or \"fibonacci\", got '%s'", lfsr_str);
        return -1;
    }
    uint64_t poly = (uint64_t)poly_raw;
    uint64_t seed = (uint64_t)seed_raw;
    uint32_t length = (uint32_t)length_raw;
    self->handle = pn_create(poly, seed, length, lfsr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "pn_create returned NULL");
        return -1;
    }
    {
        size_t _max = pn_generate_max_out(self->handle);
        if (_max) {
        self->_generate_buf = malloc(_max * sizeof(uint8_t));
        if (!self->_generate_buf) { PyErr_NoMemory(); return -1; }
            self->_generate_buf_cap = _max;
        }
    }
    return 0;
}

static PyObject *
PNObj_reset(PNObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    pn_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
PNObj_generate(PNObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Py_ssize_t n = 1;
    if (!PyArg_ParseTuple(args, "|n", &n))
        return NULL;
    size_t _need = (size_t)n;
    if (!self->_generate_buf || self->_generate_buf_cap < _need) {
        size_t _max = pn_generate_max_out(self->handle);
        if (!_max || _max < _need) _max = _need;
        uint8_t *_tmp = realloc(self->_generate_buf, _max * sizeof(uint8_t));
        if (!_tmp) { PyErr_NoMemory(); return NULL; }
        self->_generate_buf = _tmp;
        self->_generate_buf_cap = _max;
    }
    size_t n_out = pn_generate(self->handle, (size_t)n, self->_generate_buf);
    npy_intp dim = (npy_intp)n_out;
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_UINT8, self->_generate_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
PNObj_destroy(PNObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        pn_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
PNObj_enter(PNObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
PNObj_exit(PNObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        pn_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef PNObj_methods[] = {
    {"reset",    (PyCFunction)PNObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"generate", (PyCFunction)PNObj_generate, METH_VARARGS,
     "generate(n=1) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import PN\n"
     "    >>> obj = PN(96, 1, 7)\n"
     "    >>> y = obj.generate(4)\n"
     "    >>> y.dtype\n"
     "    dtype('uint8')\n"},
    {"destroy",  (PyCFunction)PNObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)PNObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)PNObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject PNObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "wfmgen.PN",
    .tp_basicsize = sizeof(PNObject),
    .tp_dealloc   = (destructor)PNObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Allocate and initialise a maximal-length-sequence LFSR. The register is seeded from ``seed`` and will produce a pseudo-random binary sequence with period 2^length - 1 for any primitive ``poly``. Both Galois and Fibonacci realizations share the same primitive polynomial and therefore the same period; they differ only in chip ordering/phase.\n",
    .tp_methods   = PNObj_methods,
    .tp_new       = PNObj_new,
    .tp_init      = (initproc)PNObj_init,
};
