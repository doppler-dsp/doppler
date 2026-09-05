- **`Acquisition` is instrumented (design §2.4).** `set_telemetry(tlm,   prefix, decim)` registers ten probes per decided dwell — the test
    statistic and its gate, the CFAR reference, the strongest cell and where
    it is, the peak count and the held twins, the peak's concentration (its main lobe over its whole
    column: the splatter discriminator) and whether the gate fired. `keep_surface` then
    `surface(out)` returns the dwell's whole surface in the gate's units,
    with `surface_doppler_hz()` / `surface_chip_phase()` as its axes, so the
    2-D test statistic can be plotted; in C `acq_set_surface_sink()` streams
    every k-th dwell's surface. Both flavours; nothing costs anything until
    attached.
