/*
 * cvt_ext.c — Python extension module cvt
 *
 * Objects: F32ToI16, I16ToF32, I32ToF32, I8ToF32, F32ToI16U32, F32ToI16U64,
 * I16U32ToF32, I16U64ToF32, F32ToUQ15, UQ15ToF32, ADC GENERATED — do not
 * hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "cvt/cvt_core.h"

#include "cvt_ext_adc.c"
#include "cvt_ext_f32_to_i16.c"
#include "cvt_ext_f32_to_i16u32.c"
#include "cvt_ext_f32_to_i16u64.c"
#include "cvt_ext_f32_to_uq15.c"
#include "cvt_ext_i16_to_f32.c"
#include "cvt_ext_i16u32_to_f32.c"
#include "cvt_ext_i16u64_to_f32.c"
#include "cvt_ext_i32_to_f32.c"
#include "cvt_ext_i8_to_f32.c"
#include "cvt_ext_uq15_to_f32.c"

static PyObject *
_bind_int_to_bin (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[]  = { "v", "n_bits", "out", "bitorder", NULL };
  unsigned long long v_raw      = 0ULL;
  unsigned long      n_bits_raw = 0UL;
  PyObject          *out_obj    = NULL;
  int                bitorder   = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KkOi", _kwlist, &v_raw,
                                    &n_bits_raw, &out_obj, &bitorder))
    return NULL;
  uint64_t v      = (uint64_t)v_raw;
  uint32_t n_bits = (uint32_t)n_bits_raw;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      return NULL;
    }
  uint8_t *out     = (uint8_t *)PyArray_DATA (out_arr);
  size_t   out_len = (size_t)PyArray_SIZE (out_arr);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)int_to_bin (v, n_bits, out, out_len, bitorder));
}

static PyObject *
_bind_hex_to_bin (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "hex", "out", "bitorder", NULL };
  const char  *hex       = NULL;
  PyObject    *out_obj   = NULL;
  int          bitorder  = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sOi", _kwlist, &hex, &out_obj,
                                    &bitorder))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      return NULL;
    }
  uint8_t *out     = (uint8_t *)PyArray_DATA (out_arr);
  size_t   out_len = (size_t)PyArray_SIZE (out_arr);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)hex_to_bin (hex, out, out_len, bitorder));
}

static PyObject *
_bind_bin_to_int (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "bits", "bitorder", NULL };
  PyObject    *bits_obj  = NULL;
  int          bitorder  = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Oi", _kwlist, &bits_obj,
                                    &bitorder))
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
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)bin_to_int (bits, bits_len, bitorder));
}

static PyObject *
_bind_bin_to_hex (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "bits", "out", "bitorder", NULL };
  PyObject    *bits_obj  = NULL;
  PyObject    *out_obj   = NULL;
  int          bitorder  = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOi", _kwlist, &bits_obj,
                                    &out_obj, &bitorder))
    return NULL;
  PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!bits_arr)
    {
      return NULL;
    }
  const uint8_t *bits     = (const uint8_t *)PyArray_DATA (bits_arr);
  size_t         bits_len = (size_t)PyArray_SIZE (bits_arr);
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      Py_DECREF (bits_arr);
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      Py_DECREF (bits_arr);
      return NULL;
    }
  uint8_t *out     = (uint8_t *)PyArray_DATA (out_arr);
  size_t   out_len = (size_t)PyArray_SIZE (out_arr);
  Py_DECREF (bits_arr);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)bin_to_hex (bits, bits_len, out, out_len, bitorder));
}

static PyObject *
_bind_bin_to_nrz (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "bits", "out", NULL };
  PyObject    *bits_obj  = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &bits_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *bits_arr = (PyArrayObject *)PyArray_FROM_OTF (
      bits_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!bits_arr)
    {
      return NULL;
    }
  const uint8_t *bits     = (const uint8_t *)PyArray_DATA (bits_arr);
  size_t         bits_len = (size_t)PyArray_SIZE (bits_arr);
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      Py_DECREF (bits_arr);
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      Py_DECREF (bits_arr);
      return NULL;
    }
  float *out     = (float *)PyArray_DATA (out_arr);
  size_t out_len = (size_t)PyArray_SIZE (out_arr);
  Py_DECREF (bits_arr);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)bin_to_nrz (bits, bits_len, out, out_len));
}

static PyObject *
_bind_nrz_to_bin (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[] = { "nrz", "out", NULL };
  PyObject    *nrz_obj   = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist, &nrz_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *nrz_arr = (PyArrayObject *)PyArray_FROM_OTF (
      nrz_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!nrz_arr)
    {
      return NULL;
    }
  const float *nrz     = (const float *)PyArray_DATA (nrz_arr);
  size_t       nrz_len = (size_t)PyArray_SIZE (nrz_arr);
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      Py_DECREF (nrz_arr);
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      Py_DECREF (nrz_arr);
      return NULL;
    }
  uint8_t *out     = (uint8_t *)PyArray_DATA (out_arr);
  size_t   out_len = (size_t)PyArray_SIZE (out_arr);
  Py_DECREF (nrz_arr);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)nrz_to_bin (nrz, nrz_len, out, out_len));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef cvt_module_methods[] = {
  { "int_to_bin", (PyCFunction)(void *)_bind_int_to_bin,
    METH_VARARGS | METH_KEYWORDS,
    "Expand the low n_bits of an integer to unpacked bits, one per byte.\n"
    "The form a frame field literal usually wants: exact, and with no\n"
    "failure mode a typo can reach, unlike the string form. bitorder is\n"
    "DP_BITORDER_BIG (0, MSB of each byte first -- as written) or\n"
    "DP_BITORDER_LITTLE (1), numpy's `bitorder` convention for this\n"
    "operation, and NOT the BLUE writer's endian (le/be) which selects a\n"
    "file's BYTE order. Returns the bits written, or 0 on refusal.\n"
    "\n"
    "The form a frame field literal usually wants, and the one to reach for\n"
    "first: exact, compiler-checked, with no failure mode a typo can reach.\n"
    "hex_to_bin is for the two cases this cannot serve -- a literal wider\n"
    "than 64 bits, and text arriving from outside.\n"
    "\n"
    "Bit 0 out is the MOST significant of the n_bits requested under\n"
    "DP_BITORDER_BIG, which is what makes `int_to_bin(0x1A, 8, ...)` read\n"
    "`0,0,0,1,1,0,1,0`. Only the low n_bits are read, so a caller need not\n"
    "mask first.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "v : int\n"
    "    the value.\n"
    "n_bits : int\n"
    "    1..64.\n"
    "out : NDArray[np.uint8]\n"
    "    receives n_bits bytes, each 0 or 1.\n"
    "bitorder : int\n"
    "    DP_BITORDER_BIG or DP_BITORDER_LITTLE.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_bits, or 0 on refusal -- out untouched.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.cvt import int_to_bin\n"
    ">>> b = np.zeros(8, np.uint8)\n"
    ">>> int_to_bin(0x1A, 8, b, 0)          # 0 = big, MSB of each byte "
    "first\n"
    "8\n"
    ">>> b.tolist()\n"
    "[0, 0, 0, 1, 1, 0, 1, 0]\n" },
  { "hex_to_bin", (PyCFunction)(void *)_bind_hex_to_bin,
    METH_VARARGS | METH_KEYWORDS,
    "Expand a hex string to unpacked bits, one per byte. For what\n"
    "int_to_bin cannot serve: a literal wider than 64 bits, or one arriving\n"
    "as TEXT from a CLI flag or a JSON record. An odd number of digits is\n"
    "accepted and yields a 4-bit tail. A bad digit is a REFUSAL, never a\n"
    "silently shortened field -- a marker that shortens syncs to nothing.\n"
    "Returns the bits written, or 0 on refusal.\n"
    "\n"
    "For what int_to_bin cannot serve: a literal wider than 64 bits, or one\n"
    "arriving as TEXT from a CLI flag or a JSON record. Each digit\n"
    "contributes 4 bits and digits read left to right, so an ODD number of\n"
    "digits is accepted and yields a 4-bit tail.\n"
    "\n"
    "A bad digit is a REFUSAL, never a skipped one: a typo'd marker that\n"
    "silently shortens is the failure this exists to prevent, and it syncs\n"
    "to nothing rather than failing loudly.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "hex : str\n"
    "    NUL-terminated `0-9a-fA-F`. No `0x`, no separators.\n"
    "out : NDArray[np.uint8]\n"
    "    receives `4 * strlen(hex)` bytes, each 0 or 1.\n"
    "bitorder : int\n"
    "    DP_BITORDER_BIG or DP_BITORDER_LITTLE.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    bits written, or 0 on refusal -- out untouched.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.cvt import hex_to_bin\n"
    ">>> b = np.zeros(32, np.uint8)\n"
    ">>> hex_to_bin(\"1ACFFC1D\", b, 0)       # the CCSDS attached sync "
    "marker\n"
    "32\n"
    ">>> b[:8].tolist()\n"
    "[0, 0, 0, 1, 1, 0, 1, 0]\n" },
  { "bin_to_int", (PyCFunction)(void *)_bind_bin_to_int,
    METH_VARARGS | METH_KEYWORDS,
    "Read unpacked bits back into an integer -- the inverse of\n"
    "int_to_bin.\n"
    "\n"
    "Returns the value rather than a status, because that is the shape a\n"
    "binding can carry. 0 is therefore both \"the value zero\" and "
    "\"refused\",\n"
    "which is acceptable only because every refusal here is a programming\n"
    "error in the WIDTH the caller chose (0, or over 64) or the bit order it\n"
    "named -- never a property of the data.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bits : NDArray[np.uint8]\n"
    "    1..64 unpacked bits; any non-zero byte reads as 1.\n"
    "bitorder : int\n"
    "    DP_BITORDER_BIG or DP_BITORDER_LITTLE.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    the value, or 0 on refusal.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.cvt import bin_to_int\n"
    ">>> bits = np.array([0, 0, 0, 1, 1, 0, 1, 0], np.uint8)\n"
    ">>> hex(bin_to_int(bits, 0))\n"
    "'0x1a'\n" },
  { "bin_to_hex", (PyCFunction)(void *)_bind_bin_to_hex,
    METH_VARARGS | METH_KEYWORDS,
    "Render unpacked bits back to hex digits -- the exact inverse of\n"
    "hex_to_bin. The digits come back as ASCII BYTES rather than a str: jm\n"
    "has no string out-parameter, and uint8_t is the same type as the\n"
    "unsigned char a C caller would use. Decode with bytes(out).decode() in\n"
    "Python. n_bits must be a multiple of 4. Returns the digits written, not\n"
    "counting the NUL, or 0 on refusal.\n"
    "\n"
    "The digits come back as ASCII BYTES rather than a string: jm has no\n"
    "string out-parameter, and `uint8_t` is the same type as the `unsigned\n"
    "char` a C caller would use anyway. A NUL is written after the digits.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bits : NDArray[np.uint8]\n"
    "    unpacked bits; any non-zero byte reads as 1.\n"
    "out : NDArray[np.uint8]\n"
    "    receives the digits plus a NUL.\n"
    "bitorder : int\n"
    "    DP_BITORDER_BIG or DP_BITORDER_LITTLE.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    digits written, NOT counting the NUL, or 0 on refusal.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.cvt import hex_to_bin, bin_to_hex\n"
    ">>> b = np.zeros(32, np.uint8)\n"
    ">>> hex_to_bin(\"1acffc1d\", b, 0)\n"
    "32\n"
    ">>> h = np.zeros(16, np.uint8)\n"
    ">>> n = bin_to_hex(b, h, 0)\n"
    ">>> bytes(h[:n]).decode()\n"
    "'1acffc1d'\n" },
  { "bin_to_nrz", (PyCFunction)(void *)_bind_bin_to_nrz,
    METH_VARARGS | METH_KEYWORDS,
    "Map unpacked bits to bipolar NRZ symbols: bit 0 -> +1.0, bit 1 ->\n"
    "-1.0. That is `1 - 2*b`, the convention already used across doppler\n"
    "(qpsk_map.c and the despreader/ber doctests), NOT the opposite sign --\n"
    "a mapper that disagreed with the receiver's would decode every bit\n"
    "inverted while looking perfectly locked. Any non-zero byte reads as a\n"
    "set bit. Returns the symbols written, or 0 on refusal.\n"
    "\n"
    "That is `1 - 2*b`, and the convention's HOME is `mpsk_core.h`: BPSK is\n"
    "M-PSK at m = 2, where phi0 is 0, so label 0 lands at +1 and label 1 at\n"
    "-1. This states the same thing in the form a per-bit loop can afford,\n"
    "and `test_cvt_core` asserts the two agree rather than trusting them to.\n"
    "A mapper that disagreed with the receiver's would decode every bit\n"
    "INVERTED while looking perfectly locked -- which a round-trip test\n"
    "cannot see.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bits : NDArray[np.uint8]\n"
    "    unpacked bits; any non-zero byte reads as 1.\n"
    "out : NDArray[np.float32]\n"
    "    receives bits_len symbols, each +1.0f or -1.0f.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    symbols written, or 0 on refusal.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.cvt import bin_to_nrz\n"
    ">>> bits = np.array([0, 1, 1, 0], np.uint8)\n"
    ">>> sym = np.zeros(4, np.float32)\n"
    ">>> bin_to_nrz(bits, sym)\n"
    "4\n"
    ">>> sym.tolist()\n"
    "[1.0, -1.0, -1.0, 1.0]\n" },
  { "nrz_to_bin", (PyCFunction)(void *)_bind_nrz_to_bin,
    METH_VARARGS | METH_KEYWORDS,
    "Hard-decide bipolar NRZ symbols back to unpacked bits -- the inverse\n"
    "of bin_to_nrz. Negative is a 1, zero and positive are a 0, matching `1\n"
    "- 2*b`. Exactly zero is a 0 rather than a coin toss, so the mapping is\n"
    "total and a round trip is exact. Returns the bits written, or 0 on\n"
    "refusal.\n"
    "\n"
    "Negative is a 1; zero and positive are a 0, matching `1 - 2*b`. Exactly\n"
    "zero decides to 0 rather than a coin toss, so the mapping is TOTAL and\n"
    "a round trip is exact. A caller that wants an erasure handled as an\n"
    "erasure wants a soft demapper, not this.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "nrz : NDArray[np.float32]\n"
    "    symbols.\n"
    "out : NDArray[np.uint8]\n"
    "    receives nrz_len bytes, each 0 or 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    bits written, or 0 on refusal.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.cvt import nrz_to_bin\n"
    ">>> sym = np.array([1.0, -1.0, -1.0, 1.0], np.float32)\n"
    ">>> bits = np.zeros(4, np.uint8)\n"
    ">>> nrz_to_bin(sym, bits)\n"
    "4\n"
    ">>> bits.tolist()\n"
    "[0, 1, 1, 0]\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef cvt_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "cvt",
  .m_doc  = "Sample-format conversion: vectorized converters between float32 "
            "IQ and fixed-point integer formats (int8/16/32, unsigned Q15), "
            "plus a scaling ADC front end.\n"
            "\n"
            "Examples\n"
            "--------\n"
            ">>> import numpy as np\n"
            ">>> from doppler.cvt import F32ToI16, I16ToF32\n"
            ">>> x = np.array([0.5, -0.25], np.float32)\n"
            ">>> I16ToF32().steps(F32ToI16().steps(x)).round(3).tolist()\n"
            "[0.5, -0.25]\n",
  .m_size = -1,
  .m_methods = cvt_module_methods,
};

PyMODINIT_FUNC
PyInit_cvt (void)
{
  import_array ();
  if (PyType_Ready (&F32ToI16ObjType) < 0)
    return NULL;
  if (PyType_Ready (&I16ToF32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&I32ToF32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&I8ToF32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&F32ToI16U32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&F32ToI16U64ObjType) < 0)
    return NULL;
  if (PyType_Ready (&I16U32ToF32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&I16U64ToF32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&F32ToUQ15ObjType) < 0)
    return NULL;
  if (PyType_Ready (&UQ15ToF32ObjType) < 0)
    return NULL;
  if (PyType_Ready (&ADCObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&cvt_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&F32ToI16ObjType);
  if (PyModule_AddObject (m, "F32ToI16", (PyObject *)&F32ToI16ObjType) < 0)
    {
      Py_DECREF (&F32ToI16ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&I16ToF32ObjType);
  if (PyModule_AddObject (m, "I16ToF32", (PyObject *)&I16ToF32ObjType) < 0)
    {
      Py_DECREF (&I16ToF32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&I32ToF32ObjType);
  if (PyModule_AddObject (m, "I32ToF32", (PyObject *)&I32ToF32ObjType) < 0)
    {
      Py_DECREF (&I32ToF32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&I8ToF32ObjType);
  if (PyModule_AddObject (m, "I8ToF32", (PyObject *)&I8ToF32ObjType) < 0)
    {
      Py_DECREF (&I8ToF32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&F32ToI16U32ObjType);
  if (PyModule_AddObject (m, "F32ToI16U32", (PyObject *)&F32ToI16U32ObjType)
      < 0)
    {
      Py_DECREF (&F32ToI16U32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&F32ToI16U64ObjType);
  if (PyModule_AddObject (m, "F32ToI16U64", (PyObject *)&F32ToI16U64ObjType)
      < 0)
    {
      Py_DECREF (&F32ToI16U64ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&I16U32ToF32ObjType);
  if (PyModule_AddObject (m, "I16U32ToF32", (PyObject *)&I16U32ToF32ObjType)
      < 0)
    {
      Py_DECREF (&I16U32ToF32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&I16U64ToF32ObjType);
  if (PyModule_AddObject (m, "I16U64ToF32", (PyObject *)&I16U64ToF32ObjType)
      < 0)
    {
      Py_DECREF (&I16U64ToF32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&F32ToUQ15ObjType);
  if (PyModule_AddObject (m, "F32ToUQ15", (PyObject *)&F32ToUQ15ObjType) < 0)
    {
      Py_DECREF (&F32ToUQ15ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&UQ15ToF32ObjType);
  if (PyModule_AddObject (m, "UQ15ToF32", (PyObject *)&UQ15ToF32ObjType) < 0)
    {
      Py_DECREF (&UQ15ToF32ObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&ADCObjType);
  if (PyModule_AddObject (m, "ADC", (PyObject *)&ADCObjType) < 0)
    {
      Py_DECREF (&ADCObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
