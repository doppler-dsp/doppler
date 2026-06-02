/*
 * ddc_fn_ext.c — Functional DDCR Python API.
 *
 * State is an opaque PyCapsule.  The output buffer is caller-managed;
 * ddcr_execute writes into it and returns a zero-copy view of the
 * filled slice.  No buffer is cached in the capsule.
 *
 * Usage:
 *
 *   from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy
 *   state  = ddcr_create(norm_freq=-0.7, rate=0.25)
 *   buf    = np.empty(1024, dtype=np.complex64)   # caller owns this
 *   y      = ddcr_execute(state, x, buf)          # view of buf[:n_out]
 *   ddcr_destroy(state)
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>
#include <stdlib.h>

#include "ddc/ddc_core.h"

/* ------------------------------------------------------------------ */
/* State wrapper and capsule mechanics                                  */
/* ------------------------------------------------------------------ */

static const char _CAPS[] = "doppler.ddc.ddcr_state";

typedef struct {
    ddcr_state_t *state;
    int           destroyed;
} _wrap_t;

static void
_wrap_destructor(PyObject *cap)
{
    _wrap_t *w = (_wrap_t *)PyCapsule_GetPointer(cap, _CAPS);
    if (!w) return;
    if (!w->destroyed)
        ddcr_destroy(w->state);
    free(w);
}

static _wrap_t *
_get_wrap(PyObject *cap)
{
    _wrap_t *w = (_wrap_t *)PyCapsule_GetPointer(cap, _CAPS);
    if (!w) return NULL;
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
    w->destroyed = 0;

    PyObject *cap = PyCapsule_New(w, _CAPS, _wrap_destructor);
    if (!cap) { ddcr_destroy(w->state); free(w); return NULL; }
    return cap;
}

/* ------------------------------------------------------------------ */
/* ddcr_execute                                                         */
/* ------------------------------------------------------------------ */

static PyObject *
_fn_ddcr_execute(PyObject *mod, PyObject *args)
{
    (void)mod;
    PyObject *cap, *x_obj, *out_obj;
    if (!PyArg_ParseTuple(args, "OOO", &cap, &x_obj, &out_obj))
        return NULL;

    _wrap_t *w = _get_wrap(cap);
    if (!w) return NULL;

    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) return NULL;

    /* Require complex64 exactly — no silent cast; a cast would write into a
     * temp copy instead of the caller's buffer. */
    if (!PyArray_Check(out_obj) ||
        PyArray_TYPE((PyArrayObject *)out_obj) != NPY_COMPLEX64 ||
        !PyArray_ISWRITEABLE((PyArrayObject *)out_obj)) {
        PyErr_SetString(PyExc_TypeError,
            "out must be a writable ndarray[complex64]");
        Py_DECREF(x_arr);
        return NULL;
    }
    PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF(
        out_obj, NPY_COMPLEX64,
        NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
    if (!out_arr) { Py_DECREF(x_arr); return NULL; }

    size_t n_in    = (size_t)PyArray_SIZE(x_arr);
    size_t max_out = (size_t)PyArray_SIZE(out_arr);

    size_t n_out = ddcr_execute(
        w->state,
        (const float *)PyArray_DATA(x_arr), n_in,
        (float _Complex *)PyArray_DATA(out_arr), max_out);
    Py_DECREF(x_arr);

    /* Return out_arr[:n_out] — zero-copy view into the caller's buffer. */
    PyObject *stop  = PyLong_FromSsize_t((Py_ssize_t)n_out);
    PyObject *slice = stop ? PySlice_New(NULL, stop, NULL) : NULL;
    Py_XDECREF(stop);
    PyObject *view  = slice
        ? PyObject_GetItem((PyObject *)out_arr, slice)
        : NULL;
    Py_XDECREF(slice);
    Py_DECREF(out_arr);
    return view;
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
    w->state     = NULL;
    w->destroyed = 1;
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
     "\n"
     "Allocate a DDCR state handle.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "norm_freq : float\n"
     "    Fine NCO frequency at the intermediate rate (fs_in / 2).\n"
     "    To tune a real tone at f_carrier (normalised to fs_in) to DC:\n"
     "    norm_freq = -(2 * f_carrier + 0.5).\n"
     "rate : float\n"
     "    Total output / input rate.  Must be in (0, 0.5).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "state : capsule\n"
     "    Opaque handle.  Pass to ddcr_execute / ddcr_reset / ddcr_destroy.\n"
     "    Released by the GC if ddcr_destroy is never called."},
    {"ddcr_execute",
     _fn_ddcr_execute, METH_VARARGS,
     "ddcr_execute(state, x, out) -> ndarray[complex64]\n"
     "\n"
     "Process a block of real float32 samples.\n"
     "\n"
     "Writes into the caller-supplied buffer and returns a zero-copy view\n"
     "of the filled slice (out[:n_out]).  The view lifetime is tied to\n"
     "``out``, not to ``state``.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.\n"
     "x : ndarray[float32]\n"
     "    Real input samples (C-contiguous).\n"
     "out : ndarray[complex64]\n"
     "    Caller-owned, writable output buffer.  Capacity >= len(x)\n"
     "    is always sufficient (DDCR is always decimating).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "ndarray[complex64]\n"
     "    Zero-copy view out[:n_out]."},
    {"ddcr_reset",
     _fn_ddcr_reset, METH_VARARGS,
     "ddcr_reset(state) -> None\n"
     "\n"
     "Zero halfband, LO phase, and resampler history.\n"
     "Equivalent to destroy + create with the same parameters, but cheaper."},
    {"ddcr_destroy",
     _fn_ddcr_destroy, METH_VARARGS,
     "ddcr_destroy(state) -> None\n"
     "\n"
     "Release C resources immediately.\n"
     "\n"
     "The GC also frees correctly if this is never called.  Any further\n"
     "call on the same state after destroy raises RuntimeError.\n"
     "Live views of previous execute() output remain valid because they\n"
     "reference the caller's buffer, not the state."},
    {"ddcr_get_norm_freq",
     _fn_ddcr_get_norm_freq, METH_VARARGS,
     "ddcr_get_norm_freq(state) -> float\n"
     "\n"
     "Return the current fine NCO normalised frequency (at fs_in / 2)."},
    {"ddcr_set_norm_freq",
     _fn_ddcr_set_norm_freq, METH_VARARGS,
     "ddcr_set_norm_freq(state, norm_freq) -> None\n"
     "\n"
     "Retune the fine NCO without resetting halfband or resampler history.\n"
     "Phase-continuous across block boundaries."},
    {"ddcr_get_rate",
     _fn_ddcr_get_rate, METH_VARARGS,
     "ddcr_get_rate(state) -> float\n"
     "\n"
     "Return the total configured rate (fs_out / fs_in).  Read-only;\n"
     "changing the rate requires destroy + re-create."},
    {NULL, NULL, 0, NULL},
};

static PyModuleDef _moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "ddc_fn",
    .m_doc     = "Functional DDCR API — state and buffer passed explicitly.",
    .m_size    = -1,
    .m_methods = _methods,
};

PyMODINIT_FUNC
PyInit_ddc_fn(void)
{
    import_array();
    return PyModule_Create(&_moduledef);
}
