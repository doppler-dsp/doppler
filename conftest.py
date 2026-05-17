def pytest_terminal_summary(terminalreporter, exitstatus, config):
    session = getattr(config, "_benchmarksession", None)
    if session is None:
        return
    rows = [
        (b.name, b.extra_info["MSa_s"])
        for b in session.benchmarks
        if "MSa_s" in b.extra_info
    ]
    if not rows:
        return
    terminalreporter.write_sep("-", "throughput")
    for name, msa in rows:
        terminalreporter.write_line(f"  {name}: {msa:.2f} MSa/s")
