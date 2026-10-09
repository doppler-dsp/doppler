/*
 * wfm_ext_wfm_synth_extra.c — _SynthEngine's hand-written setters.
 *
 * Each attaches a variable-length pattern (taps, bits, symbols, chips, a
 * spreading code) to an engine after construction, a shape jm's method
 * rows do not render. They are registered by objects/wfm_synth.toml's
 * [[wfm_synth.extra_methods]] rows: jm owns each PyMethodDef entry, its
 * forward prototype, the stub and the #include of this file (after the
 * generated _SynthEngineObject), and never touches this file (doppler#1886).
 * CPython hands every method self as PyObject *, so each body casts it.
 */

#include "doppler/wfm_synth/wfm_synth_core.h"

/* What each setter's refusal of a str says instead (jm_array_arg_hint).
 * A str is refused rather than read by numpy as a number -- the
 * str_hint a manifest parameter declares does the same for a generated
 * binding (gh-1824), and these are the sentences one would carry.
 * WFM_SYNTH_BITS_HINT serves both set_bits() and set_dsss_cont()'s
 * payload. */
#define WFM_SYNTH_TAPS_HINT                                                   \
  "pass the taps as an array of floats, e.g. rrc_taps(beta, sps, span)"
#define WFM_SYNTH_BITS_HINT "pass the bits as an array of 0/1, e.g. [0, 1, 1]"
#define WFM_SYNTH_CHIPS_HINT "pass the chips as an array of 0/1"
#define WFM_SYNTH_CODE_HINT "pass the spreading code as an array of 0/1"
#define WFM_SYNTH_SYMBOLS_HINT                                                \
  "pass the symbols as an array of complex numbers"

/* set_rrc(taps) — enable RRC pulse shaping with the given real FIR taps
 * (e.g. doppler.wfm.rrc_taps(beta, sps, span)); no-op for non-modulated. */
static PyObject *
_SynthEngine_set_rrc (PyObject *obj, PyObject *args)
{
  _SynthEngineObject *self = (_SynthEngineObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *taps_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &taps_obj))
    return NULL;
  PyArrayObject *taps
      = jm_array_arg_hint (taps_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS,
                           "taps", WFM_SYNTH_TAPS_HINT);
  if (!taps)
    return NULL;
  size_t n  = (size_t)PyArray_SIZE (taps);
  int    rc = dp_wfm_synth_set_rrc (self->handle,
                                    (const float *)PyArray_DATA (taps), n);
  Py_DECREF (taps);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_rrc: empty taps or alloc failed");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* set_bits(pattern, modulation=1) — attach a user bit pattern to a type=bits
 * synth. pattern is any array-like of 0/1 (coerced to uint8); modulation is
 * 0=none, 1=bpsk, 2=qpsk. */
