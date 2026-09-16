"""Check converter output and selected rejection cases using the official compiler.

These cases run ysc, so they are skipped when it is not on PATH. `make parity`
runs them where ysc is required.
"""

import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = [
    ("tests/vmtest.yarnproject", "vmtest"),
    ("tests/features.yarnproject", "features"),
]


def convert(project, output, prefix, *extra):
    """Run the converter CLI and capture its exit status and diagnostics."""
    command = [sys.executable, str(ROOT / "threadbobber.py"), str(project)]
    command += ["--out", str(output), "--prefix", prefix, *extra]
    return subprocess.run(
        command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=150
    )


@unittest.skipUnless(shutil.which("ysc"), "needs YarnSpinner.Console 3.2.2 on PATH")
class ConversionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="threadbobber-conversion-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def project(self, body):
        """Write a project with one node whose body is `body` and return its path."""
        source = self.directory / "Case.yarn"
        source.write_text("title: Start\n---\n" + body + "\n===\n", encoding="utf-8")
        project = self.directory / "Case.yarnproject"
        project.write_text(
            json.dumps(
                {"projectFileVersion": 4, "sourceFiles": [source.name], "baseLanguage": "en"}
            ),
            encoding="utf-8",
        )
        return project

    def succeed(self, project, output, prefix, *extra):
        process = convert(project, output, prefix, *extra)
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        self.assertEqual(
            {path.name for path in output.iterdir()},
            {prefix + "_program.c", prefix + "_program.h"},
        )

    def test_relocated_fixtures_match_checked_in_programs(self):
        for project, prefix in FIXTURES:
            with self.subTest(project=project):
                relocated = self.directory / "relocated" / prefix
                shutil.copytree(
                    (ROOT / project).parent,
                    relocated,
                    ignore=shutil.ignore_patterns("*_program.*", "*.py", "__pycache__"),
                )
                output = self.directory / "output" / prefix
                self.succeed(relocated / Path(project).name, output, prefix)
                for path in sorted(output.iterdir()):
                    checked_in = (ROOT / project).parent / path.name
                    self.assertEqual(
                        path.read_bytes(),
                        checked_in.read_bytes(),
                        "%s is stale or depends on the checkout location" % path.name,
                    )

    def test_bold_markup_is_rejected_without_creating_output(self):
        output = self.directory / "markup"
        process = convert(self.project("Hello [b]there[/b]. #line:markup"), output, "test")
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("markup", process.stderr)
        self.assertFalse(output.exists())

    def test_program_hash_changes_with_default_or_line_text(self):
        hashes = []
        for default, text in [(1, "Hello"), (2, "Hello"), (2, "Welcome")]:
            body = "<<declare $number = %d>>\n%s {$number} #line:identity" % (default, text)
            output = self.directory / "identity"
            self.succeed(self.project(body), output, "test")
            header = (output / "test_program.h").read_text()
            hashes.append(re.search(r"TEST_PROGRAM_HASH (0x[0-9a-f]+)", header).group(1))
        self.assertEqual(len(set(hashes)), 3, hashes)

    def test_yarnc_and_project_produce_matching_tables(self):
        project = self.project("Hello #line:hello")
        compiled = self.directory / "official"
        compiled.mkdir()
        subprocess.run(
            ["ysc", "compile", str(project), "-o", str(compiled), "-n", "program"],
            check=True,
            capture_output=True,
            timeout=120,
        )
        lines = str(compiled / "program-Lines.csv")
        precompiled = self.directory / "precompiled"
        self.succeed(compiled / "program.yarnc", precompiled, "test", "--lines", lines)
        source = self.directory / "source"
        self.succeed(project, source, "test")
        for filename in ("test_program.c", "test_program.h"):
            # The first line records the input filename (.yarnc or .yarnproject).
            self.assertEqual(
                (precompiled / filename).read_text().splitlines()[1:],
                (source / filename).read_text().splitlines()[1:],
                filename,
            )
        header = (precompiled / "test_program.h").read_text()
        self.assertIn("TEST_NODE_Start=", header)
        self.assertIn("TEST_LINE_hello=", header)
        self.assertIn('"Hello', (precompiled / "test_program.c").read_text())

    def test_sixty_variables_require_expanded_profile(self):
        declarations = "\n".join("<<declare $value%d = %d>>" % (i, i) for i in range(60))
        project = self.project(declarations + "\n{$value59} #line:expanded")
        self.succeed(project, self.directory / "expanded", "expanded", "--profile", "expanded")
        compact = self.directory / "compact"
        process = convert(project, compact, "compact")
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("variables exceed", process.stderr)
        self.assertFalse(compact.exists())

    def test_count_identifiers_do_not_collide_with_enum_totals(self):
        project = self.project("Hello")
        (self.directory / "Case.yarn").write_text(
            "title: COUNT\n---\nHello #line:COUNT\n===\n"
            "title: COUNT_2\n---\nAgain #line:COUNT_2\n===\n",
            encoding="utf-8",
        )
        output = self.directory / "symbols"
        self.succeed(project, output, "test")
        header = (output / "test_program.h").read_text()
        symbols = re.findall(r"\b(TEST_\w+)\s*=", header)
        self.assertEqual(len(symbols), len(set(symbols)), header)
        self.assertIn("TEST_NODE_COUNT_3=", header)
        self.assertIn("TEST_NODE_COUNT_2=", header)
        self.assertIn("TEST_LINE_COUNT_3=", header)
        compiler = shutil.which("gcc")
        if compiler:
            compiled = subprocess.run(
                [compiler, "-std=c11", "-Werror", "-fsyntax-only", "-I" + ROOT.as_posix(),
                 (output / "test_program.c").as_posix()],
                capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)


if __name__ == "__main__":
    unittest.main()
