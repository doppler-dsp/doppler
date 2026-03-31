"""
doppler_specan.terminal — real-time ASCII spectrum display.

Renders a spectrum analyzer display in the terminal using ``rich``.
Keyboard controls:

    ←  /  →     Step center frequency left / right by span/10
    -  /  +     Zoom out / in (span × 2 or ÷ 2)
    ↑  /  ↓     Raise / lower reference level by 10 dB
    r           Reset to configured defaults
    q / Ctrl-C  Quit

The display refreshes whenever a new :class:`SpectrumFrame` arrives
from the engine (up to ~30 fps).
"""

from __future__ import annotations

import threading
import time
from typing import Optional

from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.text import Text

from .engine import SpectrumFrame

# Block-drawing characters (1/8 increments, low to high)
_BLOCKS = " ▁▂▃▄▅▆▇█"
_N_LEVELS = len(_BLOCKS) - 1  # 8 sub-character levels per row

# -----------------------------------------------------------------------
# Spectrum renderer
# -----------------------------------------------------------------------

_DISPLAY_COLS = 80  # spectrum columns (frequency bins displayed)
_DISPLAY_ROWS = 16  # dB rows in the plot area


def _render_bar(
    db_values: list[float],
    top_dbm: float,
    bottom_dbm: float,
    width: int,
    height: int,
) -> list[str]:
    """
    Convert a spectrum (dBm list) into a grid of ASCII block characters.

    Returns *height* strings, each of length *width*.  Index 0 is the
    top row (highest dB), index -1 is the bottom row (noise floor).
    """
    db_range = top_dbm - bottom_dbm
    if db_range <= 0:
        db_range = 1.0

    # Down-sample or up-sample db_values to width columns
    n = len(db_values)
    cols: list[float] = []
    for i in range(width):
        src_i = int(i * n / width)
        src_j = max(int((i + 1) * n / width), src_i + 1)
        cols.append(max(db_values[src_i:src_j]))

    # Build the character grid (rows × cols)
    total_levels = height * _N_LEVELS
    grid: list[list[str]] = [[" "] * width for _ in range(height)]

    for col_idx, dbm in enumerate(cols):
        # How many sub-rows is this signal filling from the bottom?
        fill = (dbm - bottom_dbm) / db_range * total_levels
        fill = max(0.0, min(fill, total_levels))

        full_rows = int(fill) // _N_LEVELS
        partial = int(fill) % _N_LEVELS

        # Fill complete rows from the bottom up
        for row in range(full_rows):
            grid[height - 1 - row][col_idx] = _BLOCKS[_N_LEVELS]

        # Partial top row
        if full_rows < height and partial > 0:
            grid[height - 1 - full_rows][col_idx] = _BLOCKS[partial]

    return ["".join(row) for row in grid]


def _freq_label(hz: float) -> str:
    """Format a frequency in Hz as a compact string."""
    if abs(hz) >= 1e9:
        return f"{hz / 1e9:.3g}G"
    if abs(hz) >= 1e6:
        return f"{hz / 1e6:.3g}M"
    if abs(hz) >= 1e3:
        return f"{hz / 1e3:.3g}k"
    return f"{hz:.0f}"


_DB_PREFIX_WIDTH = 6  # "+XXX│" prefix on each row
_PANEL_OVERHEAD = 4  # Rich Panel: 2 borders + 2 padding (one each side)


def _build_display(
    frame: SpectrumFrame,
    top_dbm: float,
    bottom_dbm: float,
    console_width: int,
    height: int = _DISPLAY_ROWS,
) -> Panel:
    """Render a SpectrumFrame as a Rich Panel."""
    width = max(20, console_width - _PANEL_OVERHEAD - _DB_PREFIX_WIDTH)
    # Header line
    half_span = frame.span / 2
    f_start = frame.center_freq - half_span
    f_stop = frame.center_freq + half_span
    rbw_str = _freq_label(frame.rbw) + "Hz"
    span_str = _freq_label(frame.span) + "Hz"
    fc_str = _freq_label(frame.center_freq) + "Hz"

    header = f"  Fc: {fc_str}  Span: {span_str}  RBW: {rbw_str}  Ref: {top_dbm:.0f} dBm"

    # Spectrum grid
    rows = _render_bar(frame.db, top_dbm, bottom_dbm, width, height)

    # dB axis labels (right side)
    db_step = (top_dbm - bottom_dbm) / height
    lines: list[str] = []
    for i, row_chars in enumerate(rows):
        db_label = top_dbm - i * db_step
        lines.append(f"{db_label:+5.0f}│{row_chars}")

    # Frequency axis (└ is part of the dB-prefix column)
    lines.append("     └" + "─" * width)
    f_left = _freq_label(f_start)
    f_mid = _freq_label(frame.center_freq)
    f_right = _freq_label(f_stop)
    mid_pos = (width - len(f_mid)) // 2
    freq_row = (
        "      "
        + f_left
        + " " * max(0, mid_pos - len(f_left))
        + f_mid
        + " " * max(0, width - mid_pos - len(f_mid) - len(f_right))
        + f_right
    )
    lines.append(freq_row)

    body = Text("\n".join(lines), style="green")

    return Panel(
        body,
        title="[bold cyan]doppler specan[/]",
        subtitle=f"[dim]{header}[/]",
        border_style="cyan",
    )


