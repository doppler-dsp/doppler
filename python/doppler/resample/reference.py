"""Resampler reference for validation."""

import numpy as np
from doppler.dp_nco import Nco
from doppler.dp_delay import DelayCf64
from doppler.dp_accumulator import AccCf64
from doppler.polyphase import design_bank

class Resampler:
    """Continuously variable sample rate converter.

    """

    def __init__(self, r=1.1):

        # The resampler is either fundamentally increasing or decreasing the
        # sample rate. Decide which here.
        self.r = r
        if self.r < 1:
            self.nco = Nco(r)
            self.upsample = True
        else:
            self.nco = Nco(1/r)
            self.upsample = False

        # NCO helpers
        self.phase_inc = self.nco.get_phase_inc()

        # Grab the polyphase filter and flip it in memory for natural indexing
        self.polyphase_reversed = design_bank()[::-1]
        self.phases, self.taps = self.polyphase_reversed.shape
        self.log2_phases = int(np.log2(self.phases))

        # Create polyphase delay line and accumulator
        self.buffer = DelayCf64(self.taps)
        self.acc = AccCf64()

    def _get_filter(self, phase):
        """Lookup polyphase filter row from raw uint32 NCO phase."""
        branch = int(phase) >> (32 - self.log2_phases)
        return self.polyphase_reversed[branch]

    def _interpolate(self, x):
        """Resample using polyphase interpolator with nearest-neighbor phase branch.

        Output-driven: emit one sample per NCO tick, consume next input
        sample on NCO overflow.
        """
        x = np.asarray(x, dtype=np.complex128)
        out = []
        xi_idx = 0
        n_in = len(x)

        while xi_idx < n_in:
            phase, ovf = self.nco.execute_u32_ovf()
            if ovf[0]:
                self.buffer.push(x[xi_idx])
                xi_idx += 1
            h = self._get_filter(phase[0])
            self.acc.reset()
            self.acc.madd(self.buffer.ptr(), h)
            out.append(self.acc.dump())

        return np.array(out, dtype=np.complex128)

    def _decimate(self, x):
        """Resample using polyphase decimator with nearest-neighbor phase branch.

        Input-driven: push one sample per input tick, emit output on
        NCO overflow.
        """
        x = np.asarray(x, dtype=np.complex128)
        out = []

        for xi in x:
            self.buffer.push(xi)
            phase, ovf = self.nco.execute_u32_ovf()
            if ovf[0]:
                h = self._get_filter(phase[0])
                self.acc.reset()
                self.acc.madd(self.buffer.ptr(), h)
                out.append(self.acc.dump())

        return np.array(out, dtype=np.complex128)

    def execute(self, x):
        """Resample."""
        return self._interpolate(x) if self.upsample else self._decimate(x)



if __name__ == "__main__":

    resampler = Resampler()
    print(resampler.nco.get_freq())
    print(resampler.nco.get_phase_inc())
    nco_out, nco_ovf = resampler.nco.execute_u32_ovf()
    print(nco_out)
    print(resampler._get_filter(nco_out[0]))
