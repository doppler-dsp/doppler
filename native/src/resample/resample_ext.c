/*
 * resample_ext.c — Python extension module resample
 *
 * Objects: Resampler, Halfbanddecimator
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "resample/resample_core.h"
static PyObject *
_bind_kaiser_beta(PyObject *self, PyObject *args)
{
    (void)self;
    double atten = 0.0;
    if (!PyArg_ParseTuple(args, "d", &atten))
        return NULL;
    return PyFloat_FromDouble(kaiser_beta(atten));
}

static PyObject *
_bind_kaiser_num_taps(PyObject *self, PyObject *args)
{
    (void)self;
    int num_phases = 0;
    double atten = 0.0;
    double pb = 0.0;
    double sb = 0.0;
    if (!PyArg_ParseTuple(args, "iddd", &num_phases, &atten, &pb, &sb))
        return NULL;
    return PyLong_FromLong((long)kaiser_num_taps(num_phases, atten, pb, sb));
}

/* ======================================================== */
/* ResamplerObject — wraps Resampler_state_t *       */
/* ======================================================== */

#include "Resampler/Resampler_core.h"
#include "resamp/resamp_core.h"

typedef struct {
    PyObject_HEAD
    Resampler_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
    float complex *_execute_ctrl_buf;  /* pre-allocated output for execute_ctrl */
} ResamplerObject;

static void
ResamplerObj_dealloc(ResamplerObject *self)
{
    if (self->handle)
        Resampler_destroy(self->handle);
    free(self->_execute_buf);
    free(self->_execute_ctrl_buf);
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
            PyErr_SetString(PyExc_ValueError, "bank must be 2-D");
            Py_DECREF(bank_arr);
            return -1;
        }
        size_t num_phases = (size_t)PyArray_DIM(bank_arr, 0);
        size_t num_taps   = (size_t)PyArray_DIM(bank_arr, 1);
        self->handle = resamp_create_custom(num_phases, num_taps,
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
    self->_execute_buf = malloc(
        Resampler_execute_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_buf) { PyErr_NoMemory(); return -1; }
    self->_execute_ctrl_buf = malloc(
        Resampler_execute_ctrl_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_ctrl_buf) { PyErr_NoMemory(); return -1; }
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
        PyErr_SetString(PyExc_ValueError,
                        "ctrl must be at least as long as x");
        Py_DECREF(x_arr); Py_DECREF(ctrl_arr);
        return NULL;
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
    { "rate", (getter)Resampler_getprop_rate, (setter)Resampler_setprop_rate, NULL, NULL },
    { "num_phases", (getter)Resampler_getprop_num_phases, NULL, NULL, NULL },
    { "num_taps", (getter)Resampler_getprop_num_taps, NULL, NULL, NULL },
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
     "Zero-copy view into pre-allocated output buffer.\n"
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
     "Zero-copy view into pre-allocated output buffer.\n"
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
     "reset.\n"
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
    .tp_doc       = "Resampler type.",
    .tp_methods   = ResamplerObj_methods,
    .tp_getset    = Resampler_getset,
    .tp_new       = ResamplerObj_new,
    .tp_init      = (initproc)ResamplerObj_init,
};
/* ======================================================== */
/* HalfbanddecimatorObject — wraps HalfbandDecimator_state_t *       */
/* ======================================================== */

#include "HalfbandDecimator/HalfbandDecimator_core.h"
#include "hbdecim/hbdecim_core.h"
#include "hbdecim/hbdecim_r2c_core.h"

typedef struct {
    PyObject_HEAD
    HalfbandDecimator_state_t *handle;
} HalfbanddecimatorObject;

