import pathlib

_IGNORE = pathlib.Path(__file__).parent / "docs" / ".doc-snippet-ignore"


def _display_name(fullname: str, name: str) -> str:
    """Short, unique label -- same disambiguation as scripts/bench_report.py's
    _display_name(). Raw pytest-benchmark ``name``s collide across modules
    (every bench_*.py has its own test_bench_step/test_bench_steps_64k/...),
    so derive a ``module::case`` label from ``fullname`` instead."""
    if "::" in fullname:
        mod = fullname.rsplit("::", 1)[0].rsplit("/", 1)[-1]
        mod = mod.removesuffix(".py").removeprefix("bench_")
        case = name.removeprefix("test_bench_").removeprefix("test_")
        return f"{mod}::{case}" if mod else case
    return name


def pytest_terminal_summary(terminalreporter, exitstatus, config):
    # Doc-snippet burn-down backlog: how many doc pages are not yet gated by
    # the drift gate (docs/.doc-snippet-ignore). Printed every run so the
    # number stays visible and shrinks to zero. See docs/dev/doc-examples.md.
    if _IGNORE.exists():
        pending = [
            ln
            for ln in _IGNORE.read_text().splitlines()
            if ln.strip() and not ln.startswith("#")
        ]
        if pending:
            terminalreporter.write_line(
                f"doc-snippet backlog: {len(pending)} page(s) not yet gated "
                f"(docs/.doc-snippet-ignore)"
            )

    session = getattr(config, "_benchmarksession", None)
    if session is None:
        return
    rows = [
        (_display_name(b.fullname, b.name), b.extra_info["MSa_s"])
        for b in session.benchmarks
        if "MSa_s" in b.extra_info
    ]
    if not rows:
        return
    terminalreporter.write_sep("-", "throughput")
    for name, msa in rows:
        terminalreporter.write_line(f"  {name}: {msa:.2f} MSa/s")
