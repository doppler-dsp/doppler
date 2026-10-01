- **A payload is a data source, sent once: `--bits` and the cycle are
    gone** ([#1718](https://github.com/doppler-dsp/doppler/issues/1718)).
    `--bits FIELD` is `--data FIELD` (with `--data-len` for frames),
    `--bits-file PATH` is `--data-from-file PATH`, a scene's `"payload"` (and
    `"pattern"`, `"payload_gen"`) is `"data"`, and Python's `bits=`,
    `payload=` and `pattern=` are `data=`; each old spelling is refused. A
    pattern or a frame of fixed bits is sent **once**, then silence -- it no
    longer cycles to fill `--count`. More frames are more data: each frame
    carries the next `--data-len` chunk, and a finite source sets the run's
    length. A data source is a frame's payload, so `--type bits` appends a
    CRC-16 unless `--crc none`. `wfm_source_t.payload` is gone; its bits are
    `data`.
