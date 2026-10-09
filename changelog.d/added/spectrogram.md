- **A streaming spectrogram in C, `dp_spectrogram_*`.** Any-size chunks of
    cf32 in, rows of `nfft`-bin dBFS spectra out, one every `hop` samples,
    and the same rows however the stream is cut. Each row is
    `dp_psd_frame_db` of its frame, so a full-scale tone reads 0 dBFS. A
    short output buffer never loses input: `push` stops at a whole row and
    `dp_spectrogram_consumed()` says where to resume. It has no Python face
    yet; that follows the jm release with `out_cols` (#1894,
    `docs/design/spectrogram.md`).
