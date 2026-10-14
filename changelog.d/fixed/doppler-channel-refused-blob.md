- **A `DopplerChannel` that refuses a state blob is left as it was.** Its
    own fields were written before its resampler could refuse the blob;
    they are now written only once the resampler accepts (#2104).
