"""wfm_json_demo.py — JSON as the portable waveform spec format.

The wfmgen JSON schema (``docs/schema/wfmgen.schema.json``) is the canonical,
language-neutral description of a waveform scene.  This demo shows the full
round-trip:

1. **Build** a three-segment scene in Python (tone burst → QPSK burst → chirp
   sweep), each with its own sample rate and timing.
2. **Serialise** to JSON with :meth:`Composer.to_json` — the same string
   ``wfmgen --record`` writes.
3. **Reload** from that JSON string with :meth:`Composer.from_json` — the same
   path ``wfmgen --from-file`` takes.
4. **Verify** byte-identity: the original and reloaded composers produce
   numerically identical IQ samples.
5. **Visualise**: a spectrogram of the full scene, with segment boundaries and
   per-segment annotations, plus the JSON spec printed to the panel title so
   you can paste it straight into a file and feed it back.

The key insight is that ``version``, ``repeat``, and ``continuous`` are
top-level flags; everything else lives inside ``segments``.  A single JSON
file is a complete, self-contained capture recipe — no flags to remember, no
config to reconstruct.

Run::

    python examples/python/wfm_json_demo.py
"""

from __future__ import annotations

import json
import re
import textwrap

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# --8<-- [start:roundtrip]
import numpy as np
from matplotlib.offsetbox import AnchoredOffsetbox, HPacker, TextArea, VPacker

from doppler.wfm import Composer, Segment

# Three self-contained segments at 1 MHz: a tone burst, a QPSK burst,
# and a chirp sweep — each carries its own timing in the JSON spec.
FS = 1e6
tone_seg = Segment(
    "tone",
    fs=FS,
    freq=1.5e5,  # +150 kHz carrier
    snr=100.0,  # clean (no injected noise)
    num_samples=8_000,
    off_samples=2_000,
)
qpsk_seg = Segment(
    "qpsk",
    fs=FS,
    snr=12.0,
    snr_mode="esno",  # 12 dB Es/No
    sps=8,
    pulse="rrc",
    rrc_beta=0.35,
    rrc_span=8,
    num_samples=16_000,
    off_samples=2_000,
)
chirp_seg = Segment(
    "chirp",
    fs=FS,
    freq=-4.0e5,  # sweep −400 kHz → +400 kHz
    f_end=4.0e5,
    snr=100.0,
    num_samples=10_000,
)

composer_a = Composer([tone_seg, qpsk_seg, chirp_seg])
iq_a = np.asarray(composer_a.compose(), dtype=np.complex64)

spec_json = composer_a.to_json()  # → JSON string (same as --record)

composer_b = Composer.from_json(spec_json)  # → same as --from-file
iq_b = np.asarray(composer_b.compose(), dtype=np.complex64)

assert np.array_equal(iq_a, iq_b), "round-trip produced different samples"
# --8<-- [end:roundtrip]


# ── minimal JSON syntax highlighter for the figure's code-block panel ────────
# GitHub-light palette; readable on the light-gray "code" background below.
_JSON_COLOR = {
    "key": "#005cc5",  # object keys (blue)
    "str": "#22863a",  # string values (green)
    "num": "#d73a49",  # numbers (red)
    "bool": "#6f42c1",  # true / false / null (purple)
    "punct": "#57606a",  # braces, brackets, commas, colons (gray)
    "ws": "#57606a",
}
_JSON_TOKEN = re.compile(
    r'(?P<ws>\s+)|(?P<str>"(?:[^"\\]|\\.)*")'
    r"|(?P<num>-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)"
    r"|(?P<bool>true|false|null)|(?P<punct>[{}\[\],:])"
)


def _json_line_spans(line):
    """Tokenise one JSON line into (text, kind) spans; a quoted string is a
    ``key`` when the next non-space token is a colon, else a ``str`` value."""
    toks = list(_JSON_TOKEN.finditer(line))
    spans = []
    for i, m in enumerate(toks):
        kind = m.lastgroup
        if kind == "str":
            nxt = next((t for t in toks[i + 1 :] if t.lastgroup != "ws"), None)
            kind = "key" if (nxt and nxt.group() == ":") else "str"
        spans.append((m.group(), kind))
    return spans


def _highlighted_json_box(display, fontsize):
    """A VPacker of per-line HPackers of colour-coded monospace TextAreas."""
    lines = []
    for line in display.split("\n"):
        spans = _json_line_spans(line) or [(" ", "ws")]
        areas = [
            TextArea(
                text,
                textprops={
                    "color": _JSON_COLOR[kind],
                    "fontsize": fontsize,
                    "family": "monospace",
                },
            )
            for text, kind in spans
        ]
        lines.append(HPacker(children=areas, align="baseline", pad=0, sep=0))
    return VPacker(children=lines, align="left", pad=0, sep=2)


# ── the resolved spec, as a dict for the figure's JSON panel ────────────────
spec = json.loads(spec_json)
assert spec["version"] == 1

