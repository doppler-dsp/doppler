- **`dp_doppler_wfmgen()` puts the caller's SIGINT/SIGTERM handlers back**
    on every return; it used to leave its own installed, contrary to its header
    (#1594).
