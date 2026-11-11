"""The console scripts still start on an install without the [cli] extra.

`doppler.cli` exports the block API (#3005), which lives in
`doppler.cli.blocks` and imports pydantic, an optional `[cli]` dependency.
Every console script is a module under `doppler.cli`, so an eager export
would load pydantic for all of them: `doppler-source` and `doppler-fir`,
which need none, would fail to import, and `doppler` would raise a bare
ImportError instead of saying which extra to install. CI cannot see it,
because the dev group always has pydantic; these cases block it.

Each runs in a subprocess so the module cache starts clean.
"""

from __future__ import annotations

import subprocess
import sys

import pytest

#: A `None` in sys.modules makes `import pydantic` raise ImportError, which
#: is what an install without the extra looks like to the importer.
BLOCK = "import sys; sys.modules['pydantic'] = None; "


def _run(code: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, "-c", BLOCK + code],
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.mark.parametrize("module", ["doppler.cli.source", "doppler.cli.fir"])
def test_a_script_that_needs_no_pydantic_imports(module: str) -> None:
    r = _run(f"import {module}")
    assert r.returncode == 0, r.stderr


def test_doppler_says_which_extra_to_install() -> None:
    r = _run(
        "import runpy; runpy.run_module('doppler.cli', run_name='__main__')"
    )
    assert r.returncode == 1, r.stderr
    assert "pip install 'doppler-dsp[cli]'" in r.stderr
    assert "Traceback" not in r.stderr


def test_the_block_api_resolves_on_first_use() -> None:
    """With pydantic present the lazy export is the real class."""
    import doppler.cli
    from doppler.cli import blocks

    assert doppler.cli.Block is blocks.Block
    assert doppler.cli.register is blocks.register
    with pytest.raises(AttributeError):
        _ = doppler.cli.NoSuchName
