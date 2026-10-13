- **The chirp docs now say what their phase promise covers: the reads of one
    synth**, which join byte for byte, while each segment of a scene starts
    afresh. v0.68.0's known limit ("a tone or chirp repeated across segments
    restarts its phase") misread that promise; #2244 is closed. The span's
    last sample steps at `f_end − k`, and `f_end` is held from the next.
