- **carrier_nda's §2.2 tracking table judged each point on one seed's last
    sample**, so M = 8 at 10 dB read "tracking" on CI's build and "no" on
    GCC 15/16 aarch64 (#2952). Across eight seeds the point does not track
    (worst second-half error 3.5e-3, seven times the tolerance). Each point
    now runs eight seeds; a point tracks only if every seed does, and
    `locked` reports how many seeds declare. The headline counts are now
    13 track and 7 declare (were 14 and 8).
