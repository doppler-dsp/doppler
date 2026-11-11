"""doppler-cli — signal processing pipeline orchestration.

The importable surface is the block API a pipeline is extended with:
`Block`, the base class of a pipeline stage the CLI spawns; `BlockConfig`,
the pydantic model its settings are declared on; and `register`, the
decorator that makes a block reachable by name. They live in
`doppler.cli.blocks` and are exported here (#3005). They need the CLI
extra, ``pip install 'doppler-dsp[cli]'``.

The export is lazy on purpose. Every console script lives under this
package (`doppler-source` and `doppler-fir` need no pydantic, and
`doppler` prints how to install the extra when it is missing), so an
eager import here would load pydantic for all of them and break each one
on an install without the extra. The `TYPE_CHECKING` import is what the
generated reference and type checkers read; at run time `__getattr__`
imports `doppler.cli.blocks` on first use of a name.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from doppler.cli.blocks import Block, BlockConfig, register

__version__ = "0.1.0"

__all__ = ["Block", "BlockConfig", "register"]


def __getattr__(name: str) -> object:
    if name in __all__:
        from doppler.cli import blocks

        return getattr(blocks, name)
    raise AttributeError(f"module 'doppler.cli' has no attribute {name!r}")
