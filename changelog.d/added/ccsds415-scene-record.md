- **just-makeit pin 0.137.0 → 0.138.0, and the 415.1-B-1 scene from
    Python.** jm 0.138.0 lets a module function return one record by value
    (`single` + `record_name`, just-makeit#2755).
    `doppler.ccsds.ccsds415_scene(fc, spc, start_hz)` uses it, returning a
    `Ccsds415Scene` that holds the chip rate, the rate converter's ratio,
    and the Doppler's start and rate in ppm. These are bit for bit
    `dp_ccsds_415_scene()`'s. The spec demo's Python twin reads it in place
    of the literals it typed. The re-render changed nothing else.
