# doppler CLI

Simple intuitive interface to rapidly deploy DSP assets.

## Use cases

- DEMO - One liner to start streaming source and observe on specan (existing)
- Design and Test
    - Streaming sources: configurable stimulus
    - DSP sinks: Cascade of DSP blocks
    - Specan - Measure response: stimulus --> DSP --> specan
    - `doppler-compose.yml` - configure and arrange signal chain

## Implementation

### Package

New uv workspace member `python/cli/` → PyPI package `doppler-cli`.
Depends on `doppler-dsp` + `doppler-specan`. Adds the `doppler` command.

### Command surface (docker-inspired)

```
doppler ps                          # list running chains
doppler stop <ID>                   # graceful shutdown
doppler kill <ID>                   # SIGKILL
doppler logs <ID> [--block NAME]    # stream stdout/stderr
doppler inspect <ID>                # resolved config + PIDs

doppler compose up [FILE]           # start chain from compose file
doppler compose down <ID>           # stop chain
doppler compose init <BLOCKS...>    # scaffold compose file with defaults
```

`doppler-source` and `doppler-specan` are thin wrappers that call
`compose init` + `compose up` under the hood.

### Socket topology

PUSH/PULL pipeline. Each block **binds its output, connects its input**:

```
source   → binds   PUSH :5600
fir      → connects PULL :5600, binds PUSH :5601
specan   → connects PULL :5601
```

Ports are auto-assigned if not provided; explicit if given.
`doppler compose init` always writes fully-resolved ports into the
generated file (inspectable + debuggable).

Port allocation: scan `~/.doppler/chains/` for in-use ports, assign
sequentially from base port 5600 (configurable in
`~/.doppler/config.yml`).

### Compose file format

```yaml
id: a3f7c2
source:
  type: tone
  freq_hz: 1000
  sample_rate: 48000
  port: 5600          # auto-assigned if omitted

chain:
  - fir:
      taps: [...]
      port: 5601      # output port; auto-assigned if omitted

sink:
  type: specan
  # pull-only, no port binding
```

### Chain state

Persisted in `~/.doppler/chains/<ID>.json`:

```json
{
  "id": "a3f7c2",
  "started": "2026-03-31T14:22:00Z",
  "compose": "~/.doppler/chains/a3f7c2.yml",
  "blocks": [
    {"name": "tone",   "pid": 12301, "bind": "tcp://127.0.0.1:5600"},
    {"name": "fir",    "pid": 12302, "connect": "...:5600", "bind": "...:5601"},
    {"name": "specan", "pid": 12303, "connect": "...:5601"}
  ]
}
```

### Block registry

Each block is a Python class with a pydantic config schema. The CLI
discovers available blocks from this registry. `compose init` uses
the schema to populate defaults in the generated file.

```python
class FirConfig(BaseModel):
    taps: list[float] = []

class FirBlock(Block):
    name = "fir"
    Config = FirConfig

    def command(self, config, input_port, output_port): ...
```

### First blocks (MVP)

| Block        | Type   | Notes                                  |
|--------------|--------|----------------------------------------|
| `tone`       | source | wraps existing doppler-source logic    |
| `fir`        | chain  | first real DSP block                   |
| `specan`     | sink   | requires `--connect` mode in specan    |

`doppler-specan` gets a `--connect <addr>` flag so it can sit at the
end of a compose chain (while remaining usable standalone).

### Directory layout

```
python/cli/
  pyproject.toml          # name = "doppler-cli"
  doppler_cli/
    __main__.py
    compose.py            # up / down / init
    ps.py                 # ps / inspect / logs / stop / kill
    state.py              # ~/.doppler/chains/ read/write
    ports.py              # auto port allocation
    blocks/
      __init__.py         # Block base class
      tone.py
      fir.py
      specan.py
```
