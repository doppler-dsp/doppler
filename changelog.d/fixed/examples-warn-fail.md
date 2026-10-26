- **An example script that raises a `UserWarning` now fails its gate.**
    `make test-examples-python` runs every example with
    `PYTHONWARNINGS=error::UserWarning`, as the stub and doc-snippet doctests
    already do, since gallery pages include these scripts (#2594). Three
    acquisition examples were built under their own feasibility floor and now
    sit 3 dB above it; the Doppler-profile figure uses constrained layout.
