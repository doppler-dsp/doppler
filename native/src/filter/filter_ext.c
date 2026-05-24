/*
 * filter_ext.c — Python extension module filter
 *
 * Objects: FIR, CIC
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>
#include "cic/cic_core.h"

/* ======================================================== */
/* FIRObject — wraps fir_state_t *       */
/* ======================================================== */

#include "fir/fir_core.h"

typedef struct {
    PyObject_HEAD
    fir_state_t *handle;
} FIRObject;

static void
FIRObj_dealloc(FIRObject *self)
{
    if (self->handle)
        fir_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
FIRObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FIRObject *self = (FIRObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
FIRObj_init(FIRObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"taps", NULL};
    PyObject *taps_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
                                     &taps_obj))
        return -1;
    /* Use real-tap fast path when taps are float32. */
    PyArrayObject *check = (PyArrayObject *)PyArray_CheckFromAny(
        taps_obj, NULL, 0, 0, NPY_ARRAY_C_CONTIGUOUS, NULL);
    int is_real = check && (PyArray_TYPE(check) == NPY_FLOAT32);
    Py_XDECREF(check);
    PyErr_Clear();
    if (is_real) {
        PyArrayObject *taps_arr = (PyArrayObject *)PyArray_FROM_OTF(
            taps_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
        if (!taps_arr) { return -1; }
        size_t taps_len = (size_t)PyArray_SIZE(taps_arr);
        self->handle = fir_create_real(
            (const float *)PyArray_DATA(taps_arr), taps_len);
        Py_DECREF(taps_arr);
    } else {
        PyArrayObject *taps_arr = (PyArrayObject *)PyArray_FROM_OTF(
            taps_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
        if (!taps_arr) { return -1; }
        size_t taps_len = (size_t)PyArray_SIZE(taps_arr);
        self->handle = fir_create(
            (const float complex *)PyArray_DATA(taps_arr), taps_len);
        Py_DECREF(taps_arr);
    }
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "fir_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
FIRObj_reset(FIRObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    fir_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
FIRObj_execute(FIRObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!in_arr) return NULL;
    npy_intp n = PyArray_SIZE(in_arr);
    PyObject *out_arr = PyArray_SimpleNew(1, &n, NPY_COMPLEX64);
    if (!out_arr) { Py_DECREF(in_arr); return NULL; }
    size_t n_out = fir_execute(self->handle,
        (const float complex *)PyArray_DATA(in_arr), (size_t)n,
        (float complex *)PyArray_DATA((PyArrayObject *)out_arr));
    Py_DECREF(in_arr);
    if ((npy_intp)n_out == n)
        return out_arr;
    npy_intp actual = (npy_intp)n_out;
    PyArray_Dims ds = {&actual, 1};
    PyObject *result = PyArray_Newshape(
        (PyArrayObject *)out_arr, &ds, NPY_CORDER);
    Py_DECREF(out_arr);
    return result;
}
static PyObject *
FIR_getprop_num_taps(FIRObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)self->handle->num_taps);
}
static PyObject *
FIR_getprop_is_real(FIRObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyBool_FromLong((long)(fir_get_is_real(self->handle)));
}

