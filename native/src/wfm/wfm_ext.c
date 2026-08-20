/*
 * wfm_ext.c — Python extension module wfm
 *
 * Objects: PN, _SynthEngine, Gold, Frame, FrameDesc
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "wfm/wfm_core.h"

#include "wfm_ext_frame.c"
#include "wfm_ext_framedesc.c"
#include "wfm_ext_gold.c"
#include "wfm_ext_pn.c"
#include "wfm_ext_wfm_synth.c"

static PyObject *
_bind_bpsk_map (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "bits", NULL };
  PyObject    *bits_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &bits_obj))
    return NULL;
  PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!bits_arr)
    {
      return NULL;
    }
  const uint8_t *bits     = (const uint8_t *)PyArray_DATA (bits_arr);
  size_t         bits_len = (size_t)PyArray_SIZE (bits_arr);
  npy_intp       _dim     = (npy_intp)bits_len;
  PyObject      *_out     = PyArray_EMPTY (1, &_dim, NPY_COMPLEX64, 0);
  if (!_out)
    {
      Py_DECREF (bits_arr);
      return NULL;
    }
  bpsk_map (bits, bits_len,
            (float complex *)PyArray_DATA ((PyArrayObject *)_out));
  Py_DECREF (bits_arr);
  return _out;
}

static PyObject *
_bind_qpsk_map (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "syms", NULL };
  PyObject    *syms_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &syms_obj))
    return NULL;
  PyArrayObject *syms_arr = (PyArrayObject *)PyArray_FROM_OTF (
      syms_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!syms_arr)
    {
      return NULL;
    }
  const uint8_t *syms     = (const uint8_t *)PyArray_DATA (syms_arr);
  size_t         syms_len = (size_t)PyArray_SIZE (syms_arr);
  npy_intp       _dim     = (npy_intp)syms_len;
  PyObject      *_out     = PyArray_EMPTY (1, &_dim, NPY_COMPLEX64, 0);
  if (!_out)
    {
      Py_DECREF (syms_arr);
      return NULL;
    }
  qpsk_map (syms, syms_len,
            (float complex *)PyArray_DATA ((PyArrayObject *)_out));
  Py_DECREF (syms_arr);
  return _out;
}

static PyObject *
_bind_wfm_awgn_amplitude (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]    = { "snr_db", "signal_power", NULL };
  float        snr_db       = 0.0f;
  float        signal_power = 0.0f;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ff", _kwlist, &snr_db,
                                    &signal_power))
    return NULL;
  return PyFloat_FromDouble (
      (double)wfm_awgn_amplitude (snr_db, signal_power));
}

static PyObject *
_bind_wfm_ebno_to_snr_db (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]
      = { "ebno_db", "bits_per_symbol", "samples_per_symbol", NULL };
  float ebno_db            = 0.0f;
  int   bits_per_symbol    = 0;
  float samples_per_symbol = 0.0f;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "fif", _kwlist, &ebno_db,
                                    &bits_per_symbol, &samples_per_symbol))
    return NULL;
  return PyFloat_FromDouble ((double)wfm_ebno_to_snr_db (
      ebno_db, bits_per_symbol, samples_per_symbol));
}

static PyObject *
_bind_mls_poly (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char  *_kwlist[] = { "n", NULL };
  unsigned long n_raw     = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "k", _kwlist, &n_raw))
    return NULL;
  uint32_t n = (uint32_t)n_raw;
  return PyLong_FromUnsignedLongLong ((unsigned long long)mls_poly (n));
}

static PyObject *
_bind_ccsds_asm_bits (PyObject *self, PyObject *Py_UNUSED (args))
{
  (void)self;
  npy_intp  _dim = (npy_intp)(32);
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_UINT8, 0);
  if (!_out)
    {
      return NULL;
    }
  ccsds_asm_bits ((uint8_t *)PyArray_DATA ((PyArrayObject *)_out));
  return _out;
}

static PyObject *
_bind_crc16 (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "bits", NULL };
  PyObject    *bits_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &bits_obj))
    return NULL;
  PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!bits_arr)
    {
      return NULL;
    }
  const uint8_t *bits     = (const uint8_t *)PyArray_DATA (bits_arr);
  size_t         bits_len = (size_t)PyArray_SIZE (bits_arr);
  Py_DECREF (bits_arr);
  return PyLong_FromUnsignedLong ((unsigned long)crc16 (bits, bits_len));
}

static PyObject *
_bind_rrc_h (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "t", "beta", NULL };
  PyObject    *t_obj     = NULL;
  double       beta      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", _kwlist, &t_obj, &beta))
    return NULL;
  PyArrayObject *t_arr = (PyArrayObject *)PyArray_FROM_OTF (
      t_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
  if (!t_arr)
    {
      return NULL;
    }
  const double *t     = (const double *)PyArray_DATA (t_arr);
  size_t        t_len = (size_t)PyArray_SIZE (t_arr);
  npy_intp      _dim  = (npy_intp)t_len;
  PyObject     *_out  = PyArray_EMPTY (1, &_dim, NPY_DOUBLE, 0);
  if (!_out)
    {
      Py_DECREF (t_arr);
      return NULL;
    }
  rrc_h (t, t_len, (double *)PyArray_DATA ((PyArrayObject *)_out), beta);
  Py_DECREF (t_arr);
  return _out;
}

static PyObject *
_bind_rc_h (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "t", "beta", NULL };
  PyObject    *t_obj     = NULL;
  double       beta      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", _kwlist, &t_obj, &beta))
    return NULL;
  PyArrayObject *t_arr = (PyArrayObject *)PyArray_FROM_OTF (
      t_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
  if (!t_arr)
    {
      return NULL;
    }
  const double *t     = (const double *)PyArray_DATA (t_arr);
  size_t        t_len = (size_t)PyArray_SIZE (t_arr);
  npy_intp      _dim  = (npy_intp)t_len;
  PyObject     *_out  = PyArray_EMPTY (1, &_dim, NPY_DOUBLE, 0);
  if (!_out)
    {
      Py_DECREF (t_arr);
      return NULL;
    }
  rc_h (t, t_len, (double *)PyArray_DATA ((PyArrayObject *)_out), beta);
  Py_DECREF (t_arr);
  return _out;
}

static PyObject *
_bind_rrc_taps (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "beta", "sps", "span", NULL };
  double       beta      = 0.0;
  int          sps       = 0;
  int          span      = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dii", _kwlist, &beta, &sps,
                                    &span))
    return NULL;
  npy_intp  _dim = (npy_intp)(2 * span * sps + 1);
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_FLOAT, 0);
  if (!_out)
    {
      return NULL;
    }
  rrc_taps (beta, sps, span, (float *)PyArray_DATA ((PyArrayObject *)_out));
  return _out;
}

static PyObject *
_bind_dsss_spread (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "syms", "code", "sf", NULL };
  PyObject    *syms_obj  = NULL;
  PyObject    *code_obj  = NULL;
  int          sf        = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOi", _kwlist, &syms_obj,
                                    &code_obj, &sf))
    return NULL;
  PyArrayObject *syms_arr = (PyArrayObject *)PyArray_FROM_OTF (
      syms_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!syms_arr)
    {
      return NULL;
    }
  const float complex *syms = (const float complex *)PyArray_DATA (syms_arr);
  size_t               syms_len = (size_t)PyArray_SIZE (syms_arr);
  PyArrayObject       *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      Py_DECREF (syms_arr);
      return NULL;
    }
  const uint8_t *code     = (const uint8_t *)PyArray_DATA (code_arr);
  size_t         code_len = (size_t)PyArray_SIZE (code_arr);
  npy_intp       _dim     = (npy_intp)(syms_len * sf);
  PyObject      *_out     = PyArray_EMPTY (1, &_dim, NPY_COMPLEX64, 0);
  if (!_out)
    {
      Py_DECREF (syms_arr);
      Py_DECREF (code_arr);
      return NULL;
    }
  dsss_spread (syms, syms_len, code, code_len, sf,
               (float complex *)PyArray_DATA ((PyArrayObject *)_out));
  Py_DECREF (syms_arr);
  Py_DECREF (code_arr);
  return _out;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef wfm_module_methods[] = {
  { "bpsk_map", (PyCFunction)(void *)_bind_bpsk_map,
    METH_VARARGS | METH_KEYWORDS,
    "Map bits {0,1} to BPSK symbols {+1,-1} (cf32).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bits : NDArray[np.uint8]\n"
    "    Array of uint8 values; only the LSB of each byte is used.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import bpsk_map\n"
    ">>> import numpy as np\n"
    ">>> bits = np.array([0, 1, 0, 1], dtype=np.uint8)\n"
    ">>> bpsk_map(bits).tolist()\n"
    "[(1+0j), (-1+0j), (1+0j), (-1+0j)]\n" },
  { "qpsk_map", (PyCFunction)(void *)_bind_qpsk_map,
    METH_VARARGS | METH_KEYWORDS,
    "Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "syms : NDArray[np.uint8]\n"
    "    Array of uint8 symbol indices; values must be in {0,1,2,3}. Bits\n"
    "    above position 1 are ignored.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import qpsk_map\n"
    ">>> import numpy as np\n"
    ">>> idx = np.array([0, 1, 2, 3], dtype=np.uint8)\n"
    ">>> out = qpsk_map(idx)\n"
    ">>> [round(float(v.real), 4) for v in out]\n"
    "[0.7071, -0.7071, 0.7071, -0.7071]\n"
    ">>> [round(float(v.imag), 4) for v in out]\n"
    "[0.7071, 0.7071, -0.7071, -0.7071]\n" },
  { "wfm_awgn_amplitude", (PyCFunction)(void *)_bind_wfm_awgn_amplitude,
    METH_VARARGS | METH_KEYWORDS,
    "AWGN amplitude for a target SNR (dB, over fs) given signal power.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "snr_db : float\n"
    "    Target SNR in dB, referenced to the full sample rate.\n"
    "signal_power : float\n"
    "    RMS power of the signal (e.g. 1.0 for unit-power complex tones or\n"
    "    unit-energy BPSK/QPSK symbols).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Per-component AWGN amplitude (sigma for one I or Q channel).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import wfm_awgn_amplitude\n"
    ">>> round(float(wfm_awgn_amplitude(10.0, 1.0)), 6)\n"
    "0.223607\n"
    ">>> round(float(wfm_awgn_amplitude(0.0, 1.0)), 6)\n"
    "0.707107\n" },
  { "wfm_ebno_to_snr_db", (PyCFunction)(void *)_bind_wfm_ebno_to_snr_db,
    METH_VARARGS | METH_KEYWORDS,
    "Convert Eb/No (dB) to SNR (dB over fs).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ebno_db : float\n"
    "    Eb/No in dB (energy per bit over noise spectral density).\n"
    "bits_per_symbol : int\n"
    "    Bits carried per modulation symbol: 1 for BPSK, 2 for QPSK.\n"
    "samples_per_symbol : float\n"
    "    Oversampling ratio (sps), e.g. 8.0.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    SNR in dB measured over the full sample-rate bandwidth.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import wfm_ebno_to_snr_db\n"
    ">>> round(float(wfm_ebno_to_snr_db(10.0, 2, 8.0)), 4)\n"
    "3.9794\n"
    ">>> round(float(wfm_ebno_to_snr_db(10.0, 1, 8.0)), 4)\n"
    "0.9691\n" },
  { "mls_poly", (PyCFunction)(void *)_bind_mls_poly,
    METH_VARARGS | METH_KEYWORDS,
    "Maximal-length-sequence primitive polynomial for an LFSR of length\n"
    "n.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    LFSR length in stages (2..64).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Primitive-polynomial tap mask, or 0 if n is out of range.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import mls_poly\n"
    ">>> hex(mls_poly(7))\n"
    "'0x41'\n" },
  { "ccsds_asm_bits", _bind_ccsds_asm_bits, METH_NOARGS,
    "The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits —\n"
    "`out[0]` is the first bit on the wire (the top of 0x1A). Pass it to\n"
    "`doppler.detection.SyncFinder` to acquire a CADU in a bit stream; it is\n"
    "NOT randomised, so it reads the same in every frame and in exactly one\n"
    "polarity, which is what makes it the thing that reports a 180-degree\n"
    "carrier ambiguity.\n"
    "\n"
    "`out[0]` is the first bit on the wire — figure 9-1 of 131.0-B numbers\n"
    "the marker's bit 0 as the most significant bit of 0x1A. One bit per\n"
    "byte, the convention every frame path here passes around.\n"
    "\n"
    "The thing a Python receiver ACQUIRES on: pair it with\n"
    "`doppler.detection.SyncFinder` to find where a CADU starts in a bit\n"
    "stream, then slice and `Frame.check()` it. The marker is deliberately\n"
    "NOT randomised (10.4's NOTE: \"The ASM was not randomized and is not\n"
    "derandomized\"), so it reads the same in every frame and in exactly one\n"
    "polarity — which is what makes it the only thing in a CADU that can\n"
    "report a 180-degree carrier ambiguity.\n"
    "\n"
    "A function rather than a constant a caller expands, because an\n"
    "MSB-first expansion written out twice is a transcription that can\n"
    "disagree with itself. This tree's own doctests were the second copy\n"
    "until doppler#900.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import ccsds_asm_bits\n"
    ">>> b = ccsds_asm_bits()\n"
    ">>> b.size, b[:8].tolist()          # 0x1A, first bit at the top\n"
    "(32, [0, 0, 0, 1, 1, 0, 1, 0])\n"
    ">>> int(\"\".join(map(str, b.tolist())), 2) == 0x1ACFFC1D\n"
    "True\n" },
  { "crc16", (PyCFunction)(void *)_bind_crc16, METH_VARARGS | METH_KEYWORDS,
    "CRC-16-CCITT (poly 0x1021, init 0xFFFF) over an unpacked 0/1 bit\n"
    "array, MSB-first — the DSSS burst frame trailer wfmgen appends and\n"
    "BurstDemod validates.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bits : NDArray[np.uint8]\n"
    "    Array of 0/1 bit values (one per byte).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The 16-bit CRC.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import crc16\n"
    ">>> ascii_bits = np.unpackbits(np.frombuffer(b\"123456789\", np.uint8))\n"
    ">>> hex(crc16(ascii_bits))   # the standard CCITT check vector\n"
    "'0x29b1'\n" },
  { "rrc_h", (PyCFunction)(void *)_bind_rrc_h, METH_VARARGS | METH_KEYWORDS,
    "Analytic root-raised-cosine pulse at arbitrary (non-grid) times `t`, in "
    "symbol periods. The transmit half of a matched-filter pair. Use this, "
    "not a transcription of the formula, whenever a stimulus needs the pulse "
    "off the integer sample grid — a non-integer samples-per-symbol or a "
    "fractional timing offset has no grid to sample. `rrc_taps` remains the "
    "right call for filter taps.\n" },
  { "rc_h", (PyCFunction)(void *)_bind_rc_h, METH_VARARGS | METH_KEYWORDS,
    "Analytic full raised-cosine pulse at arbitrary (non-grid) times `t`, in "
    "symbol periods. Already the Nyquist response a matched TX/RX pair "
    "produces, so this is what models the matched-filter OUTPUT directly — a "
    "timing-detector S-curve reference, or a receiver test with its front end "
    "collapsed away.\n" },
  { "rrc_taps", (PyCFunction)(void *)_bind_rrc_taps,
    METH_VARARGS | METH_KEYWORDS,
    "Root-raised-cosine pulse-shaping taps (2*span*sps+1 unit-energy cf32 "
    "taps).\n" },
  { "dsss_spread", (PyCFunction)(void *)_bind_dsss_spread,
    METH_VARARGS | METH_KEYWORDS,
    "Direct-sequence spread syms by the ±1 chip code; yields len(syms)*sf "
    "chips.\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef wfm_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "wfm",
  .m_doc = "Waveform generation: PN and CCSDS Gold code generators (PN, Gold) "
           "and a configurable modulated-symbol synthesizer, with BLUE/SigMF "
           "writers and readers re-exported.\n"
           "\n"
           "Examples\n"
           "--------\n"
           ">>> from doppler.wfm import Gold\n"
           ">>> Gold().generate(1023)[:15].tolist()   # CCSDS Code #365\n"
           "[0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]\n",
  .m_size    = -1,
  .m_methods = wfm_module_methods,
};

PyMODINIT_FUNC
PyInit_wfm (void)
{
  import_array ();
  if (PyType_Ready (&PNObjType) < 0)
    return NULL;
  if (PyType_Ready (&_SynthEngineType) < 0)
    return NULL;
  if (PyType_Ready (&GoldObjType) < 0)
    return NULL;
  if (PyType_Ready (&FrameObjType) < 0)
    return NULL;
  if (PyType_Ready (&FrameDescObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&wfm_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&PNObjType);
  if (PyModule_AddObject (m, "PN", (PyObject *)&PNObjType) < 0)
    {
      Py_DECREF (&PNObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&_SynthEngineType);
  if (PyModule_AddObject (m, "_SynthEngine", (PyObject *)&_SynthEngineType)
      < 0)
    {
      Py_DECREF (&_SynthEngineType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&GoldObjType);
  if (PyModule_AddObject (m, "Gold", (PyObject *)&GoldObjType) < 0)
    {
      Py_DECREF (&GoldObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&FrameObjType);
  if (PyModule_AddObject (m, "Frame", (PyObject *)&FrameObjType) < 0)
    {
      Py_DECREF (&FrameObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&FrameDescObjType);
  if (PyModule_AddObject (m, "FrameDesc", (PyObject *)&FrameDescObjType) < 0)
    {
      Py_DECREF (&FrameDescObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
