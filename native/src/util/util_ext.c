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

static PyObject *
_bind_ema_step (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "state", "x", "alpha", NULL };
  double       state     = 0.0;
  double       x         = 0.0;
  double       alpha     = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddd", _kwlist, &state, &x,
                                    &alpha))
    return NULL;
  return PyFloat_FromDouble (ema_step (state, x, alpha));
}

static PyObject *
_bind_ema_alpha_decim (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[] = { "alpha", "d", NULL };
  double             alpha     = 0.0;
  unsigned long long d_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dK", _kwlist, &alpha, &d_raw))
    return NULL;
  size_t d = (size_t)d_raw;
  return PyFloat_FromDouble (ema_alpha_decim (alpha, d));
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
  { "ema_step", (PyCFunction)(void *)_bind_ema_step,
    METH_VARARGS | METH_KEYWORDS,
    "One step of a first-order exponential moving average, state +\n"
    "alpha*(x - state). The canonical EMA for the library: it was written\n"
    "out four times in two different algebraic forms before this existed,\n"
    "and duplicated implementations drift. The incremental form is the more\n"
    "accurate of the two everywhere the library operates, by a margin that\n"
    "grows as the average lengthens; alpha == 1 (pass-through) and alpha ==\n"
    "0 (frozen) are both exact. NOT total in x -- a non-finite observation\n"
    "poisons the state permanently, because an EMA remembers, so saturate()\n"
    "belongs on this function's input.\n"
    "\n"
    "The canonical EMA for the whole library. It was written out four times\n"
    "before this existed — `agc` (power detector), `async_dsss_receiver`\n"
    "(the lock_num/lock_den pair), `acc_trace` (ACC_TRACE_EXP) and the\n"
    "recursion `det_ema_alpha` sizes — in **two different algebraic forms**,\n"
    "which are identical on paper and not in floating point. Duplicated\n"
    "implementations drift; this is the one.\n"
    "\n"
    "### Why this form, and not `alpha*x + (1-alpha)*state`\n"
    "\n"
    "Both were measured against a 60-digit reference over 5000 steps. The\n"
    "incremental form written here is the more accurate one everywhere the\n"
    "library actually operates, by a margin that grows as the average gets\n"
    "longer — which is the direction a narrow-band estimator moves:\n"
    "\n"
    "| `alpha` | this form | `alpha*x + (1-alpha)*state` |\n"
    "|---------|-----------|------------------------------|\n"
    "| 0.05    | 9.0e-17   | 6.5e-16                      |\n"
    "| 1e-3    | 3.1e-16   | 1.6e-15                      |\n"
    "| 1e-5    | 2.7e-17   | 5.4e-15                      |\n"
    "\n"
    "The other form wins exactly one case, and it is a boundary rather than\n"
    "a regime: at `alpha == 1` it returns `x` bit-exactly while the\n"
    "incremental form does not (measured inexact for 9.6% of random `(state,\n"
    "x)` pairs, because `state + 1*(x - state)` rounds twice). That case is\n"
    "real — `det_ema_alpha` returns exactly 1.0 for \"no gain requested, so\n"
    "no averaging\" — so it is handled explicitly below rather than paid for\n"
    "at every alpha.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "state : float\n"
    "    Current EMA state.\n"
    "x : float\n"
    "    New observation.\n"
    "alpha : float\n"
    "    Coefficient in `[0, 1]`. `1` is pass-through (no averaging) and is\n"
    "    exact; `0` freezes the state and is exact. A value above 1\n"
    "    saturates to pass-through rather than overshooting.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    The updated state.\n"
    "\n"
    "Notes\n"
    "-----\n"
    "NOT total in `x`: a non-finite observation poisons the state\n"
    "permanently, because an EMA remembers. That is deliberate — the guard\n"
    "belongs at the boundary where an untrusted value first becomes\n"
    "persistent state, which is this function's input. Use ::saturate there,\n"
    "as `agc_steps` does. See `agc_core.h` for what one unguarded non-finite\n"
    "sample cost.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import ema_step\n"
    ">>> ema_step(0.0, 1.0, 0.5)          # halfway to the observation\n"
    "0.5\n"
    ">>> ema_step(2.0, 2.0, 0.25)         # at its fixed point, no motion\n"
    "2.0\n"
    ">>> ema_step(1.0, 7.0, 1.0)          # alpha 1 is exact pass-through\n"
    "7.0\n"
    ">>> ema_step(1.0, 7.0, 0.0)          # alpha 0 freezes the state\n"
    "1.0\n" },
  { "ema_alpha_decim", (PyCFunction)(void *)_bind_ema_alpha_decim,
    METH_VARARGS | METH_KEYWORDS,
    "The EMA coefficient that advances d samples in one step, 1 - (1 -\n"
    "alpha)^d. A decimated loop updates once per chunk of d samples and must\n"
    "not thereby change its own time constant. Computed through expm1/log1p\n"
    "because the direct expression cancels catastrophically for small alpha\n"
    "-- 26865 ulps off at alpha 1e-5, d 1 -- and being exact at d == 1 is\n"
    "what lets the decimated and per-sample paths be compared bit-for-bit.\n"
    "\n"
    "A decimated loop updates its average once per chunk of `d` samples and\n"
    "must not thereby change its own time constant. Compounding the pole\n"
    "exactly is what makes `decim` a performance knob instead of a retune.\n"
    "\n"
    "### Why `expm1`/`log1p` rather than the direct expression\n"
    "\n"
    "`1.0 - pow(1.0 - alpha, d)` cancels catastrophically for small `alpha`,\n"
    "and the damage is worst exactly where a narrow-band estimator lives.\n"
    "Measured at `d == 1`, where the answer must be `alpha` itself:\n"
    "\n"
    "| `alpha` | direct `1-(1-alpha)^1` | this function |\n"
    "|---------|------------------------|---------------|\n"
    "| 0.05    | 6 ulps off             | exact         |\n"
    "| 1e-5    | 26865 ulps off         | exact         |\n"
    "\n"
    "`agc_steps` used the repeated-multiply form and had this defect; it now\n"
    "forms BOTH its per-chunk coefficients with this function. Being exact\n"
    "at `d == 1` is the property that lets a caller set `decim = 1` and get\n"
    "bit-for-bit the undecimated recursion, so the decimated and per-sample\n"
    "paths can be compared at all.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "alpha : float\n"
    "    Per-sample coefficient in `[0, 1]`.\n"
    "d : int\n"
    "    Chunk length in samples, `>= 1`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    The per-chunk coefficient, in `[0, 1]`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import ema_alpha_decim\n"
    ">>> ema_alpha_decim(0.05, 1)         # d == 1 returns alpha exactly\n"
    "0.05\n"
    ">>> round(ema_alpha_decim(0.05, 8), 12)\n"
    "0.336579568711\n"
    ">>> ema_alpha_decim(1.0, 4)          # pass-through stays pass-through\n"
    "1.0\n"
    ">>> ema_alpha_decim(0.0, 8)          # frozen stays frozen\n"
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
