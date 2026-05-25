/*
 * detection_ext.c — Python extension module detection
 *
 * Objects: 
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>

#include "detection/detection_core.h"



static PyObject *
_bind_marcum_q(PyObject *self, PyObject *args)
{
    (void)self;
    int m = 0;
    double a = 0.0;
    double b = 0.0;
    if (!PyArg_ParseTuple(args, "idd", &m, &a, &b))
        return NULL;
    return PyFloat_FromDouble(marcum_q(m, a, b));
}

static PyObject *
_bind_det_threshold(PyObject *self, PyObject *args)
{
    (void)self;
    double pfa = 0.0;
    if (!PyArg_ParseTuple(args, "d", &pfa))
        return NULL;
    return PyFloat_FromDouble(det_threshold(pfa));
}

static PyObject *
_bind_det_pd(PyObject *self, PyObject *args)
{
    (void)self;
    double snr = 0.0;
    int dwell = 0;
    double threshold = 0.0;
    if (!PyArg_ParseTuple(args, "did", &snr, &dwell, &threshold))
        return NULL;
    return PyFloat_FromDouble(det_pd(snr, dwell, threshold));
}

static PyObject *
_bind_det_dwell(PyObject *self, PyObject *args)
{
    (void)self;
    double snr = 0.0;
    double pd_min = 0.0;
    double pfa = 0.0;
    int max_dwell = 0;
    if (!PyArg_ParseTuple(args, "dddi", &snr, &pd_min, &pfa, &max_dwell))
        return NULL;
    return PyLong_FromLong((long)det_dwell(snr, pd_min, pfa, max_dwell));
}

static PyObject *
_bind_det_snr(PyObject *self, PyObject *args)
{
    (void)self;
    int dwell = 0;
    double pd_min = 0.0;
    double pfa = 0.0;
    if (!PyArg_ParseTuple(args, "idd", &dwell, &pd_min, &pfa))
        return NULL;
    return PyFloat_FromDouble(det_snr(dwell, pd_min, pfa));
}

static PyObject *
_bind_det_threshold_power(PyObject *self, PyObject *args)
{
    (void)self;
    double pfa = 0.0;
    if (!PyArg_ParseTuple(args, "d", &pfa))
        return NULL;
    return PyFloat_FromDouble(det_threshold_power(pfa));
}

static PyObject *
_bind_det_pd_power(PyObject *self, PyObject *args)
{
    (void)self;
    double snr_power = 0.0;
    int dwell = 0;
    double power_threshold = 0.0;
    if (!PyArg_ParseTuple(args, "did", &snr_power, &dwell, &power_threshold))
        return NULL;
    return PyFloat_FromDouble(det_pd_power(snr_power, dwell, power_threshold));
}

static PyObject *
_bind_det_dwell_power(PyObject *self, PyObject *args)
{
    (void)self;
    double snr_power = 0.0;
    double pd_min = 0.0;
    double pfa = 0.0;
    int max_dwell = 0;
    if (!PyArg_ParseTuple(args, "dddi", &snr_power, &pd_min, &pfa, &max_dwell))
        return NULL;
    return PyLong_FromLong((long)det_dwell_power(snr_power, pd_min, pfa, max_dwell));
}

static PyObject *
_bind_det_snr_power(PyObject *self, PyObject *args)
{
    (void)self;
    int dwell = 0;
    double pd_min = 0.0;
    double pfa = 0.0;
    if (!PyArg_ParseTuple(args, "idd", &dwell, &pd_min, &pfa))
        return NULL;
    return PyFloat_FromDouble(det_snr_power(dwell, pd_min, pfa));
}


/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Detection_methods[] = {
    {"marcum_q", _bind_marcum_q, METH_VARARGS, "marcum_q."},
    {"det_threshold", _bind_det_threshold, METH_VARARGS, "det_threshold."},
    {"det_pd", _bind_det_pd, METH_VARARGS, "det_pd."},
    {"det_dwell", _bind_det_dwell, METH_VARARGS, "det_dwell."},
    {"det_snr", _bind_det_snr, METH_VARARGS, "det_snr."},
    {"det_threshold_power", _bind_det_threshold_power, METH_VARARGS, "det_threshold_power."},
    {"det_pd_power", _bind_det_pd_power, METH_VARARGS, "det_pd_power."},
    {"det_dwell_power", _bind_det_dwell_power, METH_VARARGS, "det_dwell_power."},
    {"det_snr_power", _bind_det_snr_power, METH_VARARGS, "det_snr_power."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef detection_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "detection",
    .m_doc     = "Detection module.",
    .m_size    = -1,
    .m_methods = Detection_methods,
};

PyMODINIT_FUNC
PyInit_detection(void)
{
    import_array();

    PyObject *m = PyModule_Create(&detection_moduledef);
    if (!m) return NULL;

    return m;
}
