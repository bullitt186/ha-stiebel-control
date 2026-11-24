#!/usr/bin/env python3
"""Every repo path named in the agent instructions and architecture docs must exist (#100).

Checks AGENTS.md, docs/ARCHITECTURE.md and README.md:
- file names in backticks or links (`ha-stiebel-control.h`, [x](docs/CONFIGURATION.md)) must
  exist in the repo, as a path from the root or from esphome/ha-stiebel-control, or as a file
  name anywhere outside archive/;
- the retired canbus id `my_mcp2515` and the removed dispatcher `signal_requests_model.h` must
  not appear.

Placeholders (`<model>`, `yourmodel`, `XX`) and gitignored local files are skipped.
Exit code 0 = ok, 1 = problems found.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ["AGENTS.md", "docs/ARCHITECTURE.md", "README.md"]
SEARCH_DIRS = [ROOT, ROOT / "esphome", ROOT / "esphome" / "ha-stiebel-control"]
FORBIDDEN = ["my_mcp2515", "signal_requests_model.h"]
LOCAL_ONLY = {"secrets.yaml", "esphome/secrets.yaml"}   # gitignored, created by the user
PATH_RE = re.compile(r"(?<![\w/.<>-])((?:[\w.-]+/)*[\w.-]+\.(?:h|cpp|yaml|yml|md|py|json|inc|sh))(?![\w-])")
PLACEHOLDER = re.compile(r"<|>|your|XX|\*|example\.com")


def doc_path(found: str) -> str:
    """Path as written in a doc, without a leading "./" (a leading dot of ".github" stays)."""
    return found.removeprefix("./")


def all_files():
    return {p.name for p in ROOT.rglob("*") if p.is_file() and "archive" not in p.parts and ".git" not in p.parts}


def exists(path: str, names: set) -> bool:
    if path in LOCAL_ONLY:
        return True
    for base in SEARCH_DIRS:
        if (base / path).exists():
            return True
    return "/" not in path and path in names


def main() -> int:
    names = all_files()
    problems = []
    for doc in DOCS:
        text = (ROOT / doc).read_text(encoding="utf-8")
        for word in FORBIDDEN:
            for n, line in enumerate(text.splitlines(), 1):
                if word in line:
                    problems.append(f"{doc}:{n}: mentions retired '{word}'")
        for n, line in enumerate(text.splitlines(), 1):
            for m in PATH_RE.finditer(line):
                path = doc_path(m.group(1))
                if PLACEHOLDER.search(path) or path.startswith(("http", "www.")):
                    continue
                if not exists(path, names):
                    problems.append(f"{doc}:{n}: '{path}' does not exist")
    for p in problems:
        print(p)
    print(f"{len(problems)} problem(s) in {len(DOCS)} files")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
