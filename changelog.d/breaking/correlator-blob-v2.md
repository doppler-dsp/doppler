- **Correlator state blobs are version 2**: the continuous search's blob
    carries `keep_surface`, which a restore used to drop (#2022). Every
    version-1 correlator blob is refused, and so is any composite blob that
    carries one (a capture's or a receiver's).
