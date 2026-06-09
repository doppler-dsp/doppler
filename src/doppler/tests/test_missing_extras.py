"""
Integration tests: CLI entry points give clean install hints when optional
extras are absent, not raw tracebacks.

Strategy: invoke each entry point as a subprocess of *this* interpreter, but
make the optional extras (pydantic/pyyaml/rich/fastapi/uvicorn/websockets)
unimportable via a ``sitecustomize`` import-blocker injected on PYTHONPATH.
This reproduces a stdlib-only "bare" install exactly while staying robust:
an earlier approach spun up a nested ``venv.create`` interpreter, but a venv
created *under* a uv-managed python-build-standalone interpreter (CI's 3.9/
3.10 rows) fails to bootstrap its own stdlib (``No module named 'encodings'``)
and dies before the CLI's install-hint can print. Blocking imports in the
working interpreter sidesteps that and tests identically on every version.
"""

import os
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

# src/doppler/tests/ → src/
_SRC = Path(__file__).parent.parent.parent

# Top-level packages backing the optional extras — the union of everything any
# entry point may need. With all of them blocked, each entry point reports the
# single extra it is missing, exactly as on a stdlib-only install.
_BLOCKED = ("pydantic", "yaml", "rich", "fastapi", "uvicorn", "websockets")


@pytest.fixture(scope="session")
def bare_pythonpath(tmp_path_factory) -> str:
    """
    A PYTHONPATH that makes doppler importable but the optional extras absent.

    A generated ``sitecustomize.py`` installs a meta-path finder that raises
    ``ModuleNotFoundError`` for the extra packages, so ``import pydantic`` (etc.)
    fails just as it would if the extra were never installed — regardless of
    what the outer environment actually has on disk.
    """
    d = tmp_path_factory.mktemp("doppler_blocker")
    (d / "sitecustomize.py").write_text(
        textwrap.dedent(
            f"""
            import sys

            _BLOCKED = {_BLOCKED!r}

            class _Blocker:
                def find_spec(self, name, path=None, target=None):
                    if name.split(".")[0] in _BLOCKED:
                        raise ModuleNotFoundError(
                            "No module named %r" % name.split(".")[0]
                        )
                    return None

            sys.meta_path.insert(0, _Blocker())
            """
        )
    )
    # Blocker dir first so its sitecustomize wins; src so doppler imports.
    return os.pathsep.join([str(d), str(_SRC)])


def _run(
    bare_pythonpath: str, module: str, *args: str
) -> subprocess.CompletedProcess:
    """Run ``python -m <module> [args]`` with the extras blocked."""
    env = {
        k: v
        for k, v in os.environ.items()
        if k not in ("PYTHONPATH", "VIRTUAL_ENV", "VIRTUAL_ENV_PROMPT")
    }
    env["PYTHONPATH"] = bare_pythonpath
    return subprocess.run(
        [sys.executable, "-m", module, *args],
        capture_output=True,
        text=True,
        env=env,
    )


# ---------------------------------------------------------------------------
# doppler CLI — requires doppler-dsp[cli] (pydantic, pyyaml, rich)
# ---------------------------------------------------------------------------


class TestCLIMissingExtra:
    def test_exits_nonzero(self, bare_pythonpath):
        r = _run(bare_pythonpath, "doppler.cli.__main__", "ps")
        assert r.returncode != 0

    def test_no_traceback(self, bare_pythonpath):
        r = _run(bare_pythonpath, "doppler.cli.__main__", "ps")
        assert "Traceback" not in r.stderr

    def test_names_install_command(self, bare_pythonpath):
        r = _run(bare_pythonpath, "doppler.cli.__main__", "ps")
        assert "pip install 'doppler-dsp[cli]'" in r.stderr


# ---------------------------------------------------------------------------
# doppler-specan terminal — requires doppler-dsp[specan] (rich)
# ---------------------------------------------------------------------------


class TestSpecanTerminalMissingExtra:
    def test_exits_nonzero(self, bare_pythonpath):
        r = _run(
            bare_pythonpath, "doppler.specan.__main__", "--source", "demo"
        )
        assert r.returncode != 0

    def test_no_traceback(self, bare_pythonpath):
        r = _run(
            bare_pythonpath, "doppler.specan.__main__", "--source", "demo"
        )
        assert "Traceback" not in r.stderr

    def test_names_install_command(self, bare_pythonpath):
        r = _run(
            bare_pythonpath, "doppler.specan.__main__", "--source", "demo"
        )
        assert "pip install 'doppler-dsp[specan]'" in r.stderr


# ---------------------------------------------------------------------------
# doppler-specan --web — requires doppler-dsp[specan-web] (fastapi, uvicorn)
# ---------------------------------------------------------------------------


class TestSpecanWebMissingExtra:
    def test_exits_nonzero(self, bare_pythonpath):
        r = _run(bare_pythonpath, "doppler.specan.__main__", "--web")
        assert r.returncode != 0

    def test_no_traceback(self, bare_pythonpath):
        r = _run(bare_pythonpath, "doppler.specan.__main__", "--web")
        assert "Traceback" not in r.stderr

    def test_names_install_command(self, bare_pythonpath):
        r = _run(bare_pythonpath, "doppler.specan.__main__", "--web")
        assert "pip install 'doppler-dsp[specan-web]'" in r.stderr
