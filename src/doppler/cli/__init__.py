"""doppler-cli — signal processing pipeline orchestration.

The importable surface is the block API a pipeline is extended with:
`Block`, the base class of a pipeline stage the CLI spawns; `BlockConfig`,
the pydantic model its settings are declared on; and `register`, the
decorator that makes a block reachable by name. They live in
`doppler.cli.blocks` and are exported here (#3005). They need the CLI
extra, ``pip install 'doppler-dsp[cli]'``.
"""

from doppler.cli.blocks import Block, BlockConfig, register

__version__ = "0.1.0"

__all__ = ["Block", "BlockConfig", "register"]
