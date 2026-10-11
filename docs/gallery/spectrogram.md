# A Streaming Spectrogram

![A chirp and a tone as power rows and as dB rows](../assets/spectrogram_demo.png)

## What you're seeing

Both panels show the same capture: a complex chirp sweeping from −0.4 fs to
+0.4 fs, and a tone at +0.2 fs at half amplitude. Each horizontal line is one
row of the spectrogram: the power spectrum of one 256-sample frame, a new frame
every 64 samples. Time runs down the page. The bottom row is the partial frame
flushed at the end, zero-padded to a full frame, so its levels are lower and a
little smeared.

**The horizontal axis is DC-centred.** Zero hertz is in the middle, and the
edges are ±fs/2 = ±0.5 MHz. A frequency reads straight off the axis, with no
shifting to do.

**The left panel is power.** A unit-amplitude tone reads 1.0 at its bin. The
chirp is a bright diagonal line, because at any instant it sits on one bin at
full scale. The tone is a dimmer horizontal line: half the amplitude is a
quarter of the power. Where the chirp crosses the tone, the two add coherently
in that bin, so power there can read above 1.0, past the top of the colour bar.

**The right panel is the same data in dBFS.** Full scale is 0 dB, and the
tone reads −6.02 dB, which is what the amplitude says it should. Each dB row is
10·log10 of the matching power row. Use dB when the levels span a wide range,
and power when you want to add rows together: averaging dB rows gives a
different answer from converting the average.

Power rows are the default. A dB spectrogram is asked for by name.

## How it works

Samples arrive in pieces of any size, as they would from a radio, and the
object turns them into rows as the frames complete. The example pushes the
stream in random chunks, and the script checks that the rows match a single
push of the whole capture. The chunk sizes do not change the rows. At the end
the stream is flushed, which writes the zero-padded partial frame if one is
owed:

```python
--8<-- "src/doppler/examples/spectrogram_demo.py:stream"
```

The figure is drawn from the rows the same stream produces. The checks in that
function fail the run, not just the picture. They confirm that the chirp's
peak stays within one bin of its sweep on every row, including the flushed
partial row. They confirm the tone reads a quarter of full scale and −6.02 dB
on every row the chirp is clear of, and that each dB row is 10·log10 of its
power row. They also confirm the axis edges sit at ±0.5 MHz:

```python
--8<-- "src/doppler/examples/spectrogram_demo.py:figure"
```

## Running it

```bash
python src/doppler/examples/spectrogram_demo.py
```

The script writes `spectrogram_demo.png` in the working directory, and exits
0 only when its checks hold.
