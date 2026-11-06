- **`Acquisition`'s Doppler-sidelobe sweep and sidelobe ratio run on the
    one delay-Doppler map** (#2752 step 3.4b-pre), not on private
    transforms. The sweep is now single precision: `dsl_db` moves by at
    most 6.4e-7 dB, and every acq validator's verdict is unchanged.
