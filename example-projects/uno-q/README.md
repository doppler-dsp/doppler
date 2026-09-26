# uno-q — an RTL-SDR receive front end on a small ARM board

The first stages of a receiver fed by an RTL-SDR, as a downstream project:

```text
cu8 bytes ─→ U8ToF32 ─→ DDC (mix by −offset, decimate by rate) ─→ PSD
```

It was written for, and measured on, an **Arduino UNO Q** (Qualcomm QRB2210,
four Cortex-A53-class cores, Debian 13), but nothing in it is board-specific.
It builds and runs anywhere doppler does, which is how CI checks it. Copy the
directory as the starting point for an SDR front end.

## Two modes

| command                                   | input                             | what it does                                                                                                                                                                                                  |
| ----------------------------------------- | --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `uno_q [flags]`                           | none                              | **Self-test.** Synthesises one second of what an RTL-SDR would send (a tone at `--offset` in noise, through a model of its 8-bit ADC), runs the chain and checks the result. Any failed check exits non-zero. |
| `uno_q [flags] -` or `uno_q [flags] FILE` | `cu8` on stdin, or a capture file | **Live.** Reports throughput, CPU load, the strongest peaks and the level in the channel. Nothing is checked, because live RF is not known in advance.                                                        |

| flag          | default   | meaning                                                                            |
| ------------- | --------- | ---------------------------------------------------------------------------------- |
| `--fs HZ`     | `2.4e6`   | input sample rate                                                                  |
| `--offset HZ` | `250e3`   | where the wanted channel sits relative to the tuned centre; the DDC mixes it to DC |
| `--rate R`    | `0.125`   | DDC output/input rate                                                              |
| `--n N`       | `1024`    | PSD frame length                                                                   |
| `--seconds S` | until EOF | live only: stop after `S` seconds of input                                         |

## Build and run

```sh
make build              # configure and compile
make run                # build, then self-test in both link modes
make clean
```

`PREFIX=<dir>` points at a doppler install (a `make package-c` tree or a
release tarball); `CPU_FLAGS=<flags>` adds compiler flags for this program.

## What the self-test checks

Every number is measured on the machine that runs it. Throughput is printed
and never asserted, because it is a property of the machine.

1. **The `cu8` bias is present before the DDC.** An RTL-SDR's zero sits at
    code 127.5, and `U8ToF32`'s fast `shift` mode centres on 128, so the
    converted stream reads 0.5/128 low. The self-test measures it:
    −0.003824 (I) and −0.003913 (Q), against −0.003906 predicted.
1. **The tone is mixed to DC**, within one bin.
1. **The in-channel SNR matches its prediction**, within 1 dB. The prediction
    is computed from the scene and the chain's own parameters: 48.33 dB
    predicted, 48.48 dB measured.
1. **The DDC filters the bias out.** Mixing moves the bias to `−offset`,
    outside the channel. Its image reads −62.1 dB against −62.0 dB of noise
    around it; unfiltered, it would read −45.2 dB.

Each check has been shown to fail when the thing it guards is broken:

| deliberate defect                           | caught by |
| ------------------------------------------- | --------- |
| mixing in the wrong direction               | 2 and 3   |
| the `midpoint` converter, which has no bias | 1         |
| noise 1.5× what the prediction assumes      | 3         |
| an ADC model centred on 128                 | 1         |
| `--rate 1`, so no decimation filter         | 4         |

Two lessons are built into how it measures:

- **Measure noise where the signal is.** `psd_noise_floor()` is a median over
    the whole output band. The plain DDC uses an uncompensated CIC (see
    `ddc_core.h`), so the band isn't flat: −0.2 dB within ±0.1 of the output
    rate, −3.5 dB at ±0.25 and −12 dB near the edge. Against the band-wide
    median, the SNR read 3.6 dB too high.
- **Keep wideband channels in the flat part.** For the same reason, choose
    `--rate` so the channel fits within about ±0.1 of the output rate. For
    broadcast FM (±75 kHz) that's `--rate 0.25` (600 kSa/s), not `0.125`.

