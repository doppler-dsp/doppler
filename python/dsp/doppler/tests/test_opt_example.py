"""
Test Optimization Example from Algorithm Source.

Hunter, Matthew, "Design Of Polynomial-based Filters For Continuously Variable Sample Rate Conversion
With Applications In Synthetic Instrumentati" (2008). Electronic Theses and Dissertations. 3601.
https://stars.library.ucf.edu/etd/3601

3.4.2 Example

The frequency domain optimization of a PBF using linear programming will now be illustrated by an example. A filter is to be designed with the following parameters
1. Normalized passband edge: ωpass = 2πfpass = 2π(.2)
2. Normalized stopband edge: ωstop = 2πfstop = 2π(.8)
3. Number of polynomial pieces: N = 6
4. Order of each piece: M = 3
5. Basis function constants: a = 1, b = -1/2
6. Passband weight: Kpass = 10
7. Stopband weight: Kstop = 1

Now, two uniformly spaced frequency grids are chosen for the passband and stopband of
100 and 500 points respectively. This results in a total of P = 100 + 500 = 600
points for optimization. These are the only points optimized. The transition band is a “don't care”
band. The passband extends from 0 to ωpass, while the stopband extends from ωstop to ∞.

"""

import pytest
import numpy as np
from doppler.polyphase.farrow_opt import optimize_pbf


def test_opt_example():
    delta = 0.0020
    C = np.array(
        [
            0.0138,
            0.0687,
            -0.0079,
            -0.1415,
            -0.1066,
            -0.2875,
            0.3480,
            0.8925,
            0.5923,
            1.5384,
            -0.3324,
            -1.7383,
            0.5923,
            -1.5384,
            -0.3324,
            1.7383,
            -0.1066,
            0.2875,
            0.3480,
            -0.8925,
            0.0138,
            -0.0687,
            -0.0079,
            0.1415,
        ]
    )

    C_hat, delta_hat = optimize_pbf(6, 3, 0.2, 0.8)
    assert delta_hat == pytest.approx(delta, rel=0.01)
    assert C.flatten() == pytest.approx(C_hat.flatten(), rel=0.01)
