/*
 * util_ext.c — Python extension module util
 *
 * Objects:
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/util/util_core.h"

static PyObject *
_bind_square_clip (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "y", "lin", NULL };
  Py_complex   y_raw     = { 0.0, 0.0 };
  float        lin       = 0.0f;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Df", _kwlist, &y_raw, &lin))
    return NULL;
  float _Complex y = (float)y_raw.real + (float)y_raw.imag * I;
  return PyComplex_FromDoubles ((double)crealf (square_clip (y, lin)),
                                (double)cimagf (square_clip (y, lin)));
}

static PyObject *
_bind_next_pow_two (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_raw))
    return NULL;
  size_t n = (size_t)n_raw;
  return PyLong_FromUnsignedLongLong ((unsigned long long)next_pow_two (n));
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

static PyObject *
_bind_complement_power (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "p", "x", NULL };
  double       p         = 0.0;
  double       x         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", _kwlist, &p, &x))
    return NULL;
  return PyFloat_FromDouble (complement_power (p, x));
}

static PyObject *
_bind_sinc (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "u", NULL };
  double       u         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &u))
    return NULL;
  return PyFloat_FromDouble (sinc (u));
}

static PyObject *
_bind_mean_sinc (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "umax", NULL };
  double       umax      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &umax))
    return NULL;
  return PyFloat_FromDouble (mean_sinc (umax));
}

static PyObject *
_bind_simpson_weights (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "w", NULL };
  PyObject    *w_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &w_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (w_obj)
      || PyArray_TYPE ((PyArrayObject *)w_obj) != NPY_DOUBLE
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)w_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)w_obj))
    {
      PyErr_SetString (PyExc_TypeError, "w must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF (
      w_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!w_arr)
    {
      return NULL;
    }
  double *w     = (double *)PyArray_DATA (w_arr);
  size_t  w_len = (size_t)PyArray_SIZE (w_arr);
  int     _rc   = simpson_weights (w, w_len);
  Py_DECREF (w_arr);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_RuntimeError, "simpson_weights failed (rc=%d)",
                    (int)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
_bind_midpoint_nodes (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "u", NULL };
  PyObject    *u_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &u_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (u_obj)
      || PyArray_TYPE ((PyArrayObject *)u_obj) != NPY_DOUBLE
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)u_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)u_obj))
    {
      PyErr_SetString (PyExc_TypeError, "u must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *u_arr = (PyArrayObject *)PyArray_FROM_OTF (
      u_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!u_arr)
    {
      return NULL;
    }
  double *u     = (double *)PyArray_DATA (u_arr);
  size_t  u_len = (size_t)PyArray_SIZE (u_arr);
  midpoint_nodes (u, u_len);
  Py_DECREF (u_arr);
  Py_RETURN_NONE;
}

static PyObject *
_bind_gauss_hermite (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "z", "p", NULL };
  PyObject    *z_obj     = NULL;
  PyObject    *p_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &z_obj, &p_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (z_obj)
      || PyArray_TYPE ((PyArrayObject *)z_obj) != NPY_DOUBLE
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)z_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)z_obj))
    {
      PyErr_SetString (PyExc_TypeError, "z must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *z_arr = (PyArrayObject *)PyArray_FROM_OTF (
      z_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!z_arr)
    {
      return NULL;
    }
  double *z     = (double *)PyArray_DATA (z_arr);
  size_t  z_len = (size_t)PyArray_SIZE (z_arr);
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (p_obj)
      || PyArray_TYPE ((PyArrayObject *)p_obj) != NPY_DOUBLE
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)p_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)p_obj))
    {
      PyErr_SetString (PyExc_TypeError, "p must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      Py_DECREF (z_arr);
      return NULL;
    }
  PyArrayObject *p_arr = (PyArrayObject *)PyArray_FROM_OTF (
      p_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!p_arr)
    {
      Py_DECREF (z_arr);
      return NULL;
    }
  double *p     = (double *)PyArray_DATA (p_arr);
  size_t  p_len = (size_t)PyArray_SIZE (p_arr);
  int     _rc   = gauss_hermite (z, z_len, p, p_len);
  Py_DECREF (z_arr);
  Py_DECREF (p_arr);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_RuntimeError, "gauss_hermite failed (rc=%d)",
                    (int)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef util_module_methods[] = {
  { "square_clip", (PyCFunction)(void *)_bind_square_clip,
    METH_VARARGS | METH_KEYWORDS,
    "Square-clip a complex sample: clip the real and imaginary parts "
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
  { "next_pow_two", (PyCFunction)(void *)_bind_next_pow_two,
    METH_VARARGS | METH_KEYWORDS,
    "Smallest power of two greater than or equal to n. The transform-sizing "
    "primitive: zero-padded FFT lengths, ring capacities and grow-on-demand "
    "buffers all want the same rounding, and five identical private copies of "
    "it were in the tree before this one. Saturating rather than wrapping -- "
    "0 is returned when the answer exceeds SIZE_MAX, because a doubling loop "
    "run past the top shifts to zero and spins forever.\n"
    "\n"
    "The transform-sizing primitive. A zero-padded FFT length, a ring\n"
    "capacity, a grow-on-demand buffer -- all of them want the same \"round\n"
    "up to a power of two\", and all of them had been writing the doubling\n"
    "loop out where they stood. FIVE identical private copies were in the\n"
    "tree when this landed -- `detector/det_private.h`, `delay_core.c`,\n"
    "`psd_core.c`, `specan_core.c` and `ppe_core.c`, each a `static` one\n"
    "none of the others could reach -- alongside bare `while (c < n) c *= 2`\n"
    "loops seeded from whatever each caller happened to start at, which is\n"
    "the shape that lets one quietly start at 4 and another at 1 and neither\n"
    "be wrong until they are compared. None of the four guarded the overflow\n"
    "below.\n"
    "\n"
    "Saturating rather than wrapping: a doubling loop run past the top of\n"
    "`size_t` shifts to zero and spins forever, so the one case that cannot\n"
    "be expressed returns 0 instead of hanging. A caller sizing an\n"
    "allocation gets a refusal it can see.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Value to round up. 0 and 1 both give 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The smallest power of two >= n, or 0 if that exceeds `SIZE_MAX`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import next_pow_two\n"
    ">>> next_pow_two(0), next_pow_two(1), next_pow_two(2)\n"
    "(1, 1, 2)\n"
    ">>> next_pow_two(3), next_pow_two(4), next_pow_two(5)\n"
    "(4, 4, 8)\n"
    ">>> next_pow_two(1000)        # a zero-padded transform length\n"
    "1024\n"
    ">>> next_pow_two(1 << 20)     # already a power of two, unchanged\n"
    "1048576\n" },
  { "saturate", (PyCFunction)(void *)_bind_saturate,
    METH_VARARGS | METH_KEYWORDS,
    "Saturate a value into [lo, hi], total over every double including NaN "
    "and both infinities. The NaN destination is a parameter because which "
    "end is safe is domain knowledge: a gain control guarding a measured "
    "power wants the ceiling, a lock statistic wants the floor. Use it at the "
    "boundary where an untrusted value first becomes persistent state -- the "
    "input of an EMA, accumulator or integrator.\n"
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
    "One step of a first-order exponential moving average, state + alpha*(x - "
    "state). The canonical EMA for the library: it was written out four times "
    "in two different algebraic forms before this existed, and duplicated "
    "implementations drift. The incremental form is the more accurate of the "
    "two everywhere the library operates, by a margin that grows as the "
    "average lengthens; alpha == 1 (pass-through) and alpha == 0 (frozen) are "
    "both exact. NOT total in x -- a non-finite observation poisons the state "
    "permanently, because an EMA remembers, so saturate() belongs on this "
    "function's input.\n"
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
    "The EMA coefficient that advances d samples in one step, 1 - (1 - "
    "alpha)^d. A decimated loop updates once per chunk of d samples and must "
    "not thereby change its own time constant. Computed through expm1/log1p "
    "because the direct expression cancels catastrophically for small alpha "
    "-- 26865 ulps off at alpha 1e-5, d 1 -- and being exact at d == 1 is "
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
  { "complement_power", (PyCFunction)(void *)_bind_complement_power,
    METH_VARARGS | METH_KEYWORDS,
    "1 - (1 - p)^x for p in [0, 1] and real x >= 0, computed through "
    "expm1/log1p so it stays accurate when p is small. The one kernel behind "
    "two library quantities: the EMA coefficient that advances d samples "
    "(ema_alpha_decim, x = d) and the per-cell false-alarm probability that "
    "splits a search's Pfa over n independent cells (det_pfa_cell, x = 1/n).\n"
    "\n"
    "Written directly, `1 - pow(1 - p, x)` loses everything `1 - p` rounded\n"
    "away: at `p = 1e-5` it is 26865 ulps off. `-expm1(x * log1p(-p))` is\n"
    "the same quantity with nothing cancelled.\n"
    "\n"
    "Two library quantities are this one expression, and both call it:\n"
    "\n"
    "- the EMA coefficient that advances `d` samples in one step,\n"
    "  ema_alpha_decim(alpha, d) (`x = d`);\n"
    "- the per-cell false-alarm probability that splits a search's `pfa`\n"
    "  over `n` independent cells, det_pfa_cell(pfa, n) (`x = 1/n`, Šidák).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "p : float\n"
    "    Per-trial probability, in `[0, 1]`.\n"
    "x : float\n"
    "    Number of trials, any real `x >= 0`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    `1 - (1 - p)^x`; exactly `p` at `x == 1`, 0 at `x == 0` or `p <=\n"
    "    0`, and 1 at `p >= 1` (for `x > 0`).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import complement_power\n"
    ">>> complement_power(0.05, 1.0)          # one trial is p exactly\n"
    "0.05\n"
    ">>> round(complement_power(0.5, 2.0), 12)  # 1 - 0.25\n"
    "0.75\n"
    ">>> round(complement_power(1e-3, 1 / 1000) * 1e6, 6)  # Sidak split\n"
    "1.0005\n"
    ">>> complement_power(0.3, 0.0)\n"
    "0.0\n" },
  { "sinc", (PyCFunction)(void *)_bind_sinc, METH_VARARGS | METH_KEYWORDS,
    "Normalized sinc, sin(pi u)/(pi u), with sinc(0) = 1: the amplitude "
    "response of a rectangular window, so the straddle loss of a signal u "
    "bins off a DFT bin's centre.\n"
    "\n"
    "The amplitude response of a rectangular window, which makes it the\n"
    "straddle loss of every correlator and DFT: a signal `u` bins off a\n"
    "bin's centre keeps `sinc(u)` of its amplitude in that bin.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "u : float\n"
    "    Offset, in bins (any real).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    `sin(pi u) / (pi u)`, and exactly 1 at `u == 0`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import sinc\n"
    ">>> sinc(0.0)\n"
    "1.0\n"
    ">>> round(sinc(0.5), 12)                 # half a bin: 2/pi\n"
    "0.636619772368\n"
    ">>> abs(sinc(1.0)) < 1e-15               # the first null\n"
    "True\n" },
  { "mean_sinc", (PyCFunction)(void *)_bind_mean_sinc,
    METH_VARARGS | METH_KEYWORDS,
    "The mean of sinc(u) over u in [0, umax]: the average amplitude loss of a "
    "signal whose offset from the nearest bin centre is uniform over umax "
    "bins. 1 for umax <= 0. 64-interval Simpson (simpson_weights) over "
    "segments of at most half a bin: within 3e-10 at any umax.\n"
    "\n"
    "The average amplitude loss of a signal whose offset from the nearest\n"
    "bin centre is uniform over `umax` bins: the scalloping a Pd model\n"
    "averages over, where sinc(umax) would be only the worst case.\n"
    "64-interval Simpson (simpson_weights()) over segments of at most half a\n"
    "bin: within 3e-10 at any umax, far below any model this feeds.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "umax : float\n"
    "    Upper end of the offset, in bins.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    The mean; 1 for `umax <= 0`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.util import mean_sinc\n"
    ">>> mean_sinc(0.0)\n"
    "1.0\n"
    ">>> round(mean_sinc(0.5), 9)             # uniform over half a bin\n"
    "0.8726543\n" },
  { "simpson_weights", (PyCFunction)(void *)_bind_simpson_weights,
    METH_VARARGS | METH_KEYWORDS,
    "Fill w with composite Simpson weights for the MEAN of a function over an "
    "interval: sum(w[i] * f(a + i*(b - a)/(n - 1))) approximates the mean of "
    "f over [a, b], for n = len(w) odd and at least 3. The weights sum to 1. "
    "Raises for any other length.\n"
    "\n"
    "With `n = w_len` points, `sum(w[i] * f(a + i*(b - a)/(n - 1)))` is the\n"
    "mean of `f` over `[a, b]` (multiply by `b - a` for the integral). The\n"
    "weights are `1, 4, 2, 4, ..., 2, 4, 1` over `3 (n - 1)` and sum to 1.\n"
    "Exact for any cubic; the error falls as `(n - 1)^-4` for a smooth `f`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "w : NDArray[np.float64]\n"
    "    Output, `w_len` weights.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.util import simpson_weights\n"
    ">>> w = np.empty(5)\n"
    ">>> simpson_weights(w)\n"
    ">>> w * 12                               # 1, 4, 2, 4, 1 over 12\n"
    "array([1., 4., 2., 4., 1.])\n"
    ">>> u = np.linspace(0.0, 1.0, 5)\n"
    ">>> round(float(w @ u**3), 12)           # mean of u^3 over [0, 1]\n"
    "0.25\n" },
  { "midpoint_nodes", (PyCFunction)(void *)_bind_midpoint_nodes,
    METH_VARARGS | METH_KEYWORDS,
    "Fill u with the midpoint-rule nodes on [0, 1], u[k] = (k + 1/2)/n for n "
    "= len(u): the points a uniform average over n equal cells is evaluated "
    "at, each weighted 1/n.\n"
    "\n"
    "The points a uniform average over `n` equal cells is evaluated at, each\n"
    "weighted `1/n`. Scale to `[a, b]` as `a + (b - a) * u[k]`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "u : NDArray[np.float64]\n"
    "    Output, `u_len` nodes, ascending.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.util import midpoint_nodes\n"
    ">>> u = np.empty(4)\n"
    ">>> midpoint_nodes(u)\n"
    ">>> u\n"
    "array([0.125, 0.375, 0.625, 0.875])\n" },
  { "gauss_hermite", (PyCFunction)(void *)_bind_gauss_hermite,
    METH_VARARGS | METH_KEYWORDS,
    "Fill z and p with the n-point Gauss-Hermite rule for a STANDARD NORMAL: "
    "sum(p[i] * f(z[i])) approximates E[f(Z)], Z ~ N(0, 1), exactly for any "
    "polynomial f of degree up to 2n - 1. For X ~ N(mu, sigma^2) evaluate "
    "f(mu + sigma * z[i]). Nodes ascend and the weights sum to 1. z and p "
    "must be the same length n >= 1; raises otherwise.\n"
    "\n"
    "`sum(p[i] * f(z[i]))` approximates `E[f(Z)]`, `Z ~ N(0, 1)`, and is\n"
    "exact for any polynomial `f` of degree up to `2n - 1`. For `X ~ N(mu,\n"
    "sigma^2)`, evaluate `f(mu + sigma * z[i])`. The nodes ascend and are\n"
    "symmetric about 0; the weights sum to 1.\n"
    "\n"
    "The nodes are the roots of the probabilists' Hermite polynomial `He_n`,\n"
    "found by Newton's method on its orthonormal recurrence `h[k+1] = (z\n"
    "h[k] - sqrt(k) h[k-1]) / sqrt(k+1)`, which cannot overflow the way\n"
    "`He_n` and `n!` do. Each starts from the classical asymptotic guesses\n"
    "(Numerical Recipes' `gauher`). The weight of a root is `1 / (n\n"
    "h[n-1](z)^2)`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "z : NDArray[np.float64]\n"
    "    Output, `n` nodes.\n"
    "p : NDArray[np.float64]\n"
    "    Output, `n` weights.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.util import gauss_hermite\n"
    ">>> z, p = np.empty(2), np.empty(2)\n"
    ">>> gauss_hermite(z, p)\n"
    ">>> z, p                                 # +-1, each half\n"
    "(array([-1.,  1.]), array([0.5, 0.5]))\n"
    ">>> z, p = np.empty(5), np.empty(5)\n"
    ">>> gauss_hermite(z, p)\n"
    ">>> round(float(p @ z**4), 12)           # E[Z^4] = 3\n"
    "3.0\n" },
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
