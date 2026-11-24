- **The 415.1-B-1 C++ example sends TC frames and reads them back.** The
    [rpm-consumer/ccsds-415](docs/example-projects/rpm-consumer/ccsds-415.md)
    example's emitter sends the design page's §1.4 period. Each period
    carries one CCSDS TC transfer frame, and the library describes and
    assembles it. A reader with no truth finds each frame by its
    acquisition and start sequences, reads its header back by name and
    checks its CRC. Both binaries check that every frame sent was read and
    that each CRC verdict agrees with the truth. The TUI gains a frames
    panel.