static PyObject *
_SynthEngine_set_bits (PyObject *obj, PyObject *args)
{
  _SynthEngineObject *self = (_SynthEngineObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *pat_obj    = NULL;
  int       modulation = 1;
  if (!PyArg_ParseTuple (args, "O|i", &pat_obj, &modulation))
    return NULL;
  PyArrayObject *arr
      = jm_array_arg_hint (pat_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS,
                           "pattern", WFM_SYNTH_BITS_HINT);
  if (!arr)
    return NULL;
  size_t n  = (size_t)PyArray_SIZE (arr);
  int    rc = dp_wfm_synth_set_bits (
      self->handle, (const uint8_t *)PyArray_DATA (arr), n, modulation);
  Py_DECREF (arr);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_bits: empty pattern, modulation not in 0..2, or "
                       "not a bits synth");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* set_dsss_chips(chips) — attach an ALREADY-ASSEMBLED DSSS burst to a
 * type=dsss synth: one chip per element, 0/1, BPSK-mapped by the synth.
 *
 * The burst is assembled from a frame DESCRIPTION -- a `Frame`'s bits, spread
 * by the data code behind the unspread preamble -- and not here: there is no
 * four-field form of a frame left to bind (docs/design/frame-description.md
 * section R). Any array-like of 0/1, coerced to uint8; the chips are copied.
 */
static PyObject *
_SynthEngine_set_dsss_chips (PyObject *obj, PyObject *arg)
{
  _SynthEngineObject *self = (_SynthEngineObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyArrayObject *arr = jm_array_arg_hint (
      arg, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "chips", WFM_SYNTH_CHIPS_HINT);
  if (!arr)
    return NULL;
  int rc = dp_wfm_synth_set_dsss_chips (self->handle,
                                        (const uint8_t *)PyArray_DATA (arr),
                                        (size_t)PyArray_SIZE (arr));
  Py_DECREF (arr);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_dsss_chips: the burst must be non-empty");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* set_dsss_cont(code, chips_per_symbol, data="prbs", payload=None) — configure
 * a type=dsss synth for CONTINUOUS asynchronous generation: the spreading
 * `code` repeats endlessly, data rides on it at chips_per_symbol chips/symbol
 * (non-integer). `data` selects the source: "none" (code only), "prbs" (the
 * synth's seeded PN, regenerable via doppler.wfm.PN), or "bits" (the payload
 * array, sent once). A payload forces "bits". */
static PyObject *
_SynthEngine_set_dsss_cont (PyObject *obj, PyObject *args, PyObject *kwds)
{
  _SynthEngineObject *self = (_SynthEngineObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[]
      = { "code", "chips_per_symbol", "data", "payload", NULL };
  PyObject   *code_obj = Py_None, *pay_obj = Py_None;
  double      cps      = 0.0;
  const char *data_str = "prbs";
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od|sO", kwlist, &code_obj,
                                    &cps, &data_str, &pay_obj))
    return NULL;

  int mode;
  if (pay_obj != Py_None || strcmp (data_str, "bits") == 0)
    mode = WFM_DSSS_DATA_BITS;
  else if (strcmp (data_str, "none") == 0)
    mode = WFM_DSSS_DATA_NONE;
  else if (strcmp (data_str, "prbs") == 0)
    mode = WFM_DSSS_DATA_PRBS;
  else
    {
      PyErr_SetString (
          PyExc_ValueError,
          "set_dsss_cont: data must be 'none', 'prbs', or 'bits'");
      return NULL;
    }

  PyArrayObject *code_arr
      = jm_array_arg_hint (code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS, "code",
                           WFM_SYNTH_CODE_HINT);
  if (!code_arr)
    return NULL;
  PyArrayObject *pay_arr = NULL;
  if (pay_obj != Py_None)
    {
      pay_arr = jm_array_arg_hint (pay_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS,
                                   "payload", WFM_SYNTH_BITS_HINT);
      if (!pay_arr)
        {
          Py_DECREF (code_arr);
          return NULL;
        }
    }
  int rc = dp_wfm_synth_set_dsss_cont (
      self->handle, (const uint8_t *)PyArray_DATA (code_arr),
      (size_t)PyArray_SIZE (code_arr), cps, mode,
      pay_arr ? (const uint8_t *)PyArray_DATA (pay_arr) : NULL,
      pay_arr ? (size_t)PyArray_SIZE (pay_arr) : 0);
  Py_XDECREF (pay_arr);
  Py_DECREF (code_arr);
  if (rc != 0)
    {
      PyErr_SetString (
          PyExc_ValueError,
          "set_dsss_cont: invalid geometry (need a code, chips_per_symbol >= "
          "1, "
          "a payload for data='bits', a valid pn_length for data='prbs'), or "
          "not a dsss synth");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* set_dsss_window(code_only_symbols, frame_symbols) — give the continuous
 * stream a frame on the data clock: the first code_only_symbols symbols of
 * every frame_symbols carry the pure code, the rest the data.
 * frame_symbols=0 is no window at all. */
static PyObject *
_SynthEngine_set_dsss_window (PyObject *obj, PyObject *args, PyObject *kwds)
{
  _SynthEngineObject *self = (_SynthEngineObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "code_only_symbols", "frame_symbols", NULL };
  Py_ssize_t   w = 0, f = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "nn", kwlist, &w, &f))
    return NULL;
  if (w < 0 || f < 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_dsss_window: symbol counts must be >= 0");
      return NULL;
    }
  if (dp_wfm_synth_set_dsss_window (self->handle, (size_t)w, (size_t)f) != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_dsss_window: code_only_symbols must not exceed a "
                       "non-zero frame_symbols");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* set_symbols(symbols) — attach a user complex-symbol stream to a
 * type=symbols synth. symbols is any array-like coerced to complex64; each
 * element is the constellation point itself (no bit->symbol mapping). */
static PyObject *
_SynthEngine_set_symbols (PyObject *obj, PyObject *args)
{
  _SynthEngineObject *self = (_SynthEngineObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *sym_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &sym_obj))
    return NULL;
  PyArrayObject *arr = jm_array_arg_hint (
      sym_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_FORCECAST,
      "symbols", WFM_SYNTH_SYMBOLS_HINT);
  if (!arr)
    return NULL;
  size_t n  = (size_t)PyArray_SIZE (arr);
  int    rc = dp_wfm_synth_set_symbols (
      self->handle, (const float _Complex *)PyArray_DATA (arr), n);
  Py_DECREF (arr);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_symbols: empty stream or not a symbols synth");
      return NULL;
    }
  Py_RETURN_NONE;
}
