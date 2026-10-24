- **`Writer.annotate(scene)` writes a scene's SigMF annotations into the
    capture.** The sidecar `close()` writes gets one annotation per source per
    instance, from the same builder `Composer.to_sigmf()` and wfmgen use, with
    the writer's own sample type, fs, fc and t0. A BLUE writer, or a raw/CSV
    writer with `sidecar=False`, raises `ValueError` rather than dropping the
    rows (#2564).
