- **The async despreader example is rebuilt.** It runs `track.Dll(segments=4)`
    on a `DopplerChannel`-dilated link, unaided and with the carrier's rate aid,
    at the link's 0.6 ppm and an inflated 5 ppm, sizes the lock detector for the
    link, and shows the despread output it hands the next stage; its Python and
    C twins run one draw and check the same physics
    ([the gallery page](../../docs/gallery/async-despread.md)).