# -----------------------------------------------------------------------
# Terminal runner
# -----------------------------------------------------------------------


class TerminalDisplay:
    """
    Live terminal spectrum display.

    Parameters
    ----------
    engine : SpecanEngine
    cfg : SpecanConfig
    source : Source
    """

    def __init__(self, engine, cfg, source) -> None:
        self._engine = engine
        self._cfg = cfg
        self._source = source
        self._frame: Optional[SpectrumFrame] = None
        self._lock = threading.Lock()
        self._running = False
        self._top_dbm = cfg.level if cfg.level != 0 else 10.0
        self._bottom_dbm = self._top_dbm - _DISPLAY_ROWS * 10

        # Key-step sizes (set from first frame)
        self._freq_step = 0.0
        self._defaults = (cfg.center, cfg.span, cfg.level)

    # ----------------------------------------------------------------
    # DSP thread
    # ----------------------------------------------------------------

    def _dsp_loop(self) -> None:
        block = max(self._engine.block_size, 4096)
        last_fft_size = 0
        while self._running:
            try:
                iq, fs, cf = self._source.read(block)
                frame = self._engine.process(iq, fs, cf)
                if frame is not None:
                    if frame.fft_size != last_fft_size:
                        last_fft_size = frame.fft_size
                        self._source.set_fft_size(frame.fft_size)
                        block = max(self._engine.block_size, 4096)
                    with self._lock:
                        self._frame = frame
                        if self._freq_step == 0.0:
                            self._freq_step = frame.span / 10.0
            except Exception:
                time.sleep(0.05)

    # ----------------------------------------------------------------
    # Keyboard (non-blocking, Linux/macOS)
    # ----------------------------------------------------------------

    def _read_key(self) -> Optional[str]:
        """Non-blocking single-keypress read.  Returns None if no key."""
        import select
        import sys
        import tty
        import termios

        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            if select.select([sys.stdin], [], [], 0.0)[0]:
                ch = sys.stdin.read(1)
                if ch == "\x1b":
                    # Escape sequence
                    if select.select([sys.stdin], [], [], 0.05)[0]:
                        ch2 = sys.stdin.read(1)
                        if ch2 == "[":
                            if select.select([sys.stdin], [], [], 0.05)[0]:
                                ch3 = sys.stdin.read(1)
                                return f"ESC[{ch3}"
                return ch
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)
        return None

    def _handle_key(self, key: str) -> bool:
        """Process a key.  Returns False to quit."""
        step = self._freq_step or 1e3
        if key in ("q", "Q", "\x03"):  # q or Ctrl-C
            return False
        if key == "r":
            self._cfg.center, self._cfg.span, self._cfg.level = self._defaults
            self._top_dbm = self._cfg.level or 10.0
            self._bottom_dbm = self._top_dbm - _DISPLAY_ROWS * 10
            self._engine.zoom(self._cfg.span)
            self._engine.retune(self._cfg.center)
            self._freq_step = 0.0
        elif key == "ESC[D":  # left arrow
            self._cfg.center -= step
            self._engine.retune(self._cfg.center)
        elif key == "ESC[C":  # right arrow
            self._cfg.center += step
            self._engine.retune(self._cfg.center)
        elif key in ("+", "="):
            new_span = (self._cfg.span or step * 10) / 2
            self._cfg.span = max(new_span, 1e3)
            self._engine.zoom(self._cfg.span)
            self._freq_step = 0.0
        elif key == "-":
            new_span = (self._cfg.span or step * 10) * 2
            self._cfg.span = new_span
            self._engine.zoom(self._cfg.span)
            self._freq_step = 0.0
        elif key == "ESC[A":  # up arrow — raise ref level
            self._top_dbm += 10
            self._bottom_dbm += 10
        elif key == "ESC[B":  # down arrow — lower ref level
            self._top_dbm -= 10
            self._bottom_dbm -= 10
        return True

    # ----------------------------------------------------------------
    # Main run loop
    # ----------------------------------------------------------------

    def run(self) -> None:
        """Start DSP thread and enter the Rich live-display loop."""
        self._running = True
        dsp_thread = threading.Thread(target=self._dsp_loop, daemon=True)
        dsp_thread.start()

        console = Console()
        placeholder = Panel(
            Text("Waiting for signal...", style="dim"),
            title="[bold cyan]doppler specan[/]",
            border_style="cyan",
        )

        with Live(
            placeholder, console=console, auto_refresh=False, screen=True
        ) as live:
            try:
                while True:
                    key = self._read_key()
                    if key and not self._handle_key(key):
                        break

                    with self._lock:
                        frame = self._frame

                    if frame is not None:
                        panel = _build_display(
                            frame,
                            self._top_dbm,
                            self._bottom_dbm,
                            console.width,
                        )
                        live.update(panel, refresh=True)
                    time.sleep(1 / 30)
            except KeyboardInterrupt:
                pass
            finally:
                self._running = False