static void
HalfbanddecimatorObj_dealloc(HalfbanddecimatorObject *self)
{
    if (self->handle)
        HalfbandDecimator_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
HalfbanddecimatorObj_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HalfbanddecimatorObject *self = (HalfbanddecimatorObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
HalfbanddecimatorObj_init(HalfbanddecimatorObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"h", NULL};
    PyObject *h_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
                                     &h_obj))
        return -1;
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!h_arr) { return -1; }
    if (PyArray_NDIM(h_arr) != 1) {
        PyErr_SetString(PyExc_ValueError, "h must be 1-D");
        Py_DECREF(h_arr);
        return -1;
    }
    size_t h_len = (size_t)PyArray_SIZE(h_arr);
    self->handle = HalfbandDecimator_create(
        h_len, (const float *)PyArray_DATA(h_arr));
    Py_DECREF(h_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "HalfbandDecimator_create returned NULL");
        return -1;
    }
    return 0;
}








static PyObject *
HalfbanddecimatorObj_execute(HalfbanddecimatorObject *self, PyObject *args)
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
    npy_intp n_in = PyArray_SIZE(x_arr);
    npy_intp alloc = n_in / 2 + 1;
    PyObject *out_arr = PyArray_SimpleNew(1, &alloc, NPY_COMPLEX64);
    if (!out_arr) { Py_DECREF(x_arr); return NULL; }
    size_t n_out = HalfbandDecimator_execute(self->handle,
        (const float complex *)PyArray_DATA(x_arr), (size_t)n_in,
        (float complex *)PyArray_DATA((PyArrayObject *)out_arr));
    Py_DECREF(x_arr);
    npy_intp actual = (npy_intp)n_out;
    if (actual == alloc)
        return out_arr;
    PyObject *result = PyArray_SimpleNew(1, &actual, NPY_COMPLEX64);
    if (!result) { Py_DECREF(out_arr); return NULL; }
    memcpy(PyArray_DATA((PyArrayObject *)result),
           PyArray_DATA((PyArrayObject *)out_arr),
           n_out * sizeof(float complex));
    Py_DECREF(out_arr);
    return result;
}

static PyObject *
HalfbanddecimatorObj_reset(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    HalfbandDecimator_reset(self->handle);
    Py_RETURN_NONE;
}
static PyObject *
Halfbanddecimator_getprop_rate(HalfbanddecimatorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyFloat_FromDouble(HalfbandDecimator_get_rate(self->handle));
}
static PyObject *
Halfbanddecimator_getprop_num_taps(HalfbanddecimatorObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    /* <<IMPLEMENT: return the computed or stored value>> */
    return PyLong_FromUnsignedLongLong((unsigned long long)HalfbandDecimator_get_num_taps(self->handle));
}

static PyGetSetDef Halfbanddecimator_getset[] = {
    { "rate", (getter)Halfbanddecimator_getprop_rate, NULL, NULL, NULL },
    { "num_taps", (getter)Halfbanddecimator_getprop_num_taps, NULL, NULL, NULL },
    { NULL }
};

