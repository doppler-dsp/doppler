- **The searcher no longer hands off an emitter on a tile's edge a whole
    tile away** (#1270, design §12.18). Both neighbouring tiles read such
    an emitter at the same row within 0.03 dB, so the pick was the noise's
    half the time on the edge; every listed peak is now asked at its row's
    own frequency against the block's raw epochs, and none of 59 decided
    blocks is a tile off where 27 of 57 were. The acq state blob carries
    the raw block (version 4).
