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

    /* Release the GIL across the pure-C kernel. Safe by the one-capsule-per-
     * stream contract: the kernel touches only this stream's state and the
     * caller's input/output buffers — no Python objects, no shared mutable
     * state — and we still hold references to x_arr / out_arr, so their data
     * cannot be freed underneath us. Pointers are fetched before the block so
     * no Python C-API runs while the GIL is dropped. This is what lets a
     * thread-per-shard worker scale across cores instead of serialising on
     * the GIL. */
    const float    *in_data  = (const float *)PyArray_DATA(x_arr);
    float _Complex *out_data = (float _Complex *)PyArray_DATA(out_arr);
    size_t n_out;
    Py_BEGIN_ALLOW_THREADS
    n_out = ddcr_execute(w->state, in_data, n_in, out_data, max_out);
    Py_END_ALLOW_THREADS
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
     "Allocate and initialise a DDCR down-converter state handle.\n"
     "\n"
     "The DDCR (real-input Digital Down-Converter) implements the chain::\n"
     "\n"
     "    float in (fs_in)\n"
     "      -> halfband R2C decimator (2:1, with an embedded fs/4 shift)\n"
     "      -> fine NCO mixer at the intermediate rate (fs_in/2)\n"
     "      -> RateConverter (polyphase + optional CIC + halfband)\n"
     "      -> complex64 out (fs_out)\n"
     "\n"
     "RateConverter selects the cheapest cascade for the requested rate at\n"
     "create time.  The handle is an opaque PyCapsule; all subsequent\n"
     "operations take it as their first argument.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "norm_freq : float\n"
     "    Fine NCO frequency at the intermediate rate (fs_in / 2).\n"
     "    The halfband R2C stage applies a fixed +fs/4 shift before the NCO,\n"
     "    so to park a real tone at f_carrier (normalised to fs_in) at DC::\n"
     "\n"
     "        norm_freq = -(2 * f_carrier + 0.5)\n"
     "\n"
     "    For example, a carrier at 0.1*fs_in requires norm_freq = -0.7.\n"
     "rate : float\n"
     "    Total output-to-input rate ratio (fs_out / fs_in).  Must be in\n"
     "    the open interval (0, 0.5) -- DDCR always decimates.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "state : capsule\n"
     "    Opaque handle wrapping the C ddcr_state_t.  Pass to\n"
     "    ddcr_execute / ddcr_reset / ddcr_destroy / ddcr_get_norm_freq /\n"
     "    ddcr_set_norm_freq / ddcr_get_rate.  If ddcr_destroy is never\n"
     "    called, the GC releases the C resources when the capsule is\n"
     "    collected.\n"
     "\n"
     "Raises\n"
     "------\n"
     "MemoryError\n"
     "    If the underlying ddcr_create C call returns NULL (OOM or\n"
     "    invalid arguments).\n"
     "\n"
     "Examples\n"
     "--------\n"
     "Create a DDCR that tunes a real carrier at 0.1*fs to DC and\n"
     "decimates by 4 (rate = 0.25)::\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy\n"
     "    >>> f_carrier = 0.1\n"
     "    >>> norm_freq = -(2 * f_carrier + 0.5)   # -0.7\n"
     "    >>> state = ddcr_create(norm_freq, 0.25)\n"
     "    >>> x = np.zeros(64, dtype=np.float32)\n"
     "    >>> out = np.empty(64, dtype=np.complex64)\n"
     "    >>> y = ddcr_execute(state, x, out)\n"
     "    >>> y.dtype\n"
     "    dtype('complex64')\n"
     "    >>> y.shape[0]\n"
     "    16\n"
     "    >>> ddcr_destroy(state)\n"},
    {"ddcr_execute",
     _fn_ddcr_execute, METH_VARARGS,
     "ddcr_execute(state, x, out) -> ndarray[complex64]\n"
     "\n"
     "Process a block of real float32 samples through the DDCR.\n"
     "\n"
     "Applies the full DDCR chain (halfband R2C -> NCO -> RateConverter) to\n"
     "the input block and writes the complex baseband output into the\n"
     "caller-supplied buffer ``out``.  Returns a zero-copy view out[:n_out];\n"
     "the view lifetime is tied to ``out``, not to ``state``.\n"
     "\n"
     "The GIL is released across the pure-C kernel.  A thread-per-shard\n"
     "worker -- each thread owning its own capsule and ``out`` buffer --\n"
     "therefore scales across CPU cores without serialising on the GIL.\n"
     "\n"
     "Processing is phase-continuous across calls: feeding a signal as\n"
     "successive blocks through the same state is bit-identical to\n"
     "processing the entire signal in one shot.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.  Must not have been passed to\n"
     "    ddcr_destroy.\n"
     "x : ndarray[float32]\n"
     "    Real input samples (C-contiguous).  Any length; the output count\n"
     "    scales by rate (plus any residual in the filter history).\n"
     "out : ndarray[complex64]\n"
     "    Caller-owned, writable, C-contiguous output buffer.  Capacity\n"
     "    >= len(x) elements is always sufficient (DDCR always decimates).\n"
     "    The buffer may be reused across calls.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "ndarray[complex64]\n"
     "    Zero-copy view out[:n_out].  Data are owned by ``out``; the view\n"
     "    remains valid as long as ``out`` is alive.\n"
     "\n"
     "Raises\n"
     "------\n"
     "RuntimeError\n"
     "    If ``state`` has already been passed to ddcr_destroy.\n"
     "TypeError\n"
     "    If ``out`` is not a writable ndarray[complex64].\n"
     "\n"
     "Examples\n"
     "--------\n"
     "Tune a cosine at 0.1*fs to DC and decimate 4x::\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy\n"
     "    >>> N = 4096\n"
     "    >>> f_carrier = 0.1\n"
     "    >>> state = ddcr_create(-(2 * f_carrier + 0.5), 0.25)\n"
     "    >>> t = np.arange(N, dtype=np.float32)\n"
     "    >>> x = np.cos(2 * np.pi * f_carrier * t)\n"
     "    >>> out = np.empty(N, dtype=np.complex64)\n"
     "    >>> y = ddcr_execute(state, x, out)\n"
     "    >>> y.shape[0]           # output length = N * rate = 1024\n"
     "    1024\n"
     "    >>> np.shares_memory(y, out)    # zero-copy view of out[:1024]\n"
     "    True\n"
     "    >>> int(np.argmax(np.abs(np.fft.fft(y))))    # peak at bin 0 (DC)\n"
     "    0\n"
     "    >>> ddcr_destroy(state)\n"},
    {"ddcr_reset",
     _fn_ddcr_reset, METH_VARARGS,
     "ddcr_reset(state) -> None\n"
     "\n"
     "Zero all DDCR filter history without freeing or recreating the state.\n"
     "\n"
     "Resets the halfband R2C decimator taps, the LO phase accumulator,\n"
     "and the RateConverter history buffers to zero.  The configured\n"
     "norm_freq and rate are preserved.  Equivalent to ddcr_destroy +\n"
     "ddcr_create with the same parameters, but cheaper -- no allocation\n"
     "or deallocation occurs.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.  Must not have been passed to\n"
     "    ddcr_destroy.\n"
     "\n"
     "Raises\n"
     "------\n"
     "RuntimeError\n"
     "    If ``state`` has already been passed to ddcr_destroy.\n"
     "\n"
     "Examples\n"
     "--------\n"
     "Processing the same block twice through a reset state produces\n"
     "identical output each time::\n"
     "\n"
     "    >>> import numpy as np\n"
     "    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_reset, ddcr_destroy\n"
     "    >>> state = ddcr_create(-0.7, 0.25)\n"
     "    >>> x = np.ones(64, dtype=np.float32)\n"
     "    >>> out = np.empty(64, dtype=np.complex64)\n"
     "    >>> y1 = ddcr_execute(state, x, out).copy()\n"
     "    >>> ddcr_reset(state)\n"
     "    >>> y2 = ddcr_execute(state, x, out).copy()\n"
     "    >>> bool(np.array_equal(y1, y2))\n"
     "    True\n"
     "    >>> ddcr_destroy(state)\n"},
    {"ddcr_destroy",
     _fn_ddcr_destroy, METH_VARARGS,
     "ddcr_destroy(state) -> None\n"
     "\n"
     "Release DDCR C resources immediately, without waiting for the GC.\n"
     "\n"
     "Frees the ddcr_state_t struct and all heap members allocated by\n"
     "ddcr_create.  After this call the capsule is marked destroyed: any\n"
     "subsequent call that accepts it as ``state`` raises RuntimeError.\n"
     "\n"
     "If ddcr_destroy is never called, the GC will also free the resources\n"
     "correctly when the capsule is collected.  Explicit destruction is\n"
     "preferred in long-running processes or when managing many streams.\n"
     "\n"
     "Live views returned by earlier ddcr_execute calls remain valid after\n"
     "this call, because they reference the caller-supplied ``out`` buffer,\n"
     "not the state.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.  Must not have been passed to\n"
     "    ddcr_destroy already.\n"
     "\n"
     "Raises\n"
     "------\n"
     "RuntimeError\n"
     "    If ``state`` has already been destroyed.\n"
     "\n"
     "Examples\n"
     "--------\n"
     "Explicit destruction is safe; subsequent calls raise RuntimeError::\n"
     "\n"
     "    >>> from doppler.ddc import ddcr_create, ddcr_destroy, ddcr_get_rate\n"
     "    >>> state = ddcr_create(-0.7, 0.25)\n"
     "    >>> ddcr_destroy(state)\n"
     "    >>> try:\n"
     "    ...     ddcr_get_rate(state)\n"
     "    ... except RuntimeError as exc:\n"
     "    ...     print(exc)\n"
     "    ddcr_state has already been destroyed\n"},
    {"ddcr_get_norm_freq",
     _fn_ddcr_get_norm_freq, METH_VARARGS,
     "ddcr_get_norm_freq(state) -> float\n"
     "\n"
     "Return the current fine NCO normalised frequency.\n"
     "\n"
     "The returned value is the NCO frequency at the intermediate rate\n"
     "(fs_in / 2), as set by ddcr_create or most recently updated by\n"
     "ddcr_set_norm_freq.  Calling ddcr_reset does not alter this value.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.  Must not have been passed to\n"
     "    ddcr_destroy.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "norm_freq : float\n"
     "    Current NCO frequency in cycles/sample at the intermediate rate.\n"
     "\n"
     "Raises\n"
     "------\n"
     "RuntimeError\n"
     "    If ``state`` has already been passed to ddcr_destroy.\n"
     "\n"
     "Examples\n"
     "--------\n"
     "::\n"
     "\n"
     "    >>> from doppler.ddc import ddcr_create, ddcr_get_norm_freq, ddcr_destroy\n"
     "    >>> state = ddcr_create(-0.7, 0.25)\n"
     "    >>> ddcr_get_norm_freq(state)\n"
     "    -0.7\n"
     "    >>> ddcr_destroy(state)\n"},
    {"ddcr_set_norm_freq",
     _fn_ddcr_set_norm_freq, METH_VARARGS,
     "ddcr_set_norm_freq(state, norm_freq) -> None\n"
     "\n"
     "Retune the fine NCO frequency without disturbing filter history.\n"
     "\n"
     "Updates the LO phase increment so subsequent ddcr_execute calls use\n"
     "the new norm_freq.  The halfband R2C, polyphase, CIC, and halfband\n"
     "decimator histories are untouched -- processing resumes\n"
     "phase-continuously from the current state, with no filter transient\n"
     "at the retune boundary.\n"
     "\n"
     "To shift a real carrier at f_new (normalised to fs_in) to DC after\n"
     "retuning::\n"
     "\n"
     "    ddcr_set_norm_freq(state, -(2 * f_new + 0.5))\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.  Must not have been passed to\n"
     "    ddcr_destroy.\n"
     "norm_freq : float\n"
     "    New fine NCO frequency in cycles/sample at the intermediate\n"
     "    rate (fs_in / 2).\n"
     "\n"
     "Raises\n"
     "------\n"
     "RuntimeError\n"
     "    If ``state`` has already been passed to ddcr_destroy.\n"
     "\n"
     "Examples\n"
     "--------\n"
     "Retune from carrier 0.18*fs to 0.14*fs::\n"
     "\n"
     "    >>> from doppler.ddc import (\n"
     "    ...     ddcr_create, ddcr_set_norm_freq,\n"
     "    ...     ddcr_get_norm_freq, ddcr_destroy,\n"
     "    ... )\n"
     "    >>> state = ddcr_create(-(2 * 0.18 + 0.5), 0.25)\n"
     "    >>> ddcr_get_norm_freq(state)\n"
     "    -0.86\n"
     "    >>> ddcr_set_norm_freq(state, -(2 * 0.14 + 0.5))\n"
     "    >>> ddcr_get_norm_freq(state)\n"
     "    -0.78\n"
     "    >>> ddcr_destroy(state)\n"},
    {"ddcr_get_rate",
     _fn_ddcr_get_rate, METH_VARARGS,
     "ddcr_get_rate(state) -> float\n"
     "\n"
     "Return the total configured output-to-input rate ratio.\n"
     "\n"
     "Returns fs_out / fs_in exactly as passed to ddcr_create.  The rate\n"
     "is read-only; changing it requires ddcr_destroy + ddcr_create with\n"
     "a new value.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "state : capsule\n"
     "    Handle returned by ddcr_create.  Must not have been passed to\n"
     "    ddcr_destroy.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "rate : float\n"
     "    Configured fs_out / fs_in ratio.  Always in (0, 0.5).\n"
     "\n"
     "Raises\n"
     "------\n"
     "RuntimeError\n"
     "    If ``state`` has already been passed to ddcr_destroy.\n"
     "\n"
     "Examples\n"
     "--------\n"
     "::\n"
     "\n"
     "    >>> from doppler.ddc import ddcr_create, ddcr_get_rate, ddcr_destroy\n"
     "    >>> state = ddcr_create(-0.7, 0.25)\n"
     "    >>> ddcr_get_rate(state)\n"
     "    0.25\n"
     "    >>> ddcr_destroy(state)\n"},
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
