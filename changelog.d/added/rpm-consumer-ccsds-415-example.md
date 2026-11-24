- **The 415.1-B-1 receiver, from C++, linked from the RPM.** The
    [rpm-consumer/ccsds-415](docs/example-projects/rpm-consumer/ccsds-415.md)
    example builds against the installed `-devel` RPM. It runs the receiver
    at both specified carriers and checks lock, BER and the telemetry
    capture. It has a batch binary and an FTXUI live view; on the reference
    box, the receiver's processing time per sample is under 6% of the
    sample period. CI holds it with `make rpm-consumer-check`, and the
    Fedora package-smoke leg (#3344).
