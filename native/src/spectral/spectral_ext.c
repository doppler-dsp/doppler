/*
 * spectral_ext.c — Python extension module spectral
 *
 * Objects: FFT, FFT2D, Corr, Corr2D, CorrDetector, CorrDetector2D, PSD
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "spectral/spectral_core.h"

#include "spectral_ext_fft.c"
#include "spectral_ext_fft2d.c"
#include "spectral_ext_corr.c"
#include "spectral_ext_corr2d.c"
#include "spectral_ext_detector.c"
#include "spectral_ext_detector2d.c"
#include "spectral_ext_psd.c"

static PyObject *
_bind_kaiser_enbw(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"w", NULL};
    PyObject *w_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &w_obj))
        return NULL;
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!w_arr) { return NULL; }
    const float *w = (const float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    Py_DECREF(w_arr);
    return PyFloat_FromDouble((double)kaiser_enbw(w, w_len));
}

static PyObject *
_bind_kaiser_window(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"w", "beta", NULL};
    PyObject *w_obj = NULL;
    float beta = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Of",
            _kwlist, &w_obj, &beta))
        return NULL;
    /* Require the exact dtype AND C-contiguity — either mismatch makes
     * the marshal write into a temp copy, not the caller's buffer. */
    if (!PyArray_Check(w_obj) ||
        PyArray_TYPE((PyArrayObject *)w_obj) != NPY_FLOAT ||
        !PyArray_IS_C_CONTIGUOUS((PyArrayObject *)w_obj) ||
        !PyArray_ISWRITEABLE((PyArrayObject *)w_obj)) {
        PyErr_SetString(PyExc_TypeError,
            "w must be a writable, C-contiguous"
            " ndarray of the output dtype");
        return NULL;
    }
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
    if (!w_arr) { return NULL; }
    float *w = (float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    kaiser_window(w, w_len, beta);
    Py_DECREF(w_arr);
    Py_RETURN_NONE;
}

static PyObject *
_bind_kaiser_beta_for_sidelobe(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"atten_db", NULL};
    double atten_db = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "d",
            _kwlist, &atten_db))
        return NULL;
    return PyFloat_FromDouble(kaiser_beta_for_sidelobe(atten_db));
}

static PyObject *
_bind_hann_window(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"w", NULL};
    PyObject *w_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &w_obj))
        return NULL;
    /* Require the exact dtype AND C-contiguity — either mismatch makes
     * the marshal write into a temp copy, not the caller's buffer. */
    if (!PyArray_Check(w_obj) ||
        PyArray_TYPE((PyArrayObject *)w_obj) != NPY_FLOAT ||
        !PyArray_IS_C_CONTIGUOUS((PyArrayObject *)w_obj) ||
        !PyArray_ISWRITEABLE((PyArrayObject *)w_obj)) {
        PyErr_SetString(PyExc_TypeError,
            "w must be a writable, C-contiguous"
            " ndarray of the output dtype");
        return NULL;
    }
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
    if (!w_arr) { return NULL; }
    float *w = (float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    hann_window(w, w_len);
    Py_DECREF(w_arr);
    Py_RETURN_NONE;
}

static PyObject *
_bind_blackman_harris_window(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"w", NULL};
    PyObject *w_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &w_obj))
        return NULL;
    /* Require the exact dtype AND C-contiguity — either mismatch makes
     * the marshal write into a temp copy, not the caller's buffer. */
    if (!PyArray_Check(w_obj) ||
        PyArray_TYPE((PyArrayObject *)w_obj) != NPY_FLOAT ||
        !PyArray_IS_C_CONTIGUOUS((PyArrayObject *)w_obj) ||
        !PyArray_ISWRITEABLE((PyArrayObject *)w_obj)) {
        PyErr_SetString(PyExc_TypeError,
            "w must be a writable, C-contiguous"
            " ndarray of the output dtype");
        return NULL;
    }
    PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF(
        w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
    if (!w_arr) { return NULL; }
    float *w = (float *)PyArray_DATA(w_arr);
    size_t w_len = (size_t)PyArray_SIZE(w_arr);
    blackman_harris_window(w, w_len);
    Py_DECREF(w_arr);
    Py_RETURN_NONE;
}

