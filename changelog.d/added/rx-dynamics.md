- **`native/validation/rx_dynamics.c` — the receiver under a coupled Doppler
    ramp across a data onset.** The continuous flavor's own scenario, which no
    existing harness covered: NRZ BPSK, I&D, `m_out = 4`, DTTL, 12 dB Es/N0,
    half the record with modulation **off** (carrier on, so the TED has no edge
    and timing cannot close), then dense transitions as a step, all under a
    ramp through `doppler_channel` so the carrier and every clock move
    together. `rx_battery` runs RRC with dense transitions throughout — the
    *burst* flavor's signal — and `rx_nda_tap` sweeps NRZ but **noiseless**.

    It captures every telemetry probe (`--out DIR`), and `make   plot-rx-dynamics` renders `docs/assets/rx-dynamics.png` from that capture,
    so the figure plots the receiver's own records rather than a Python
    re-derivation.

    | tap          | lock, modulation OFF | min at the onset | end    |
    | ------------ | -------------------- | ---------------- | ------ |
    | **`strobe`** | +0.935               | **+0.860**       | +0.920 |
    | `mf_out`     | +0.934               | +0.478           | +0.802 |
    | `mf_in`      | +0.761               | +0.417           | +0.714 |

    **`strobe`'s timing dependency costs nothing in the half where timing is
    impossible**, because an unmodulated NRZ carrier is *sampling-phase
    invariant* — every sample is the same constellation point, so the
    M-th-power discriminator does not care which one the timing loop would
    have nominated. Timing closure gates demodulation, not carrier
    acquisition. The **TED** is the largest single effect on the page: the same
    record through Gardner deepens `strobe`'s onset dip from 0.075 to 0.306.
