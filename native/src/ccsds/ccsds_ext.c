/*
 * ccsds_ext.c — Python extension module ccsds
 *
 * Objects:
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "ccsds/ccsds_core.h"

static PyObject *
_bind_asm_bits (PyObject *self, PyObject *Py_UNUSED (args))
{
  (void)self;
  npy_intp  _dim = (npy_intp)(32);
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_UINT8, 0);
  if (!_out)
    {
      return NULL;
    }
  asm_bits ((uint8_t *)PyArray_DATA ((PyArrayObject *)_out));
  return _out;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef ccsds_module_methods[] = {
  { "asm_bits", _bind_asm_bits, METH_NOARGS,
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
    "until doppler#900, and this alias exists so the third copy is not a\n"
    "Python one: it delegates to `ccsds_tm_asm_bits`, which is where the\n"
    "expansion is written and where `test_ccsds_tm_asm` holds it to the\n"
    "published pattern.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> b = asm_bits()\n"
    ">>> b.size, b[:8].tolist()          # 0x1A, first bit at the top\n"
    "(32, [0, 0, 0, 1, 1, 0, 1, 0])\n"
    ">>> int(\"\".join(map(str, b.tolist())), 2) == 0x1ACFFC1D\n"
    "True\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef ccsds_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "ccsds",
  .m_doc
  = "CCSDS 131.0-B TM Synchronization and Channel Coding — the literals the "
    "standard picked, beside the general layer rather than inside it.\n"
    "\n"
    "`asm_bits()` is the Attached Sync Marker as bits, which is what a "
    "receiver acquires on: pair it with `doppler.detection.SyncFinder` to "
    "find where a CADU starts in a bit stream. The standard's *transforms* "
    "are not here and are not coming here — describe a CADU with "
    "`doppler.wfm.FrameDesc` and the outer code, the randomiser and the inner "
    "code run through the general assembler.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> b = asm_bits()\n"
    ">>> int(\"\".join(map(str, b.tolist())), 2) == 0x1ACFFC1D\n"
    "True\n",
  .m_size    = -1,
  .m_methods = ccsds_module_methods,
};

PyMODINIT_FUNC
PyInit_ccsds (void)
{
  import_array ();

  PyObject *m = PyModule_Create (&ccsds_moduledef);
  if (!m)
    return NULL;

  return m;
}
