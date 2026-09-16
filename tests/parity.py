#!/usr/bin/env python3
"""Compare dialogue event traces with the official Yarn Spinner runtime.

Each scenario runs a node of tests/features.yarn in the pinned Yarn Spinner
assemblies (tests/OfficialTrace) and in ThreadBobber (tests/driver.c), in both
capacity profiles. Traces contain line and option IDs, substitutions, option
availability, command text and completion. The Fairness scenario ignores event
order. Choices are fed in order; once they run out, the first available option
is chosen. Trace mismatches are collected and produce a nonzero exit status.

Needs ysc 3.2.2 on PATH, .NET 10 and a C compiler.
"""

import argparse
import difflib
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCENARIOS = [
    ("OptionExpressions", []),
    ("CommandExpressions", []),
    ("Numbers", []),
    ("SmartVariables", []),
    ("Saliency", []),
    ("Fairness", []),
    ("Enums", []),
    ("Strings", []),
    ("StringStress", []),
    ("Expressions", []),
    ("Modulo", []),
    ("Visits", []),
    ("ParityStart", [0, 0]),
    ("ParityStart", [0, 1]),
    ("ParityStart", [2]),
    ("ParityDetour", [0]),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ysc-directory", type=Path, help="Directory containing YarnSpinner.dll")
    parser.add_argument("--dotnet", default="dotnet")
    parser.add_argument("--cc", default="gcc")
    args = parser.parse_args()
    if not args.ysc_directory:
        executable = shutil.which("ysc")
        candidates = (
            []
            if not executable
            else list(
                (
                    Path(executable).resolve().parent / ".store" / "yarnspinner.console" / "3.2.2"
                ).glob("**/YarnSpinner.dll")
            )
        )
        if len(candidates) != 1:
            parser.error("Pass --ysc-directory with the directory containing YarnSpinner.dll.")
        args.ysc_directory = candidates[0].parent
    work = ROOT / "build" / "parity"
    work.mkdir(parents=True, exist_ok=True)
    env = {k.upper(): v for k, v in os.environ.items()}

    def run(command):
        result = subprocess.run(
            [str(x) for x in command],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=180,
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        return result.stdout

    version = run(["ysc", "--version"]).strip()
    if version.split("+")[0] != "3.2.2":
        raise RuntimeError("Expected YarnSpinner.Console 3.2.2, found " + version)
    run(["ysc", "compile", "tests/features.yarnproject", "-o", work, "-n", "features"])
    official = work / "official"
    run(
        [
            args.dotnet,
            "build",
            "tests/OfficialTrace/OfficialTrace.csproj",
            "--configuration",
            "Release",
            "--output",
            official,
            "-p:YscDirectory=" + str(args.ysc_directory.resolve()),
            "-p:BaseIntermediateOutputPath=" + str(work / "obj") + "/",
            "--nologo",
        ]
    )
    official_trace = [args.dotnet, official / "OfficialTrace.dll", work / "features.yarnc"]
    failures = []
    for profile in ("compact", "expanded"):
        exe = work / ("driver-" + profile + ".exe")
        flags = [] if profile == "compact" else ["-DTHREADBOBBER_PROFILE_EXPANDED"]
        run(
            [
                args.cc,
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                *flags,
                "-I.",
                "-Itests",
                "tests/driver.c",
                "tests/features_program.c",
                "-lm",
                "-o",
                exe,
            ]
        )
        for node, choices in SCENARIOS:
            label = "%s: %s %s" % (profile, node, choices)
            arguments = [node, *map(str, choices)]
            try:
                expected = run(official_trace + arguments)
                actual = run([exe, *arguments])
            except RuntimeError as error:
                failures.append(label)
                print(label + " did not run:\n" + str(error))
                continue
            if node == "Fairness":
                # Either tied item may run first; both must run once across two selections.
                expected = "\n".join(sorted(expected.splitlines()))
                actual = "\n".join(sorted(actual.splitlines()))
            if expected != actual:
                failures.append(label)
                print(label + " differs from Yarn Spinner 3.2.2:")
                sys.stdout.writelines(
                    difflib.unified_diff(
                        expected.splitlines(True),
                        actual.splitlines(True),
                        fromfile="Yarn Spinner",
                        tofile="ThreadBobber",
                    )
                )
                continue
            print(label + " matches Yarn Spinner 3.2.2")
    if failures:
        sys.exit(
            "%d of %d scenario runs differ from Yarn Spinner 3.2.2."
            % (len(failures), 2 * len(SCENARIOS))
        )


if __name__ == "__main__":
    main()
