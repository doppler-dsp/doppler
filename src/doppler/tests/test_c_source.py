"""`scripts/_c_source.py`: how every gate reads C source.

Two gates read C through it, `check_tests_ssot.py` and
`check_tlm_name_join.py`, and each failure mode below has shipped in a
private copy before it was shared. The module's doctests run here too:
nothing else runs doctests under `scripts/`, so without this they would be
prose.
"""

from __future__ import annotations

import doctest
import importlib.util
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from types import ModuleType

REPO = repo_root(__file__)


def _module() -> ModuleType:
    spec = importlib.util.spec_from_file_location(
        "_t_c_source", REPO / "scripts" / "_c_source.py"
    )
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


C = _module()


def test_the_doctests_hold() -> None:
    result = doctest.testmod(C)
    assert result.attempted > 0
    assert result.failed == 0


def test_a_comment_opener_inside_a_string_is_not_a_comment() -> None:
    code = C.strip_comments('a = "x/*y"; b = 1; /* real */ c = "*/";')
    assert "b = 1;" in code
    assert "real" not in code


def test_an_unterminated_char_literal_stops_at_its_line() -> None:
    """The #1944 review's miss: "it's" in an #if 0 block."""
    code = C.strip_comments('#if 0\nit\'s\n#endif\nf ("%s.%s");\n')
    assert C.string_literals(code) == [(4, "%s.%s")]


def test_adjacent_literals_are_one_across_lines() -> None:
    assert C.string_literals('f ("%s"\n   "."\n "e");') == [(1, "%s.e")]


def test_code_between_literals_keeps_them_apart() -> None:
    assert C.string_literals('f ("a", "b");') == [(1, "a"), (1, "b")]


def test_a_char_literal_quote_opens_no_string() -> None:
    assert C.string_literals('c = \'"\'; s = "x";') == [(1, "x")]


def test_escaped_quotes_stay_inside() -> None:
    assert C.string_literals(r'f ("a\"b");') == [(1, r"a\"b")]


def test_lines_after_a_continuation_are_counted() -> None:
    code = 'f ("a\\\nb");\ng ("c");'
    assert C.string_literals(code) == [(1, "a\\\nb"), (3, "c")]
