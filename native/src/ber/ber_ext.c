/*
 * ber_ext.c — Python extension module ber
 *
 * Objects: BerMeter
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "ber/ber_core.h"

#include "ber_ext_ber_meter.c"

static PyObject *
_bind_ber_theory_ser(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"m", "esn0", NULL};
    int m = 0;
    double esn0 = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "id",
            _kwlist, &m, &esn0))
        return NULL;
    return PyFloat_FromDouble(ber_theory_ser(m, esn0));
}

static PyObject *
_bind_ber_theory_ber(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"m", "esn0", NULL};
    int m = 0;
    double esn0 = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "id",
            _kwlist, &m, &esn0))
        return NULL;
    return PyFloat_FromDouble(ber_theory_ber(m, esn0));
}

static PyObject *
_bind_ber_esn0_db_for_ser(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"m", "ser", NULL};
    int m = 0;
    double ser = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "id",
            _kwlist, &m, &ser))
        return NULL;
    return PyFloat_FromDouble(ber_esn0_db_for_ser(m, ser));
}

static PyObject *
_bind_ber_evm_scatter_floor_db(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"m", NULL};
    int m = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i",
            _kwlist, &m))
        return NULL;
    return PyFloat_FromDouble(ber_evm_scatter_floor_db(m));
}

static PyObject *
_bind_ber_settle_syms(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"bn_timing", "bn_carrier", NULL};
    double bn_timing = 0.0;
    double bn_carrier = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dd",
            _kwlist, &bn_timing, &bn_carrier))
        return NULL;
    return PyLong_FromUnsignedLongLong((unsigned long long)ber_settle_syms(bn_timing, bn_carrier));
}

static PyObject *
_bind_ber_lock_symbol(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"flags", "sustain", "min_frac", NULL};
    PyObject *flags_obj = NULL;
    unsigned long long sustain_raw = 200;
    double min_frac = 0.9;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|Kd",
            _kwlist, &flags_obj, &sustain_raw, &min_frac))
        return NULL;
    size_t sustain = (size_t)sustain_raw;
    PyArrayObject *flags_arr = (PyArrayObject *)PyArray_FROM_OTF(
        flags_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
    if (!flags_arr) { return NULL; }
    const uint8_t *flags = (const uint8_t *)PyArray_DATA(flags_arr);
    size_t flags_len = (size_t)PyArray_SIZE(flags_arr);
    Py_DECREF(flags_arr);
    return PyLong_FromLong((long)ber_lock_symbol(flags, flags_len, sustain, min_frac));
}

static PyObject *
_bind_ber_evm_db(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"rx", "lo", "hi", "m", NULL};
    PyObject *rx_obj = NULL;
    unsigned long long lo_raw = 0;
    unsigned long long hi_raw = 0;
    int m = 4;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|KKi",
            _kwlist, &rx_obj, &lo_raw, &hi_raw, &m))
        return NULL;
    size_t lo = (size_t)lo_raw;
    size_t hi = (size_t)hi_raw;
    PyArrayObject *rx_arr = (PyArrayObject *)PyArray_FROM_OTF(
        rx_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!rx_arr) { return NULL; }
    const float complex *rx = (const float complex *)PyArray_DATA(rx_arr);
    size_t rx_len = (size_t)PyArray_SIZE(rx_arr);
    Py_DECREF(rx_arr);
    return PyFloat_FromDouble(ber_evm_db(rx, rx_len, lo, hi, m));
}

static PyObject *
_bind_ber_settle_from(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"budget", "timing_lock", "carrier_lock", "handover", NULL};
    unsigned long long budget_raw = 0ULL;
    int timing_lock = -1;
    int carrier_lock = -1;
    int handover = -1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "K|iii",
            _kwlist, &budget_raw, &timing_lock, &carrier_lock, &handover))
        return NULL;
    size_t budget = (size_t)budget_raw;
    return PyLong_FromUnsignedLongLong((unsigned long long)ber_settle_from(budget, timing_lock, carrier_lock, handover));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef ber_module_methods[] = {
    {"ber_theory_ser", (PyCFunction)(void *)_bind_ber_theory_ser, METH_VARARGS | METH_KEYWORDS,
     "Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR,\n"
     "not dB). BPSK Q(sqrt(2 Es/N0)); QPSK 2Q(sqrt(Es/N0)); 8PSK 2Q(sqrt(2\n"
     "Es/N0) sin(pi/8)). This is a COHERENT bound: a differentially-decoded\n"
     "rate is ~2x it, so pairing a differential measurement with this curve\n"
     "invents a factor of two of implementation loss.\n"
     "\n"
     "`BPSK: Q(sqrt(2 Es/N0))`, `QPSK: 2 Q(sqrt(Es/N0))`, `8PSK: 2 Q(sqrt(2\n"
     "Es/N0) sin(pi/8))` — the nearest-neighbour union bound, tight to well\n"
     "under a percent at any Es/N0 worth testing at.\n"
     "\n"
     "**This is a COHERENT bound.** A differentially-decoded rate is ~2x it,\n"
     "because a differential decision fails when either of its two symbols is\n"
     "wrong (measured 1.88-2.11 across M and both receiver paths). Pairing a\n"
     "differential measurement with this curve invents a factor of two of\n"
     "\"implementation loss\".\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "m : int\n"
     "    Input.\n"
     "esn0 : float\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Output.\n"},
    {"ber_theory_ber", (PyCFunction)(void *)_bind_ber_theory_ber, METH_VARARGS | METH_KEYWORDS,
     "Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR). BPSK and\n"
     "Gray QPSK are exactly Q(sqrt(2 Eb/N0)); 8PSK uses SER/log2(M).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "m : int\n"
     "    Input.\n"
     "esn0 : float\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Output.\n"},
    {"ber_esn0_db_for_ser", (PyCFunction)(void *)_bind_ber_esn0_db_for_ser, METH_VARARGS | METH_KEYWORDS,
     "Es/N0 (dB) at which the coherent bound equals `ser`. How an\n"
     "implementation loss is quoted honestly: convert the MEASURED rate to\n"
     "the Es/N0 theory would need to produce it, and subtract. A loss in dB\n"
     "is comparable across M and across operating points; a ratio of rates is\n"
     "not.\n"
     "\n"
     "How an implementation loss is quoted honestly: convert the MEASURED\n"
     "rate to the Es/N0 theory would need to produce it, and subtract. A loss\n"
     "in dB is comparable across M and across operating points; a ratio of\n"
     "rates is not.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "m : int\n"
     "    Input.\n"
     "ser : float\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Output.\n"},
    {"ber_evm_scatter_floor_db", (PyCFunction)(void *)_bind_ber_evm_scatter_floor_db, METH_VARARGS | METH_KEYWORDS,
     "EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation --\n"
     "the FLOOR of a self-referenced EVM, i.e. what a completely destroyed\n"
     "constellation reads: -1.4 dB at BPSK, -7.0 at QPSK, -12.9 at 8PSK. ANY\n"
     "fixed EVM threshold must be stated against this, never against 0 dB:\n"
     "'scattered reads ~0 dB' is the BPSK limit only, and at 8PSK a stream\n"
     "with no carrier recovery reads the same -12.9 dB a healthy 13 dB link\n"
     "does, so a `< -12.0` assertion is satisfied by pure noise. The room\n"
     "between 'on the bound at the SER=1e-3 anchor' and 'completely broken'\n"
     "collapses as M grows (5.4 / 3.3 / 2.8 dB), so at high M the EVM cannot\n"
     "carry a verdict alone. Not to be confused with the NOISE floor\n"
     "-(Es/N0).\n"
     "\n"
     "The FLOOR of a self-referenced EVM: what a completely destroyed\n"
     "constant-modulus constellation reads. Slicing a unit-modulus point at a\n"
     "uniformly random phase to its nearest of M neighbours leaves `E|e|^2 =\n"
     "2 - 2 sin(pi/M)/(pi/M)`: **-1.4 dB at BPSK, -7.0 at QPSK, -12.9 at\n"
     "8PSK**.\n"
     "\n"
     "**Any fixed EVM threshold must be stated against this, never against 0\n"
     "dB.** \"Scattered reads ~0 dB\" is the BPSK limit only. At 8PSK a stream\n"
     "with no carrier recovery at all reads -12.9 dB — which is also what a\n"
     "perfectly healthy 13 dB link reads — so a `< -12.0` assertion is\n"
     "satisfied by pure noise. That was live in this repo's own receiver\n"
     "tests until 2026-07-27. The room between \"on the bound at the SER=1e-3\n"
     "anchor\" and \"completely broken\" collapses as M grows: 5.4 dB at BPSK,\n"
     "3.3 at QPSK, 2.8 at 8PSK, so at high M the EVM cannot carry a verdict\n"
     "by itself.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "m : int\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Output.\n"},
    {"ber_settle_syms", (PyCFunction)(void *)_bind_ber_settle_syms, METH_VARARGS | METH_KEYWORDS,
     "Symbols to discard before a steady-state measurement means anything:\n"
     "2*(5/bn_timing + 5/bn_carrier). Three factors, and skipping any\n"
     "produces a confident wrong number -- 5/Bn per loop is the standard\n"
     "second-order settling time (in SYMBOLS, since both bn are symbol-rate\n"
     "normalised); the two budgets ADD because the loops are cascaded (the\n"
     "carrier discriminator reads the on-time strobe, so it cannot converge\n"
     "until timing has); and the sum DOUBLES for joint tracking. This is a\n"
     "FLOOR, not the answer: take the max of it and every lock indicator the\n"
     "receiver publishes, plus the handover instant again if one is enabled.\n"
     "Pass a loop's bn as 0 if it is not running.\n"
     "\n"
     "`2 * (5/bn_timing + 5/bn_carrier)`. Three factors, and skipping any of\n"
     "them produces a confident wrong number: 5/Bn per loop is the standard\n"
     "second-order settling time (in symbols, because both `bn` are\n"
     "normalised to the SYMBOL rate); the two budgets ADD because the loops\n"
     "are CASCADED (the carrier discriminator reads the on-time strobe, so it\n"
     "cannot converge until timing has); and the sum DOUBLES for joint\n"
     "tracking, where each loop sees the other's transient as a disturbance.\n"
     "\n"
     "This is a floor, not the answer — take the max of it and every lock\n"
     "indicator the receiver publishes, plus the handover instant again if\n"
     "one is enabled. Pass a loop's `bn` as 0 if it is not running.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "bn_timing : float\n"
     "    Input.\n"
     "bn_carrier : float\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Output.\n"},
    {"ber_lock_symbol", (PyCFunction)(void *)_bind_ber_lock_symbol, METH_VARARGS | METH_KEYWORDS,
     "First symbol from which a verify-counted lock flag is SUSTAINED, or\n"
     "-1 for 'never locked'. Sustained means `sustain` consecutive symbols\n"
     "high AND at least `min_frac` of everything after that point high too:\n"
     "the run rejects a single lucky decision, the fraction rejects a\n"
     "detector that declares early then flaps. Dating the lock by the FINAL\n"
     "contiguous run instead is right with no noise and badly wrong with it\n"
     "-- one late dip once moved a reported lock from 415 to 2286 and left no\n"
     "measurement window at all. The -1 is deliberate: it forces the caller\n"
     "to say 'never locked' rather than quietly measure a transient.\n"
     "\n"
     "\"Sustained\" is sustain consecutive symbols high AND at least min_frac\n"
     "of everything after that point high too. Both halves carry weight: the\n"
     "run rejects a single lucky decision, the fraction rejects a detector\n"
     "that declares early then flaps. Dating the lock by the FINAL contiguous\n"
     "run instead is right with no noise and badly wrong with it — one late\n"
     "dip once moved a reported lock from 415 to 2286 and left no measurement\n"
     "window.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "flags : NDArray[np.uint8]\n"
     "    Input.\n"
     "sustain : int\n"
     "    Input.\n"
     "min_frac : float\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    The symbol index, or -1 for \"never locked\" — the honest answer,\n"
     "    which forces the caller to say so rather than measure a transient.\n"},
    {"ber_evm_db", (PyCFunction)(void *)_bind_ber_evm_db, METH_VARARGS | METH_KEYWORDS,
     "Self-referenced EVM (dB) over an EXPLICIT window [lo, hi): each\n"
     "symbol against the stream's OWN hard decision, with the constellation\n"
     "rotation estimated from the data. References neither the transmitted\n"
     "symbols nor a lag, so it cannot be fooled by an alignment search. A\n"
     "locked matched-filter output reads EVM_dB ~ -(Es/N0)_dB (an I/Q-plane\n"
     "quantity -- no factor of two; quoting one flatters the result by 3 dB).\n"
     "Read it against ber_evm_scatter_floor_db(m), NEVER against 0 dB. The\n"
     "window is explicit because BER and EVM must be measured on the SAME\n"
     "one: a convenience back-half default scores a different window than the\n"
     "error rate did, and the two eventually disagree in a way that reads as\n"
     "a receiver defect rather than the harness bug it is. Returns 0.0 for a\n"
     "window under 20 symbols.\n"
     "\n"
     "Scores each symbol against the stream's OWN hard decision, with the\n"
     "constellation rotation estimated from the data — so it references\n"
     "neither the transmitted symbols nor a lag, and cannot be fooled by an\n"
     "alignment search. At a matched-filter output the error vector IS the\n"
     "complex noise, so a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`. EVM\n"
     "is an I/Q-plane quantity: there is no factor of two — that belongs to\n"
     "an I-only measurement, and quoting it flatters the result by 3 dB.\n"
     "\n"
     "**Pass the real `m`**, and read the result against\n"
     "ber_evm_scatter_floor_db(m), never against 0 dB.\n"
     "\n"
     "The window is EXPLICIT because BER and EVM must be measured on the SAME\n"
     "one. A convenience \"back half\" default silently scores a different\n"
     "window than the error rate did, and the two eventually disagree in a\n"
     "way that reads as a receiver defect rather than the harness bug it is.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "rx : NDArray[np.complex64]\n"
     "    Input.\n"
     "lo : int\n"
     "    Input.\n"
     "hi : int\n"
     "    Input.\n"
     "m : int\n"
     "    Input.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    EVM in dB, or 0.0 (\"no lock\") for a window under 20 symbols.\n"},
    {"ber_settle_from", (PyCFunction)(void *)_bind_ber_settle_from, METH_VARARGS | METH_KEYWORDS,
     "Where a steady-state measurement may start: max(budget, timing lock,\n"
     "carrier lock, handover + budget). The analytic budget and the\n"
     "receiver's own indicators are both fallible in the SAME direction, so\n"
     "whichever settles last decides. A handover settles last of all -- it\n"
     "fires on carrier lock plus a warmup, strictly after every other term,\n"
     "and the decision-directed loop then has its own transient, so it\n"
     "contributes its instant PLUS the budget again (measured on 8PSK:\n"
     "handover at symbol 2525 against a 2000-symbol budget, SER 5.95x the\n"
     "coherent bound from 2000 versus 1.68x from 4525). Pass -1 for an\n"
     "indicator the receiver does not publish, which is what\n"
     "ber_lock_symbol() returns for 'never locked'. A -1 timing or carrier\n"
     "lock means there is NO valid steady-state window -- check that yourself\n"
     "before trusting the return; a -1 handover is not a failure, since a\n"
     "pure-NDA receiver never publishes one.\n"
     "\n"
     "The POLICY for where a steady-state window may start, in one place:\n"
     "`max(budget, timing lock, carrier lock, handover + budget)`. The\n"
     "analytic budget and the receiver's own indicators are both fallible in\n"
     "the SAME direction, so whichever settles last decides.\n"
     "\n"
     "**A handover settles last of all.** With `acq_to_track` on it fires on\n"
     "carrier lock plus a warmup — strictly after the budget and after every\n"
     "lock indicator — and the decision-directed loop then has its own\n"
     "transient, so it contributes `its instant + the budget again`. Measured\n"
     "on 8PSK at its SER=1e-3 anchor: handover at symbol 2525 against a\n"
     "2000-symbol budget, SER 5.95x the coherent bound measured from 2000 and\n"
     "1.68x from 4525.\n"
     "\n"
     "Pass -1 for any indicator the receiver does not publish (which is what\n"
     "ber_lock_symbol() returns for \"never locked\"). **A -1 timing or carrier\n"
     "lock means there is NO valid steady-state window** — check that\n"
     "yourself before trusting the return; a -1 handover is not a failure,\n"
     "because a pure-NDA receiver never publishes one.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "budget : int\n"
     "    ber_settle_syms() of the loops in use.\n"
     "timing_lock : int\n"
     "    ber_lock_symbol() of the timing flag, or -1.\n"
     "carrier_lock : int\n"
     "    ber_lock_symbol() of the carrier flag, or -1.\n"
     "handover : int\n"
     "    ber_lock_symbol() of the tracking flag, or -1.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    First symbol of the measurement window.\n"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef ber_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "ber",
    .m_doc     = "Bit-error-rate measurement: a BerMeter that aligns a recovered bit stream to a reference and scores errors.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> import numpy as np\n"
     ">>> from doppler.ber import BerMeter\n"
     ">>> truth = (np.arange(64) & 1).astype(np.uint8)\n"
     ">>> m = BerMeter()\n"
     ">>> _ = m.set_truth(truth)\n"
     ">>> m.score((1.0 - 2.0 * truth).astype(np.complex64))\n"
     "0\n",
    .m_size    = -1,
    .m_methods = ber_module_methods,
};

PyMODINIT_FUNC
PyInit_ber(void)
{
    import_array();
    if (PyType_Ready(&BerMeterObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&ber_moduledef);
    if (!m) return NULL;
    Py_INCREF(&BerMeterObjType);
    if (PyModule_AddObject(m, "BerMeter", (PyObject *)&BerMeterObjType) < 0) {
        Py_DECREF(&BerMeterObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
