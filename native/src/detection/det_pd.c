#include "detection/detection_core.h"
#include <math.h>
double det_pd(double snr, int dwell, double threshold) {
    return marcum_q(1, sqrt(2.0 * dwell) * snr, threshold);
}