static PyObject *
HalfbanddecimatorObj_destroy(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        HalfbandDecimator_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
HalfbanddecimatorObj_enter(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
HalfbanddecimatorObj_exit(HalfbanddecimatorObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        HalfbandDecimator_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef HalfbanddecimatorObj_methods[] = {

    {"execute", (PyCFunction)HalfbanddecimatorObj_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Halfbanddecimator\n"
     "    >>> obj = Halfbanddecimator(np.zeros(1, dtype=np.float32))\n"
     "    >>> y = obj.execute(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"reset", (PyCFunction)HalfbanddecimatorObj_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "reset.\n"
     "\n"
     "    >>> from doppler import Halfbanddecimator\n"
     "    >>> obj = Halfbanddecimator(np.zeros(1, dtype=np.float32))\n"
     "    >>> obj.reset()\n"},
    {"destroy",  (PyCFunction)HalfbanddecimatorObj_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)HalfbanddecimatorObj_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)HalfbanddecimatorObj_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject HalfbanddecimatorObjType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.Halfbanddecimator",
    .tp_basicsize = sizeof(HalfbanddecimatorObject),
    .tp_dealloc   = (destructor)HalfbanddecimatorObj_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Halfbanddecimator type.",
    .tp_methods   = HalfbanddecimatorObj_methods,
    .tp_getset    = Halfbanddecimator_getset,
    .tp_new       = HalfbanddecimatorObj_new,
    .tp_init      = (initproc)HalfbanddecimatorObj_init,
};

/* ======================================================== */
/* HalfbandDecimatorDpObject — wraps hbdecim_state_t *      */
/* CF32→CF32, direct double-precision halfband decimator    */
/* ======================================================== */

typedef struct {
    PyObject_HEAD
    hbdecim_state_t *handle;
} HalfbandDecimatorDpObject;

static void
HalfbandDecimatorDp_dealloc(HalfbandDecimatorDpObject *self)
{
    if (self->handle)
        hbdecim_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
HalfbandDecimatorDp_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HalfbandDecimatorDpObject *self =
        (HalfbandDecimatorDpObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
HalfbandDecimatorDp_init(HalfbandDecimatorDpObject *self,
                         PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"h", NULL};
    PyObject *h_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &h_obj))
        return -1;
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT32,
        NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED | NPY_ARRAY_FORCECAST);
    if (!h_arr) return -1;
    if (PyArray_NDIM(h_arr) != 1) {
        Py_DECREF(h_arr);
        PyErr_SetString(PyExc_ValueError, "h must be a 1-D float32 array");
        return -1;
    }
    size_t num_taps = (size_t)PyArray_DIM(h_arr, 0);
    self->handle = hbdecim_create(num_taps,
                                  (const float *)PyArray_DATA(h_arr));
    Py_DECREF(h_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError, "hbdecim_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
HalfbandDecimatorDp_reset(HalfbandDecimatorDpObject *self,
                          PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    hbdecim_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
HbDecimDp_rate(HalfbandDecimatorDpObject *self, void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(hbdecim_get_rate(self->handle));
}

static PyObject *
HbDecimDp_num_taps(HalfbandDecimatorDpObject *self,
                   void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromSize_t(hbdecim_get_num_taps(self->handle));
}

static PyGetSetDef HalfbandDecimatorDp_getset[] = {
    {"rate", (getter)HbDecimDp_rate, NULL,
     "Output-to-input rate ratio.", NULL},
    {"num_taps", (getter)HbDecimDp_num_taps, NULL,
     "FIR branch tap count.", NULL},
    {NULL}
};

static PyObject *
HalfbandDecimatorDp_execute(HalfbandDecimatorDpObject *self, PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_COMPLEX64,
        NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
    if (!in_arr) return NULL;
    size_t num_in   = (size_t)PyArray_SIZE(in_arr);
    size_t num_taps = hbdecim_get_num_taps(self->handle);
    size_t max_out  = (num_in + 1) / 2 + num_taps + 2;
    npy_intp out_dim = (npy_intp)max_out;
    PyArrayObject *out_arr =
        (PyArrayObject *)PyArray_SimpleNew(1, &out_dim, NPY_COMPLEX64);
    if (!out_arr) { Py_DECREF(in_arr); return NULL; }
    size_t n = hbdecim_execute(
        self->handle,
        (const float _Complex *)PyArray_DATA(in_arr), num_in,
        (float _Complex *)PyArray_DATA(out_arr), max_out);
    Py_DECREF(in_arr);
    PyObject *sliced = PySequence_GetSlice(
        (PyObject *)out_arr, 0, (Py_ssize_t)n);
    Py_DECREF(out_arr);
    return sliced;
}

static PyObject *
HbDecimDp_enter(HalfbandDecimatorDpObject *self,
                PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
HbDecimDp_exit(HalfbandDecimatorDpObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        hbdecim_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorDp_methods[] = {
    {"reset",    (PyCFunction)HalfbandDecimatorDp_reset,   METH_NOARGS,  NULL},
    {"execute",  (PyCFunction)HalfbandDecimatorDp_execute, METH_VARARGS, NULL},
    {"__enter__",(PyCFunction)HbDecimDp_enter,             METH_NOARGS,  NULL},
    {"__exit__", (PyCFunction)HbDecimDp_exit,              METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject HalfbandDecimatorDpType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.HalfbandDecimatorDp",
    .tp_basicsize = sizeof(HalfbandDecimatorDpObject),
    .tp_dealloc   = (destructor)HalfbandDecimatorDp_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "HbDecimDp(h) — C-library halfband 2:1 decimator "
                    "(CF32→CF32).",
    .tp_methods   = HalfbandDecimatorDp_methods,
    .tp_getset    = HalfbandDecimatorDp_getset,
    .tp_new       = HalfbandDecimatorDp_new,
    .tp_init      = (initproc)HalfbandDecimatorDp_init,
};

/* ======================================================== */
/* HalfbandDecimatorR2CObject — wraps hbdecim_r2c_state_t * */
/* float32→CF32, embedded fs/4 mix (Architecture D2)       */
/* ======================================================== */

typedef struct {
    PyObject_HEAD
    hbdecim_r2c_state_t *handle;
} HalfbandDecimatorR2CObject;

static void
HalfbandDecimatorR2C_dealloc(HalfbandDecimatorR2CObject *self)
{
    if (self->handle)
        hbdecim_r2c_destroy(self->handle);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
HalfbandDecimatorR2C_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HalfbandDecimatorR2CObject *self =
        (HalfbandDecimatorR2CObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
HalfbandDecimatorR2C_init(HalfbandDecimatorR2CObject *self,
                          PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"h", NULL};
    PyObject *h_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &h_obj))
        return -1;
    PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF(
        h_obj, NPY_FLOAT32,
        NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED);
    if (!h_arr) return -1;
    if (PyArray_NDIM(h_arr) != 1) {
        Py_DECREF(h_arr);
        PyErr_SetString(PyExc_ValueError, "h must be a 1-D float32 array");
        return -1;
    }
    size_t num_taps = (size_t)PyArray_DIM(h_arr, 0);
    self->handle = hbdecim_r2c_create(num_taps,
                                      (const float *)PyArray_DATA(h_arr));
    Py_DECREF(h_arr);
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "hbdecim_r2c_create returned NULL");
        return -1;
    }
    return 0;
}

static PyObject *
HalfbandDecimatorR2C_reset(HalfbandDecimatorR2CObject *self,
                           PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    hbdecim_r2c_reset(self->handle);
    Py_RETURN_NONE;
}

static PyObject *
HbDecimR2C_rate(HalfbandDecimatorR2CObject *self,
                void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyFloat_FromDouble(hbdecim_r2c_get_rate(self->handle));
}

static PyObject *
HbDecimR2C_num_taps(HalfbandDecimatorR2CObject *self,
                    void *Py_UNUSED(closure))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    return PyLong_FromSize_t(hbdecim_r2c_get_num_taps(self->handle));
}

static PyGetSetDef HalfbandDecimatorR2C_getset[] = {
    {"rate", (getter)HbDecimR2C_rate, NULL,
     "Output-to-input rate ratio.", NULL},
    {"num_taps", (getter)HbDecimR2C_num_taps, NULL,
     "FIR branch tap count.", NULL},
    {NULL}
};

static PyObject *
HalfbandDecimatorR2C_execute(HalfbandDecimatorR2CObject *self,
                             PyObject *args)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    PyObject *in_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &in_obj))
        return NULL;
    PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF(
        in_obj, NPY_FLOAT32,
        NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED | NPY_ARRAY_FORCECAST);
    if (!in_arr) return NULL;
    size_t num_in   = (size_t)PyArray_SIZE(in_arr);
    size_t num_taps = hbdecim_r2c_get_num_taps(self->handle);
    size_t max_out  = (num_in + 1) / 2 + num_taps + 2;
    npy_intp out_dim = (npy_intp)max_out;
    PyArrayObject *out_arr =
        (PyArrayObject *)PyArray_SimpleNew(1, &out_dim, NPY_COMPLEX64);
    if (!out_arr) { Py_DECREF(in_arr); return NULL; }
    size_t n = hbdecim_r2c_execute(
        self->handle,
        (const float *)PyArray_DATA(in_arr), num_in,
        (float _Complex *)PyArray_DATA(out_arr), max_out);
    Py_DECREF(in_arr);
    PyObject *sliced = PySequence_GetSlice(
        (PyObject *)out_arr, 0, (Py_ssize_t)n);
    Py_DECREF(out_arr);
    return sliced;
}

