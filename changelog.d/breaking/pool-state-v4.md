- **`AsyncDsssPool` state blobs are now version 4.** A blob saved by v0.74.0
    (version 2) is refused. v0.74.0's pool took over the old pool's magic and
    restarted its numbering, so the next bump would have reused version 3, the
    old pool's number. Such a blob would then pass the envelope check and be
    misread instead of refused. A new lint gate, `make state-versions-check`,
    now refuses any object whose state version is one a release has already
    shipped with a different layout
    ([#2569](https://github.com/doppler-dsp/doppler/issues/2569)).
