"""
Integration tests: CLI entry points give clean install hints when optional
extras are absent, not raw tracebacks.

Strategy: create a fresh venv (stdlib only — no pip, no optional deps),
inject src/ via PYTHONPATH, and invoke each entry point as a subprocess.
This guarantees the outer environment's pydantic/rich/fastapi cannot leak in.
"""

import os
import subprocess
import venv
from pathlib import Path

import pytest

# src/doppler/tests/ → src/
_SRC = Path(__file__).parent.parent.parent


@pytest.fixture(scope="session")
def bare_python(tmp_path_factory) -> Path:
    """
    Python interpreter in a fresh stdlib-only venv.

    ``with_pip=False`` means pip itself is absent, so there is no way for the
    test to accidentally install anything.  The doppler package is made
    importable by setting PYTHONPATH at subprocess launch time.
    """
    d = tmp_path_factory.mktemp("doppler_bare")
    venv.create(str(d), with_pip=False)
    return d / "bin" / "python"


def _run(python: Path, module: str, *args: str) -> subprocess.CompletedProcess:
    """Run ``python -m <module> [args]`` with only stdlib and doppler/src."""
    env = {
        k: v
        for k, v in os.environ.items()
        if k not in ("PYTHONPATH", "VIRTUAL_ENV", "VIRTUAL_ENV_PROMPT")
    }
    env["PYTHONPATH"] = str(_SRC)
    return subprocess.run(
        [str(python), "-m", module, *args],
        capture_output=True,
        text=True,
        env=env,
    )


# ---------------------------------------------------------------------------
# doppler CLI — requires doppler-dsp[cli] (pydantic, pyyaml, rich)
# ---------------------------------------------------------------------------


class TestCLIMissingExtra:
    def test_exits_nonzero(self, bare_python):
        r = _run(bare_python, "doppler.cli.__main__", "ps")
        assert r.returncode != 0

    def test_no_traceback(self, bare_python):
        r = _run(bare_python, "doppler.cli.__main__", "ps")
        assert "Traceback" not in r.stderr

    def test_names_install_command(self, bare_python):
        r = _run(bare_python, "doppler.cli.__main__", "ps")
        assert "pip install 'doppler-dsp[cli]'" in r.stderr


# ---------------------------------------------------------------------------
# doppler-specan terminal — requires doppler-dsp[specan] (rich)
# ---------------------------------------------------------------------------


class TestSpecanTerminalMissingExtra:
    def test_exits_nonzero(self, bare_python):
        r = _run(bare_python, "doppler.specan.__main__", "--source", "demo")
        assert r.returncode != 0

    def test_no_traceback(self, bare_python):
        r = _run(bare_python, "doppler.specan.__main__", "--source", "demo")
        assert "Traceback" not in r.stderr

    def test_names_install_command(self, bare_python):
        r = _run(bare_python, "doppler.specan.__main__", "--source", "demo")
        assert "pip install 'doppler-dsp[specan]'" in r.stderr


# ---------------------------------------------------------------------------
# doppler-specan --web — requires doppler-dsp[specan-web] (fastapi, uvicorn)
# ---------------------------------------------------------------------------


class TestSpecanWebMissingExtra:
    def test_exits_nonzero(self, bare_python):
        r = _run(bare_python, "doppler.specan.__main__", "--web")
        assert r.returncode != 0

    def test_no_traceback(self, bare_python):
        r = _run(bare_python, "doppler.specan.__main__", "--web")
        assert "Traceback" not in r.stderr

    def test_names_install_command(self, bare_python):
        r = _run(bare_python, "doppler.specan.__main__", "--web")
        assert "pip install 'doppler-dsp[specan-web]'" in r.stderr
