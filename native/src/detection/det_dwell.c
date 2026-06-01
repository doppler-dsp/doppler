#include "detection/detection_core.h"
int det_dwell(double snr, double pd_min, double pfa, int max_dwell) {
    double eta = det_threshold(pfa);
    for (int m = 1; m <= max_dwell; m++)
        if (det_pd(snr, m, eta) >= pd_min) return m;
    return -1;
}