static PyObject *
_bind_magnitude_db_cf32(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"x", "lin_floor", "offset_db", NULL};
    PyObject *x_obj = NULL;
    float lin_floor = 0.0f;
    float offset_db = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Off",
            _kwlist, &x_obj, &lin_floor, &offset_db))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const float complex *x = (const float complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    npy_intp _dim = (npy_intp)x_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_FLOAT, 0);
    if (!_out) {Py_DECREF(x_arr); return NULL; }
    magnitude_db_cf32(x, x_len, (float *)PyArray_DATA((PyArrayObject *)_out), lin_floor, offset_db);
    Py_DECREF(x_arr);
    return _out;
}

static PyObject *
_bind_magnitude_db_cf64(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"x", "lin_floor", "offset_db", NULL};
    PyObject *x_obj = NULL;
    double lin_floor = 0.0;
    float offset_db = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Odf",
            _kwlist, &x_obj, &lin_floor, &offset_db))
        return NULL;
    PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF(
        x_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
    if (!x_arr) { return NULL; }
    const double complex *x = (const double complex *)PyArray_DATA(x_arr);
    size_t x_len = (size_t)PyArray_SIZE(x_arr);
    npy_intp _dim = (npy_intp)x_len;
    PyObject *_out = PyArray_EMPTY(1, &_dim, NPY_FLOAT, 0);
    if (!_out) {Py_DECREF(x_arr); return NULL; }
    magnitude_db_cf64(x, x_len, (float *)PyArray_DATA((PyArrayObject *)_out), lin_floor, offset_db);
    Py_DECREF(x_arr);
    return _out;
}

static PyObject *
_bind_find_peaks_f32(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"db", "n_peaks", "min_db", NULL};
    PyObject *db_obj = NULL;
    unsigned long long n_peaks_raw = 0ULL;
    float min_db = 0.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OKf",
            _kwlist, &db_obj, &n_peaks_raw, &min_db))
        return NULL;
    size_t n_peaks = (size_t)n_peaks_raw;
    PyArrayObject *db_arr = (PyArrayObject *)PyArray_FROM_OTF(
        db_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!db_arr) { return NULL; }
    const float *db = (const float *)PyArray_DATA(db_arr);
    size_t db_len = (size_t)PyArray_SIZE(db_arr);
    size_t _max = (size_t)n_peaks;
    dp_peak_t *_results = (dp_peak_t *)malloc(_max * sizeof(dp_peak_t));
    if (!_results) {Py_DECREF(db_arr); return PyErr_NoMemory(); }
    size_t _n = find_peaks_f32(db, db_len, n_peaks, min_db, _results);
    Py_DECREF(db_arr);
    PyObject *_lst = PyList_New((Py_ssize_t)_n);
    if (!_lst) { free(_results); return NULL; }
    for (size_t _i = 0; _i < _n; _i++) {
        PyObject *_tup = Py_BuildValue("(NN)", PyFloat_FromDouble((double)_results[_i].freq_norm), PyFloat_FromDouble((double)_results[_i].amplitude_db));
        if (!_tup) { free(_results); Py_DECREF(_lst); return NULL; }
        PyList_SET_ITEM(_lst, (Py_ssize_t)_i, _tup);
    }
    free(_results);
    return _lst;
}

static PyObject *
_bind_obw_from_power(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"pwr", "fs", "frac", NULL};
    PyObject *pwr_obj = NULL;
    double fs = 0.0;
    double frac = 0.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Odd",
            _kwlist, &pwr_obj, &fs, &frac))
        return NULL;
    PyArrayObject *pwr_arr = (PyArrayObject *)PyArray_FROM_OTF(
        pwr_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
    if (!pwr_arr) { return NULL; }
    const double *pwr = (const double *)PyArray_DATA(pwr_arr);
    size_t pwr_len = (size_t)PyArray_SIZE(pwr_arr);
    Py_DECREF(pwr_arr);
    return PyFloat_FromDouble(obw_from_power(pwr, pwr_len, fs, frac));
}

