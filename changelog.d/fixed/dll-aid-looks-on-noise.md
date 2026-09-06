- **`Dll`'s symbol-aided lock detector no longer re-locks on noise once a
    second** ([#1264](https://github.com/doppler-dsp/doppler/issues/1264)).
    On noise the aid's best timing hypothesis flips between neighbours whose
    windows share all but one partial, so a decision's `n_looks` read the
    same noise `n` times against a threshold sized for `n` independent
    looks: 1.7e-2 exceedances per decision for a configured 1e-3 at
    45 dB-Hz, 4.2e-2 at 40, a false code lock every one to two seconds —
    which restarted `AsyncDsssReceiver`'s release clock and made the
    release fire one to three intervals late (design §12.14). A look whose
    window overlaps the last look's is no longer a look: 2.1e-3 per
    decision and no false lock in twenty seconds, the unaided detector's
    own rate. With a signal present nothing changes — a held hypothesis's
    windows were already a symbol apart. The `Dll` state blob is v10.
