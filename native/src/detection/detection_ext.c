/*
 * detection_ext.c — Python extension module detection
 *
 * Objects: LockDet
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "detection/detection_core.h"

#include "detection_ext_lockdet.c"

static PyObject *
_bind_marcum_q(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"m", "a", "b", NULL};
    int m = 0;
    double a = 0.0;
    double b = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "idd",
            _kwlist, &m, &a, &b))
        return NULL;
    return PyFloat_FromDouble(marcum_q(m, a, b));
}

static PyObject *
_bind_det_threshold(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"pfa", NULL};
    double pfa = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "d",
            _kwlist, &pfa))
        return NULL;
    return PyFloat_FromDouble(det_threshold(pfa));
}

static PyObject *
_bind_det_pd(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr", "dwell", "threshold", NULL};
    double snr = 0.0;
    int dwell = 0;
    double threshold = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "did",
            _kwlist, &snr, &dwell, &threshold))
        return NULL;
    return PyFloat_FromDouble(det_pd(snr, dwell, threshold));
}

static PyObject *
_bind_det_dwell(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr", "pd_min", "pfa", "max_dwell", NULL};
    double snr = 0.0;
    double pd_min = 0.0;
    double pfa = 0.0;
    int max_dwell = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dddi",
            _kwlist, &snr, &pd_min, &pfa, &max_dwell))
        return NULL;
    return PyLong_FromLong((long)det_dwell(snr, pd_min, pfa, max_dwell));
}

static PyObject *
_bind_det_snr(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"dwell", "pd_min", "pfa", NULL};
    int dwell = 0;
    double pd_min = 0.0;
    double pfa = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "idd",
            _kwlist, &dwell, &pd_min, &pfa))
        return NULL;
    return PyFloat_FromDouble(det_snr(dwell, pd_min, pfa));
}

static PyObject *
_bind_det_threshold_noncoherent(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"pfa", "n_noncoh", NULL};
    double pfa = 0.0;
    int n_noncoh = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "di",
            _kwlist, &pfa, &n_noncoh))
        return NULL;
    return PyFloat_FromDouble(det_threshold_noncoherent(pfa, n_noncoh));
}

static PyObject *
_bind_det_ema_alpha(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr_in_db", "snr_out_db", NULL};
    double snr_in_db = 0.0;
    double snr_out_db = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dd",
            _kwlist, &snr_in_db, &snr_out_db))
        return NULL;
    return PyFloat_FromDouble(det_ema_alpha(snr_in_db, snr_out_db));
}

static PyObject *
_bind_det_verify_count(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"p_look", "p_target", NULL};
    double p_look = 0.0;
    double p_target = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dd",
            _kwlist, &p_look, &p_target))
        return NULL;
    return PyLong_FromLong((long)det_verify_count(p_look, p_target));
}

static PyObject *
_bind_det_verify_delay(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"p_look", "n", NULL};
    double p_look = 0.0;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "di",
            _kwlist, &p_look, &n))
        return NULL;
    return PyFloat_FromDouble(det_verify_delay(p_look, n));
}

static PyObject *
_bind_det_threshold_f(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"pfa", "n", NULL};
    double pfa = 0.0;
    int n = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "di",
            _kwlist, &pfa, &n))
        return NULL;
    return PyFloat_FromDouble(det_threshold_f(pfa, n));
}

static PyObject *
_bind_det_pd_noncoherent(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr", "n_coh", "n_noncoh", "threshold", NULL};
    double snr = 0.0;
    int n_coh = 0;
    int n_noncoh = 0;
    double threshold = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "diid",
            _kwlist, &snr, &n_coh, &n_noncoh, &threshold))
        return NULL;
    return PyFloat_FromDouble(det_pd_noncoherent(snr, n_coh, n_noncoh, threshold));
}

static PyObject *
_bind_det_n_noncoh(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr", "n_coh", "pd_min", "pfa", "max_n_noncoh", NULL};
    double snr = 0.0;
    int n_coh = 0;
    double pd_min = 0.0;
    double pfa = 0.0;
    int max_n_noncoh = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "diddi",
            _kwlist, &snr, &n_coh, &pd_min, &pfa, &max_n_noncoh))
        return NULL;
    return PyLong_FromLong((long)det_n_noncoh(snr, n_coh, pd_min, pfa, max_n_noncoh));
}

static PyObject *
_bind_det_threshold_power(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"pfa", NULL};
    double pfa = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "d",
            _kwlist, &pfa))
        return NULL;
    return PyFloat_FromDouble(det_threshold_power(pfa));
}

static PyObject *
_bind_det_pd_power(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr_power", "dwell", "power_threshold", NULL};
    double snr_power = 0.0;
    int dwell = 0;
    double power_threshold = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "did",
            _kwlist, &snr_power, &dwell, &power_threshold))
        return NULL;
    return PyFloat_FromDouble(det_pd_power(snr_power, dwell, power_threshold));
}

static PyObject *
_bind_det_dwell_power(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"snr_power", "pd_min", "pfa", "max_dwell", NULL};
    double snr_power = 0.0;
    double pd_min = 0.0;
    double pfa = 0.0;
    int max_dwell = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "dddi",
            _kwlist, &snr_power, &pd_min, &pfa, &max_dwell))
        return NULL;
    return PyLong_FromLong((long)det_dwell_power(snr_power, pd_min, pfa, max_dwell));
}

static PyObject *
_bind_det_snr_power(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"dwell", "pd_min", "pfa", NULL};
    int dwell = 0;
    double pd_min = 0.0;
    double pfa = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "idd",
            _kwlist, &dwell, &pd_min, &pfa))
        return NULL;
    return PyFloat_FromDouble(det_snr_power(dwell, pd_min, pfa));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef detection_module_methods[] = {
    {"marcum_q", (PyCFunction)(void *)_bind_marcum_q, METH_VARARGS | METH_KEYWORDS,
     "Marcum Q function Q_M(a, b) for integer M >= 1.\n"
     "\n"
     "Probability that a Rice(a, sigma=1) random variable exceeds b. For M=1:\n"
     "Q_1(a, b) = P(Rice(a,1) > b). General integer M relates to the\n"
     "noncentral chi-squared CDF with 2M degrees of freedom.\n"
     "\n"
     "Computed via the Poisson-weighted chi-squared series (exact for M=1,\n"
     "converges in ~60 terms for practical a, b <= 15):\n"
     "\n"
     "Q_M(a, b) = sum_{k=0}^inf w_k * Q_{M+k}(0, b)\n"
     "\n"
     "where: w_k = exp(-u) * u^k/k! (u = a^2/2) Q_n(0,b) = exp(-v) *\n"
     "sum_{j=0}^{n-1} v^j/j! (v = b^2/2)\n"
     "\n"
     "Each iteration advances both the Poisson weight and the chi-sum in O(1)\n"
     "using the recurrences w_{k+1} = w_k * u/(k+1) and Q_{n+1}(0,b) =\n"
     "Q_n(0,b) + exp(-v)*v^n/n!. Total cost: O(K) where K ~ max(u, M) + safety\n"
     "margin.\n"
     "\n"
     "Special cases:\n"
     "\n"
     "- a = 0:   Q_M(0, b) = exp(-b^2/2) * sum_{j=0}^{M-1} (b^2/2)^j/j!\n"
     "- b <= 0:  Q_M(a, b) = 1.0\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "m : int\n"
     "    Integration order; must be >= 1.\n"
     "a : float\n"
     "    Non-centrality parameter (signal strength). a = 0 for H0.\n"
     "b : float\n"
     "    Threshold (same units as test_stat).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Q_M(a, b) in &#91;0, 1&#93;.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import marcum_q\n"
     ">>> round(marcum_q(m=1, a=0.0, b=1.0), 5)   # P(Rayleigh > 1) = exp(-0.5)\n"
     "0.60653\n"
     ">>> round(marcum_q(m=1, a=0.0, b=2.0), 5)   # exp(-2)\n"
     "0.13534\n"
     ">>> round(marcum_q(m=2, a=0.0, b=2.0), 5)   # 3*exp(-2)\n"
     "0.40601\n"
     ">>> round(marcum_q(m=1, a=2.0, b=1.0), 5)   # signal present (a=2)\n"
     "0.91811\n"},
    {"det_threshold", (PyCFunction)(void *)_bind_det_threshold, METH_VARARGS | METH_KEYWORDS,
     "Threshold eta for a given false-alarm probability.\n"
     "\n"
     "Exact closed-form inversion of Pfa = exp(-eta^2/2):\n"
     "\n"
     "eta = sqrt(-2 * ln(pfa))\n"
     "\n"
     "The threshold is independent of dwell and SNR; it depends only on the\n"
     "desired Pfa.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "pfa : float\n"
     "    Desired false-alarm probability; must be in (0, 1).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Threshold eta > 0.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_threshold\n"
     ">>> round(det_threshold(pfa=1e-6), 4)\n"
     "5.2565\n"},
    {"det_pd", (PyCFunction)(void *)_bind_det_pd, METH_VARARGS | METH_KEYWORDS,
     "Detection probability for given per-sample amplitude SNR and dwell.\n"
     "\n"
     "Computes Pd = Q_1(a, eta) where a = sqrt(2 * dwell) * snr.\n"
     "\n"
     "At snr = 0, det_pd returns Pfa (the false-alarm rate, as expected for a\n"
     "noise-only input). As snr or dwell increase, Pd approaches 1.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr : float\n"
     "    Per-sample amplitude SNR (signal / noise amplitude, linear). snr = 0\n"
     "    gives Pd = Pfa.\n"
     "dwell : int\n"
     "    Coherent integration depth; must be >= 1.\n"
     "threshold : float\n"
     "    Test-stat threshold eta, e.g. from det_threshold().\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Detection probability in &#91;0, 1&#93;.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_pd, det_threshold\n"
     ">>> thr = det_threshold(pfa=1e-6)\n"
     ">>> round(det_pd(snr=1.613, dwell=8, threshold=thr), 2)  # 8-dwell -> Pd~0.9\n"
     "0.9\n"
     ">>> round(det_pd(snr=0.0, dwell=8, threshold=thr), 6)    # snr=0 -> Pd=Pfa\n"
     "1e-06\n"},
    {"det_dwell", (PyCFunction)(void *)_bind_det_dwell, METH_VARARGS | METH_KEYWORDS,
     "Minimum dwell such that Pd >= pd_min for the given SNR and Pfa.\n"
     "\n"
     "Iterates dwell = 1, 2, ..., max_dwell, computing det_pd() at each step.\n"
     "Returns the first dwell that satisfies the Pd requirement, or -1 if none\n"
     "is found within max_dwell iterations.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr : float\n"
     "    Per-sample amplitude SNR (linear).\n"
     "pd_min : float\n"
     "    Required detection probability, e.g. 0.9.\n"
     "pfa : float\n"
     "    False-alarm probability; used to derive eta.\n"
     "max_dwell : int\n"
     "    Search upper bound; prevents infinite loops for low SNR.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Minimum dwell >= 1, or -1 if not achievable.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_dwell\n"
     ">>> det_dwell(snr=0.5, pd_min=0.9, pfa=1e-6, max_dwell=256)\n"
     "84\n"},
    {"det_snr", (PyCFunction)(void *)_bind_det_snr, METH_VARARGS | METH_KEYWORDS,
     "Minimum per-sample amplitude SNR achieving Pd >= pd_min.\n"
     "\n"
     "Binary search over SNR in &#91;0, hi&#93; where hi is doubled from 1.0\n"
     "until det_pd(hi, dwell, threshold) >= pd_min. 64 bisection iterations\n"
     "yield ~1e-19 relative precision on the final interval.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "dwell : int\n"
     "    Coherent integration depth; must be >= 1.\n"
     "pd_min : float\n"
     "    Required detection probability.\n"
     "pfa : float\n"
     "    False-alarm probability; used to derive eta.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Minimum amplitude SNR >= 0.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_snr, det_pd, det_threshold\n"
     ">>> snr = det_snr(dwell=8, pd_min=0.9, pfa=1e-6)\n"
     ">>> round(snr, 3)\n"
     "1.613\n"
     ">>> pd = det_pd(snr=snr, dwell=8, threshold=det_threshold(pfa=1e-6))\n"
     ">>> abs(pd - 0.9) < 1e-9   # det_snr inverts det_pd, to solver tolerance\n"
     "True\n"},
    {"det_threshold_noncoherent", (PyCFunction)(void *)_bind_det_threshold_noncoherent, METH_VARARGS | METH_KEYWORDS,
     "CFAR threshold eta_nc for a non-coherent detector of n_noncoh looks.\n"
     "\n"
     "Solves marcum_q(n_noncoh, 0, eta_nc) = pfa (the order-M central tail,\n"
     "monotone decreasing in eta_nc) by bisection. For n_noncoh = 1 this is\n"
     "the exact closed form sqrt(-2 ln pfa) (== det_threshold).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "pfa : float\n"
     "    Per-test false-alarm probability in (0, 1).\n"
     "n_noncoh : int\n"
     "    Number of non-coherent looks; must be >= 1.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Threshold eta_nc on the normalized statistic R.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_threshold_noncoherent, det_threshold\n"
     ">>> round(det_threshold_noncoherent(pfa=1e-3, n_noncoh=4), 3)\n"
     "5.111\n"
     ">>> det_threshold_noncoherent(pfa=1e-6, n_noncoh=1) == det_threshold(pfa=1e-6)\n"
     "True\n"},
    {"det_ema_alpha", (PyCFunction)(void *)_bind_det_ema_alpha, METH_VARARGS | METH_KEYWORDS,
     "EMA coefficient for a target estimator SNR (DC level in noise).\n"
     "\n"
     "Sizes a first-order EMA `y = (1-alpha)*y + alpha*x` that estimates a DC\n"
     "level from noisy i.i.d. measurements x. Per sample the estimator SNR\n"
     "(mean^2 / variance) is `snr_in`; the EMA improves it by its variance\n"
     "reduction `(2-alpha)/alpha`, so the output SNR is `snr_out = snr_in *\n"
     "(2-alpha)/alpha`. Solving for the coefficient:\n"
     "\n"
     "alpha = 2 * snr_in / (snr_in + snr_out) (SNRs linear)\n"
     "\n"
     "Returns 1.0 (no averaging) when snr_out_db <= snr_in_db. Typical inputs:\n"
     "a signal-free power reference |n|^2 is exponential (0 dB per sample); a\n"
     "lock signal at known C/N0 has per-look SNR from its coherent integration\n"
     "(minus squaring loss), and this picks the smoothing bandwidth that makes\n"
     "the lock decision variable meet a chosen decision SNR.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr_in_db : float\n"
     "    Per-sample estimator SNR, dB (mean^2 / variance).\n"
     "snr_out_db : float\n"
     "    Desired EMA-output estimator SNR, dB.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    EMA coefficient alpha in (0, 1].\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_ema_alpha\n"
     ">>> det_ema_alpha(0.0, 0.0)      # no gain requested -> no averaging\n"
     "1.0\n"
     ">>> round(1 / det_ema_alpha(0.0, 20.0), 1)   # 20 dB gain ~ 50 looks\n"
     "50.5\n"
     ">>> round(1 / det_ema_alpha(10.0, 30.0), 1)  # same 20 dB gain, shifted\n"
     "50.5\n"},
    {"det_verify_count", (PyCFunction)(void *)_bind_det_verify_count, METH_VARARGS | METH_KEYWORDS,
     "Verify count: consecutive looks needed to compound to a budget.\n"
     "\n"
     "n consecutive independent looks at per-look probability p compound to\n"
     "p^n, so the smallest n with `p_look^n <= p_target` is `ceil(ln p_target\n"
     "/ ln p_look)` (clamped to >= 1). One function serves both sides of a\n"
     "lock detector (lockdet_core.h): the declare count from (per-look pfa,\n"
     "false-declare budget) and the drop count from (per-look miss rate 1 -\n"
     "pd, false-drop budget). Degenerate inputs resolve naturally: a target\n"
     "already met by one look returns 1; p_look >= 1 can never compound below\n"
     "a smaller target and returns INT_MAX.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "p_look : float\n"
     "    Per-look probability (pfa or 1 - pd), in (0, 1).\n"
     "p_target : float\n"
     "    Compound probability budget, in (0, 1).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Smallest verify count n with p_look^n <= p_target.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_verify_count\n"
     ">>> det_verify_count(1e-3, 1e-6)   # two 1e-3 looks reach 1e-6\n"
     "2\n"
     ">>> det_verify_count(1e-3, 1e-9)\n"
     "3\n"
     ">>> det_verify_count(0.5, 1e-3)    # drop side: pd = 0.5 per look\n"
     "10\n"
     ">>> det_verify_count(1e-3, 0.5)    # budget already met -> 1\n"
     "1\n"},
    {"det_verify_delay", (PyCFunction)(void *)_bind_det_verify_delay, METH_VARARGS | METH_KEYWORDS,
     "Expected looks until a run of n consecutive successes completes.\n"
     "\n"
     "The mean waiting time of the consecutive-run process a lockdet verify\n"
     "counter implements: at per-look success probability p, the first run of\n"
     "n straight successes takes on average\n"
     "\n"
     "`E[T]` = (1 - p^n) / (p^n * (1 - p)) looks,\n"
     "\n"
     "which is the declare latency bought by a verify count of n (multiply by\n"
     "the look period for time). Limits are handled exactly: p = 1 gives n\n"
     "(the run completes immediately), p = 0 gives infinity.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "p_look : float\n"
     "    Per-look success probability (e.g. pd), in &#91;0, 1&#93;.\n"
     "n : int\n"
     "    Run length (the verify count); clamped to >= 1.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Expected number of looks to the first length-n run.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_verify_delay\n"
     ">>> det_verify_delay(1.0, 8)             # certain hits: exactly n\n"
     "8.0\n"
     ">>> round(det_verify_delay(0.5, 2), 6)   # 2 straight coin heads: 6\n"
     "6.0\n"
     ">>> round(det_verify_delay(0.9, 8), 1)\n"
     "13.2\n"},
    {"det_threshold_f", (PyCFunction)(void *)_bind_det_threshold_f, METH_VARARGS | METH_KEYWORDS,
     "Upper quantile of F(n, n) — the exact H0 law for a ratio test whose noise reference is estimated from as many samples as the signal sum.\n"
     "\n"
     "A chi-square threshold (det_threshold_noncoherent) prices a statistic\n"
     "normalised by a KNOWN noise power. When the noise power is instead\n"
     "estimated from n same-burst samples (the BurstDespreader lock test: sum\n"
     "Re^2 against sum Im^2), the ratio's tail fattens to F(n, n) and the\n"
     "chi-square gate realizes tens of times the priced pfa (41x at n = 16,\n"
     "pfa = 1e-3). This helper returns the exact gate: P(chi2_n / chi2_n > g)\n"
     "= I_{1/(1+g)}(n/2, n/2) = pfa, solved on the regularized incomplete beta\n"
     "— valid for every n >= 1, odd included. As n grows the estimate hardens\n"
     "and g approaches the known-noise value. Threshold a BurstDespreader as\n"
     "`lock_stat > sqrt(stat_n * det_threshold_f(pfa, stat_n))`.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "pfa : float\n"
     "    Tail probability budget, in (0, 1).\n"
     "n : int\n"
     "    Degrees of freedom on each side (>= 1).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    The F(n, n) upper-pfa quantile; 0 on invalid input.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_threshold_f\n"
     ">>> round(det_threshold_f(1e-3, 2), 6)  # exact: (1 - pfa)/pfa\n"
     "999.0\n"
     ">>> round(det_threshold_f(1e-3, 4), 4)\n"
     "53.4358\n"
     ">>> round(det_threshold_f(1e-3, 64), 4)  # hardens toward known-noise\n"
     "2.1931\n"},
    {"det_pd_noncoherent", (PyCFunction)(void *)_bind_det_pd_noncoherent, METH_VARARGS | METH_KEYWORDS,
     "Detection probability for n_noncoh non-coherent looks.\n"
     "\n"
     "Computes Pd = Q_{n_noncoh}(a, threshold) with the non-centrality a =\n"
     "sqrt(2 * n_coh * n_noncoh) * snr. At n_noncoh = 1 this is exactly\n"
     "det_pd(snr, n_coh, threshold); at snr = 0 it returns the per-test Pfa.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr : float\n"
     "    Per-sample amplitude SNR (signal / noise amplitude).\n"
     "n_coh : int\n"
     "    Coherent integration length in samples (dwell * N).\n"
     "n_noncoh : int\n"
     "    Number of non-coherent looks; must be >= 1.\n"
     "threshold : float\n"
     "    Threshold eta_nc, e.g. from det_threshold_noncoherent().\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Detection probability in &#91;0, 1&#93;.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_pd_noncoherent, det_pd, det_threshold\n"
     ">>> from doppler.detection import det_threshold_noncoherent\n"
     ">>> eta = det_threshold(pfa=1e-6)\n"
     ">>> det_pd_noncoherent(snr=0.5, n_coh=8, n_noncoh=1, threshold=eta) \\\n"
     "...     == det_pd(snr=0.5, dwell=8, threshold=eta)        # reduces to coherent\n"
     "True\n"
     ">>> eta4 = det_threshold_noncoherent(pfa=1e-3, n_noncoh=4)\n"
     ">>> round(det_pd_noncoherent(snr=0.3, n_coh=16, n_noncoh=4, threshold=eta4), 2)\n"
     "0.19\n"},
    {"det_n_noncoh", (PyCFunction)(void *)_bind_det_n_noncoh, METH_VARARGS | METH_KEYWORDS,
     "Minimum non-coherent looks achieving Pd >= pd_min at fixed n_coh.\n"
     "\n"
     "Iterates n_noncoh = 1, 2, ..., max_n_noncoh, recomputing the threshold\n"
     "(det_threshold_noncoherent, which grows with the look count) at each\n"
     "step. Returns the first look count that meets the Pd requirement, or -1\n"
     "if none does within max_n_noncoh. Used by the acquisition engine's (M,\n"
     "N_nc) split.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr : float\n"
     "    Per-sample amplitude SNR (linear).\n"
     "n_coh : int\n"
     "    Coherent integration length in samples (dwell * N).\n"
     "pd_min : float\n"
     "    Required detection probability, e.g. 0.9.\n"
     "pfa : float\n"
     "    Per-test false-alarm probability.\n"
     "max_n_noncoh : int\n"
     "    Search upper bound on the look count.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Minimum n_noncoh >= 1, or -1 if not achievable.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_n_noncoh\n"
     ">>> det_n_noncoh(snr=2.0, n_coh=16, pd_min=0.9, pfa=1e-3, max_n_noncoh=64)\n"
     "1\n"},
    {"det_threshold_power", (PyCFunction)(void *)_bind_det_threshold_power, METH_VARARGS | METH_KEYWORDS,
     "Power threshold p from Pfa for the power detector.\n"
     "\n"
     "Exact closed-form: P(Exponential(1) > p) = exp(-p) = Pfa, so\n"
     "\n"
     "p = -ln(Pfa)\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "pfa : float\n"
     "    Desired false-alarm probability; must be in (0, 1).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Threshold p > 0.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_threshold_power\n"
     ">>> round(det_threshold_power(pfa=1e-6), 3)   # -ln(1e-6) = 6*ln(10)\n"
     "13.816\n"},
    {"det_pd_power", (PyCFunction)(void *)_bind_det_pd_power, METH_VARARGS | METH_KEYWORDS,
     "Detection probability for the power detector.\n"
     "\n"
     "Pd = Q_1(sqrt(2·dwell·snr_power), sqrt(2·power_threshold))\n"
     "\n"
     "The result equals det_pd() at the equivalent amplitude SNR: power SNR\n"
     "`s` corresponds to amplitude SNR `sqrt(s)`, and the Q_1 arguments match.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr_power : float\n"
     "    Per-sample power SNR (signal power / noise power at the correlator\n"
     "    output, linear). 0 gives Pd = Pfa.\n"
     "dwell : int\n"
     "    Coherent integration depth; must be >= 1.\n"
     "power_threshold : float\n"
     "    Threshold p, e.g. from det_threshold_power().\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Detection probability in &#91;0, 1&#93;.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_pd_power, det_threshold_power\n"
     ">>> thr = det_threshold_power(pfa=1e-6)\n"
     ">>> round(det_pd_power(snr_power=2.6017, dwell=8, power_threshold=thr), 2)\n"
     "0.9\n"},
    {"det_dwell_power", (PyCFunction)(void *)_bind_det_dwell_power, METH_VARARGS | METH_KEYWORDS,
     "Minimum dwell such that Pd >= pd_min for the power detector.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "snr_power : float\n"
     "    Per-sample power SNR (linear).\n"
     "pd_min : float\n"
     "    Required detection probability.\n"
     "pfa : float\n"
     "    False-alarm probability; used to derive p.\n"
     "max_dwell : int\n"
     "    Search upper bound.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "int\n"
     "    Minimum dwell >= 1, or -1 if not achievable.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import det_dwell_power\n"
     ">>> det_dwell_power(snr_power=0.25, pd_min=0.9, pfa=1e-6, max_dwell=256)\n"
     "84\n"},
    {"det_snr_power", (PyCFunction)(void *)_bind_det_snr_power, METH_VARARGS | METH_KEYWORDS,
     "Minimum per-sample power SNR achieving Pd >= pd_min.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "dwell : int\n"
     "    Coherent integration depth; must be >= 1.\n"
     "pd_min : float\n"
     "    Required detection probability.\n"
     "pfa : float\n"
     "    False-alarm probability.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Minimum power SNR >= 0.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import (det_snr_power, det_pd_power,\n"
     "...                                det_threshold_power)\n"
     ">>> sp = det_snr_power(dwell=8, pd_min=0.9, pfa=1e-6)\n"
     ">>> round(sp, 4)\n"
     "2.6017\n"
     ">>> pd = det_pd_power(snr_power=sp, dwell=8,\n"
     "...                   power_threshold=det_threshold_power(pfa=1e-6))\n"
     ">>> abs(pd - 0.9) < 1e-9   # det_snr_power inverts det_pd_power\n"
     "True\n"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef detection_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "detection",
    .m_doc     = "Detection primitives: a portable lock detector (LockDet) applying level and time hysteresis to any scalar lock metric.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.detection import LockDet\n"
     ">>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)\n"
     ">>> [d.step(2.0), d.step(2.0)]\n"
     "[0, 1]\n",
    .m_size    = -1,
    .m_methods = detection_module_methods,
};

PyMODINIT_FUNC
PyInit_detection(void)
{
    import_array();
    if (PyType_Ready(&LockDetObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&detection_moduledef);
    if (!m) return NULL;
    Py_INCREF(&LockDetObjType);
    if (PyModule_AddObject(m, "LockDet", (PyObject *)&LockDetObjType) < 0) {
        Py_DECREF(&LockDetObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
