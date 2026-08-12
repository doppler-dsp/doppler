/*
 * util_ext.c — Python extension module util
 *
 * Objects:
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "util/util_core.h"

static PyObject *
_bind_square_clip (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "y", "lin", NULL };
  Py_complex   y_raw     = { 0.0, 0.0 };
  float        lin       = 0.0f;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Df", _kwlist, &y_raw, &lin))
    return NULL;
  float complex y = (float)y_raw.real + (float)y_raw.imag * I;
  return PyComplex_FromDoubles ((double)crealf (square_clip (y, lin)),
                                (double)cimagf (square_clip (y, lin)));
}

static PyObject *
_bind_saturate (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "v", "lo", "hi", "nan_to", NULL };
  double       v         = 0.0;
  double       lo        = 0.0;
  double       hi        = 0.0;
  double       nan_to    = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dddd", _kwlist, &v, &lo, &hi,
                                    &nan_to))
    return NULL;
  return PyFloat_FromDouble (saturate (v, lo, hi, nan_to));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef util_module_methods[] = {
  { "square_clip", (PyCFunction)(void *)_bind_square_clip,
    METH_VARARGS | METH_KEYWORDS,
    "Square-clip a complex sample: clip the real and imaginary parts\n"
    "independently to [-lin, lin] (a square region in the IQ plane).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "y : complex\n"
    "    Complex CF32 input sample.\n"
    "lin : float\n"
    "    Per-component clip threshold (linear amplitude, >= 0). Values\n"
    "    outside `[-lin, lin]` are clamped; values on the boundary are\n"
    "    preserved exactly.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    Sample with each component limited to `[-lin, lin]`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import square_clip\n"
    ">>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through\n"
    "(0.5+0.25j)\n"
    ">>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged\n"
    "(1+0.5j)\n"
    ">>> square_clip(3.0-4.0j, 1.0)    # both components clipped\n"
    "(1-1j)\n"
    ">>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both\n"
    "(0.25+0.25j)\n"
    ">>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped\n"
    "(-1+0j)\n" },
  { "saturate", (PyCFunction)(void *)_bind_saturate,
    METH_VARARGS | METH_KEYWORDS,
    "Saturate a value into [lo, hi], total over every double including\n"
    "NaN and both infinities. The NaN destination is a parameter because\n"
    "which end is safe is domain knowledge: a gain control guarding a\n"
    "measured power wants the ceiling, a lock statistic wants the floor. Use\n"
    "it at the boundary where an untrusted value first becomes persistent\n"
    "state -- the input of an EMA, accumulator or integrator.\n"
    "\n"
    "`fmin`/`fmax` are not enough for this job. A plain `fmin(fmax(v, lo),\n"
    "hi)` propagates NaN on some platforms and silently returns a bound on\n"
    "others, and a hand-written `v > hi ? hi : v` leaves NaN untouched,\n"
    "because every comparison against NaN is false. This function has no\n"
    "fall-through: a value that is neither inside the interval, nor below\n"
    "it, nor above it can only be NaN.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "v : float\n"
    "    Value to saturate. Any double.\n"
    "lo : float\n"
    "    Lower bound, returned for any `v < lo`.\n"
    "hi : float\n"
    "    Upper bound, returned for any `v > hi`.\n"
    "nan_to : float\n"
    "    Returned when `v` is NaN. Pick the end that is safe in the caller's\n"
    "    own terms; it is usually `lo` or `hi`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    `v` when `lo <= v <= hi`, otherwise `lo`, `hi` or `nan_to`.\n"
    "\n"
    "Notes\n"
    "-----\n"
    "Why the NaN destination is the caller's Which end is *safe* is domain\n"
    "knowledge, not arithmetic. A gain control guarding a measured power\n"
    "wants NaN at the **ceiling** — an unknown level must drive the gain\n"
    "down, because too little gain loses a signal while too much rails\n"
    "everything downstream. A lock statistic wants NaN at the **floor** — an\n"
    "unknown lock is not a lock. Baking either choice in would hand the\n"
    "wrong default to half its callers, so `nan_to` is a parameter and each\n"
    "call site states its own safe direction.\n"
    "\n"
    "Where to use it At the boundary where an untrusted value first becomes\n"
    "**persistent state** — the input of an EMA, an accumulator, or an\n"
    "integrator. Ahead of that boundary a bad value corrupts one output and\n"
    "is gone; past it, it is remembered and every quantity derived from it\n"
    "inherits the damage. One guard there makes the whole downstream chain\n"
    "total, where a clamp at each stage is several chances to miss one.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import saturate\n"
    ">>> saturate(0.5, 0.0, 1.0, 1.0)     # inside the interval\n"
    "0.5\n"
    ">>> saturate(2.0, 0.0, 1.0, 1.0)     # above the ceiling\n"
    "1.0\n"
    ">>> saturate(-3.0, 0.0, 1.0, 1.0)    # below the floor\n"
    "0.0\n"
    ">>> saturate(float(\"inf\"), 0.0, 1.0, 1.0)   # infinity is just above\n"
    "1.0\n"
    ">>> saturate(float(\"nan\"), 0.0, 1.0, 1.0)   # NaN takes the caller's "
    "end\n"
    "1.0\n"
    ">>> saturate(float(\"nan\"), 0.0, 1.0, 0.0)   # ... which may be the "
    "other\n"
    "0.0\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef util_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "util",
  .m_doc     = "Shared numeric utilities used across the doppler modules.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> from doppler.util import square_clip\n"
               ">>> square_clip(2 + 0j, 1.0)\n"
               "(1+0j)\n",
  .m_size    = -1,
  .m_methods = util_module_methods,
};

PyMODINIT_FUNC
PyInit_util (void)
{
  import_array ();

  PyObject *m = PyModule_Create (&util_moduledef);
  if (!m)
    return NULL;

  return m;
}
