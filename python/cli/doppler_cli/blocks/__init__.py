"""Block base class and registry."""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import ClassVar, Type

from pydantic import BaseModel


class BlockConfig(BaseModel):
    """Base config for all blocks."""

    pass


class Block(ABC):
    """Base class for all doppler pipeline blocks.

    Subclasses declare a ``name`` and a ``Config`` pydantic model.
    The CLI uses the Config schema to scaffold compose file defaults
    and to validate user-supplied overrides.

    A block has zero or one input port and zero or one output port:

    - source: no input, one output (binds PUSH)
    - chain:  one input (connects PULL), one output (binds PUSH)
    - sink:   one input (connects PULL), no output
    """

    name: ClassVar[str]
    Config: ClassVar[Type[BlockConfig]]

    #: "source" | "chain" | "sink"
    role: ClassVar[str]

    @abstractmethod
    def command(
        self,
        config: BlockConfig,
        input_addr: str | None,
        output_addr: str | None,
    ) -> list[str]:
        """Return the argv list to spawn this block as a subprocess."""
        ...

    def status_lines(self, config: BlockConfig) -> list[str]:
        """Return human-readable status lines printed after the block starts.

        Override in sink/source subclasses to surface URLs, ports, etc.
        """
        return []


# Registry: name → Block subclass
_REGISTRY: dict[str, Type[Block]] = {}


def register(cls: Type[Block]) -> Type[Block]:
    """Decorator to register a Block subclass by name."""
    _REGISTRY[cls.name] = cls
    return cls


def get(name: str) -> Type[Block]:
    if name in _REGISTRY:
        return _REGISTRY[name]
    # Fall back to dopplerfile discovery
    from doppler_cli import dopplerfile  # noqa: PLC0415

    cls = dopplerfile.discover(name)
    if cls is not None:
        return cls
    available = ", ".join(sorted(_REGISTRY))
    raise KeyError(
        f"Unknown block {name!r}. Built-ins: {available}\n"
        f"  Or add a dopplerfile: ~/.doppler/blocks/{name}.yml "
        f"or ./{name}.yml"
    )


def all_blocks() -> dict[str, Type[Block]]:
    return dict(_REGISTRY)
