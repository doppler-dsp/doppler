"""
FastAPI + WebSocket spectrum analyzer server (web mode).

Endpoints
---------
GET  /              → Serve index.html
WS   /ws            → Stream SpectrumFrame JSON at ~30 fps
POST /tune          → Update center, span, rbw, level
GET  /state         → Return current config as JSON
"""

from __future__ import annotations

import asyncio
import concurrent.futures
import json
import logging
from pathlib import Path
from typing import TYPE_CHECKING

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles

if TYPE_CHECKING:
    from doppler.specan.config import SpecanConfig
    from doppler.specan.engine import SpectrumFrame

log = logging.getLogger(__name__)

# -----------------------------------------------------------------------
# Application
# -----------------------------------------------------------------------

app = FastAPI(title="doppler specan", docs_url=None, redoc_url=None)

_STATIC = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=_STATIC), name="static")

# Engine and source are injected at startup via main()
_engine = None
_source = None
_cfg = None

# -----------------------------------------------------------------------
# HTTP routes
# -----------------------------------------------------------------------


@app.get("/", response_class=HTMLResponse)
async def index() -> str:
    return (_STATIC / "index.html").read_text()


@app.get("/state")
async def get_state() -> dict:
    if _cfg is None:
        return {}
    from doppler.specan.source import DemoSource

    state: dict = {
        "source": _cfg.source,
        "center": _cfg.center,
        "span": _cfg.span,
        "rbw": _cfg.rbw,
        "level": _cfg.level,
    }
    if isinstance(_source, DemoSource):
        t0 = _source._tones[0]
        state["demo"] = {
            "tone_freq": t0["fn"],
            "tone_power": t0["dbm"],
            "noise_floor": _source._noise_floor_dbm,
            "tones": _source.get_tones(),
        }
    return state


async def _apply_cmd(cmd: dict) -> None:
    """Apply a control command (from HTTP POST or WebSocket)."""
    from doppler.specan.source import DemoSource

    if _engine is None:
        return
    if "center" in cmd:
        _cfg.center = float(cmd["center"])
        _engine.retune(_cfg.center)
    if "span" in cmd:
        _cfg.span = float(cmd["span"])
        _engine.zoom(_cfg.span)
    if "rbw" in cmd:
        _cfg.rbw = float(cmd["rbw"])
        _engine.zoom(_cfg.span)
    if "level" in cmd:
        _cfg.level = float(cmd["level"])
    if isinstance(_source, DemoSource):
        if "tone_freq" in cmd:
            _source.set_tone_freq(float(cmd["tone_freq"]))
        if "tone_power" in cmd:
            _source.set_tone_power(float(cmd["tone_power"]))
        if "noise_floor" in cmd:
            _source.set_noise_floor(float(cmd["noise_floor"]))
        if "chirp" in cmd:
            # true → default sweep rate; false → off; float → explicit rate
            v = cmd["chirp"]
            if v is True:
                _source.set_chirp(0.025)
            elif v is False or v == 0:
                _source.set_chirp(0.0)
            else:
                _source.set_chirp(float(v))
        if "add_tone" in cmd:
            v = cmd["add_tone"]
            freq_hz = float(v.get("freq_hz", 0.0))
            dbm = float(v.get("power_dbm", -20.0))
            # NCO runs at input rate; fn is offset from DC normalised
            # to input sample rate, not the (possibly decimated) fs_out.
            fn = (freq_hz - _source._cf) / _source._fs
            _source.add_tone(fn, dbm)
        if "remove_tone" in cmd:
            _source.remove_tone(int(cmd["remove_tone"]))


@app.post("/tune")
async def tune(body: dict) -> dict:
    """
    Update display or demo parameters.

    Body keys (all optional):
      center      float  Hz
      span        float  Hz
      rbw         float  Hz
      level       float  dBm
      tone_freq   float  normalised [0, 1)  (demo source only)
      tone_power  float  dBm               (demo source only)
      noise_floor float  dBm               (demo source only)
    """
    if _engine is None:
        return {"ok": False, "error": "engine not ready"}
    await _apply_cmd(body)
    return {"ok": True}


# -----------------------------------------------------------------------
# WebSocket — stream frames
# -----------------------------------------------------------------------

# Single-worker executor: stream connections are not thread-safe; they must
# be created and used in the same OS thread.
_dsp_executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)

_FRAME_RATE = 30
_FRAME_DT = 1.0 / _FRAME_RATE


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket) -> None:
    await ws.accept()
    log.info("specan client connected")
    try:
        while True:
            t0 = asyncio.get_event_loop().time()

            frame = await asyncio.get_event_loop().run_in_executor(
                _dsp_executor, _next_frame
            )

            if frame is not None:
                from doppler.specan.source import DemoSource

                tones = (
                    _source.get_tones()
                    if isinstance(_source, DemoSource)
                    else []
                )
                payload = json.dumps(
                    {
                        "fft_size": frame.fft_size,
                        "fs_out": frame.fs_out,
                        "center_freq": frame.center_freq,
                        "span": frame.span,
                        "rbw": frame.rbw,
                        "db": frame.db,
                        "peaks": [
                            {"freq_hz": p.freq_hz, "db": p.db}
                            for p in frame.peaks
                        ],
                        "tones": tones,
                    }
                )
                await ws.send_text(payload)

            # Non-blocking check for incoming tune commands
            try:
                msg = await asyncio.wait_for(ws.receive_text(), timeout=0.001)
                await _apply_cmd(json.loads(msg))
            except asyncio.TimeoutError:
                pass

            elapsed = asyncio.get_event_loop().time() - t0
            await asyncio.sleep(max(0.0, _FRAME_DT - elapsed))

    except WebSocketDisconnect:
        log.info("specan client disconnected")


def _next_frame() -> SpectrumFrame | None:
    """Read one source block and return a SpectrumFrame (or None)."""
    if _engine is None or _source is None:
        return None
    try:
        iq, fs, cf = _source.read(_engine.block_size)
        return _engine.process(iq, fs, cf)
    except Exception as exc:
        log.warning("frame error: %s", exc)
        return None


# -----------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------


def _is_wsl() -> bool:
    try:
        return "microsoft" in Path("/proc/version").read_text().lower()
    except OSError:
        return False


def main(
    host: str = "127.0.0.1",
    port: int = 8765,
    open_browser: bool = True,
    cfg: SpecanConfig | None = None,
) -> None:
    global _engine, _source, _cfg

    import uvicorn

    from doppler.specan.engine import SpecanEngine
    from doppler.specan.source import make_source

    _cfg = cfg
    _source = make_source(cfg)
    _engine = SpecanEngine(cfg)

    url = f"http://{host}:{port}"
    print(f"  doppler specan  →  {url}")

    if open_browser and not _is_wsl():
        import threading
        import webbrowser

        threading.Timer(1.0, lambda: webbrowser.open(url)).start()

    uvicorn.run(
        app,
        host=host,
        port=port,
        log_level="warning",
        ws_ping_interval=None,
    )
