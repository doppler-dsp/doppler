# doppler — runnable demos

doppler is installed (the exact PyPI wheel this release published), plus numpy,
scipy and matplotlib. You're in `/examples`, a folder of self-checking demo
scripts. `MPLBACKEND=Agg` is set, so the plotting demos render to file headless.

## Run one

```sh
python awgn_demo.py
python receiver_lock_demo.py
python ber_awgn_demo.py
ls *.py                      # ~70 more
```

Each script asserts its own physics — exit 0 means it ran *and* checked out.

## The CLIs are here too

```sh
doppler --help               # the sample-stream CLI
wfmgen --help                # waveform generator
doppler-specan --help        # spectrum analyzer
```

## Build ON doppler instead?

This image is for *using* doppler. To write your own C / Python project against
it — headers, the static+shared library, `find_package(doppler)`, the jm
codegen loop — pull the SDK image:

```sh
docker run --rm -it ghcr.io/doppler-dsp/doppler-sdk:latest
```