static PyGetSetDef FIR_getset[] = {
    { "num_taps", (getter)FIR_getprop_num_taps, NULL, NULL, NULL },
    { "is_real", (getter)FIR_getprop_is_real, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
FIRObj_destroy(FIRObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        fir_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
FIRObj_enter(FIRObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
FIRObj_exit(FIRObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        fir_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef FIRObj_methods[] = {
    {"reset",    (PyCFunction)FIRObj_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)FIRObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import FIR\n"
     "    >>> obj = FIR(np.zeros(1, dtype=np.complex64))\n"
     "    >>> y = obj.execute(1.0 + 0.0j)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"destroy",  (PyCFunction)FIRObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)FIRObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)FIRObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject FIRObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "filter.FIR",
    .tp_basicsize = sizeof(FIRObject),
    .tp_dealloc   = (destructor)FIRObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "FIR type.",
    .tp_methods   = FIRObj_methods,
    .tp_getset    = FIR_getset,
    .tp_new       = FIRObj_new,
    .tp_init      = (initproc)FIRObj_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef filter_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "filter",
    .m_doc     = "Filter module.",
    .m_size    = -1,
    .m_methods = NULL,
};

/* ======================================================== */
/* CICObject — wraps cic_state_t *                          */
/* ======================================================== */

typedef struct {
    PyObject_HEAD
    cic_state_t *handle;
    float complex *_decimate_buf;
} CICObject;

static void
CICObj_dealloc(CICObject *self)
{
    cic_destroy(self->handle);
    free(self->_decimate_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
CICObj_new(PyTypeObject *type, PyObject *Py_UNUSED(args),
           PyObject *Py_UNUSED(kwds))
{
    CICObject *self = (CICObject *)type->tp_alloc(type, 0);
    if (self) { self->handle = NULL; self->_decimate_buf = NULL; }
    return (PyObject *)self;
}

static int
CICObj_init(CICObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"R", "N", "M", NULL};
    unsigned int R = 1, N = 4, M = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|III", kwlist, &R, &N, &M))
        return -1;
    self->handle = cic_create((uint32_t)R, (uint32_t)N, (uint32_t)M);
    if (!self->handle) {
        PyErr_SetString(PyExc_ValueError,
                        "cic_create failed: invalid R/N/M or OOM");
        return -1;
    }
    return 0;
}

static PyObject *
CICObj_reset(CICObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    cic_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
CICObj_decimate(CICObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &x_obj)) return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;
    size_t n_in = (size_t)PyArray_SIZE(x_arr);
    /* Lazy-allocate output buffer sized to n_in (always >= n_in/R). */
    if (!self->_decimate_buf) {
        self->_decimate_buf = malloc(n_in * sizeof(float complex));
        if (!self->_decimate_buf) {
            Py_DECREF(x_arr); PyErr_NoMemory(); return NULL;
        }
    }
    size_t n_out = cic_decimate(self->handle,
                                (const float complex *)PyArray_DATA(x_arr),
                                n_in, self->_decimate_buf);
    Py_DECREF(x_arr);
    npy_intp dim = (npy_intp)n_out;
    /* Zero-copy view into the pre-allocated buffer; lifetime tied to self. */
    PyObject *arr = PyArray_SimpleNewFromData(
        1, &dim, NPY_COMPLEX64, self->_decimate_buf);
    if (!arr) return NULL;
    PyArray_SetBaseObject((PyArrayObject *)arr, (PyObject *)self);
    Py_INCREF(self);
    return arr;
}

static PyObject *
CICObj_reconfigure(CICObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    unsigned int R, N, M;
    if (!PyArg_ParseTuple(args, "III", &R, &N, &M)) return NULL;
    cic_reconfigure(self->handle, (uint32_t)R, (uint32_t)N, (uint32_t)M);
    Py_RETURN_NONE;
}

static PyObject *
CICObj_destroy(CICObject *self, PyObject *Py_UNUSED(ignored))
{
    cic_destroy(self->handle);
    self->handle = NULL;
    free(self->_decimate_buf);
    self->_decimate_buf = NULL;
    Py_RETURN_NONE;
}

static PyObject *
CICObj_enter(CICObject *self, PyObject *Py_UNUSED(ignored))
    { Py_INCREF(self); return (PyObject *)self; }

static PyObject *
CICObj_exit(CICObject *self, PyObject *Py_UNUSED(args))
{
    cic_destroy(self->handle);
    self->handle = NULL;
    free(self->_decimate_buf);
    self->_decimate_buf = NULL;
    Py_RETURN_FALSE;
}

/* Properties — direct struct field reads */
static PyObject *
CIC_getprop_R(CICObject *self, void *Py_UNUSED(c))
    { return PyLong_FromUnsignedLong(self->handle->R); }
static PyObject *
CIC_getprop_N(CICObject *self, void *Py_UNUSED(c))
    { return PyLong_FromUnsignedLong(self->handle->N); }
static PyObject *
CIC_getprop_M(CICObject *self, void *Py_UNUSED(c))
    { return PyLong_FromUnsignedLong(self->handle->M); }
static PyObject *
CIC_getprop_input_scale(CICObject *self, void *Py_UNUSED(c))
    { return PyFloat_FromDouble(self->handle->input_scale); }
static PyObject *
CIC_getprop_output_scale(CICObject *self, void *Py_UNUSED(c))
    { return PyFloat_FromDouble(self->handle->output_scale); }

static PyGetSetDef CIC_getset[] = {
    {"R",            (getter)CIC_getprop_R,            NULL, NULL, NULL},
    {"N",            (getter)CIC_getprop_N,            NULL, NULL, NULL},
    {"M",            (getter)CIC_getprop_M,            NULL, NULL, NULL},
    {"input_scale",  (getter)CIC_getprop_input_scale,  NULL, NULL, NULL},
    {"output_scale", (getter)CIC_getprop_output_scale, NULL, NULL, NULL},
    {NULL}
};

static PyMethodDef CICObj_methods[] = {
    {"reset",       (PyCFunction)CICObj_reset,       METH_NOARGS,  "reset"},
    {"decimate",    (PyCFunction)CICObj_decimate,    METH_VARARGS, "decimate"},
    {"reconfigure", (PyCFunction)CICObj_reconfigure, METH_VARARGS, "reconfigure"},
    {"destroy",     (PyCFunction)CICObj_destroy,     METH_NOARGS,  "destroy"},
    {"__enter__",   (PyCFunction)CICObj_enter,       METH_NOARGS,  "__enter__"},
    {"__exit__",    (PyCFunction)CICObj_exit,        METH_VARARGS, "__exit__"},
    {NULL}
};

static PyTypeObject CICObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "filter.CIC",
    .tp_basicsize = sizeof(CICObject),
    .tp_dealloc   = (destructor)CICObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "CIC decimation filter.",
    .tp_methods   = CICObj_methods,
    .tp_getset    = CIC_getset,
    .tp_new       = CICObj_new,
    .tp_init      = (initproc)CICObj_init,
};

PyMODINIT_FUNC
PyInit_filter(void)
{
    import_array();
    if (PyType_Ready(&FIRObjType) < 0) return NULL;
    if (PyType_Ready(&CICObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&filter_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FIRObjType);
    if (PyModule_AddObject(m, "FIR", (PyObject *)&FIRObjType) < 0) {
        Py_DECREF(&FIRObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CICObjType);
    if (PyModule_AddObject(m, "CIC", (PyObject *)&CICObjType) < 0) {
        Py_DECREF(&CICObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
