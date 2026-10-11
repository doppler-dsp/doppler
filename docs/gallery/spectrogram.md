# A Streaming Spectrogram

![A chirp and a tone as power rows and as dB rows](../assets/spectrogram_demo.png)

## What you're seeing

Both panels show the same capture: a complex chirp sweeping from −0.4 fs to
+0.4 fs, and a tone at +0.2 fs at half amplitude. Each horizontal line is one
row of the spectrogram: the power spectrum of one 256-sample frame, a new frame
every 64 samples. Time runs down the page.

**The horizontal axis is DC-centred.** Zero hertz is in the middle, and the
edges are ±fs/2 = ±0.5 MHz. A frequency reads straight off the axis, with no
shifting to do.

**The left panel is power.** A unit-amplitude tone reads 1.0 at its bin. The
chirp is a bright diagonal line, because at any instant it sits on one bin at
full scale. The tone is a dimmer horizontal line: half the amplitude is a
quarter of the power.

**The right panel is the same data in dBFS.** Full scale is 0 dB, and the
tone reads −6.02 dB, which is what the amplitude says it should. Use dB when
the levels span a wide range, and power when you want to add rows together:
averaging dB rows gives a different answer from converting the average.

Power rows are the default. A dB spectrogram is asked for by name.

## How it works

Samples arrive in pieces of any size, as they would from a radio, and the
object turns them into rows as the frames complete. Chunk sizes do not change
the rows. The example pushes the stream in random chunks to show this, and
flushes the last partial frame at the end:

--8<-- "src/doppler/examples/spectrogram_demo.py:stream"

The figure is drawn from the rows the same stream produces. The checks in that
function fail the run, not just the picture, if the chirp's peak leaves its
expected bin or the tone's level is wrong:

--8<-- "src/doppler/examples/spectrogram_demo.py:figure"

## Running it

```bash
python src/doppler/examples/spectrogram_demo.py
```

The script writes `spectrogram_demo.png` in the working directory, and exits
0 only when its checks hold.
