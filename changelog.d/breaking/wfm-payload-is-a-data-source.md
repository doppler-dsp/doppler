- **A payload is a data source, sent once: `--bits` and the cycle are
    gone** ([#1718](https://github.com/doppler-dsp/doppler/issues/1718)).
    `--bits`/`--bits-file`/`"payload"`/`bits=` are `--data`/
    `--data-from-file`/`"data"`/`data=`, each old spelling refused; a fixed
    pattern plays once, then silence. Migration:
    [design §5](https://doppler-dsp.github.io/doppler/design/payload-data-source/#5-what-is-deleted).
