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


# Registry: name → Block subclass
_REGISTRY: dict[str, Type[Block]] = {}


def register(cls: Type[Block]) -> Type[Block]:
    """Decorator to register a Block subclass by name."""
    _REGISTRY[cls.name] = cls
    return cls


def get(name: str) -> Type[Block]:
    if name not in _REGISTRY:
        available = ", ".join(sorted(_REGISTRY))
        raise KeyError(f"Unknown block {name!r}. Available: {available}")
    return _REGISTRY[name]


def all_blocks() -> dict[str, Type[Block]]:
    return dict(_REGISTRY)
