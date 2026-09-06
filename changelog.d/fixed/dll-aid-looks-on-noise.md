- **`Dll`'s symbol-aided lock detector no longer re-locks on noise about
    once a second** ([#1264](https://github.com/doppler-dsp/doppler/issues/1264)):
    on noise its best timing hypothesis flipped between neighbours with
    overlapping windows, so a decision read the same noise `n` times — 1.7e-2
    exceedances per decision for a configured 1e-3, which restarted
    `AsyncDsssReceiver`'s release clock and made the pool release one to
    three intervals late. A window overlapping the last look's is no longer
    a look: 2.1e-3, no false lock in twenty seconds, releases at 2.02 s in
    the soak. Nothing changes with a signal present. `Dll` blob v10. Design
    §12.15.
