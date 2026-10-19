- **The `AsyncDsssReceiver` lifecycle example reads the receiver's own
    telemetry.** Both twins attach `set_telemetry()`, keep every record in a
    memory capture, and build their rows from `rx.state`, `rx.sym.locked`,
    `rx.code.locked`, `rx.sym.lock` and `rx.handover_hz`. The figure adds a
    carrier panel (the hand-over against the loop's command in Hz), and
    [the gallery page](../../docs/gallery/async-dsss-receiver-lifecycle.md)
    shows the file capture and `scripts/plot_capture.py`. The telemetry
    page's table lists the receivers, the pool and `Acquisition`.
