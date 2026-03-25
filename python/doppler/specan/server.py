"""
FastAPI + WebSocket spectrum analyzer server.

Endpoints
---------
GET  /              → Serve index.html (the spectrum analyzer UI)
WS   /ws            → Stream FFT frames as JSON
POST /tune          → Update center frequency and tone list
GET  /state         → Return current SpecanState as JSON
"""

from __future__ import annotations

import asyncio
import json
import logging
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles

from .signal import SpecanState, generate_frame

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

app = FastAPI(title="doppler specan", docs_url=None, redoc_url=None)

_STATIC = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=_STATIC), name="static")

# Shared state (single-session, in-process)
_state = SpecanState()

# ---------------------------------------------------------------------------
# HTTP routes
# ---------------------------------------------------------------------------


@app.get("/", response_class=HTMLResponse)
async def index():
    return (_STATIC / "index.html").read_text()


@app.get("/state")
async def get_state():
    return {
        "sample_rate": _state.sample_rate,
        "center_freq": _state.center_freq,
        "fft_size": _state.fft_size,
        "tone_freq": _state.tone_freq,
        "tone_amp_db": _state.tone_amp_db,
        "noise_floor_db": _state.noise_floor_db,
    }


@app.post("/tune")
async def tune(body: dict):
    """
    Update signal parameters.

    Body keys (all optional):
      center_freq    float  Hz offset from DC
      fft_size       int    power of two, 64–65536
      tone_freq      float  normalised [-0.5, 0.5)
      tone_amp_db    float  dBFS
      noise_floor_db float  dBFS
    """
    if "center_freq" in body:
        _state.center_freq = float(body["center_freq"])
    if "fft_size" in body:
        fft_size = int(body["fft_size"])
        if fft_size & (fft_size - 1) == 0 and 64 <= fft_size <= 65536:
            _state.fft_size = fft_size
    if "tone_freq" in body:
        _state.tone_freq = float(body["tone_freq"])
    if "tone_amp_db" in body:
        _state.tone_amp_db = float(body["tone_amp_db"])
    if "noise_floor_db" in body:
        _state.noise_floor_db = float(body["noise_floor_db"])
    return {"ok": True}


# ---------------------------------------------------------------------------
# WebSocket — stream FFT frames
# ---------------------------------------------------------------------------

_FRAME_RATE = 30   # target frames per second
_FRAME_DT   = 1.0 / _FRAME_RATE


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    log.info("specan client connected")
    try:
        while True:
            t0 = asyncio.get_event_loop().time()

            # Generate frame in a thread pool so the event loop stays free
            loop = asyncio.get_event_loop()
            db = await loop.run_in_executor(None, generate_frame, _state)

            payload = json.dumps({
                "fft_size": _state.fft_size,
                "sample_rate": _state.sample_rate,
                "center_freq": _state.center_freq,
                "db": db,
            })
            await ws.send_text(payload)

            # Check for incoming tune commands over the same socket
            try:
                msg = await asyncio.wait_for(ws.receive_text(), timeout=0.001)
                cmd = json.loads(msg)
                if "center_freq" in cmd:
                    _state.center_freq = float(cmd["center_freq"])
                if "fft_size" in cmd:
                    fft_size = int(cmd["fft_size"])
                    if fft_size & (fft_size - 1) == 0 and \
                            64 <= fft_size <= 65536:
                        _state.fft_size = fft_size
                if "tone_freq" in cmd:
                    _state.tone_freq = float(cmd["tone_freq"])
                if "tone_amp_db" in cmd:
                    _state.tone_amp_db = float(cmd["tone_amp_db"])
                if "noise_floor_db" in cmd:
                    _state.noise_floor_db = float(cmd["noise_floor_db"])
            except asyncio.TimeoutError:
                pass

            elapsed = asyncio.get_event_loop().time() - t0
            await asyncio.sleep(max(0.0, _FRAME_DT - elapsed))

    except WebSocketDisconnect:
        log.info("specan client disconnected")


# ---------------------------------------------------------------------------
# Entry point (called by __main__.py)
# ---------------------------------------------------------------------------

def _is_wsl() -> bool:
    try:
        return "microsoft" in Path("/proc/version").read_text().lower()
    except OSError:
        return False


def main(
    host: str = "127.0.0.1",
    port: int = 8765,
    open_browser: bool = True,
) -> None:
    import uvicorn  # noqa: PLC0415

    url = f"http://{host}:{port}"
    print(f"  doppler specan  →  {url}")

    if open_browser and not _is_wsl():
        import threading   # noqa: PLC0415
        import webbrowser  # noqa: PLC0415
        threading.Timer(1.0, lambda: webbrowser.open(url)).start()

    # ws_ping_interval=None disables uvicorn's keepalive pings, which
    # trigger an AssertionError in websockets >= 13 legacy protocol.
    uvicorn.run(
        app, host=host, port=port,
        log_level="warning",
        ws_ping_interval=None,
    )
