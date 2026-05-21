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

typedef struct {
    PyObject_HEAD
    Resampler_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
    float complex *_execute_ctrl_buf;  /* pre-allocated output for execute_ctrl */
} ResamplerObject;

static void
Resampler_dealloc(ResamplerObject *self)
{
    if (self->handle)
        Resampler_destroy(self->handle);
    free(self->_execute_buf);
    free(self->_execute_ctrl_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Resampler_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    ResamplerObject *self = (ResamplerObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Resampler_init(ResamplerObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"rate", NULL};
    double rate = 0.0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|d", kwlist,
                                     &rate))
        return -1;
    self->handle = Resampler_create(rate);
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
Resampler_reset(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    Resampler_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Resampler_execute(ResamplerObject *self, PyObject *args)
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
Resampler_execute_ctrl(ResamplerObject *self, PyObject *args)
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
    if (!ctrl_arr) return NULL;
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
Resampler_reset(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
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
Resampler_destroy(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        Resampler_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Resampler_enter(ResamplerObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Resampler_exit(ResamplerObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        Resampler_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Resampler_methods[] = {
    {"reset",    (PyCFunction)Resampler_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)Resampler_execute, METH_VARARGS,
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
    {"execute_ctrl", (PyCFunction)Resampler_execute_ctrl, METH_VARARGS,
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
    {"reset", (PyCFunction)Resampler_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "reset.\n"
     "\n"
     "    >>> from doppler import Resampler\n"
     "    >>> obj = Resampler(0.0)\n"
     "    >>> obj.reset()\n"},
    {"destroy",  (PyCFunction)Resampler_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Resampler_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Resampler_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject ResamplerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.Resampler",
    .tp_basicsize = sizeof(ResamplerObject),
    .tp_dealloc   = (destructor)Resampler_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Resampler type.",
    .tp_methods   = Resampler_methods,
    .tp_getset    = Resampler_getset,
    .tp_new       = Resampler_new,
    .tp_init      = (initproc)Resampler_init,
};
/* ======================================================== */
/* HalfbanddecimatorObject — wraps HalfbandDecimator_state_t *       */
/* ======================================================== */

#include "HalfbandDecimator/HalfbandDecimator_core.h"

typedef struct {
    PyObject_HEAD
    HalfbandDecimator_state_t *handle;
    float complex *_execute_buf;  /* pre-allocated output for execute */
} HalfbanddecimatorObject;

static void
Halfbanddecimator_dealloc(HalfbanddecimatorObject *self)
{
    if (self->handle)
        HalfbandDecimator_destroy(self->handle);
    free(self->_execute_buf);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Halfbanddecimator_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HalfbanddecimatorObject *self = (HalfbanddecimatorObject *)type->tp_alloc(type, 0);
    if (self)
        self->handle = NULL;
    return (PyObject *)self;
}

static int
Halfbanddecimator_init(HalfbanddecimatorObject *self, PyObject *args, PyObject *kwds)
{
    (void)args;
    (void)kwds;
    self->handle = HalfbandDecimator_create();
    if (!self->handle) {
        PyErr_SetString(PyExc_MemoryError,
                        "HalfbandDecimator_create returned NULL");
        return -1;
    }
    self->_execute_buf = malloc(
        HalfbandDecimator_execute_max_out(self->handle) * sizeof(float complex));
    if (!self->_execute_buf) { PyErr_NoMemory(); return -1; }
    return 0;
}

static PyObject *
Halfbanddecimator_reset(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "destroyed");
        return NULL;
    }
    HalfbandDecimator_reset(self->handle);
    Py_RETURN_NONE;
}






static PyObject *
Halfbanddecimator_execute(HalfbanddecimatorObject *self, PyObject *args)
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
    size_t n_out = HalfbandDecimator_execute(self->handle, (const float complex *)PyArray_DATA(x_arr), (size_t)PyArray_SIZE(x_arr), self->_execute_buf);
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
Halfbanddecimator_reset(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
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
Halfbanddecimator_destroy(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->handle) {
        HalfbandDecimator_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
Halfbanddecimator_enter(HalfbanddecimatorObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Halfbanddecimator_exit(HalfbanddecimatorObject *self, PyObject *args)
{
    (void)args;
    if (self->handle) {
        HalfbandDecimator_destroy(self->handle);
        self->handle = NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Halfbanddecimator_methods[] = {
    {"reset",    (PyCFunction)Halfbanddecimator_reset,    METH_NOARGS,
     "Reset state to post-create defaults."},

    {"execute", (PyCFunction)Halfbanddecimator_execute, METH_VARARGS,
     "execute(x) -> ndarray\n"
     "\n"
     "Zero-copy view into pre-allocated output buffer.\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler import Halfbanddecimator\n"
     "    >>> obj = Halfbanddecimator()\n"
     "    >>> y = obj.execute(np.zeros(4))\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"},
    {"reset", (PyCFunction)Halfbanddecimator_reset, METH_NOARGS,
     "reset() -> None\n"
     "\n"
     "reset.\n"
     "\n"
     "    >>> from doppler import Halfbanddecimator\n"
     "    >>> obj = Halfbanddecimator()\n"
     "    >>> obj.reset()\n"},
    {"destroy",  (PyCFunction)Halfbanddecimator_destroy,  METH_NOARGS,
     "Release resources."},
    {"__enter__", (PyCFunction)Halfbanddecimator_enter,   METH_NOARGS,  NULL},
    {"__exit__",  (PyCFunction)Halfbanddecimator_exit,    METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject HalfbanddecimatorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "resample.Halfbanddecimator",
    .tp_basicsize = sizeof(HalfbanddecimatorObject),
    .tp_dealloc   = (destructor)Halfbanddecimator_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Halfbanddecimator type.",
    .tp_methods   = Halfbanddecimator_methods,
    .tp_getset    = Halfbanddecimator_getset,
    .tp_new       = Halfbanddecimator_new,
    .tp_init      = (initproc)Halfbanddecimator_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Resample_methods[] = {
    {"kaiser_beta", _bind_kaiser_beta, METH_VARARGS, "kaiser_beta."},
    {"kaiser_num_taps", _bind_kaiser_num_taps, METH_VARARGS, "kaiser_num_taps."},
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
    if (PyType_Ready(&ResamplerType) < 0) return NULL;
    if (PyType_Ready(&HalfbanddecimatorType) < 0) return NULL;
    PyObject *m = PyModule_Create(&resample_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ResamplerType);
    if (PyModule_AddObject(m, "Resampler", (PyObject *)&ResamplerType) < 0) {
        Py_DECREF(&ResamplerType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HalfbanddecimatorType);
    if (PyModule_AddObject(m, "Halfbanddecimator", (PyObject *)&HalfbanddecimatorType) < 0) {
        Py_DECREF(&HalfbanddecimatorType); Py_DECREF(m); return NULL;
    }
    return m;
}
