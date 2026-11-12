# jm:generated test_project.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
"""The doppler package as a whole: it imports, and states its version.

just-makeit writes this test with the project, so ``make test`` runs at least
one test before any component exists -- an empty suite passes and proves
nothing, and a newer unittest fails it outright. Each component adds
its own ``test_<component>.py`` beside this one.
"""

import unittest

import doppler

#: ``[project] version`` in just-makeit.toml. ``jm config version`` rewrites
#: this line together with ``__version__`` in the package's ``__init__.py``.
MANIFEST_VERSION = "0.83.0"


class TestProject(unittest.TestCase):
    def test_version_is_a_nonempty_string(self):
        """`__init__.py` computes `__version__`, so the package, not
        the manifest, says what it is: here, that it says one."""
        self.assertIsInstance(doppler.__version__, str)
        self.assertTrue(doppler.__version__)