# ── step 5: visualise ────────────────────────────────────────────────────────
NFFT = 256
HOP = 64
win = np.hanning(NFFT)
cols = [
    np.fft.fftshift(np.abs(np.fft.fft(iq_a[i : i + NFFT] * win)))
    for i in range(0, len(iq_a) - NFFT, HOP)
]
sgram = 20.0 * np.log10(np.array(cols).T + 1e-9)
sgram -= sgram.max()

t_ms = np.arange(sgram.shape[1]) * HOP / FS * 1e3
f_khz = np.fft.fftshift(np.fft.fftfreq(NFFT, 1 / FS)) / 1e3

# Segment boundary times (ms) — on-time only (off-samples are silent padding).
seg_starts_ms = [0.0]
for seg in spec["segments"][:-1]:
    seg_starts_ms.append(
        seg_starts_ms[-1]
        + (seg["num_samples"] + seg.get("off_samples", 0)) / FS * 1e3
    )
seg_labels = [
    "tone burst\n+150 kHz",
    "QPSK\n12 dB Es/No\nRRC β=0.35",
    "chirp\n−400→+400 kHz",
]

fig, (ax_sg, ax_json) = plt.subplots(
    1, 2, figsize=(14, 6), gridspec_kw={"width_ratios": [2, 1]}
)
fig.suptitle(
    "wfmgen JSON round-trip — to_json() → from_json() → byte-identical",
    fontsize=12,
)

# Spectrogram panel
ax_sg.imshow(
    sgram,
    aspect="auto",
    origin="lower",
    extent=[t_ms[0], t_ms[-1], f_khz[0], f_khz[-1]],
    cmap="magma",
    vmin=-55,
    vmax=0,
)
ax_sg.set_title("Spectrogram of the reconstructed scene (from JSON)")
ax_sg.set_xlabel("time (ms)")
ax_sg.set_ylabel("frequency (kHz)")

for t, label in zip(seg_starts_ms, seg_labels):
    ax_sg.axvline(t, color="cyan", lw=0.8, ls="--", alpha=0.7)
    ax_sg.text(
        t + 0.15,
        f_khz[-1] * 0.88,
        label,
        color="cyan",
        fontsize=7.5,
        va="top",
    )

# JSON spec panel — compact segment objects, wrapped to a panel-safe line
# length so nothing runs off the JSON panel (which would squish the
# spectrogram beside it). The `(", ", ":")` separators put break points only
# BETWEEN key/value pairs, so wrapping never splits a `"key":value` token.
compact_segs = []
for seg in spec["segments"]:
    segf = {
        k: v for k, v in seg.items() if k not in ("seed", "pn_poly", "lfsr")
    }
    # One segment per block: opening brace, `type`, the wrapped remaining
    # attributes, then the closing brace — each on its own line.
    rest = {k: v for k, v in segf.items() if k != "type"}
    rest_json = json.dumps(rest, separators=(", ", ":"))[1:-1]  # strip braces
    wrapped_rest = textwrap.fill(
        rest_json,
        width=50,
        initial_indent="      ",
        subsequent_indent="      ",
        break_long_words=False,
        break_on_hyphens=False,
    )
    compact_segs.append(
        "    {\n"
        f'      "type":{json.dumps(segf.get("type", ""))},\n'
        f"{wrapped_rest}\n"
        "    }"
    )
top = {k: v for k, v in spec.items() if k != "segments"}
spec_display = (
    json.dumps(top, separators=(", ", ":"))[:-1]
    + ',\n  "segments": [\n'
    + ",\n".join(compact_segs)
    + "\n  ]\n}"
)

ax_json.axis("off")
ax_json.text(
    0.5,
    0.99,
    "The resolved JSON spec — paste into a file and feed back:",
    transform=ax_json.transAxes,
    fontsize=10,
    va="top",
    ha="center",
    fontweight="bold",
)
# Syntax-highlighted JSON in a light-gray "code block" box, auto-sized to the
# text and centered in the panel below the heading.
_json_box = AnchoredOffsetbox(
    loc="upper center",
    child=_highlighted_json_box(spec_display, fontsize=9),
    pad=0.5,
    borderpad=0.0,
    frameon=True,
    bbox_to_anchor=(0.5, 0.93),
    bbox_transform=ax_json.transAxes,
)
_json_box.patch.set(
    facecolor="#f3f4f6",
    edgecolor="#d0d7de",
    linewidth=1.0,
    boxstyle="round,pad=0.5",
)
ax_json.add_artist(_json_box)
ax_json.text(
    0.5,
    0.06,
    f"✓  {len(iq_a):,} samples — byte-identical after round-trip",
    transform=ax_json.transAxes,
    fontsize=9,
    va="bottom",
    ha="center",
    color="green",
)

fig.tight_layout(rect=(0, 0, 1, 0.95))
fig.savefig("wfm_json_demo.png", dpi=110)
print("wrote wfm_json_demo.png")
