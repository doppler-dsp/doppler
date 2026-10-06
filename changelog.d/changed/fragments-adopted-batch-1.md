- **Four more binding fragments are jm's, not hand-maintained** (#1446).
    `specan`, `burst_demod`, `dsss_burst_receiver` and `dp_event_log` now set
    `fragment = "generated"`, so jm re-renders them on every apply and the
    drift gate holds them, and they leave the `-Wall -Wextra` exempt list.
    Visible: `EventLog` reports `doppler.telemetry` as its module, and its
    `write` and `finalize` errors now name the 16 KiB event limit and the
    not-an-event-log refusal the C core already enforced.
