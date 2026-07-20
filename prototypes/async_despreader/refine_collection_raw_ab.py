"""Direct, sample-level A/B of the refine-stage COLLECTION streams
themselves (before either even reaches `CarrierAcquisition`): the real C
`doppler.track.Dll(segments=W)` vs. the real Python `CoupledAsyncDespreader
(windows=W, freeze_carrier=True, aid_code=False)`, on the IDENTICAL raw
prefix, same W. `refine_stage_ab.py` found removing `DLL_LOOKBACK_MARGIN`
(the suspected divergence from the validated no-margin `find_max_power`
design) changed NOTHING -- so the bug isn't (only) the margin. This script
looks one level earlier: does the C collection's own per-epoch OUTPUT
MAGNITUDE and the code loop's own `code_rate`/lock health diverge from
Python's, and if so, from which epoch?

Run: `python refine_collection_raw_ab.py` (needs numpy + matplotlib +
doppler). Writes `refine_collection_raw_ab.png` into this folder.
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from acq_handoff import search_and_handoff
from despreader_coupled import CoupledAsyncDespreader
from spec_full_characterization import (
    CHIP_RATE,
    CODE,
    DOPPLER_UNCERTAINTY_HZ,
    FS_GEN,
    SF,
    SPC,
    SYM_RATE,
    es_n0_to_cn0_dbhz,
    make_ramp_signal,
)

from doppler.dsss import Acquisition
from doppler.resample import RateConverter
from doppler.track import Dll

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
ESN0_DB = 10.0
STATIC_DOPPLER_HZ = 15000.0
SEED = 8001
N_EPOCHS_PROBE = 200
BN_CODE = 0.002
BN_CARRIER = 0.01
W = 1


def main():
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, _data_bits = make_ramp_signal(
        cn0_dbhz, SEED, n_sym=8000, rate_hz_per_s=0.0,
        static_doppler_hz=STATIC_DOPPLER_HZ,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen)

    acq = Acquisition(
        CODE, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ, pfa=1e-3, pd=0.9,
        symbol_rate=SYM_RATE,
    )
    handoff, consumed = search_and_handoff(acq, x, SPC, FS_FRONT)
    coarse_norm_freq = handoff.doppler_hz_est / FS_FRONT
    tracker_init_chip = (SF - handoff.chip_phase) % SF
    print(
        f"handoff: doppler_hz_est={handoff.doppler_hz_est:.1f}  "
        f"chip_phase={handoff.chip_phase:.4f}  "
        f"tracker_init_chip(mirror)={tracker_init_chip:.4f}"
    )

    prefix = x[consumed : consumed + N_EPOCHS_PROBE * TE].astype(
        np.complex64
    )

    # ── Python collection (real CoupledAsyncDespreader) ──
    d0 = CoupledAsyncDespreader(
        CODE, SPC, bn=BN_CODE, zeta=0.707, windows=W, bn_car=BN_CARRIER,
        init_chip=tracker_init_chip, init_car_norm_freq=coarse_norm_freq,
        aid_code=False, sample_rate_hz=FS_FRONT, freeze_carrier=True,
    )
    out_py = d0.run(prefix)
    code_rate_py = d0.code_rate
    print(f"Python: final code_rate={code_rate_py:.6f}")

    # ── C collection (real doppler.track.Dll) -- frozen carrier is a
    # static complex exponential, the exact equivalent of costas_wipeoff
    # on a never-updated NCO (see refine_stage_ab.py's own docstring). ──
    n = np.arange(len(prefix))
    wiped = (prefix * np.exp(-2j * np.pi * coarse_norm_freq * n)).astype(
        np.complex64
    )
    # native (non-mirrored) chip-phase convention for the real C Dll.
    dll = Dll(
        CODE, sps=SPC, init_chip=handoff.chip_phase, bn=BN_CODE, zeta=0.707,
        spacing=0.5, segments=W,
    )
    out_c = dll.steps(wiped)
    code_rate_c = dll.code_rate
    print(f"C:      final code_rate={code_rate_c:.6f}")

    # Per-EPOCH power (mean |.|^2 over each epoch's W chunks) for both --
    # comparable units, since both output W samples/epoch.
    n_ep_py = len(out_py) // W
    n_ep_c = len(out_c) // W
    pow_py = np.abs(out_py[: n_ep_py * W].reshape(n_ep_py, W)) ** 2
    pow_c = np.abs(out_c[: n_ep_c * W].reshape(n_ep_c, W)) ** 2
    epoch_pow_py = pow_py.mean(axis=1)
    epoch_pow_c = pow_c.mean(axis=1)

    print(f"\nn_epochs: python={n_ep_py}  C={n_ep_c}")
    print(
        f"first-10-epoch mean power:  python={epoch_pow_py[:10].mean():.4g}  "
        f"C={epoch_pow_c[:10].mean():.4g}"
    )
    print(
        f"last-10-epoch  mean power:  python={epoch_pow_py[-10:].mean():.4g}  "
        f"C={epoch_pow_c[-10:].mean():.4g}"
    )

    fig, axes = plt.subplots(2, 1, figsize=(10, 7), sharex=True)
    axes[0].plot(epoch_pow_py, label="Python (CoupledAsyncDespreader)", color="tab:blue")
    axes[0].plot(epoch_pow_c, label="C (doppler.track.Dll)", color="tab:orange")
    axes[0].set_ylabel("mean epoch power |out|^2")
    axes[0].set_yscale("log")
    axes[0].legend()
    axes[0].set_title(
        f"Refine-stage collection: per-epoch output power, segments=windows={W}"
    )
    axes[0].grid(True, alpha=0.3)

    # Instantaneous phase progression of the FIRST chunk of each epoch --
    # a cheap way to see if one stream is tracking a coherent tone and the
    # other looks like noise/garbage.
    ph_py = np.unwrap(np.angle(out_py[::W]))
    ph_c = np.unwrap(np.angle(out_c[::W]))
    axes[1].plot(ph_py, label="Python", color="tab:blue")
    axes[1].plot(ph_c, label="C", color="tab:orange")
    axes[1].set_xlabel("epoch index")
    axes[1].set_ylabel("unwrapped phase (rad), first chunk/epoch")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig("refine_collection_raw_ab.png", dpi=130)
    print("\nWrote refine_collection_raw_ab.png")


if __name__ == "__main__":
    main()
