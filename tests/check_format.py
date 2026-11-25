#!/usr/bin/env python3
"""Lint and format check for C++ and shell scripts (repo-kit CI standard, category 2).

- shellcheck on every tracked shell script outside archive/;
- clang-format (style in .clang-format) on the C++ lines changed against a base, by default
  the merge base with origin/main. The existing code keeps its hand-aligned tables and is not
  reformatted in bulk; new and changed lines follow the style. Vendored code (tests/catch2/,
  the Elster tables under elster/) is not checked.

Usage: python3 tests/check_format.py [BASE]
Exit code 0 = ok, 1 = problems found (clang-format prints the diff to apply).
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP_EXTENSIONS = ("h", "cpp", "inc")
CPP_DIRS = ("esphome", "tests", "tools")
VENDORED = ("tests/catch2/", "esphome/ha-stiebel-control/elster/")
SHELL_EXCLUDED = ("archive/",)


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout


def shell_scripts(tracked: list) -> list:
    """Tracked shell scripts that shellcheck covers."""
    return sorted(p for p in tracked if p.endswith(".sh") and not p.startswith(SHELL_EXCLUDED))


def clang_format_pathspec() -> list:
    """Pathspec for git clang-format: the C++ directories without the vendored code."""
    return [*CPP_DIRS, *(f":(exclude){path}" for path in VENDORED)]


def clang_format_clean(output: str) -> bool:
    """True when git clang-format --diff found nothing to change."""
    text = output.strip()
    return text == "" or text.startswith(("no modified files to format", "clang-format did not modify any files"))


def main() -> int:
    base = sys.argv[1] if len(sys.argv) > 1 else "origin/main"
    ok = True

    scripts = shell_scripts(git("ls-files").splitlines())
    print(f"shellcheck: {len(scripts)} scripts")
    if subprocess.run(["shellcheck", *scripts], cwd=ROOT).returncode != 0:
        ok = False

    merge_base = git("merge-base", base, "HEAD").strip()
    result = subprocess.run(
        ["git", "clang-format", "--diff", f"--extensions={','.join(CPP_EXTENSIONS)}", merge_base,
         "--", *clang_format_pathspec()],
        cwd=ROOT, capture_output=True, text=True)
    print(f"clang-format: C++ lines changed against {base}")
    if result.returncode > 1 or (result.returncode == 0 and result.stderr.strip()):
        print(result.stderr, file=sys.stderr)
    if not clang_format_clean(result.stdout):
        print(result.stdout)
        print("Format the changed lines: git clang-format " + merge_base, file=sys.stderr)
        ok = False

    print("ok" if ok else "problems found")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