static PyObject *
_bind_noise_floor_db(PyObject *self, PyObject *args, PyObject *kwds)
{
    (void)self;
    static char *_kwlist[] = {"db", NULL};
    PyObject *db_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O",
            _kwlist, &db_obj))
        return NULL;
    PyArrayObject *db_arr = (PyArrayObject *)PyArray_FROM_OTF(
        db_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
    if (!db_arr) { return NULL; }
    const float *db = (const float *)PyArray_DATA(db_arr);
    size_t db_len = (size_t)PyArray_SIZE(db_arr);
    Py_DECREF(db_arr);
    return PyFloat_FromDouble(noise_floor_db(db, db_len));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef spectral_module_methods[] = {
    {"kaiser_enbw", (PyCFunction)(void *)_bind_kaiser_enbw, METH_VARARGS | METH_KEYWORDS,
     "Compute the equivalent noise bandwidth of a window in bins. ENBW = N\n"
     "* sum(w²) / (sum(w))² quantifies how many noise bins the window smears\n"
     "into the main lobe. A rectangular window has ENBW = 1.0; tapered\n"
     "windows are > 1.0. Works with any window type, not just Kaiser.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "w : NDArray[np.float32]\n"
     "    Float32 window coefficients array; any length >= 1.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    ENBW in bins (dimensionless).\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import kaiser_enbw, hann_window\n"
     ">>> import numpy as np\n"
     ">>> w = np.zeros(8, dtype=np.float32)\n"
     ">>> hann_window(w)\n"
     ">>> round(kaiser_enbw(w), 4)\n"
     "1.7143\n"},
    {"kaiser_window", (PyCFunction)(void *)_bind_kaiser_window, METH_VARARGS | METH_KEYWORDS,
     "Fill w with a Kaiser window of shape parameter beta. I0 is computed\n"
     "via the converging power-series expansion. Increasing beta raises\n"
     "sidelobe attenuation at the cost of a wider main lobe (beta=0 →\n"
     "rectangular, beta≈6 → ~60 dB sidelobe rejection). The output is\n"
     "normalised so that `w[0]` = `w[N-1]` = I0(0)/I0(beta).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "w : NDArray[np.float32]\n"
     "    Output buffer modified in-place; must be length >= 1.\n"
     "beta : float\n"
     "    Window shape parameter (float, >= 0).\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import kaiser_window\n"
     ">>> import numpy as np\n"
     ">>> w = np.zeros(8, dtype=np.float32)\n"
     ">>> kaiser_window(w, 6.0)\n"
     ">>> [round(v, 4) for v in w.tolist()]\n"
     "[0.0149, 0.1998, 0.5913, 0.9454, 0.9454, 0.5913, 0.1998, 0.0149]\n"},
    {"kaiser_beta_for_sidelobe", (PyCFunction)(void *)_bind_kaiser_beta_for_sidelobe, METH_VARARGS | METH_KEYWORDS,
     "Kaiser beta achieving a target *window* peak-sidelobe attenuation.\n"
     "\n"
     "Inverts the Kaiser window-design formula (Kaiser 1974) so the window's\n"
     "own peak sidelobe sits at -atten_db: A > 60 dB : beta = 0.12438 * (A +\n"
     "6.3) 13.26 < A <= 60 dB : beta = 0.76609*(A-13.26)^0.4 +\n"
     "0.09834*(A-13.26) A <= 13.26 dB : beta = 0.0 (rectangular, sidelobes ~\n"
     "-13.3 dB) Picking the smallest beta meeting a dynamic-range target\n"
     "keeps the main lobe (hence ENBW / resolution bandwidth) as narrow as\n"
     "the requirement allows — the basis of the measurement suite's\n"
     "auto-window selection.\n"
     "\n"
     "This differs from doppler.resample.kaiser_beta(), which uses the Kaiser\n"
     "*FIR-filter* formula (A there is a filter stopband ripple, not a window\n"
     "sidelobe — about 13 dB lower for the same beta).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "atten_db : float\n"
     "    Desired window peak-sidelobe attenuation in dB (positive).\n"
     "\n"
     "Returns\n"
     "-------\n"
     "float\n"
     "    Kaiser beta (>= 0.0).\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import kaiser_beta_for_sidelobe\n"
     ">>> round(kaiser_beta_for_sidelobe(90.0), 4)\n"
     "11.9778\n"
     ">>> kaiser_beta_for_sidelobe(10.0)\n"
     "0.0\n"},
    {"hann_window", (PyCFunction)(void *)_bind_hann_window, METH_VARARGS | METH_KEYWORDS,
     "Fill w with a Hann (raised-cosine) window. Computes w(k) = 0.5*(1 -\n"
     "cos(2π k/(N-1))) for k = 0..N-1. The window tapers smoothly to zero at\n"
     "both endpoints, providing ~31 dB first-sidelobe rejection. Takes no\n"
     "shape parameter; use Kaiser for adjustable roll-off.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "w : NDArray[np.float32]\n"
     "    Output buffer modified in-place; must be length >= 1.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import hann_window\n"
     ">>> import numpy as np\n"
     ">>> w = np.zeros(8, dtype=np.float32)\n"
     ">>> hann_window(w)\n"
     ">>> [round(v, 4) for v in w.tolist()]\n"
     "[0.0, 0.1883, 0.6113, 0.9505, 0.9505, 0.6113, 0.1883, 0.0]\n"},
    {"blackman_harris_window", (PyCFunction)(void *)_bind_blackman_harris_window, METH_VARARGS | METH_KEYWORDS,
     "Fill w with a 4-term Blackman-Harris window. Computes the minimum\n"
     "4-term Blackman-Harris window: w(k) = 0.35875 - 0.48829*cos(2πk/(N-1))\n"
     "+ 0.14128*cos(4πk/(N-1)) - 0.01168*cos(6πk/(N-1)) for k = 0..N-1.\n"
     "Provides approximately 92 dB first-sidelobe rejection, far deeper than\n"
     "Hann (~31 dB) or Kaiser at β=8 (~80 dB). Use for quantization and\n"
     "decimation spectra where you need to see low-level artefacts below the\n"
     "noise floor.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "w : NDArray[np.float32]\n"
     "    Output buffer modified in-place; must be length >= 1.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import blackman_harris_window\n"
     ">>> import numpy as np\n"
     ">>> w = np.zeros(8, dtype=np.float32)\n"
     ">>> blackman_harris_window(w)\n"
     ">>> [round(v, 4) for v in w.tolist()]\n"
     "[0.0001, 0.0334, 0.3328, 0.8894, 0.8894, 0.3328, 0.0334, 0.0001]\n"},
    {"magnitude_db_cf32", (PyCFunction)(void *)_bind_magnitude_db_cf32, METH_VARARGS | METH_KEYWORDS,
     "Convert a CF32 complex spectrum to F32 dB magnitudes. Computes\n"
     "out(k) = 20*log10(max(|x(k)|, lin_floor)) + offset_db for each bin. The\n"
     "lin_floor guard prevents log10(0); a value of 1e-12 corresponds to a\n"
     "-240 dB noise floor. offset_db shifts the entire output for calibration\n"
     "(e.g., normalise to 0 dBFS).\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "x : NDArray[np.complex64]\n"
     "    CF32 complex spectrum array, length x_len.\n"
     "lin_floor : float\n"
     "    Linear amplitude floor (must be > 0, e.g. 1e-12).\n"
     "offset_db : float\n"
     "    Calibration offset added to every output bin.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.float32]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import magnitude_db_cf32\n"
     ">>> import numpy as np\n"
     ">>> x = np.array([1+0j, 0.1+0j, 0+0j], dtype=np.complex64)\n"
     ">>> magnitude_db_cf32(x, 1e-12, 0.0).tolist()\n"
     "[0.0, -20.0, -240.0]\n"},
    {"magnitude_db_cf64", (PyCFunction)(void *)_bind_magnitude_db_cf64, METH_VARARGS | METH_KEYWORDS,
     "Convert a CF64 complex spectrum to F32 dB magnitudes.\n"
     "Double-precision variant of magnitude_db_cf32(). Accepts a CF64 input\n"
     "array and a double lin_floor; output is still F32 because downstream\n"
     "display code typically works in single precision. The formula and\n"
     "offset_db semantics are identical.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "x : NDArray[np.complex128]\n"
     "    CF64 complex spectrum array, length x_len.\n"
     "lin_floor : float\n"
     "    Linear amplitude floor (double, must be > 0).\n"
     "offset_db : float\n"
     "    Calibration offset added to every output bin.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "NDArray[np.float32]\n"
     "    Output.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import magnitude_db_cf64\n"
     ">>> import numpy as np\n"
     ">>> x = np.array([1+0j, 10+0j], dtype=np.complex128)\n"
     ">>> magnitude_db_cf64(x, 1e-12, 0.0).tolist()\n"
     "[0.0, 20.0]\n"},
    {"find_peaks_f32", (PyCFunction)(void *)_bind_find_peaks_f32, METH_VARARGS | METH_KEYWORDS,
     "Find up to n_peaks local maxima in a DC-centred F32 dB spectrum.\n"
     "Three-step algorithm: (1) local-max scan — `db[k]` > `db[k-1]` &&\n"
     "`db[k]` >= `db[k+1]` with `db[k]` > min_db; (2) parabolic interpolation\n"
     "on each local maximum to produce sub-bin freq_norm accuracy; (3) sort\n"
     "descending and return the top n_peaks. freq_norm is DC-centred: bin i\n"
     "maps to freq_norm = (i - N/2) / N so DC (bin N/2) → 0.0 and the first\n"
     "negative frequency bin → −0.5. The spectrum must have at least 3 bins.\n"
     "\n"
     "Parameters\n"
     "----------\n"
     "db : NDArray[np.float32]\n"
     "    F32 dB spectrum, DC-centred, length >= 3.\n"
     "n_peaks : int\n"
     "    Maximum number of peaks to return.\n"
     "min_db : float\n"
     "    Amplitude gate; local maxima below this are discarded.\n"
     "\n"
     "Returns\n"
     "-------\n"
     "Any\n"
     "    Number of dp_peak_t entries written to result.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> from doppler.spectral import find_peaks_f32\n"
     ">>> import numpy as np\n"
     ">>> db = np.full(32, -60.0, dtype=np.float32)\n"
     ">>> db[7] = -15.0; db[8] = -10.0; db[9] = -15.0\n"
     ">>> peaks = find_peaks_f32(db, 2, -30.0)\n"
     ">>> peaks\n"
     "[(-0.25, -10.0)]\n"},
    {"obw_from_power", (PyCFunction)(void *)_bind_obw_from_power, METH_VARARGS | METH_KEYWORDS,
     "obw_from_power.\n"},
    {"noise_floor_db", (PyCFunction)(void *)_bind_noise_floor_db, METH_VARARGS | METH_KEYWORDS,
     "noise_floor_db.\n"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef spectral_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "spectral",
    .m_doc     = "Spectral transforms: reusable FFT and FFT2D engines (fixed length, pocketfft plans cached at construction) plus Corr / Corr2D cross-correlators and PSD power-spectrum estimators.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> import numpy as np\n"
     ">>> from doppler.spectral import FFT\n"
     ">>> f = FFT(n=1024)\n"
     ">>> X = f.execute_cf32(np.ones(1024, np.complex64))\n"
     ">>> bool(abs(X[0].real - 1024) < 1)\n"
     "True\n",
    .m_size    = -1,
    .m_methods = spectral_module_methods,
};

PyMODINIT_FUNC
PyInit_spectral(void)
{
    import_array();
    if (PyType_Ready(&FFTObjType) < 0) return NULL;
    if (PyType_Ready(&FFT2DObjType) < 0) return NULL;
    if (PyType_Ready(&CorrObjType) < 0) return NULL;
    if (PyType_Ready(&Corr2DObjType) < 0) return NULL;
    if (PyType_Ready(&CorrDetectorObjType) < 0) return NULL;
    if (PyType_Ready(&CorrDetector2DObjType) < 0) return NULL;
    if (PyType_Ready(&PSDObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&spectral_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FFTObjType);
    if (PyModule_AddObject(m, "FFT", (PyObject *)&FFTObjType) < 0) {
        Py_DECREF(&FFTObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&FFT2DObjType);
    if (PyModule_AddObject(m, "FFT2D", (PyObject *)&FFT2DObjType) < 0) {
        Py_DECREF(&FFT2DObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CorrObjType);
    if (PyModule_AddObject(m, "Corr", (PyObject *)&CorrObjType) < 0) {
        Py_DECREF(&CorrObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&Corr2DObjType);
    if (PyModule_AddObject(m, "Corr2D", (PyObject *)&Corr2DObjType) < 0) {
        Py_DECREF(&Corr2DObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CorrDetectorObjType);
    if (PyModule_AddObject(m, "CorrDetector", (PyObject *)&CorrDetectorObjType) < 0) {
        Py_DECREF(&CorrDetectorObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CorrDetector2DObjType);
    if (PyModule_AddObject(m, "CorrDetector2D", (PyObject *)&CorrDetector2DObjType) < 0) {
        Py_DECREF(&CorrDetector2DObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&PSDObjType);
    if (PyModule_AddObject(m, "PSD", (PyObject *)&PSDObjType) < 0) {
        Py_DECREF(&PSDObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