static PyObject *
HbDecimR2C_enter(HalfbandDecimatorR2CObject *self,
                 PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
HbDecimR2C_exit(HalfbandDecimatorR2CObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        hbdecim_r2c_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef HalfbandDecimatorR2C_methods[] = {
    {"reset",    (PyCFunction)HalfbandDecimatorR2C_reset,   METH_NOARGS,  NULL},
    {"execute",  (PyCFunction)HalfbandDecimatorR2C_execute, METH_VARARGS, NULL},
    {"__enter__",(PyCFunction)HbDecimR2C_enter,             METH_NOARGS,  NULL},
    {"__exit__", (PyCFunction)HbDecimR2C_exit,              METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject HalfbandDecimatorR2CType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.HalfbandDecimatorR2C",
    .tp_basicsize = sizeof(HalfbandDecimatorR2CObject),
    .tp_dealloc   = (destructor)HalfbandDecimatorR2C_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "HbDecimR2C(h) — Architecture D2 halfband decimator "
                    "(float32→CF32).",
    .tp_methods   = HalfbandDecimatorR2C_methods,
    .tp_getset    = HalfbandDecimatorR2C_getset,
    .tp_new       = HalfbandDecimatorR2C_new,
    .tp_init      = (initproc)HalfbandDecimatorR2C_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Resample_methods[] = {
    {"kaiser_beta", _bind_kaiser_beta, METH_VARARGS, "kaiser_beta."},
    {"kaiser_num_taps", _bind_kaiser_num_taps, METH_VARARGS,
     "kaiser_num_taps."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef resample_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "resample",
    .m_doc     = "Resample module.",
    .m_size    = -1,
    .m_methods = Resample_methods,
};

PyMODINIT_FUNC
PyInit_resample(void)
{
    import_array();
    if (PyType_Ready(&ResamplerObjType) < 0) return NULL;
    if (PyType_Ready(&HalfbanddecimatorObjType) < 0) return NULL;
    if (PyType_Ready(&HalfbandDecimatorDpType) < 0) return NULL;
    if (PyType_Ready(&HalfbandDecimatorR2CType) < 0) return NULL;
    PyObject *m = PyModule_Create(&resample_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ResamplerObjType);
    if (PyModule_AddObject(m, "Resampler",
                           (PyObject *)&ResamplerObjType) < 0) {
        Py_DECREF(&ResamplerObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbanddecimatorObjType);
    if (PyModule_AddObject(m, "Halfbanddecimator",
                           (PyObject *)&HalfbanddecimatorObjType) < 0) {
        Py_DECREF(&HalfbanddecimatorObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbanddecimatorObjType);
    if (PyModule_AddObject(m, "HalfbandDecimator",
                           (PyObject *)&HalfbanddecimatorObjType) < 0) {
        Py_DECREF(&HalfbanddecimatorObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbandDecimatorDpType);
    if (PyModule_AddObject(m, "HalfbandDecimatorDp",
                           (PyObject *)&HalfbandDecimatorDpType) < 0) {
        Py_DECREF(&HalfbandDecimatorDpType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbandDecimatorR2CType);
    if (PyModule_AddObject(m, "HalfbandDecimatorR2C",
                           (PyObject *)&HalfbandDecimatorR2CType) < 0) {
        Py_DECREF(&HalfbandDecimatorR2CType); Py_DECREF(m); return NULL;
    }
    return m;
}
