- **`doppler-specan`'s live NATS sources show real streams.** The socket
    source imported `Subscriber` from the wrong module and read the dict
    header as an object, so it could not show a frame. Both sources cast
    integer I/Q (`ci8`/`ci16`/`ci32`) straight to complex, drawing a mirrored
    spectrum at twice the rate. The display now centres on the stream's own
    frequency unless `--center` is given. Tests drive both sources through a
    stand-in transport.
