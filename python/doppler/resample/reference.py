"""Resampler reference for validation."""

import numpy as np
from doppler.dp_nco import Nco
from doppler.dp_delay import DelayCf64
from doppler.dp_accumulator import AccCf64
from doppler.polyphase import kaiser_prototype


class Resampler:
    """Continuously variable sample rate converter."""

    def __init__(self, r=1.1):

        # The resampler is either fundamentally increasing or decreasing the
        # sample rate. Decide which here.
        self.r = r
        if self.r >= 1:
            self.nco = Nco(1 / r)
            self.upsample = True
        else:
            self.nco = Nco(r)
            self.upsample = False

        # NCO helpers
        self.phase_inc = self.nco.get_phase_inc()

        # Polyphase bank: strided decomposition, branch k = fractional delay k/L
        _, self.bank = kaiser_prototype()
        self.phases, self.taps = self.bank.shape
        self.log2_phases = int(np.log2(self.phases))

        # Decimator accumulates ~1/r input samples per output;
        # pre-scale coefficients so the output has unity gain.
        if not self.upsample:
            self.bank = self.bank[:, ::-1] * np.float32(r)

        # Interpolator state: delay line + accumulator
        self.buffer = DelayCf64(self.taps)
        self.acc = AccCf64()

        # Decimator state (transposed form):
        #   N integrate-and-dump accumulators (one per tap)
        #   N-1 transposed delay line registers
        self.iad = np.zeros(self.taps, dtype=np.complex128)
        self.tfd = np.zeros(self.taps - 1, dtype=np.complex128)

    def _get_filter(self, phase):
        """Lookup polyphase filter row from raw uint32 NCO phase."""
        branch = int(phase) >> (32 - self.log2_phases)
        return self.bank[branch]

    def _interpolate(self, x):
        """Resample using polyphase interpolator with nearest-neighbor phase branch.

        Output-driven: emit one sample per NCO tick, consume next input
        sample on NCO overflow.
        """
        x = np.asarray(x, dtype=np.complex128)
        n_in = len(x)
        out = np.empty(int(n_in * self.r) + 1, dtype=np.complex128)
        out_idx = 0
        xi_idx = 0

        while xi_idx < n_in:
            phase, ovf = self.nco.execute_u32_ovf()
            h = self._get_filter(phase[0])
            self.acc.reset()
            self.acc.madd(self.buffer.ptr(), h)
            out[out_idx] = self.acc.dump()
            out_idx += 1
            if ovf[0]:
                self.buffer.push(x[xi_idx])
                xi_idx += 1

        return out[:out_idx]

    def _decimate(self, x):
        """Resample using polyphase decimator, transposed form.

        Input-driven: each input sample x[n] is multiplied by all N
        branch coefficients and the products accumulate in N
        integrate-and-dump (I&D) registers.  On NCO overflow all I&D
        registers dump into a transposed tapped delay line which
        shifts and produces one output sample, then the I&D registers
        reset.

        Diagram (from RESAMPLER.md):

            id[N-1]    id[N-2]         id[1]          id[0]
              |  _____   |               |   _____      |
              -->| T |-->+ --> ... --> -->+ ->| T |--> ->+ --> y
                 |___|                       |___|
        """
        x = np.asarray(x, dtype=np.complex128)
        n_in = len(x)
        out = np.empty(int(n_in * self.r) + 1, dtype=np.complex128)
        out_idx = 0
        _ = self.taps

        for xi in x:
            phase, ovf = self.nco.execute_u32_ovf()
            h = self._get_filter(phase[0])

            # Scalar × all N branch coefficients → I&D
            self.iad += xi * h

            if ovf[0]:
                d = self.iad.copy()
                self.iad[:] = 0

                # Transposed delay line: shift and add
                y = d[0] + self.tfd[0]
                self.tfd[:-1] = d[1:-1] + self.tfd[1:]
                self.tfd[-1] = d[-1]

                out[out_idx] = y
                out_idx += 1

        return out[:out_idx]

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