## On an Arduino UNO Q

Measured on 2026-09-26. The board runs Debian 13 on four Cortex-A53-class
cores (`/proc/cpuinfo` part `0x801`); GCC 14.2 and CMake 3.31 from Debian.

**Toolchain.** The board ships without one:

```sh
sudo apt install build-essential cmake ninja-build
```

**Install doppler** from a clone of this repository, then build this project
against it:

```sh
make package-c PREFIX=$HOME/.local                 # from doppler's root
make -C example-projects/uno-q PREFIX=$HOME/.local run
```

**Tuning.** `-march=native` does nothing on this board: GCC does not know the
core and resolves it to plain `armv8-a+crc+crypto` with no tuning. The
Cortex-A53 scheduling model is what helps, because the core is in-order.
Across 63 of doppler's C benchmarks, measured alternately against a portable
build, a `-mcpu=cortex-a53` build was slower on none by more than the ±5%
run-to-run noise, and faster on 13, by up to 32% on multiply-accumulate loops
(real FIR +25–32%). To build doppler that way:

```sh
make package-c PREFIX=$HOME/.local CMAKE_ARGS=-DCMAKE_C_FLAGS=-mcpu=cortex-a53
```

**Throughput of this chain**, from the self-test on one core: 20.0 MSa/s,
8.3× real time at 2.4 MSa/s (portable build). The desktop x86-64 machine
used for comparison ran it at 275 MSa/s.

**Board notes:**

- The CPU governor defaults to `schedutil`. For repeatable timing, run
    `echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`;
    it resets on reboot.
- If Wi-Fi connects but gets no IPv4 address, check the NetworkManager log for
    `acd conflict`. That means the router offered an address another device
    already holds. Reserve an address for the board on the router, or give it
    a fixed one with `nmcli`.
- Under `adb shell`, `TMPDIR` is `/data/local/tmp`, which doesn't exist on
    Debian. Set `TMPDIR=/tmp`, or use SSH.

## Live input from an RTL-SDR

Measured with a Nooelec NESDR SMArt v5 (R820T tuner). On any Linux host with
the dongle, first stop the kernel's DVB-T driver from claiming it: install
`rtl-sdr` (which usually ships a blacklist), or blacklist `dvb_usb_rtl28xxu`
yourself and unload it. `rtl_test -t` then finds the tuner.

**Dongle on the board.** The UNO Q has one USB-C port, and it can only report
itself as a power sink. To use the dongle directly, the board must be the USB
host of a powered hub, with the dongle on the hub and SSH over Wi-Fi for
control. Then:

```sh
rtl_sdr -f 99.8e6 -s 2.4e6 - | build/uno_q_shared --offset 100e3 --rate 0.25 -
```

**Dongle on a PC, stream to the board** (how this was measured). This works
when the board is a USB device of the PC, directly or through a hub. The PC
serves the dongle with `rtl_tcp`, and `adb reverse` carries the stream over
the same USB cable:

```sh
rtl_tcp -a 127.0.0.1 -f 100e6 -s 2.4e6        # on the PC
adb reverse tcp:1234 tcp:1234                 # on the PC
nc 127.0.0.1 1234 | tail -c +13 | build/uno_q_shared --offset -100e3 --rate 0.25 --seconds 5 -   # on the board
```

`tail -c +13` drops `rtl_tcp`'s 12-byte header. The link delivered
2.38 MSa/s to the board. With the dongle tuned to 100.0 MHz, a station at
99.9 MHz (`--offset -100e3`) was brought to DC at 32% of one core. The
dongle's own DC spike moved to +100 kHz, out of the channel, which is the
point of offset tuning.

**Two `rtl_tcp` quirks.** It prints "Signal caught, exiting!" each time a
client disconnects, yet keeps serving. And it sends the 12-byte header before
the samples, which `tail -c +13` removes.
