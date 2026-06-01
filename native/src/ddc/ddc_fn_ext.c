/*
 * ddc_fn_ext.c — Functional DDCR Python API.
 *
 * Exposes ddcr_create / ddcr_execute / ddcr_reset / ddcr_destroy and the
 * norm_freq / rate accessors as module-level free functions.  State is an
 * opaque PyCapsule; the wrapper struct caches the output buffer so
 * successive execute calls avoid repeated malloc.
 *
 * Usage:
 *
 *   from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy
 *   state = ddcr_create(norm_freq=-0.7, rate=0.25)
 *   y     = ddcr_execute(state, x)
 *   ddcr_destroy(state)
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>
#include <stdlib.h>
#include <string.h>

#include "ddc/ddc_core.h"

/* ------------------------------------------------------------------ */
/* State wrapper and capsule mechanics                                  */
/* ------------------------------------------------------------------ */

static const char _CAPS[] = "doppler.ddc.ddcr_state";

typedef struct {
    ddcr_state_t  *state;
    float complex *buf;
    size_t         buf_cap;
    int            destroyed;
} _wrap_t;

static void
_wrap_destructor(PyObject *cap)
{
    _wrap_t *w = (_wrap_t *)PyCapsule_GetPointer(cap, _CAPS);
    if (!w) return;
    if (!w->destroyed) {
        ddcr_destroy(w->state);
        free(w->buf);
    }
    free(w);
}

/* Check capsule, set exception and return NULL on any error. */
static _wrap_t *
_get_wrap(PyObject *cap)
{
    _wrap_t *w = (_wrap_t *)PyCapsule_GetPointer(cap, _CAPS);
    if (!w) return NULL;   /* PyCapsule_GetPointer already set TypeError */
    if (w->destroyed) {
        PyErr_SetString(PyExc_RuntimeError,
                        "ddcr_state has already been destroyed");
        return NULL;
    }
    return w;
}

/* ------------------------------------------------------------------ */
/* ddcr_create                                                          */
/* ------------------------------------------------------------------ */

static PyObject *
_fn_ddcr_create(PyObject *mod, PyObject *args)
{
    (void)mod;
    double norm_freq, rate;
    if (!PyArg_ParseTuple(args, "dd", &norm_freq, &rate))
        return NULL;

    _wrap_t *w = (_wrap_t *)malloc(sizeof(_wrap_t));
    if (!w) return PyErr_NoMemory();

    w->state = ddcr_create(norm_freq, rate);
    if (!w->state) { free(w); return PyErr_NoMemory(); }
    w->buf       = NULL;
    w->buf_cap   = 0;
    w->destroyed = 0;

    PyObject *cap = PyCapsule_New(w, _CAPS, _wrap_destructor);
    if (!cap) {
        ddcr_destroy(w->state);
        free(w);
        return NULL;
    }
    return cap;
}

/* ------------------------------------------------------------------ */
/* ddcr_execute                                                         */
/* ------------------------------------------------------------------ */

static PyObject *
_fn_ddcr_execute(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap, *x_obj;
    if (!PyArg_ParseTuple(args, "OO", &cap, &x_obj))
        return NULL;

    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;

    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;

    size_t n_in = (size_t)PyArray_SIZE(x_arr);
    if (!n_in) {
        Py_DECREF(x_arr);
        npy_intp zero = 0;
        return PyArray_EMPTY(1, &zero, NPY_COMPLEX64, 0);
    }

    /* Buffer: n_in is a safe upper bound (DDC is always decimating). */
    if (w->buf_cap < n_in) {
        float complex *tmp = (float complex *)realloc(
            w->buf, n_in * sizeof(float complex));
        if (!tmp) { Py_DECREF(x_arr); return PyErr_NoMemory(); }
        w->buf     = tmp;
        w->buf_cap = n_in;
    }

    size_t n_out = ddcr_execute(
        w->state,
        (const float *)PyArray_DATA(x_arr), n_in,
        w->buf, w->buf_cap);
    Py_DECREF(x_arr);

    npy_intp dim = (npy_intp)n_out;
    PyObject *out = PyArray_EMPTY(1, &dim, NPY_COMPLEX64, 0);
    if (!out) return NULL;
    memcpy(PyArray_DATA((PyArrayObject *)out),
           w->buf, n_out * sizeof(float complex));
    return out;
}

/* ------------------------------------------------------------------ */
/* ddcr_reset                                                           */
/* ------------------------------------------------------------------ */

static PyObject *
_fn_ddcr_reset(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap;
    if (!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;
    ddcr_reset(w->state);
    Py_RETURN_NONE;
}

/* ------------------------------------------------------------------ */
/* ddcr_destroy                                                         */
/* ------------------------------------------------------------------ */

static PyObject *
_fn_ddcr_destroy(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap;
    if (!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;
    ddcr_destroy(w->state);
    free(w->buf);
    w->state     = NULL;
    w->buf       = NULL;
    w->buf_cap   = 0;
    w->destroyed = 1;
    /* Destructor is still live — it checks destroyed and skips the free. */
    Py_RETURN_NONE;
}

/* ------------------------------------------------------------------ */
/* Accessors                                                            */
/* ------------------------------------------------------------------ */

static PyObject *
_fn_ddcr_get_norm_freq(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap;
    if (!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;
    return PyFloat_FromDouble(ddcr_get_norm_freq(w->state));
}

static PyObject *
_fn_ddcr_set_norm_freq(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap;
    double norm_freq;
    if (!PyArg_ParseTuple(args, "Od", &cap, &norm_freq)) return NULL;
    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;
    ddcr_set_norm_freq(w->state, norm_freq);
    Py_RETURN_NONE;
}

static PyObject *
_fn_ddcr_get_rate(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap;
    if (!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;
    return PyFloat_FromDouble(ddcr_get_rate(w->state));
}

/* ------------------------------------------------------------------ */
/* Module                                                               */
/* ------------------------------------------------------------------ */

static PyMethodDef _methods[] = {
    {"ddcr_create",
     _fn_ddcr_create, METH_VARARGS,
     "ddcr_create(norm_freq, rate) -> state\n"
     "Allocate a DDCR state handle."},
    {"ddcr_execute",
     _fn_ddcr_execute, METH_VARARGS,
     "ddcr_execute(state, x) -> ndarray[complex64]\n"
     "Process a block of real float32 samples."},
    {"ddcr_reset",
     _fn_ddcr_reset, METH_VARARGS,
     "ddcr_reset(state) -> None\n"
     "Zero halfband, LO, and resampler history."},
    {"ddcr_destroy",
     _fn_ddcr_destroy, METH_VARARGS,
     "ddcr_destroy(state) -> None\n"
     "Release C resources immediately.  Further use raises RuntimeError."},
    {"ddcr_get_norm_freq",
     _fn_ddcr_get_norm_freq, METH_VARARGS,
     "ddcr_get_norm_freq(state) -> float"},
    {"ddcr_set_norm_freq",
     _fn_ddcr_set_norm_freq, METH_VARARGS,
     "ddcr_set_norm_freq(state, norm_freq) -> None"},
    {"ddcr_get_rate",
     _fn_ddcr_get_rate, METH_VARARGS,
     "ddcr_get_rate(state) -> float"},
    {NULL, NULL, 0, NULL},
};

static PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "ddc_fn",
    .m_doc     = "Functional DDCR API — state passed explicitly.",
    .m_size    = -1,
    .m_methods = _methods,
};

PyMODINIT_FUNC
PyInit_ddc_fn(void)
{
    import_array();
    return PyModule_Create(&_moduledef);
}
